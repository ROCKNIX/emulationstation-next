#include "guis/GuiGameAchievements.h"
#include "guis/GuiSettings.h"
#include "components/WebImageComponent.h"

#include "Window.h"
#include <string>
#include "Log.h"
#include "Settings.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiLoading.h"

#include "components/MultiLineMenuEntry.h"
#include "GuiGameAchievements.h"
#include "views/ViewController.h"

#define WINDOW_WIDTH (float)Math::min(Renderer::getScreenHeight() * 1.125f, Renderer::getScreenWidth() * 0.90f)
#define IMAGESIZE (Renderer::getScreenHeight() * (48.0 / 720.0))
#define IMAGESPACER (Renderer::getScreenHeight() * (10.0 / 720.0))
// Where the completion bar sits when the header lines leave it room: the
// layout as it has always been at 1280x800, as fractions of the header's
// text column (the part of the header left of the game image).
#define PROGRESS_LEFT  0.55f
#define PROGRESS_WIDTH 0.36f

void GuiGameAchievements::show(Window* window, int gameId, const std::string& cheevosHash)
{
	window->pushGui(new GuiLoading<GameInfoAndUserProgress>(window, _("PLEASE WAIT"),
		[window, gameId, cheevosHash](auto gui)
	{
		return RetroAchievements::getGameInfoAndUserProgress(gameId, "", cheevosHash);
	},
		[window](GameInfoAndUserProgress ra)
	{
		// Offline, and the proxy has never cached this game: not an error,
		// and not an empty page -- the one line that says what makes it
		// viewable (#180). The row that does it is named as the cards name
		// theirs (es-player-text.md, Recover).
		if (ra.NotOnDevice)
			window->pushGui(new GuiMsgBox(window, _("YOU'RE NOT ONLINE, AND THIS GAME'S ACHIEVEMENTS AREN'T SAVED ON THIS DEVICE YET. SCAN GAMES FOR OFFLINE ACHIEVEMENTS, OR START THE GAME ONCE WHILE YOU'RE CONNECTED."), _("OK")));
		else if (ra.ID == 0 && !ra.Title.empty())
			window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + "\r\n" + ra.Title, _("OK")));
		else if (ra.ID == 0)
			window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED"), _("OK")));
		else
			window->pushGui(new GuiGameAchievements(window, ra));
	}));
}


class GameAchievementEntry : public ComponentGrid
{
public:
	GameAchievementEntry(Window* window, Achievement& ra) :
		ComponentGrid(window, Vector2i(3, 4))
	{
		mGameInfo = ra;

		auto theme = ThemeData::getMenuTheme();

		mImage = std::make_shared<WebImageComponent>(mWindow);

		setEntry(mImage, Vector2i(0, 0), false, false, Vector2i(1, 4));
				
		std::string desc = mGameInfo.Description;
		desc += _U(" - ") + _("Points") + ": " + mGameInfo.Points;

		if (!mGameInfo.DateEarnedHardcore.empty())
			desc += _U("  \uf091  ") + _("Unlocked on") + ": " + mGameInfo.DateEarnedHardcore + _U(" - ") + _("HARDCORE MODE");
		else if (!mGameInfo.DateEarned.empty())
			desc += _U("  \uf091  ") + _("Unlocked on") + ": " + mGameInfo.DateEarned;			
		// From the device the cache knows the unlock and not its date, and
		// an award still queued is said so in the words the sync cards use
		// (D-RA-017: achievements are sent). Appended to the same line, so
		// the row stays two lines (D-UI-023).
		else if (mGameInfo.UnlockedOnDevice && mGameInfo.Pending)
			desc += _U("  \uf091  ") + _("Unlocked - will be sent when you're connected");
		else if (mGameInfo.UnlockedOnDevice)
			desc += _U("  \uf091  ") + _("Unlocked");

		mText = std::make_shared<TextComponent>(mWindow, mGameInfo.Title, theme->Text.font, theme->Text.color);
		mText->setVerticalAlignment(ALIGN_TOP);

		mSubstring = std::make_shared<TextComponent>(mWindow, desc, theme->TextSmall.font, theme->Text.color);
		mSubstring->setOpacity(192);

		setEntry(mText, Vector2i(2, 1), false, true);
		setEntry(mSubstring, Vector2i(2, 2), false, true);

		int height = Math::max(IMAGESIZE + IMAGESPACER, mText->getSize().y() + mSubstring->getSize().y());

		float hTxt = mText->getSize().y() / height;
		float hSub = mSubstring->getSize().y() / height;
		float topPadding = Math::max(0.0f, (height - mText->getSize().y() - mSubstring->getSize().y()) / height / 2.0f);

		setRowHeightPerc(0, topPadding);
		setRowHeightPerc(1, hTxt);
		setRowHeightPerc(2, hSub);
		setRowHeightPerc(3, Math::max(0.0f, 1.0f - topPadding - hTxt - hSub));

		setColWidthPerc(0, (height - IMAGESPACER) / WINDOW_WIDTH);
		setColWidthPerc(1, IMAGESPACER / WINDOW_WIDTH);
	
		mImage->setMaxSize(height - IMAGESPACER, height - IMAGESPACER);
		mImage->setImage(mGameInfo.getBadgeUrl());

		if (!mGameInfo.isUnlocked())
			mImage->setOpacity(120);

		setSize(0, height);
	}

	virtual void setColor(unsigned int color)
	{
		mText->setColor(color);
		mSubstring->setColor(color);
	}

private:
	std::shared_ptr<TextComponent> mText;
	std::shared_ptr<TextComponent> mSubstring;

	std::shared_ptr<WebImageComponent> mImage;

	Achievement mGameInfo;
};


GuiGameAchievements::GuiGameAchievements(Window* window, GameInfoAndUserProgress ra) : 
	GuiSettings(window, "", "", nullptr)
{
	// Required for WebImageComponent
	setUpdateType(ComponentListFlags::UPDATE_ALWAYS);

	setTitle(ra.Title);

	mMenu.clearButtons();

	mFile = GuiRetroAchievements::getFileData(std::to_string(ra.ID));
	if (mFile != nullptr)
	{
		auto file = mFile;
		mMenu.addButton(_("LAUNCH"), _("LAUNCH"), [this, file]
		{ 			
			Window* window = mWindow;
			while (window->peekGui() && window->peekGui() != ViewController::get())
				delete window->peekGui();

			ViewController::get()->launch(file);
		});
	}

	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });

	int totalPoints = 0;
	int userPoints = 0;

	for (auto game : ra.Achievements)
	{
		if (game.isUnlocked())
			userPoints += Utils::String::toInteger(game.Points);

		totalPoints += Utils::String::toInteger(game.Points);
	}

	std::string header;

	if (ra.Achievements.size() == 0)
		setSubTitle(_("THIS GAME HAS NO ACHIEVEMENTS YET"));
	else if (ra.FromDevice)
	{
		// The proxy is casual-only and its cache holds no hardcore count, so
		// that line is not made up; its place says where this came from.
		header = _("Achievements (softcore)") + ": \t" + std::to_string(ra.NumAwardedToUser) + "/" + std::to_string(ra.NumAchievements);
		header += "\r\n" + _("Points") + ": \t" + std::to_string(userPoints) + "/" + std::to_string(totalPoints);
		header += "\r\n" + _("YOU'RE OFFLINE. SHOWING YOUR MOST RECENT PROGRESS.");

		setSubTitle(header);
	}
	else
	{
		header = _("Achievements (softcore)") + ": \t" + std::to_string(ra.NumAwardedToUser) + "/" + std::to_string(ra.NumAchievements);
		header += "\r\n" + _("Achievements (hardcore)") + ": \t" + std::to_string(ra.NumAwardedToUserHardcore) + "/" + std::to_string(ra.NumAchievements);
		header += "\r\n" + _("Points") + ": \t" + std::to_string(userPoints) + "/" + std::to_string(totalPoints);

		setSubTitle(header);
	}

	auto image = std::make_shared<WebImageComponent>(mWindow);
	image->setImage(ra.getImageUrl());
	setTitleImage(image);

	if (ra.Achievements.size() > 0)
	{
		int percent = Math::round(ra.NumAwardedToUser * 100.0f / ra.Achievements.size());

		char trstring[256];
		snprintf(trstring, 256, _("%d%% complete").c_str(), percent);
		mProgress = std::make_shared<RetroAchievementProgress>(mWindow, ra.NumAwardedToUser, ra.NumAwardedToUserHardcore, ra.Achievements.size(), Utils::String::trim(trstring));
		// A row one header line tall: bar and percentage on its one centre
		// line, not the summary column's bar-over-label stack (fork #193).
		mProgress->setLabelBeside(true);
	}

	for (auto game : ra.Achievements)
	{
		ComponentListRow row;

		auto itstring = std::make_shared<GameAchievementEntry>(mWindow, game);
		row.addElement(itstring, true);

		addRow(row);
	}

	centerWindow();	

	// The bar goes beside the header lines where they end before its place,
	// and under them where they do not (#160: at 640x480 with a larger menu
	// font the lines ran past it, and the bar and its percentage were drawn
	// over them). Decided once, here, because the second layout needs the
	// header one line taller and the header's height is the subtitle's:
	// an empty fourth line is the bar's row. Measured on the subtitle as
	// the menu draws it -- its font, its tab stops, its padding -- so a
	// larger font setting or a longer translation moves the decision, not
	// the bar over the text.
	//
	// The measure is a left-aligned block's right edge, and the menu
	// left-aligns the header wherever it has a title image (in both menu
	// modes since #160's second half: under full-screen menus the block
	// stayed centred, and the bar landed on lines whose measured edge said
	// there was room). A centred block has no fixed right edge -- it moves
	// with the column -- so if the header ever comes back centred the bar
	// takes the row below, the one place it cannot overlap.
	auto lines = mMenu.getSubTitle();
	if (mProgress != nullptr && lines != nullptr)
	{
		const float textRight = lines->getPosition().x() + lines->getPadding().x()
			+ lines->getFont()->sizeTabbedText(lines->getText(), lines->getLineSpacing()).x();

		if (lines->getHorizontalAlignment() != ALIGN_LEFT || textRight > headerTextColumn() * PROGRESS_LEFT)
		{
			mProgressBelow = true;
			setSubTitle(header + "\r\n");
			centerWindow();
		}
	}
}

// The header's text column: what is left of the menu's width once the game
// image has its share, as MenuComponent::setTitleImage divides it.
float GuiGameAchievements::headerTextColumn()
{
	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
	float iw = mMenu.getTitleHeight() / width;

	return mMenu.getSize().x() - (mMenu.getSize().x() * iw);
}

void GuiGameAchievements::centerWindow()
{
	float width = (float)Math::min((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		mMenu.setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.901f);

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
}

void GuiGameAchievements::render(const Transform4x4f& parentTrans)
{
	GuiSettings::render(parentTrans);

	if (mProgress == nullptr)
		return;

	auto lines = mMenu.getSubTitle();
	if (lines == nullptr)
		return;

	// The bar and its percentage share the header line they sit on: the row
	// is one line tall and the component centres both on half of it (fork
	// #193). It used to keep the summary page's shape -- the bar in the
	// upper half of the line, the percentage centred in the lower half -- so
	// the number sat below and to the right of a bar whose empty part was
	// not drawn: at 640x480 a 4% bar was a blue dash at the left with "4%
	// complete" floating under the middle of the row, and the two never
	// shared a line. The row starts where the text does, its top padding
	// included, so a line's centre and the row's are the same point.
	const float lineHeight = lines->getFont()->getHeight(lines->getLineSpacing());
	const float textTop = lines->getPosition().y() + lines->getPadding().y();

	if (mProgressBelow)
	{
		// Its own row: the empty last line of the header, as wide as the
		// lines above it and starting where they start -- which for a
		// centred header is where the widest line starts.
		const Vector2f text = lines->getFont()->sizeTabbedText(lines->getText(), lines->getLineSpacing());

		float left = lines->getPosition().x() + lines->getPadding().x();
		if (lines->getHorizontalAlignment() == ALIGN_CENTER)
			left += (lines->getSize().x() - lines->getPadding().x() - lines->getPadding().z() - text.x()) / 2.0f;

		mProgress->setPosition(left, textTop + text.y() - lineHeight);
		mProgress->setSize(text.x(), lineHeight);
	}
	else
	{
		// Beside the first line, on that line's centre.
		const float column = headerTextColumn();

		mProgress->setPosition(column * PROGRESS_LEFT, textTop);
		mProgress->setSize(column * PROGRESS_WIDTH, lineHeight);
	}

	Transform4x4f trans = parentTrans * mMenu.getTransform();
	mProgress->render(trans);
}

bool GuiGameAchievements::input(InputConfig* config, Input input)
{
	if (config->isMappedTo("x", input) && input.value != 0)
	{
		if (mFile != nullptr)
		{
			auto file = mFile;
			if (file != nullptr)
			{
				Window* window = mWindow;
				while (window->peekGui() && window->peekGui() != ViewController::get())
					delete window->peekGui();

				ViewController::get()->launch(file);
			}
		}

		return true;
	}

	return GuiSettings::input(config, input);
}
std::vector<HelpPrompt> GuiGameAchievements::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));

	if (mFile != nullptr)
		prompts.push_back(HelpPrompt("x", _("LAUNCH")));

	return prompts;
}
