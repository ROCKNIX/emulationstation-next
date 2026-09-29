#include "AsyncNotificationComponent.h"
#include "math/Misc.h"
#include "ThemeData.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "LocaleES.h"
#include "Window.h"
// The choosing itself is pure and lives with the rest of the cloud text, so
// a test binary can reach it (es-app/tests/unit). Reached the way Window.cpp
// reaches ApiSystem: es-core's include path does not carry es-app.
#include "../../es-app/src/CloudText.h"
#include <SDL_timer.h>

#define PADDING_PX  (Renderer::getScreenWidth()*0.01)

// How solid the card is over whatever it covers.
//
// It was 200 of 255, and at that weight the themed panel -- 0x111111 in the
// shipped theme -- washes out to roughly a fifth of the game art behind it, so
// a progress line sat on a mid-grey smear that changed with the artwork. Every
// other themed surface in the app (menus, dialogs, and GuiInfoPopup once its
// fade-in completes) draws its background at full opacity and lets the theme
// decide how solid to be; this was the one that did not, and it is the one
// that is hard to read.
//
// Full opacity is therefore the standard, not a new choice. The card still
// reads as an overlay rather than a page: frame.png's corners fade to nothing,
// so the panel keeps its soft rounded edge, and the fade in and out below
// still animates through this value.
#define NOTIFICATION_OPACITY  255

AsyncNotificationComponent::AsyncNotificationComponent(Window* window, bool actionLine)
	: GuiComponent(window)
{
	mRunning = true;
	mFadeTime = 0;
	mFadeOut = 0;
	mPercent = -1;
	mClosing = false;

	auto theme = ThemeData::getMenuTheme();

	// Note : Don't localize this text -> It is only used to guess width calculation for the component.
	//
	// Wide enough for what actually goes in it. The card was sized to fit
	// thirty characters, which suits "Scraping: <short name>" and truncates
	// everything else -- an rclone stats line ("Transferred: 12.3 MiB / 45.6
	// MiB, 27%, 1.2 MiB/s, ETA 27s") is twice that, so the one number
	// somebody wants was always off the end. A share of the screen rather
	// than a character count, because these panels run from 640x480 to
	// 1920x1080 and a fixed width is right on exactly one of them; the
	// measured width stays as the floor so nothing gets narrower than it was.
	//
	// 0.9 is GuiInfoPopup's cap -- the widest this app lets a non-blocking
	// overlay get -- and taking it settles a question the in-between sizes
	// kept asking. Past half the screen a card pinned to one corner stops
	// reading as a deliberate placement and starts reading as a panel that
	// failed to fit; at the same width centred (see renderAsyncNotifications)
	// it reads as one. It also puts this card where the message that follows
	// it already appears, so nothing jumps.
	float width = Math::max(
		theme->TextSmall.font->sizeText("TEXT FOR SIZE CALCULATION TEST").x(),
		Renderer::getScreenWidth() * 0.9f);

	mTitle = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->TextSmall.color, ALIGN_LEFT);
	mGameName = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);

	if (actionLine)
		mAction = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);

	Vector2f fullSize(width + 2 * PADDING_PX, 2 * PADDING_PX + mTitle->getSize().y() + mGameName->getSize().y() + (mAction == nullptr ? 0 : mAction->getSize().y()));
	Vector2f gridSize(width, mTitle->getSize().y() + mGameName->getSize().y() + (mAction == nullptr ? 0 : mAction->getSize().y()));

	setSize(fullSize);
	mFullSize = fullSize;

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(theme->Background.path);
	// The theme's own edge colour, like every other panel in the app.
	//
	// A drawn-in rim was tried here and was wrong: this card is what a
	// RetroAchievements sync, a scrape and a theme install all appear in, and
	// giving it a stroke no menu or dialog has made those look like a
	// different application's widget. The card was hard to read because it was
	// translucent, not because it lacked an outline -- NOTIFICATION_OPACITY
	// above is the fix that was actually needed, and a long transfer that
	// really does need to dominate the screen now has a page of its own
	// (GuiCloudTransfer) instead of asking this card to shout.
	mFrame->setEdgeColor(theme->Background.color);
	mFrame->setCenterColor(theme->Background.centerColor);
	mFrame->setCornerSize(theme->Background.cornerSize);
	mFrame->setPostProcessShader(theme->Background.menuShader, false);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	addChild(mFrame);

	mGrid = new ComponentGrid(window, Vector2i(1, mAction == nullptr ? 2 : 3));
	mGrid->setPosition((fullSize.x() - gridSize.x()) / 2.0, (fullSize.y() - gridSize.y()) / 2.0);
	mGrid->setSize(gridSize);
	mGrid->setEntry(mTitle, Vector2i(0, 0), false, true);
	mGrid->setEntry(mGameName, Vector2i(0, 1), false, true);

	if (mAction != nullptr)
		mGrid->setEntry(mAction, Vector2i(0, 2), false, true);

	addChild(mGrid);

	// Centred at the top, like GuiInfoPopup. Window::renderAsyncNotifications
	// re-applies this every frame (it has to, to stack several of them), so
	// this is only the position the card holds before its first render --
	// but the two must agree, or the card lands in one place and moves.
	setPosition(Renderer::getScreenWidth() * 0.5f - mSize.x() * 0.5f,
		Renderer::getScreenHeight() * 0.02f, 0);
	setOpacity(NOTIFICATION_OPACITY);
}

void AsyncNotificationComponent::close()
{
	mClosing = true;	
	mFadeTime = -1;
}

AsyncNotificationComponent::~AsyncNotificationComponent()
{
	delete mFrame;
	delete mGrid;	
}

// The first candidate that fits the row, else the last one offered.
//
// Sizing text is a glyph lookup per character and the font atlas is a GL
// resource, so this runs on the interface thread (render) and never on the
// worker that composed the strings. A row that has not been sized yet gets
// the first candidate: the full form is the right answer when nothing is
// known, and the row is re-chosen when the candidates next change.
std::string AsyncNotificationComponent::chooseThatFits(const std::shared_ptr<TextComponent>& row, const std::vector<std::string>& candidates)
{
	const float width = (row == nullptr) ? 0.0f : row->getSize().x();

	std::function<float(const std::string&)> measure;
	if (row != nullptr && row->getFont() != nullptr)
		measure = [row](const std::string& candidate) { return row->getFont()->sizeText(candidate).x(); };

	return CloudText::chooseThatFits(candidates, width, measure);
}

void AsyncNotificationComponent::updateText(const std::string text, const std::string action)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextGameName.clear();
	mNextGameName.push_back(text);
	mNextAction.clear();
	mNextAction.push_back(action);
}

void AsyncNotificationComponent::updateText(const std::string text, const std::vector<std::string>& actionCandidates)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextGameName.clear();
	mNextGameName.push_back(text);
	mNextAction = actionCandidates;
	if (mNextAction.empty())
		mNextAction.push_back("");
}

void AsyncNotificationComponent::updateText(const std::vector<std::string>& textCandidates, const std::vector<std::string>& actionCandidates)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextGameName = textCandidates;
	if (mNextGameName.empty())
		mNextGameName.push_back("");
	mNextAction = actionCandidates;
	if (mNextAction.empty())
		mNextAction.push_back("");
}

void AsyncNotificationComponent::updatePercent(int percent)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mPercent = percent;
}

void AsyncNotificationComponent::updateTitle(const std::string text)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextTitle = text;
}

void AsyncNotificationComponent::render(const Transform4x4f& parentTrans)
{
	std::unique_lock<std::mutex> lock(mMutex);

	Transform4x4f trans = parentTrans * getTransform();

	// Both rows re-chosen only when their candidates change: sizing text is
	// a glyph lookup per character, and this runs every frame.
	if (mGameName != nullptr)
	{
		std::string key;
		for (auto& c : mNextGameName)
			key += c + "\n";
		if (key != mAppliedGameName)
		{
			mAppliedGameName = key;
			mGameName->setText(chooseThatFits(mGameName, mNextGameName));
		}
	}

	if (mAction != nullptr)
	{
		std::string key;
		for (auto& c : mNextAction)
			key += c + "\n";
		if (key != mAppliedAction)
		{
			mAppliedAction = key;
			mAction->setText(chooseThatFits(mAction, mNextAction));
		}
	}

	if (mTitle != nullptr && mNextTitle != mTitle->getText())
		mTitle->setText(mNextTitle);

	mFrame->render(trans);

	auto lastControl = mGameName;
	if (mAction != nullptr)
		lastControl = mAction;

	float x = mGrid->getPosition().x() + lastControl->getPosition().x();
	float y = mGrid->getPosition().y() + lastControl->getPosition().y();
	float w = lastControl->getSize().x();
	float h = lastControl->getSize().y();

	h /= 10.0;
	y += lastControl->getSize().y();

	Renderer::setMatrix(trans);

	if (mPercent >= 0)
	{
		float percent = mPercent / 100.0;
		if (percent < 0)
			percent = 0;
		else if (percent > 1)
			percent = 1;

		auto theme = ThemeData::getMenuTheme();
		auto color = theme->Text.color & 0xFFFFFF00 | (unsigned char)((theme->Text.color & 0xFF) * (getOpacity() / 255.0));
		Renderer::drawRect(x, y, (w*percent), h, color);
	}

	mGrid->render(trans);
}

void AsyncNotificationComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	// compute fade effects

	if (mClosing && mFadeTime == -1)
	{
		mFadeOutOpacity = getOpacity();
		mFadeTime = 0;
		return;
	}
	else if (mFadeTime < 500000)
		mFadeTime += deltaTime;

	int alpha = NOTIFICATION_OPACITY;
	int duration = 500;

	if (mClosing)
	{
		if (mFadeTime <= duration)
		{
			alpha = ((duration - mFadeTime) * 255 / duration);
			mFadeOut = (float)(255 - alpha) / 255.0;
			alpha = (alpha * mFadeOutOpacity) / 255;
		}
		else
		{
			mRunning = false;
			mFadeOut = 0;
		}
	}
	else
	{
		if (mFadeTime <= duration)
		{
			int dt = ((duration - mFadeTime) * 255 / duration);
			mFadeOut = (float)(255 - dt) / 255.0;
			alpha = (mFadeTime * alpha / duration);
		}
		else
			mFadeOut = 0;
	}

	setOpacity((unsigned char)alpha);
}
