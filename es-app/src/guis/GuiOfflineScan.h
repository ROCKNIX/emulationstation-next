#pragma once

#include "GuiComponent.h"
#include "OfflineScanJob.h"
#include "components/BusyComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// The page SCAN GAMES FOR OFFLINE ACHIEVEMENTS runs on (fork #179, D-RA-010).
//
// A scan hashes every game on the console and asks RetroAchievements about
// each one, with the client's own pauses between requests: minutes for a
// library, an hour for a thousand games. That is the fourth surface tier
// (es-native-ui.md): a page that owns the screen, shows the live line and
// the elapsed time -- no bar, because the client gives no percentage worth
// drawing one from -- and stays with the outcome until the player dismisses
// it. The page is sat in for the run's length (D-UI-078, #241): the one way
// out while it runs is CANCEL, a confirmation that says what cancelling
// means (what was saved stays saved, the next scan carries on from there),
// then OfflineScanJob::cancel(). GuiCloudTransfer's shape, without its
// rclone parser: the backend is raofflineproxy-ctl scan, which talks to the
// run through ">>> " lines (its header spells them) and exits with the codes
// CloudExit.h names for a run that did nothing.
class GuiOfflineScan : public GuiComponent
{
public:
	// onChanged runs on the interface thread whenever the run's state
	// changes, once when it ends, and after this page is gone, whichever
	// button closed it -- so the row that opened the page can re-read the
	// stamp. The page attaches to the run in flight when there is one (a
	// TRY AGAIN's predecessor, ending), and starts command otherwise.
	GuiOfflineScan(Window* window, const std::string& command, const std::function<void()>& onChanged = nullptr);
	virtual ~GuiOfflineScan();

	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void update(int deltaTime) override;

	// "N GAMES READY FOR OFFLINE PLAY": the sentence the row under SCAN GAMES
	// and this page's line 3 both end on, so the two never disagree.
	static std::string readyPhrase(int ready);

private:
	// The done page's word (D-UI-028), from the run's state.
	struct Outcome
	{
		bool completed;
		bool skipped;      // a sentinel: not online, another scan running, or the player's cancel
		std::string word;
	};
	static Outcome outcome(const OfflineScanJob::State& s);
	// The CANCEL confirmation (D-UI-078): what cancelling means, then
	// OfflineScanJob::cancel() on YES.
	void askCancel();
	static std::string fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width);
	static std::string countsLine(int cached, int skipped, int errors);
	void close();

	BusyComponent mBusyAnim;
	NinePatchComponent mBackground;

	std::shared_ptr<TextComponent> mTitle;
	// Seven lines under the title, GuiCloudTransfer's layout (D-UI-024/026):
	// the state, GAME i OF n, the game being looked at, the counts so far,
	// the spinner's row, elapsed, the notice. Once the run is over the same
	// rows carry the outcome, the counts, how many games are ready, the why,
	// and what to do next.
	std::shared_ptr<TextComponent> mStatus;    // 1. SCANNING... / the outcome word
	std::shared_ptr<TextComponent> mCounter;   // 2. GAME i OF n / the run's counts
	std::shared_ptr<TextComponent> mActivity;  // 3. the game / N GAMES READY FOR OFFLINE PLAY
	std::shared_ptr<TextComponent> mDetail;    // 4. the counts so far / the why
	std::shared_ptr<TextComponent> mNote;      // 5. where the spinner was: what to do next, once done
	std::shared_ptr<TextComponent> mElapsed;   // 6. elapsed
	std::shared_ptr<TextComponent> mFooter;    // 7. the notice / the buttons
	std::shared_ptr<Font> mTextFont;
	std::shared_ptr<Font> mSmallFont;
	float mLineWidth;

	std::string mCommand;
	std::function<void()> mOnChanged;
	std::shared_ptr<OfflineScanJob> mJob;

	Vector2f mPanelPos;
	Vector2f mPanelSize;

	bool mShownFinished;
};
