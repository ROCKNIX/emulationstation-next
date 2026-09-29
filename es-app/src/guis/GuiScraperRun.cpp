#include "guis/GuiScraperRun.h"

#include "CloudText.h"
#include "TextFit.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "Log.h"

#include <cstdio>

GuiScraperRun::GuiScraperRun(Window* window)
	: GuiComponent(window), mBusyAnim(window, ""), mBackground(window, ":/frame.png"), mShownFinished(false)
{
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	const float SW = Renderer::getScreenWidth();
	const float SH = Renderer::getScreenHeight();
	setSize(SW, SH);

	mTextFont  = theme->Text.font;
	mSmallFont = theme->TextSmall.font;
	mTitle    = std::make_shared<TextComponent>(window, _("SCRAPING GAMES"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mStatus   = std::make_shared<TextComponent>(window, _("PREPARING..."), mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mCounter  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mActivity = std::make_shared<TextComponent>(window, "",                mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mDetail   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mNote     = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mElapsed  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mFooter   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);

	// One column, 0.78 of the width, every line fitted to it, and the rows
	// stacked from the fonts' own heights -- GuiCloudTransfer's geometry,
	// so the long-job pages of this app sit the same way on every panel
	// (its constructor carries the derivation and the numbers per panel).
	const float w  = SW * 0.78f;
	const float cx = SW * 0.5f;
	const float x  = cx - w / 2.0f;
	mLineWidth = w;
	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->setSize(w, 0);

	const float hT = theme->Title.font->getHeight();
	const float hM = mTextFont->getHeight();
	const float hS = mSmallFont->getHeight();
	const float natural = 2 * SH * 0.05f + hT + 3 * hM + 4 * hS + 5.5f * SH * 0.02f;
	const float fit = natural > SH * 0.9f ? (SH * 0.9f) / natural : 1.0f;
	const float pad = SH * 0.05f * fit;
	const float gap = SH * 0.02f * fit;
	const float rT = hT * fit, rM = hM * fit, rS = hS * fit;
	const float H = 2 * pad + rT + 3 * rM + 4 * rS + 5.5f * gap;
	const float top = (SH - H) / 2.0f;

	float y = top + pad;
	mTitle   ->setPosition(x, y);
	y += rT + 1.5f * gap;
	mStatus  ->setPosition(x, y);
	mCounter ->setPosition(x, y + rM);
	y += rM + rS + gap;
	mActivity->setPosition(x, y);
	mDetail  ->setPosition(x, y + rM);
	y += rM + rS + 0.5f * gap;
	// 5. The spinner while it runs, the note once done, one row. No caption
	// on the spinner: line 1 says what is happening.
	mBusyAnim.setBackgroundVisible(false);
	mBusyAnim.setSize(w, rM);
	mBusyAnim.setPosition(x, y);
	mNote    ->setPosition(x, y + (rM - hS) / 2.0f);
	y += rM + 1.5f * gap;
	mElapsed ->setPosition(x, y);
	y += rS + gap;
	mFooter  ->setPosition(x, y);

	mPanelSize = Vector2f(w + SW * 0.06f, H);
	mPanelPos  = Vector2f(cx - mPanelSize.x() / 2.0f, top);
	mBackground.fitTo(mPanelSize, Vector3f(mPanelPos.x(), mPanelPos.y(), 0), Vector2f(-32, -32));
}

// While the scrape runs, back asks whether to cancel it (D-UI-078: the page is
// sat in, and CANCEL is the one way out); every other press is refused,
// since there is nothing to choose and a stray press should not dismiss a
// page somebody is waiting on. Once it has finished, any button dismisses
// it: the SCRAPER page it came from is right behind, and a new scrape
// starts there.
bool GuiScraperRun::input(InputConfig* config, Input input)
{
	if (!input.value)
		return true;
	const ThreadedScraper::Progress p = ThreadedScraper::progress();
	if (!p.finished)
	{
		if (config->isMappedTo(BUTTON_BACK, input))
			askCancel();
		return true;
	}
	delete this;
	return true;
}

// What cancelling means, read at the moment of deciding (D-UI-023): each
// game's result is saved as it comes in, so what is scraped stays, and the
// game lists show it once they are updated. YES first, NO last so B
// answers NO. Nothing of the page is captured: the stop is a static call on
// the run in flight, and the page shows the outcome as the run ends.
void GuiScraperRun::askCancel()
{
	mWindow->pushGui(new GuiMsgBox(mWindow,
		_("CANCEL SCRAPING?") + std::string("\n\n")
			+ _("WHAT'S SCRAPED SO FAR IS KEPT. UPDATE GAMELISTS TO APPLY IT."),
		_("YES"), [] { ThreadedScraper::stop(); },
		_("NO"), nullptr));
}

std::vector<HelpPrompt> GuiScraperRun::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	const ThreadedScraper::Progress p = ThreadedScraper::progress();
	if (!p.finished)
	{
		prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
		return prompts;
	}
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	return prompts;
}

// The word for the run (D-UI-028): COMPLETED when every game was looked
// at; SKIPPED for the player's own cancel (nothing went wrong, and the
// games not reached are the next scrape's); COULDN'T FINISH when the
// scraper refused the run -- the why on line 4 is its own words -- or when
// games among many could not be scraped (a run whose parts disagree is
// COULDN'T FINISH, D-UI-030).
std::string GuiScraperRun::outcomeWord(const ThreadedScraper::Progress& p)
{
	if (p.cancelled)
		return _("SKIPPED - YOU CANCELLED IT");
	if (p.failed || p.errors > 0)
		return _("COULDN'T FINISH");
	return _("COMPLETED");
}

void GuiScraperRun::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);
	// Dim the whole screen: this is a takeover, and the carousel showing
	// through would say the app is idle while a scrape is in flight.
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x000000D0);

	mBackground.render(trans);

	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->render(trans);

	// A spinner and never a bar: the scraper reports each game as it
	// finishes it, and a game with a video to fetch takes as long as ten
	// without, so a bar at i/n would be an invented position
	// (es-native-ui.md: a bar only where a real percentage exists).
	if (!mShownFinished)
		mBusyAnim.render(trans);

	// The keys are the help bar's, drawn here: with full-screen menus on
	// (every handheld) Window draws no help while a second page is open, so
	// the footer spelled the cancel out as PRESS B -- the wrong button on a
	// swapped mapping (#308 8-es claude F-ES-05, 8a gpt F-ES-15;
	// es-ui-style-guide.md, Interaction rules). As the transfer and scan
	// pages do.
	if (mWindow->peekGui() == this && Renderer::ScreenSettings::fullScreenMenus())
		mWindow->renderHelpPromptsEarly(parentTrans);
}

// Clip to one line: a long game name gets an ellipsis, never a second line.
// On characters, not bytes (TextFit; #308 8a gpt F-ES-13): a title ending in
// an accented or Japanese character was measured, and could be shown, with
// half a character on its end.
std::string GuiScraperRun::fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width)
{
	if (!font)
		return text;
	return TextFit::fitOneLine(text, width, [&font](const std::string& t) { return font->sizeText(t).x(); });
}

// "GAMES SCRAPED: 12  -  COULDN'T SCRAPE: 1" (the separator on screen is a
// middle dot; ASCII here, es-code-traps.md) -- what this run did. A game the
// scraper could not do (no match, a timeout) counts against the run and
// is the next scrape's (D-UI-030).
std::string GuiScraperRun::countsLine(const ThreadedScraper::Progress& p)
{
	const int scraped = p.done - p.errors > 0 ? p.done - p.errors : 0;
	std::string line = std::string(_("GAMES SCRAPED:")) + " " + std::to_string(scraped);
	if (p.errors > 0)
		line += "  ·  " + std::string(_("COULDN'T SCRAPE:")) + " " + std::to_string(p.errors);
	return line;
}

void GuiScraperRun::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);
	const ThreadedScraper::Progress p = ThreadedScraper::progress();
	if (p.finished && !mShownFinished)
	{
		mShownFinished = true;
		// The help bar changes with the page: CLOSE now.
		updateHelpPrompts();
	}
	const int mins = p.elapsedMs / 60000;
	const int secs = (p.elapsedMs / 1000) % 60;
	char elapsed[32];
	snprintf(elapsed, sizeof(elapsed), "%d:%02d", mins, secs);

	if (p.finished)
	{
		// The same seven rows, now carrying the outcome (D-UI-028): 1 the
		// word; 2 what this run did; 3 blank; 4 why it stopped, when the
		// scraper said; 5 what to do next; 6 elapsed; 7 the buttons.
		mStatus  ->setText(fitOneLine(mTextFont, outcomeWord(p), mLineWidth));
		mCounter ->setText(fitOneLine(mSmallFont, countsLine(p), mLineWidth));
		mActivity->setText("");
		mDetail  ->setText(p.failed ? fitOneLine(mSmallFont, p.failure, mLineWidth) : "");
		// Each result is saved as it comes in; the lists show it once they
		// are rebuilt (the sentence upstream's toast ended on). The longest
		// form that fits the line (D-UI-035): the French of the full
		// sentence ran off a 640x480 panel and was cut mid-word.
		std::shared_ptr<Font> small = mSmallFont;
		mNote    ->setText(p.done - p.errors > 0 ? CloudText::chooseThatFits(
			{ _("UPDATE GAMELISTS TO APPLY CHANGES."), _("UPDATE GAMELISTS TO APPLY IT.") },
			mLineWidth, [small](const std::string& t) { return small ? small->sizeText(t).x() : 0.0f; }) : "");
		mElapsed ->setText(std::string(_("ELAPSED")) + " " + elapsed);
		mFooter  ->setText(_("PRESS ANY BUTTON TO CLOSE"));
	}
	else
	{
		// 1 what it is doing; 2 GAME i OF n; 3 the game it is on -- the line
		// that says it is alive, because a video can take a while and a
		// frozen count is indistinguishable from a hang; 4 the counts.
		mStatus->setText(p.done > 0 || !p.game.empty() ? _("SCRAPING...") : _("PREPARING..."));
		std::string counter;
		if (p.total > 0 && (p.done > 0 || !p.game.empty()))
		{
			const int at = p.done + 1 <= p.total ? p.done + 1 : p.total;
			counter = std::string(_("GAME")) + " " + std::to_string(at) + " " + std::string(_("OF")) + " " + std::to_string(p.total);
		}
		mCounter ->setText(counter);
		mActivity->setText(fitOneLine(mTextFont, p.game, mLineWidth));
		mDetail  ->setText(p.done > 0 ? fitOneLine(mSmallFont, countsLine(p), mLineWidth) : "");
		mNote    ->setText("");
		mElapsed ->setText(std::string(_("ELAPSED")) + " " + elapsed);
		// 7. That it takes a while; how to cancel is the help bar's, on the
		// player's own back button (render).
		mFooter  ->setText(fitOneLine(mSmallFont, _("THIS CAN TAKE A WHILE."), mLineWidth));
	}
}
