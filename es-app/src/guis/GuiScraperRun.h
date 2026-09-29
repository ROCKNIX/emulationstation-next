#pragma once

#include "GuiComponent.h"
#include "components/BusyComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "scrapers/ThreadedScraper.h"

#include <memory>
#include <string>
#include <vector>

// The page a scrape runs on (fork #241, D-UI-078). A scrape asks the
// scraper about every selected game and fetches its media -- minutes for a
// library -- which is the fourth surface tier (es-native-ui.md): a page
// that owns the screen, shows the live line and the elapsed time, stays
// with the outcome until the player dismisses it, and whose one way out
// while it runs is CANCEL. Upstream drew the run on a corner card and
// ended it with a toast, both gone before anybody who walked away came
// back. GuiOfflineScan's shape: the run is ThreadedScraper's, the page is
// a view of it through ThreadedScraper::progress(), and the geometry is
// the scan page's so the app's long-job pages sit the same way on every
// panel.
class GuiScraperRun : public GuiComponent
{
public:
	GuiScraperRun(Window* window);

	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void update(int deltaTime) override;

private:
	// The done page's word (D-UI-028), from how the run ended.
	static std::string outcomeWord(const ThreadedScraper::Progress& p);
	static std::string fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width);
	// "GAMES SCRAPED: n  -  COULDN'T SCRAPE: e" (a middle dot on screen; ASCII here)
	static std::string countsLine(const ThreadedScraper::Progress& p);
	// The CANCEL confirmation: what cancelling means, then
	// ThreadedScraper::stop() on YES.
	void askCancel();

	BusyComponent mBusyAnim;
	NinePatchComponent mBackground;

	std::shared_ptr<TextComponent> mTitle;
	// Seven lines under the title, the scan page's layout: the state, GAME
	// i OF n, the game being looked up, the counts so far, the spinner's
	// row, elapsed, the notice. Once the run is over the same rows carry
	// the outcome, the counts, the why, what to do next.
	std::shared_ptr<TextComponent> mStatus;    // 1. SCRAPING... / the outcome word
	std::shared_ptr<TextComponent> mCounter;   // 2. GAME i OF n / the run's counts
	std::shared_ptr<TextComponent> mActivity;  // 3. the game / blank once done
	std::shared_ptr<TextComponent> mDetail;    // 4. the counts so far / the why
	std::shared_ptr<TextComponent> mNote;      // 5. where the spinner was: what to do next, once done
	std::shared_ptr<TextComponent> mElapsed;   // 6. elapsed
	std::shared_ptr<TextComponent> mFooter;    // 7. the notice / the buttons
	std::shared_ptr<Font> mTextFont;
	std::shared_ptr<Font> mSmallFont;
	float mLineWidth;

	Vector2f mPanelPos;
	Vector2f mPanelSize;

	bool mShownFinished;
};
