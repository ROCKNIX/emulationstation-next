#include "guis/GuiCloudTransfer.h"
#include "TextFit.h"

#include "guis/GuiMsgBox.h"

#include "CloudExit.h"
#include "CloudOffer.h"
#include "CloudText.h"
#include "CloudTransferJob.h"
#include "ThreadedCloudSync.h"
#include "Window.h"
#include "ThemeData.h"
#include "LocaleES.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"
#include "SystemData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

GuiCloudTransfer::GuiCloudTransfer(Window* window, const std::string& command, const std::string& title,
	int itemsExpected, int itemsAfterContent)
	: GuiCloudTransfer(window, CloudTransferJob::start(command, title, itemsExpected, itemsAfterContent))
{
	// start() hands back a run already in flight rather than starting a
	// second (the scripts' flock would refuse it), so a page opened for one
	// command may be showing another's run. It shows it truthfully -- the
	// title is the run's -- and it must not take the command's completed
	// action when that other run ends: a settings restore's restart (#114)
	// fired by somebody else's backup finishing is the case this guards.
	mOtherRun = mJob->command() != command;
	if (mOtherRun)
		LOG(LogWarning) << "GuiCloudTransfer: a run is already in flight; showing it in place of: " << command;
}

GuiCloudTransfer::GuiCloudTransfer(Window* window, const std::shared_ptr<CloudTransferJob>& job)
	: GuiComponent(window), mBusyAnim(window, ""), mBackground(window, ":/frame.png"),
	  mJob(job), mOtherRun(false), mShownFinished(false), mShownPercent(-1)
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
	mTitle    = std::make_shared<TextComponent>(window, Utils::String::toUpper(mJob->title()), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mStatus   = std::make_shared<TextComponent>(window, _("PREPARING..."), mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mCounter  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mActivity = std::make_shared<TextComponent>(window, "",                mTextFont,  theme->Text.color,      ALIGN_CENTER);
	mDetail   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mNote     = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mElapsed  = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);
	mFooter   = std::make_shared<TextComponent>(window, "",                mSmallFont, theme->TextSmall.color, ALIGN_CENTER);

	// One column, 0.78 of the width, every line fitted to it (fitOneLine) so a
	// long ROM name is clipped rather than wrapped into the line beneath -- the
	// overlap this layout replaces came from boxes that grew with their text
	// while their neighbours sat at fixed heights.
	const float w  = SW * 0.78f;
	const float cx = SW * 0.5f;
	const float x  = cx - w / 2.0f;
	mLineWidth = w;
	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->setSize(w, 0);

	// Rows are stacked from the fonts' own heights, not from a table of
	// fractions. A TextComponent given (w, 0) is exactly font->getHeight()
	// tall -- 1.5 x its tallest glyph -- and the menu fonts are the theme's:
	// the shipped theme sets the title at 0.029 of the screen where ES's
	// default is 0.085, so a table that clears the default is three times too
	// loose for what ships. Fonts scale with min(W, H), so every row here is
	// screen-relative all the same; the fractions below are what the sums
	// come to.
	//
	//   pad  0.05 SH   above the title and below the footer. Equal: the
	//                  footer used to sit on the panel's bottom edge with
	//                  0.04 above the title (maintainer, 2026-09-08).
	//   gap  0.02 SH   the unit of space between groups
	//
	//   title                          hT
	//   1.5 gap
	//   1 the item    2 ITEM i OF n    hM + hS   a tight pair
	//   gap
	//   3 the doing   4 its totals     hM + hS   a tight pair
	//   0.5 gap                                  the bar is the item's, so it sits close
	//   5 bar / spinner / done-note    hM        one row, whichever of the three is showing
	//   1.5 gap                                  elapsed is the whole run's, so the gap opens beneath the bar
	//   6 elapsed                      hS
	//   gap
	//   7 footer                       hS
	//
	//   H = 2 pad + hT + 3 hM + 4 hS + 5.5 gap
	//
	// Shipped theme (Roboto Bold; title 0.029, text 0.033, small 0.025 of
	// min(W, H), x1.31 menu scale under 720 px):
	//   640x480    hT 28.5  hM 30    hS 24    pad 24  gap 9.6   H 315 = 0.66 SH, top 0.17
	//   1280x800   hT 37.5  hM 40.5  hS 31.5  pad 40  gap 16    H 453 = 0.57 SH
	//   1920x1080  hT 48    hM 54    hS 42    pad 54  gap 21.6  H 605 = 0.56 SH
	// ES defaults (ubuntu condensed 0.085 / 0.045 / 0.035), for a theme that
	// sets no menu fonts:
	//   640x480    hT 76.5  hM 42    hS 30                      H 423 = 0.88 SH
	//   1920x1080  hT 135   hM 70.5  hS 55.5                    H 795 = 0.74 SH
	// That is the tallest a real theme makes it. Past 0.9 SH every pitch is
	// scaled down together; a glyph is two thirds of its row, so rows stay
	// apart down to a factor of 0.67, further than any theme pushes it.
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
	mStatus  ->setPosition(x, y);          // 1. the item
	mCounter ->setPosition(x, y + rM);     // 2. ITEM i OF n
	y += rM + rS + gap;
	mActivity->setPosition(x, y);          // 3. what it is doing on it
	mDetail  ->setPosition(x, y + rM);     // 4. that item's files and bytes
	y += rM + rS + 0.5f * gap;
	// 5. The bar, the spinner and the done-note share one row, centred on it.
	// The bar used to be drawn a row below the spinner it replaced, so the
	// page's centre of gravity moved every time the percentage came and went.
	// No caption on the spinner: line 3 already says WORKING..., and the
	// caption is set at construction because BusyComponent::setText("") is a
	// no-op against its empty initial state -- the default WORKING... showed
	// beside the spinner, twice on one screen (maintainer, 2026-09-06).
	mBusyAnim.setBackgroundVisible(false);
	mBusyAnim.setSize(w, rM);
	mBusyAnim.setPosition(x, y);
	mBarW = SW * 0.6f;
	mBarH = SH * 0.014f;
	mBarX = cx - mBarW / 2.0f;
	mBarY = y + (rM - mBarH) / 2.0f;
	// Once the run is over the bar's row is free, and it is where the one
	// thing left to do goes: a content run that changed the ROMs on this
	// device is not visible in the game lists until they are rebuilt, and
	// nobody should have to know that (maintainer, 2026-09-07).
	mNote    ->setPosition(x, y + (rM - hS) / 2.0f);
	y += rM + 1.5f * gap;
	mElapsed ->setPosition(x, y);          // 6. elapsed
	y += rS + gap;
	mFooter  ->setPosition(x, y);          // 7. the notice; y + rS + pad == top + H

	mPanelSize = Vector2f(w + SW * 0.06f, H);
	mPanelPos  = Vector2f(cx - mPanelSize.x() / 2.0f, top);
	mBackground.fitTo(mPanelSize, Vector3f(mPanelPos.x(), mPanelPos.y(), 0), Vector2f(-32, -32));
}

// Nothing to join: the run is the job's (CloudTransferJob), not this
// page's. The page that owned its worker made quitting the interface wait
// on a restore (before #187); since D-UI-078 the page is sat in and a run
// ends by finishing or by CANCEL, so this destructor meets a finished or a
// cancelled run in practice -- and a running one only if the page were
// popped by something other than the player, which the job survives.
GuiCloudTransfer::~GuiCloudTransfer()
{
}

// Set before the page is pushed and read on the interface thread alone
// (input, update, getHelpPrompts), so no lock: the run's mutex guards the
// run's state, and this is the page's.
void GuiCloudTransfer::setCompletedAction(const std::function<void()>& action, const std::string& helpVerb,
	const std::string& footer, const std::string& note)
{
	if (mOtherRun)
	{
		LOG(LogWarning) << "GuiCloudTransfer: completed action refused, the page shows another command's run";
		return;
	}
	mCompletedAction = action;
	mCompletedHelpVerb = helpVerb;
	mCompletedFooter = footer;
	mCompletedNote = note;
}

void GuiCloudTransfer::clearDoneRows()
{
	if (mNote) mNote->setText("");
	if (mCounter) mCounter->setText("");
	if (mDetail) mDetail->setText("");
}

// While the transfer runs, B asks whether to cancel it (D-UI-078: the page
// is sat in, and CANCEL is the one way out); every other press is
// refused, since there is nothing to choose and a stray press should not
// dismiss a page somebody is waiting on. Not B either on a page whose
// completed run has an action in place of an exit (setCompletedAction): a
// settings restore is replacing the configuration under this process, and
// there is nowhere safe to go while it does. Once it has finished the page
// waits for the person rather than the other way round: any button
// dismisses it, and when the run did not complete, A runs the same command
// again from this page -- the surface that reported the failure carries the
// retry (D-CLOUD-077, D-UI-028). A question the run asked us to put to the
// player is raised as the page goes, once the outcome has been read (#145).
bool GuiCloudTransfer::input(InputConfig* config, Input input)
{
	if (!input.value)
		return true;
	CloudTransferJob& job = *mJob;
	std::unique_lock<std::mutex> lock(job.mMutex);
	if (!job.mFinished)
	{
		if (cancellable() && config->isMappedTo(BUTTON_BACK, input))
		{
			lock.unlock();
			askCancel();
		}
		return true;
	}
	const Outcome o = outcome(job);
	// OK, the player's mapping: the literal "a" was back's own button on a
	// swapped one, so back retried (#308 5-cloud claude F-CS-07). Not a
	// match: the same apply again is always refused (retries(), below).
	if (!o.completed && retries(job) && config->isMappedTo(BUTTON_OK, input))
	{
		// A new run of the same command; the finished one is let go, and the
		// page shows the new run from its first line. The command is
		// unchanged, so a backup that includes the settings archive
		// re-archives on retry -- rotation keeps three, and stripping the
		// parts that finished is not worth the complexity.
		const std::string command = job.mCommand, title = job.mTitle;
		const int items = job.mItemsExpected, after = job.mItemsAfterContent;
		lock.unlock();
		mJob = CloudTransferJob::start(command, title, items, after);
		clearDoneRows();
		mShownFinished = false;
		mShownPercent = -1;
		updateHelpPrompts();
		return true;
	}
	// A restore may have brought files into folders the lists scanned at
	// boot -- screenshots in particular (#82). Re-read what changed once this
	// page is gone -- when any part of the run finished, not only when all
	// of it did: a saves restore that landed under a ROMs restore that did
	// not still put screenshots where the lists cannot see them.
	bool anyTierOk = false;
	for (auto& t : job.mTiers)
		if (t.rc == 0 || t.rc == 9)
			anyTierOk = true;
	const bool restored = (o.completed || anyTierOk) && job.mCommand.find("restore") != std::string::npos;
	// A completed run with an action set has nowhere to go back to
	// (setCompletedAction), so the press that would have closed the page
	// takes the action instead. Copied out first: the page is gone by the
	// time it runs.
	std::function<void()> completedAction;
	if (o.completed && mCompletedAction)
		completedAction = mCompletedAction;
	// A question a script asked us to put to the player (#100, #127, #145),
	// raised here rather than when the line arrived: this page ends when it
	// is dismissed (es-native-ui.md, the fourth tier), and the run's outcome
	// is what it exists to show. Raising the offer mid-run would put a
	// dialog over a page somebody is watching, and raising it the instant
	// the run ended would cover the outcome with a question about why there
	// was nothing to restore -- least-surprise.md: what the screen told them
	// happens first, then what to do about it.
	//
	// Not when the completed run has an action instead of an exit: that
	// action is a restart, and a dialog it is about to take away is worse
	// than no dialog. Nothing composes the two today -- a settings restore
	// is its own run -- and if one ever does, the restart owns the moment.
	std::string offer;
	std::vector<std::string> offerArgs;
	if (o.completed && !completedAction)
	{
		offer = job.mOffer;
		offerArgs = job.mOfferArgs;
	}
	lock.unlock();
	// The outcome has been read: the run is no longer the CLOUD page's to
	// report, and its row goes back to saying what it does (fork #187).
	CloudTransferJob::dismiss(mJob);
	Window* window = mWindow;
	delete this;
	if (completedAction)
	{
		completedAction();
		return true;
	}
	if (restored)
		window->postToUiThread([] { SystemData::rescanChangedFolders(); });
	CloudOffer::present(window, offer, offerArgs);
	return true;
}

// What cancelling means, read at the moment of deciding (D-UI-023): each
// file rclone moves is renamed into place whole, so what is across stays,
// and the next run finishes what this one did not (D-CLOUD-077). YES first,
// NO last so B answers NO. Nothing of the page is captured: the stop is a
// static call on the run in flight, and the page shows the outcome as the
// run ends.
void GuiCloudTransfer::askCancel()
{
	mWindow->pushGui(new GuiMsgBox(mWindow,
		_("CANCEL THIS BACKUP OR RESTORE?") + std::string("\n\n")
			+ _("WHAT'S ALREADY IN PLACE STAYS. THE NEXT BACKUP OR RESTORE FINISHES WHAT THIS ONE DIDN'T."),
		_("YES"), [] { CloudTransferJob::stopByPlayer(); },
		_("NO"), nullptr));
}

// Whether the same command again can do what this run did not. A match
// cannot: its apply spends the preview's plan before anything runs
// (cloud_content_restore, PL-001), so a second apply is refused with
// SOMETHING CHANGED SINCE YOU CHECKED, and the page names the match's row
// instead (CloudText::matchRecovery, #308 gpt F-CS-26).
bool GuiCloudTransfer::retries(const CloudTransferJob& job)
{
	return CloudText::transferKind(job.mCommand) != CloudText::TransferKind::Match;
}

std::vector<HelpPrompt> GuiCloudTransfer::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	CloudTransferJob& job = *mJob;
	std::unique_lock<std::mutex> lock(job.mMutex);
	if (!job.mFinished)
	{
		if (cancellable())
			prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
		return prompts;
	}
	const Outcome o = outcome(job);
	if (!o.completed && retries(job))
		prompts.push_back(HelpPrompt(BUTTON_OK, _("TRY AGAIN")));
	// The page's one exit says what it does: CLOSE, or the word the
	// action was given (setCompletedAction).
	prompts.push_back(HelpPrompt(BUTTON_BACK, o.completed && mCompletedAction && !mCompletedHelpVerb.empty()
		? mCompletedHelpVerb : _("CLOSE")));
	return prompts;
}

// The word for the run (D-UI-028; es-native-ui.md "Outcome vocabulary").
//
// COMPLETED when every part did -- rclone's 9, nothing needed moving,
// counts. Everything short of that is COULDN'T FINISH, including a run
// whose parts disagree: a tier that finished
// beside one that did not, a unit that finished inside a tier that did not
// (its later units failed with a why of their own), or a match cut off
// after it had already removed files. Those used to read COMPLETED WITH
// GAPS; a half-outcome the player cannot act on reads as "working kind of"
// and costs more trust than a plain failure does (maintainer, 2026-09-10,
// D-UI-030). `partial` survives as the flag that shapes the lines below --
// what moved is still said, and line 4 still names the items that did not
// finish -- but it no longer changes the word. SKIPPED for the two sentinels the
// scripts exit before touching anything (CloudExit.h), when that is all
// the run has to report; they are not failures, and FAILED would send
// somebody to a log to find nothing wrong. COULDN'T FINISH for everything
// else; the why goes on line 4 with the items it stopped. A run with no
// tier lines -- the match, or a command composed before they existed -- is
// read from its exit code alone, as it always was.
//
// Skipped means nothing was touched. A run that had moved bytes or finished
// a file before the network went was cut, not skipped, and its exit 69 reads
// COULDN'T FINISH like any other cut (#153, 2026-09-21: a settings archive
// cut at 75 % on S3 read SKIPPED - YOU'RE NOT ONLINE over WHAT MADE IT IS IN
// YOUR CLOUD, two words wrong in the same direction the vocabulary keeps
// apart).
GuiCloudTransfer::Outcome GuiCloudTransfer::outcome(const CloudTransferJob& job)
{
	Outcome o;
	// Stopped by a launch the player chose over it (D-CLOUD-129): the
	// card's own phrase for the same thing (ThreadedCloudSync's cancel), and
	// no failed item -- nothing went wrong, and the next run finishes what
	// this one did not.
	if (job.mStoppedForGame || job.mStoppedByPlayer)
	{
		o.completed = false;
		o.partial = false;
		o.skipped = true;
		o.word = job.mStoppedByPlayer ? _("SKIPPED - YOU CANCELLED IT") : _("SKIPPED - YOU STARTED A GAME");
		return o;
	}
	o.completed = job.mExit == 0 || job.mExit == 9;
	bool anyOk = false, anyBad = false, anyUnitOk = false;
	int onlyCode = -2;   // the one code every failed tier shares, or -1 when they differ
	for (auto& t : job.mTiers)
	{
		const bool ok = t.rc == 0 || t.rc == 9;
		anyOk = anyOk || ok;
		anyBad = anyBad || !ok;
		if (!ok)
		{
			if (!t.tierLevelFail && t.unitsFailed > 0 && t.unitsAnnounced > t.unitsFailed)
				anyUnitOk = true;
			onlyCode = onlyCode == -2 ? t.rc : onlyCode == t.rc ? onlyCode : -1;
		}
	}
	const bool match = job.mCommand.find("--match") != std::string::npos;
	o.partial = !o.completed && ((anyOk && anyBad) || anyUnitOk || (match && job.mRemovedFiles > 0));
	const int code = job.mTiers.empty() ? job.mExit : onlyCode;
	const bool progressed = job.mAnyTransferred || job.mRunFiles > 0 || job.mUnitFiles > 0;
	o.skipped = !o.completed && !o.partial && !progressed && (code == CloudExit::LockHeld || code == CloudExit::NoNetwork);
	if (o.completed)
		o.word = _("COMPLETED");
	else if (o.partial)
		o.word = _("COULDN'T FINISH");
	else if (o.skipped && code == CloudExit::LockHeld)
		o.word = _("SKIPPED - A SYNC IS ALREADY RUNNING");
	else if (o.skipped)
		o.word = _("SKIPPED - YOU'RE NOT ONLINE");
	else
		o.word = _("COULDN'T FINISH");
	return o;
}

// The verb's word for a run, from its command (CloudText::transferKind), in
// the running form the cards use (BACKING UP SAVES, SYNCING SAVES...): the
// head of the hub row's line for a run that is current while the hub is
// the top page (the seam left by D-UI-078; see GuiCloudTransfer.h).
std::string GuiCloudTransfer::verbWord(const CloudTransferJob& job)
{
	switch (CloudText::transferKind(job.mCommand))
	{
		case CloudText::TransferKind::Backup:  return _("BACKING UP...");
		case CloudText::TransferKind::Restore: return _("RESTORING...");
		case CloudText::TransferKind::Match:   return _("MATCHING...");
		default:                               return _("WORKING...");
	}
}

// "BACKING UP... - ITEM 2 OF 4", and the item, in the words this page's
// rows 1 and 2 use, so the row and the page say the same thing about the
// same run. Never an OF with nothing on either side: while the count is
// unknown the row reads ITEM i alone, as the page does.
GuiCloudTransfer::RowWords GuiCloudTransfer::rowWords(const std::shared_ptr<CloudTransferJob>& job)
{
	RowWords w;
	if (job == nullptr)
		return w;
	std::unique_lock<std::mutex> lock(job->mMutex);
	w.word = verbWord(*job);
	w.counted = w.word;
	if (job->mItemIndex > 0)
	{
		w.counted += " - " + std::string(_("ITEM")) + " " + std::to_string(job->mItemIndex);
		if (job->mItemCount > 0)
			w.counted += " " + std::string(_("OF")) + " " + std::to_string(job->mItemCount);
		w.item = unitName(job->mUnitLabel);
	}
	return w;
}

std::string GuiCloudTransfer::outcomeWord(const std::shared_ptr<CloudTransferJob>& job)
{
	if (job == nullptr)
		return "";
	std::unique_lock<std::mutex> lock(job->mMutex);
	return outcome(*job).word;
}

std::string GuiCloudTransfer::stillRunningSentence(const std::shared_ptr<CloudTransferJob>& job)
{
	if (job == nullptr)
		return "";
	switch (CloudText::transferKind(job->command()))
	{
		case CloudText::TransferKind::Backup:  return _("YOUR BACKUP TO THE CLOUD IS STILL RUNNING.");
		case CloudText::TransferKind::Restore: return _("YOUR RESTORE FROM THE CLOUD IS STILL RUNNING.");
		case CloudText::TransferKind::Match:   return _("THIS DEVICE IS STILL BEING MATCHED TO THE CLOUD.");
		default:                               return _("YOUR CLOUD TRANSFER IS STILL RUNNING.");
	}
}

void GuiCloudTransfer::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);
	// Dim the whole screen: this is a takeover, and the carousel showing
	// through would say the app is idle while a transfer is in flight.
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x000000D0);

	auto theme = ThemeData::getMenuTheme();

	// A themed nine-patch panel and nothing else, the same surface a menu or a
	// dialog draws. What separates this page from what it covers is the dim
	// above, not an outline: a stroke of its own would make it the one panel
	// in the app that has one.
	mBackground.render(trans);

	for (auto& t : { mTitle, mStatus, mCounter, mActivity, mDetail, mNote, mElapsed, mFooter })
		t->render(trans);

	// The bar is drawn from the snapshot update() took under the lock, along
	// with the text above it -- not from the run's percent, which its worker may have
	// moved on since. One block per frame, for every row and the bar alike.
	if (!mShownFinished)
	{
		if (mShownPercent >= 0)
		{
			// A bar only where there is a real number behind it. An
			// indeterminate spinner is honest; a bar at an invented
			// position is not. The rectangle is the constructor's, on the
			// spinner's row, so the two take turns in one place.
			Renderer::setMatrix(trans);
			Renderer::drawRect(mBarX, mBarY, mBarW, mBarH, (theme->Text.color & 0xFFFFFF00) | 0x40);
			Renderer::drawRect(mBarX, mBarY, mBarW * (mShownPercent / 100.0f), mBarH, theme->Text.color);
		}
		else
			mBusyAnim.render(trans);
	}

	// The keys are the help bar's, drawn here: with full-screen menus on
	// (every handheld) Window draws no help while a second page is open, so
	// the footer used to spell them out as A and B (es-ui-style-guide.md,
	// Interaction rules; the save state manager draws its bar the same way).
	if (mWindow->peekGui() == this && Renderer::ScreenSettings::fullScreenMenus())
		mWindow->renderHelpPromptsEarly(parentTrans);
}

// Clip to one line: a long ROM name gets an ellipsis, never a second line.
std::string GuiCloudTransfer::fitOneLine(const std::shared_ptr<Font>& font, std::string text, float width)
{
	if (!font)
		return text;
	// On characters, not bytes (TextFit; #308 gpt F-CS-32).
	return TextFit::fitOneLine(text, width, [&font](const std::string& t) { return font->sizeText(t).x(); });
}

// The scripts name their two fixed units in English (">>> unit SETTINGS||",
// ">>> unit SAVES||"), and the page used to show that word as it came, so
// the French page read SETTINGS over ELEMENT 1 SUR 1 (#153, 2026-09-21). A
// system's name from the content scripts is a directory and stays as it is.
// The composers' labels in the player's language, a system's folder as it
// came (CloudText::unitLabel).
std::string GuiCloudTransfer::unitName(const std::string& label)
{
	return CloudText::unitLabel(label);
}

std::string GuiCloudTransfer::fitSentences(const std::shared_ptr<Font>& font, std::string text, float width)
{
	if (!font)
		return text;
	while (font->sizeText(text).x() > width)
	{
		// the last sentence boundary before the end: ". " with something after it
		const size_t end = text.find_last_not_of(" .");
		if (end == std::string::npos)
			break;
		const size_t cut = text.rfind(". ", end);
		if (cut == std::string::npos)
			break;
		text = text.substr(0, cut + 1);
	}
	return fitOneLine(font, text, width);
}

// The units table, the size parser and the two formatters live in
// CloudText (#140), where the sync card shares them and the unit tests can
// reach them. These three stay as the names this page's callers -- and the
// content picker and the match confirmation, through sizeLabel -- use.
std::string GuiCloudTransfer::sizeLabel(unsigned long bytes)
{
	return CloudText::sizeLabel(bytes);
}

std::string GuiCloudTransfer::roundSizes(const std::string& f)
{
	return CloudText::roundSizes(f);
}

// rclone's fragments in the player's units, precision and separators:
//   "45% /2.5Mi, 300Ki/s, 5s"                            -> "45% OF 2.5 MB . 300 KB/S . 5S LEFT"
//   "1.4 GiB / 2.0 GiB, 70%, 2.5 MiB/s, ETA 3m2s"        -> "1.40 GB OF 2.00 GB . 70% . 2.5 MB/S . 3M2S LEFT"
//   "16.521 MiB / 16.521 MiB, 100%, 519.844 KiB/s, ETA 0s" -> "16.5 MB OF 16.5 MB . 100% . 520 KB/S . 0S LEFT"
std::string GuiCloudTransfer::prettyRclone(std::string f)
{
	// Piped -- there is no terminal here -- rclone cuts every per-file line
	// at 80 columns, so the last field often arrives torn: "5.722 MiB/",
	// "976.547 Ki". A field is a percentage, a size (ends in B), a speed
	// (ends in /s), or a time (digits and h/m/s); anything else is a
	// fragment and is dropped rather than shown as "5.722 MB/". A bare "-"
	// is rclone's word for a value it does not have yet -- the ETA of a
	// transfer that has not started, the percentage of nothing -- and is
	// dropped too: it is honest, but on the page it read as broken
	// ("0 B OF 0 B . - . 0 B/S . -", review 2026-09-08).
	{
		std::vector<std::string> kept;
		for (auto& raw : Utils::String::split(f, ',', true))
		{
			std::string t = Utils::String::trim(raw);
			if (t.empty()) continue;
			std::string tail = t;
			if (tail.rfind("ETA ", 0) == 0) tail = tail.substr(4);
			const bool pct   = tail.back() == '%';
			const bool size  = tail.back() == 'B' || (tail.size() > 2 && tail.compare(tail.size() - 2, 2, "iB") == 0);
			const bool speed = tail.size() > 2 && tail.compare(tail.size() - 2, 2, "/s") == 0;
			bool time = !tail.empty() && isdigit((unsigned char) tail[0]);
			for (char ch : tail) if (!(isdigit((unsigned char) ch) || ch == 'h' || ch == 'm' || ch == 's')) { time = false; break; }
			if (time && isdigit((unsigned char) tail.back())) time = false;
			// the first field may be "45% /2.5Mi" -- a percentage and a size in one
			const bool pctSize = t.find('%') != std::string::npos && t.find('/') != std::string::npos && (size || t.back() == 'i');
			if (pct || size || speed || time || pctSize)
				kept.push_back(t);
		}
		f.clear();
		for (size_t i = 0; i < kept.size(); i++)
			f += (i ? ", " : "") + kept[i];
	}
	f = roundSizes(f);
	auto rep = [&f](const std::string& from, const std::string& to) { f = Utils::String::replace(f, from, to); };
	for (const auto& u : CloudText::rcloneUnits())
		if (std::string(u.rclone) != u.shown)
			rep(u.rclone, u.shown);
	rep("ETA ", "");
	rep(" / ", " OF "); rep(" /", " OF ");
	rep(", ", " · ");
	f = Utils::String::toUpper(f);
	// a trailing duration -- digits then a unit letter, no percent, no bytes --
	// is time left. The separator is four bytes, not three: the middle dot is
	// two in UTF-8, and skipping three left a space on the front of the last
	// segment that isdigit() refused, so LEFT was never appended (2026-09-09).
	static const std::string SEP = " · ";
	size_t sep = f.rfind(SEP);
	std::string last = sep == std::string::npos ? f : f.substr(sep + SEP.size());
	if (!last.empty() && isdigit((unsigned char) last[0]) && last.find('%') == std::string::npos
		&& last.find('B') == std::string::npos && !isdigit((unsigned char) last.back()))
		f += " " + std::string(_("LEFT"));
	return f;
}

void GuiCloudTransfer::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);
	CloudTransferJob& job = *mJob;
	std::unique_lock<std::mutex> lock(job.mMutex);
	// One snapshot per frame for the bar (render() draws from it) and the
	// rows below, so they cannot show two different stats blocks. The help
	// bar changes with the page -- TRY AGAIN and CLOSE once it is done --
	// and is refreshed after the lock: getHelpPrompts takes it too.
	const bool justFinished = job.mFinished && !mShownFinished;
	mShownFinished = job.mFinished;
	mShownPercent  = job.mPercent;
	// The run's clock, not a page's: this page may be the second one opened
	// on the run, and elapsed is how long the run has been going.
	const int elapsedMs = job.elapsedMsLocked();
	const int mins = elapsedMs / 60000;
	const int secs = (elapsedMs / 1000) % 60;
	char elapsed[32];
	snprintf(elapsed, sizeof(elapsed), "%d:%02d", mins, secs);

	if (job.mFinished)
	{
		// Seven lines, the same rows as the run (D-UI-024/026), now carrying
		// the outcome (D-UI-028): 1 the word; 2 how many items did not finish,
		// when partial; 3 what moved; 4 the items that did not finish and why; 5
		// what is in place, or what to do next; 6 elapsed; 7 the buttons.
		const Outcome o = outcome(job);
		const bool restore = job.mCommand.find("restore") != std::string::npos;
		const bool match = job.mCommand.find("--match") != std::string::npos;
		mStatus->setText(fitOneLine(mTextFont, o.word, mLineWidth));

		// 2. When partial, the count: N distinct items that did not finish of the
		// run's M -- the run's count when a script announced it, else what
		// was reached, never fewer than N.
		std::string counter;
		if (o.partial)
		{
			std::vector<std::string> names;
			for (auto& f : job.mFailed)
			{
				const std::string name = unitName(f.label);
				if (std::find(names.begin(), names.end(), name) == names.end())
					names.push_back(name);
			}
			const int n = (int) names.size();
			const int m = std::max(std::max(job.mItemCount, job.mItemIndex), n);
			counter = std::to_string(n) + " " + std::string(_("OF")) + " " + std::to_string(m) + " "
				+ std::string(n == 1 && m == 1 ? _("ITEM DID NOT FINISH") : _("ITEMS DID NOT FINISH"));
		}
		mCounter->setText(fitOneLine(mSmallFont, counter, mLineWidth));

		// 3 and 4: what moved, and what did not.
		if (job.mRemovedFiles > 0)
		{
			// A match is mostly deletion, and rclone's totals for a deletion
			// are "0 B / 0 B" -- true and useless. Line 3 carries what the
			// confirmation showed instead: what went. Line 4 is per system
			// when the match completed, and the items it did not reach when
			// it was cut (below).
			std::string removed = std::string(_("REMOVED")) + " " + std::to_string(job.mRemovedFiles) + " "
				+ std::string(job.mRemovedFiles == 1 ? _("FILE FROM THIS DEVICE") : _("FILES FROM THIS DEVICE"));
			if (job.mRemovedBytes > 0)
				removed += " · " + sizeLabel(job.mRemovedBytes);
			mActivity->setText(fitOneLine(mTextFont, removed, mLineWidth));
			if (o.completed)
			{
				std::string detail;
				for (auto& d : job.mRemovedDetail)
					detail += (detail.empty() ? "" : "   ") + d;
				mDetail->setText(fitOneLine(mSmallFont, detail, mLineWidth));
				if (job.mAnyTransferred)
					mCounter->setText(fitOneLine(mSmallFont, _("FILES YOUR CLOUD HAD AND THIS DEVICE DIDN'T CAME DOWN TOO."), mLineWidth));
			}
		}
		else
		{
			// Line 3 answers for the whole run, not its last unit: the page
			// used to end on "SNES  3 OF 3" over that unit's totals, or over
			// nothing when the last unit only compared (#85). It carries the
			// sum of every unit's final "Transferred:" pair, in the run's own
			// verb -- shown on a partial run too, because what moved is the half of
			// the answer that is good news. Only what rclone printed: a run
			// that never printed a byte line says the word and nothing more.
			//
			// The count is of finished files. The bytes are rclone's
			// bytes-read counter, and on a run that stopped or failed that
			// includes what the transfers in flight had read when it died
			// and never completed -- so a run that did not complete names
			// its files and no size, rather than claim as BACKED UP bytes
			// that were not.
			std::string summary;
			const bool sized = o.completed && job.mRunBytes > 0;
			if (job.mRunSized && (job.mRunFiles > 0 || sized))
			{
				if (job.mRunFiles > 0)
					summary = std::to_string(job.mRunFiles) + " " + std::string(job.mRunFiles == 1 ? _("FILE") : _("FILES"));
				if (sized)
					summary += (summary.empty() ? "" : " · ") + sizeLabel((unsigned long) job.mRunBytes);
				summary += " " + std::string(restore ? _("RESTORED") : _("BACKED UP"));
			}
			else if (job.mRunSized && o.completed)
			{
				// Nothing moved and the run succeeded: everything was there
				// already. On a failure the same zero means something else,
				// so the sentence is not offered. The clause after the dash
				// is dropped whole on a panel too narrow for it, rather than
				// ending in an ellipsis -- measured in the font it is set in.
				summary = restore ? _("NOTHING NEW - YOU ALREADY HAD IT ALL")
				                  : _("NOTHING NEW - YOUR CLOUD ALREADY HAD IT ALL");
				if (mTextFont && mTextFont->sizeText(summary).x() > mLineWidth)
					summary = restore ? _("NOTHING NEW TO BRING DOWN") : _("NOTHING NEW TO SEND UP");
			}
			mActivity->setText(fitOneLine(mTextFont, summary, mLineWidth));
			mDetail  ->setText("");
		}
		if (!o.completed && !o.skipped)
		{
			// 4. The items that did not finish, and why: "NES, SETTINGS - YOUR
			// CLOUD STOPPED ANSWERING". Items sharing a why share the line's
			// one dash; a second why gets its own group. A why with no item
			// (the scripts spoke before any unit, and no tier line followed)
			// stands alone. Not on a skip: line 1 has said the one thing
			// there is to say about every part, and a list of them under it
			// would read as a list of failures.
			std::vector<std::pair<std::string, std::vector<std::string>>> groups;
			for (auto& f : job.mFailed)
			{
				auto g = groups.begin();
				for (; g != groups.end(); ++g)
					if (g->first == f.why)
						break;
				if (g == groups.end())
					g = groups.insert(groups.end(), std::make_pair(f.why, std::vector<std::string>()));
				const std::string name = unitName(f.label);
				if (!name.empty() && std::find(g->second.begin(), g->second.end(), name) == g->second.end())
					g->second.push_back(name);
			}
			std::string detail;
			for (auto& g : groups)
			{
				std::string names;
				for (size_t i = 0; i < g.second.size(); i++)
					names += (i ? ", " : "") + g.second[i];
				// In the interface's language: a script says its why in
				// English (">>> why"), the code's phrase is translated
				// already, and localizedWhy passes one it does not list as it
				// came (#308 F-CS-31).
				detail += (detail.empty() ? "" : "  ·  ") + names + (names.empty() ? "" : " - ") + CloudText::localizedWhy(g.first);
			}
			mDetail->setText(fitOneLine(mSmallFont, detail, mLineWidth));
		}

		// 5. What is in place -- one clause per verb, true because rclone
		// renames each file into place when it is complete and the content
		// scripts delete nothing outside a match (D-CLOUD-077) -- when the
		// run did not complete. When it did, the one thing left to do: a
		// content run that changed the ROMs on this device is not visible in
		// the game lists until they are rebuilt, and nobody should have to
		// know that (maintainer, 2026-09-07).
		const bool contentRun = job.mCommand.find("cloud_content_restore") != std::string::npos;
		std::string note;
		if (!o.completed)
		{
			// Files, not bytes: rclone renames a file into place only once it
			// is whole and a cut S3 PUT lands nothing, so a run that moved nine
			// megabytes of one archive has put nothing anywhere (#153).
			// mUnitFiles is the last unit's count where foldUnit has not run.
			const bool moved = job.mRunFiles > 0 || job.mUnitFiles > 0;
			// A match: what it removed, counted from what rclone deleted
			// (#308 gpt F-CS-26), and nothing about the cloud having it --
			// a match removes only what the cloud does not have.
			if (match)
				note = CloudText::matchRemovedNote(job.mRemovedFiles);
			else if (restore)
				note = moved ? _("WHAT MADE IT IS ON THIS DEVICE. NOTHING ELSE CHANGED.") : _("DON'T WORRY, NOTHING CHANGED.");
			else
				note = moved ? _("WHAT MADE IT IS IN YOUR CLOUD. THE REST IS STILL HERE.") : _("DON'T WORRY, NOTHING CHANGED.");
		}
		else if (contentRun && (job.mRemovedFiles > 0 || job.mAnyTransferred))
			note = _("UPDATE GAMELISTS UNDER GAME SETTINGS TO SEE THE CHANGE.");
		// A completed run whose only exit is an action says what that
		// action does here, where a run that finished cleanly otherwise has
		// nothing to put (setCompletedAction).
		if (o.completed && mCompletedAction && !mCompletedNote.empty())
			note = mCompletedNote;
		// Two sentences on a 640px panel do not fit the small font; the
		// first alone says what is in place, so it is what survives.
		mNote->setText(fitSentences(mSmallFont, note, mLineWidth));

		mElapsed ->setText(std::string(_("ELAPSED")) + " " + elapsed);
		// 7. The retry lives on the surface that reported the failure: OK
		// runs the same command again (input), back closes, and the help bar
		// -- drawn by this page (render) -- carries the two on the player's
		// own buttons, which may be swapped; the line named them A and B
		// (#308 5-cloud claude F-CS-07, gpt F-CS-36). A run that completed
		// has nothing to retry.
		//
		// Not for a match: its apply spends the preview's plan (PL-001), so
		// the same command again is refused with SOMETHING CHANGED SINCE YOU
		// CHECKED. Line 7 names the row that checks again instead, as the
		// card names its row (#308 gpt F-CS-26).
		mFooter  ->setText(!o.completed ? (match ? CloudText::matchRecovery() : std::string())
			: mCompletedAction && !mCompletedFooter.empty() ? mCompletedFooter
			: _("PRESS ANY BUTTON TO CLOSE"));
	}
	else
	{
		// 1 and 2: the item, and which of how many. The item is the row that
		// changes when the run moves on, so it leads; the file in flight used
		// to, and the page read bottom-up -- a file, its percentage, and only
		// then what they belonged to (maintainer, 2026-09-09, D-UI-026). The
		// count is the whole run's (handleLine), so settings, saves and two
		// systems read ITEM 1 OF 4 through ITEM 4 OF 4 whichever script is
		// speaking. Never an OF with nothing on either side: while the count
		// is unknown the row reads ITEM i alone.
		if (job.mItemIndex == 0)
		{
			mStatus ->setText(_("PREPARING..."));
			mCounter->setText("");
		}
		else
		{
			const std::string item = unitName(job.mUnitLabel);
			mStatus->setText(item.empty() ? _("WORKING...") : fitOneLine(mTextFont, item, mLineWidth));
			std::string counter = std::string(_("ITEM")) + " " + std::to_string(job.mItemIndex);
			if (job.mItemCount > 0)
				counter += " " + std::string(_("OF")) + " " + std::to_string(job.mItemCount);
			mCounter->setText(counter);
		}

		// 3: what it is doing on this item -- the line that says it is alive.
		// A thousand small BIOS files spend minutes between percentage
		// changes, and a frozen percentage is indistinguishable from a hang.
		std::string doing;
		if (!job.mCurrent.empty())
		{
			// "TRANSFERRING name.zip . 45% OF 2.5 MB . 300 KB/S . AND 3 MORE
			// FILES": a single space inside a segment and " . " between them,
			// the same as every other row (#85). The name is the part that
			// has to show; the rest is shed a segment at a time, least useful
			// first, until the line fits this font: the AND N MORE count goes
			// before the file's own progress, and last the name alone is
			// clipped -- half a percentage after an ellipsis says nothing,
			// and line 4 carries the item's percentage regardless.
			const std::string head     = std::string(_("TRANSFERRING")) + " " + job.mCurrent;
			const std::string progress = job.mFileProgress.empty() ? "" : " · " + prettyRclone(job.mFileProgress);
			const std::string more     = job.mFilesThisBlock > 1
				? " · " + std::string(_("AND")) + " " + std::to_string(job.mFilesThisBlock - 1) + " " + std::string(_("MORE FILES")) : "";
			const auto fits = [this](const std::string& t) { return mTextFont && mTextFont->sizeText(t).x() <= mLineWidth; };
			if (fits(head + progress + more))
				doing = head + progress + more;
			else if (fits(head + progress))
				doing = head + progress;
			else
				doing = fitOneLine(mTextFont, head, mLineWidth);   // unchanged when it fits, clipped when it does not
		}
		else if (job.mChecksTotal > 0 || job.mListed > 0)
		{
			// Nothing in flight, plenty happening: rclone is comparing what
			// is here with what is there, and for a device whose saves are
			// all in the cloud already that is the whole run. It prints no
			// per-file line for a comparison, so this is the count it does
			// print -- and the name, when it caught one mid-comparison and
			// there is room beside the count. A run that showed neither
			// looked hung until it said COMPLETED (maintainer, 2026-09-08).
			// Before anything is queued to compare the only count is what
			// rclone has listed, and that counts both sides -- 40 saves list
			// as 80 -- so it is not shown as a number the player would try
			// to reconcile with their files; the spinner on row 5 is the sign
			// of life until the first check is queued. The word is the card's
			// (COMPARING SAVES, D-UI-075): one compare, one name, on both
			// surfaces (#157).
			if (job.mChecksTotal > 0)
				doing = std::string(_("COMPARING")) + " " + std::to_string(job.mChecksDone) + " " + std::string(_("OF")) + " "
					+ std::to_string(job.mChecksTotal) + " " + std::string(_("FILES"));
			else
				doing = _("COMPARING FILES...");
			// the name is the line's one optional segment, and the first to go
			if (!job.mChecking.empty())
			{
				const std::string named = doing + " · " + job.mChecking;
				if (mTextFont && mTextFont->sizeText(named).x() <= mLineWidth)
					doing = named;
			}
		}
		else if (job.mDoing == "archive")
		{
			// backuptool is writing the settings archive and prints nothing
			// this page can use, so ES announces it (">>> doing archive") and
			// the settings item says what it is doing like every other item,
			// rather than sitting on a spinner (maintainer, 2026-09-09).
			doing = _("PACKING UP YOUR SETTINGS...");
		}
		else if (job.mDoing == "unpack")
		{
			// The same item on the way back: backuptool is extracting the
			// archive over the live configuration, and says nothing this
			// page can use either (">>> doing unpack", #114).
			doing = _("PUTTING YOUR SETTINGS BACK...");
		}
		else if (job.mItemIndex > 0)
			doing = _("WORKING...");
		mActivity->setText(doing);

		// 4: this item's totals -- the count line and the byte line of the
		// last stats block. "12 / 45, 27%" -> "12 OF 45 FILES . 27%".
		std::string totals;
		if (!job.mFilesTotals.empty())
		{
			auto pct = job.mFilesTotals.find(", ");
			totals = Utils::String::replace(job.mFilesTotals.substr(0, pct), " / ", " " + std::string(_("OF")) + " ") + " " + std::string(_("FILES"));
			if (pct != std::string::npos)
				totals += " · " + job.mFilesTotals.substr(pct + 2);
		}
		// "0 B / 0 B, -, 0 B/s, ETA -" is the byte line while nothing is queued
		// to move -- the whole of a run that only compares -- and it is true of
		// nothing anybody asked about. Line 3 says what such a run is doing;
		// this line stays blank rather than read "0 B OF 0 B . 0 B/S".
		if (!job.mTotals.empty() && job.mTotals.rfind("0 B / 0 B", 0) != 0)
			totals += (totals.empty() ? "" : "   ") + prettyRclone(job.mTotals);
		mDetail->setText(fitOneLine(mSmallFont, totals, mLineWidth));

		mElapsed->setText(std::string(_("ELAPSED")) + " " + elapsed);
		// 7. That it takes a while. How to cancel, where the page can be, is
		// the help bar's, on the player's own back button: the line said
		// PRESS B TO CANCEL, the wrong button on a swapped mapping (#308
		// 5-cloud claude F-CS-07, gpt F-CS-36).
		mFooter->setText(fitOneLine(mSmallFont, _("THIS CAN TAKE A WHILE."), mLineWidth));
	}
	lock.unlock();
	if (justFinished)
		updateHelpPrompts();
}
