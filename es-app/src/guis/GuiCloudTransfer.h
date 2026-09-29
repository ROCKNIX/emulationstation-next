#pragma once

#include "GuiComponent.h"
#include "CloudTransferJob.h"
#include "components/BusyComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// A screen for a transfer that takes minutes, not seconds.
//
// The progress card is the right surface for work you can keep playing
// through -- a scrape, a hash. It is the wrong one for a restore: the card
// closes itself when the job ends, and a 1.4 GiB restore is precisely the
// thing somebody walks away from. Come back to a sleeping handheld and the
// only record that anything happened is a log file.
//
// So a long transfer owns the screen and stays there until it is dismissed.
// The result is the last thing on it, not the first thing to disappear.
//
// And it is sat in (D-UI-078, fork #241): the page owns the screen for the
// run's length, and the one way out while it runs is CANCEL -- a
// confirmation that says what cancelling means for this job (what has
// moved stays, the next run finishes the rest), then the stop. Every other
// press is refused while it runs, because there is nothing to choose. For
// a week (#187, D-UI-060/070, the shape audit #186 PL-07 gave the scan
// page) it was the other way: B left the page with the run going on
// behind it, the row that launched it followed the run in its line, and a
// press on that row reopened this page. The maintainer met the same B
// prompt on the scan page and asked what would happen; "nothing tells you"
// was the whole case, and D-UI-078 reversed it. The run is still a
// CloudTransferJob apart from this page -- the job, not the page, reads
// the pipe to its end -- so the seams built for the leavable page (the hub
// row's line, the launch gate's question, the sync card's refusal) stay
// true of a run that is current, and are reached only in the moment
// between the run's end and the page's dismissal, or if this page were
// ever gone while its run was not. A page whose completed run has an
// action in place of an exit (a settings restore: the configuration under
// this process is being replaced, #114) is the exception and takes every
// button, as it did.
class GuiCloudTransfer : public GuiComponent
{
public:
	// itemsExpected: how many items ES chained into this run -- one for saves,
	// one per system the picker left ticked, one for settings -- so row 2 can
	// read ITEM 1 OF 4 before the first script has said anything. 0 means
	// unknown, and row 2 shows ITEM i alone until a script says. A content
	// script announces its own count and corrects it (it may add bios to
	// what was ticked); itemsAfterContent is how many single-item phases ES
	// chained after the content phase, so that correction still counts them.
	// Starts the run -- or, when one is already in flight, opens on that one
	// (CloudTransferJob::start): the scripts' flock would refuse a second.
	GuiCloudTransfer(Window* window, const std::string& command, const std::string& title,
		int itemsExpected = 0, int itemsAfterContent = 0);
	// The page on a run already started: the one in flight, or one that
	// ended while nobody was watching and has not been dismissed.
	GuiCloudTransfer(Window* window, const std::shared_ptr<CloudTransferJob>& job);
	virtual ~GuiCloudTransfer();

	// What the page does instead of closing, when the run completed.
	//
	// A settings restore replaces the configuration under a running
	// EmulationStation, so there is nowhere safe to go back to: the menu
	// behind this page is drawn from values the restore has just made
	// stale, and the next thing that writes them puts the old ones back
	// over the new. So the page's only exit becomes the action -- any
	// button takes it -- and the three strings say so: `helpVerb` is the
	// word on the help bar, `footer` is line 7, and `note` is line 5, what
	// happens next. Nothing is called when the run did not complete; TRY
	// AGAIN and CLOSE apply then exactly as they always do (#114). A page
	// with an action set is not left while it runs, for the same reason.
	void setCompletedAction(const std::function<void()>& action, const std::string& helpVerb,
		const std::string& footer, const std::string& note);

	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void update(int deltaTime) override;

	// A size at the precision it has: "200 KB", "1.2 MB", "1.20 GB". The one
	// formatter for every size a cloud page prints -- this page's summary,
	// the content picker's rows, and the match confirmation -- so no two of
	// them disagree on a number.
	static std::string sizeLabel(unsigned long bytes);

	// The words the row under BACK UP / RESTORE / MATCH borrows for a run
	// that is current while the hub is the top page -- since D-UI-078 the
	// page is sat in, so that is the outcome's moment, not the run's; the
	// running words remain for the seam (see the class comment) -- so the
	// row and this page say the same thing about the same run: the verb's
	// word (BACKING UP...), the same with ITEM i OF n behind it once an item
	// is known, and that item's label (SAVES, NES) for a panel with room.
	struct RowWords
	{
		std::string word;
		std::string counted;
		std::string item;
	};
	static RowWords rowWords(const std::shared_ptr<CloudTransferJob>& job);
	// Line 1 of the done page (D-UI-028), for the row once the run has
	// ended and nobody has opened the page: COMPLETED, COULDN'T FINISH, or
	// SKIPPED with its reason.
	static std::string outcomeWord(const std::shared_ptr<CloudTransferJob>& job);
	// The sentence the launch gate asks over a run that is still current
	// when a game is launched (D-CLOUD-129: STOP IT AND PLAY or KEEP
	// WAITING): YOUR BACKUP TO THE CLOUD IS STILL RUNNING., in the run's own
	// verb. With the page sat in (D-UI-078) a launch cannot reach the gate
	// while a run goes; the gate stays as the seam's guard.
	static std::string stillRunningSentence(const std::shared_ptr<CloudTransferJob>& job);

private:
	// The done page's word and what follows it (D-UI-028), from the tiers,
	// the failed items and the exit code. Called with job.mMutex held.
	struct Outcome
	{
		bool completed;   // every part finished (0 or 9)
		bool partial;     // some finished and some did not; a failure to the player
		bool skipped;     // a sentinel, and nothing else to report
		std::string word; // line 1
	};
	static Outcome outcome(const CloudTransferJob& job);
	// Whether TRY AGAIN, the same command again, is offered: not for a match.
	static bool retries(const CloudTransferJob& job);
	// BACKING UP... / RESTORING... / MATCHING..., from the run's command.
	static std::string verbWord(const CloudTransferJob& job);
	// The rows only the done state writes, cleared for a TRY AGAIN so the
	// new run does not start under last time's note.
	void clearDoneRows();
	// Whether B leaves the page while the run goes on: not when the completed
	// run has an action in place of an exit.
	// Whether B while it runs offers CANCEL (D-UI-078). Not on a page whose
	// completed run has an action in place of an exit (setCompletedAction):
	// a settings restore is replacing the configuration under this process,
	// and there is nowhere safe to stop.
	bool cancellable() const { return !mCompletedAction; }
	// The CANCEL confirmation: what cancelling means, then
	// CloudTransferJob::stopByPlayer() on YES.
	void askCancel();

	BusyComponent mBusyAnim;
	NinePatchComponent mBackground;

	std::shared_ptr<TextComponent> mTitle;
	// Seven lines under the title (maintainer's layout, 2026-09-06, D-UI-024;
	// the item first and numbered across the run, 2026-09-09, D-UI-026): the
	// item, ITEM i OF n, what it is doing on it, that item's files and bytes,
	// the bar, elapsed, and the notice. Each is one line, fitted to the
	// width, so nothing wraps into the line below. Once the run is over the
	// same four rows carry the outcome, the run's summary and its detail.
	std::shared_ptr<TextComponent> mStatus;    // 1. the item (BIOS, NES, SAVES, SETTINGS); the outcome, once done
	std::shared_ptr<TextComponent> mCounter;   // 2. ITEM i OF n, counted across the whole run
	std::shared_ptr<TextComponent> mActivity;  // 3. what it is doing on that item; the run's summary, once done
	std::shared_ptr<TextComponent> mDetail;    // 4. that item's files and bytes so far; the summary's detail, once done
	std::shared_ptr<TextComponent> mNote;      // 5. where the bar was: what to do next, once done
	std::shared_ptr<TextComponent> mElapsed;   // 6. elapsed
	std::shared_ptr<TextComponent> mFooter;    // 7. the notice / press any button
	std::shared_ptr<Font> mTextFont;
	std::shared_ptr<Font> mSmallFont;
	float mLineWidth;
	static std::string fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width);
	// Drop whole sentences from the end before clipping: "WHAT WAS SENT IS IN
	// YOUR CLOUD. THE REST IS STILL ON THIS DEVICE." keeps its first sentence
	// on a panel too narrow for both, rather than ending mid-word in an
	// ellipsis. The last sentence standing is clipped if even it does not fit.
	static std::string fitSentences(const std::shared_ptr<Font>& font, std::string text, float width);
	// A unit's name as the scripts spell it, in the player's language.
	static std::string unitName(const std::string& label);
	static std::string prettyRclone(std::string fragment);
	static std::string roundSizes(const std::string& fragment);

	// Panel geometry, decided once in the constructor and never re-derived:
	// fitTo() is given this rectangle, the text rows are stacked inside it,
	// and the bar is drawn at mBar*, on the row the spinner and the done-note
	// share -- so the frame, the rows and the bar cannot drift apart.
	Vector2f mPanelPos;
	Vector2f mPanelSize;
	float mBarX, mBarY, mBarW, mBarH;

	// The run this page shows. Replaced by TRY AGAIN with a new run of the
	// same command; never touched off the interface thread.
	std::shared_ptr<CloudTransferJob> mJob;
	// The page was opened for one command and shows another's run, already
	// in flight (CloudTransferJob::start): no completed action is taken.
	bool mOtherRun;

	// The frame's copy of (finished, percent), taken in update() under the
	// run's lock that also reads the text -- render() draws the bar from
	// these, so the bar and the eight rows are always the same stats block.
	// Reading the percent again in render() let the worker advance it
	// between the two, and the bar ran one block ahead of the text for a
	// frame.
	bool mShownFinished;
	int mShownPercent;

	// The completed run's one exit, and the words for it (setCompletedAction).
	std::function<void()> mCompletedAction;
	std::string mCompletedHelpVerb, mCompletedFooter, mCompletedNote;
};
