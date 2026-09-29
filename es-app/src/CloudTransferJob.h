#pragma once
#ifndef ES_APP_CLOUD_TRANSFER_JOB_H
#define ES_APP_CLOUD_TRANSFER_JOB_H

#include "CloudText.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <sys/types.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// One run of a transfer command -- a back up, a restore or a match as
// GuiMenu composes them, or the journey's first restore -- apart from the
// page that shows it (GuiCloudTransfer): the job reads the pipe to its end
// whatever happens to the page. It was built so the page could be left
// with the run going on (fork #187); since D-UI-078 the page is sat in and
// CANCEL is the way out, and the separation earns its keep the other way
// round -- the page's lifetime never decides the run's, and a cancel is a
// signal to the run, not a page closing. A 1.4 GiB restore is an hour on a
// handheld's Wi-Fi, and the fourth surface tier (es-native-ui.md) is a
// page that outlives the job; the scan page's OfflineScanJob is the model
// this follows.
//
// The run is a thread of its own reading the command's stdout -- rclone's
// stats blocks and the scripts' ">>> " lines; handleLine is the reader and
// its comment the contract -- into the state below, and it keeps itself
// alive (the thread holds the shared_ptr) until the command exits, whether
// or not a page is watching. One run at a time: start() hands back the run
// in flight when there is one, as the scripts' own flock would refuse a
// second (CloudExit::LockHeld). A finished run stays current() until a page
// has shown its outcome and been dismissed (dismiss()), so the row that
// launched it can say how it went and a press can open the outcome page --
// with TRY AGAIN on it -- however long nobody was watching.
//
// The page is the run's one view and reads the state under mMutex as a
// friend: forty fields, every one of them a row on the page, and a copy of
// them per frame would be the pattern for its own sake.
class CloudTransferJob : public std::enable_shared_from_this<CloudTransferJob>
{
public:
	// What each part of a composed run reported as it ended (">>> tier
	// <label>|<rc>", echoed by GuiMenu's run composition after each part),
	// and which items did not finish, with why (D-UI-028). A ">>> why
	// <sentence>" from a script is attached to the unit it arrived under --
	// or, printed before any unit, to the tier that reports next -- so the
	// done page can name NES and SETTINGS and say what stopped them. A tier
	// that fails with no why under it is itself the item that did not
	// finish, with the code's phrase; its units are not presumed to have
	// finished, because nothing said they did.
	struct Tier { std::string label; int rc; int unitsAnnounced; int unitsFailed; bool tierLevelFail; };
	struct Failed { std::string label; std::string why; };

	// The run in flight, or the last one not yet dismissed; null when none.
	static std::shared_ptr<CloudTransferJob> current();
	// Whether a run is in flight.
	static bool running();
	// Start a run of command, or hand back the one in flight. itemsExpected
	// and itemsAfterContent are the page's estimate of ITEM i OF n
	// (GuiCloudTransfer's constructor says how they are counted).
	static std::shared_ptr<CloudTransferJob> start(const std::string& command, const std::string& title,
		int itemsExpected, int itemsAfterContent);
	// The outcome has been read on a page: the run is no longer current, and
	// the row that launched it goes back to saying what it does.
	static void dismiss(const std::shared_ptr<CloudTransferJob>& job);
	// A launch the player chose over the run in flight (STOP IT AND PLAY,
	// D-CLOUD-129): the run's process group is sent SIGTERM -- SIGKILL when
	// hard -- and the run is marked stopped for a game, so its outcome reads
	// SKIPPED - YOU STARTED A GAME rather than a failure. False when no run
	// is in flight. The caller waits for running() to turn false before the
	// game starts: the signal is not the end of the run, the process ending
	// is (ThreadedCloudSync::cancelForLaunch says why).
	static bool stopForLaunch(bool hard);
	// The player's CANCEL on the transfer page (D-UI-078): the same SIGTERM
	// to the run's process group, and the run marked stopped by the player,
	// so its outcome reads SKIPPED - YOU CANCELLED IT and the parts it was
	// inside are restamped with the player's own token. False when no run
	// is in flight.
	static bool stopByPlayer();
	// Tests only: run inside a stop between its look at whether the run
	// has finished and its marking the run stopped -- the seam audit G-E2-05
	// named. Empty, and never set, in the application.
	static std::function<void()> testPauseInStop;

	const std::string& command() const { return mCommand; }
	const std::string& title() const { return mTitle; }
	int itemsExpected() const { return mItemsExpected; }
	int itemsAfterContent() const { return mItemsAfterContent; }
	bool finished() const;
	// When the run ended, epoch seconds; 0 while it runs.
	time_t finishedAt() const;
	// Stopped by stopForLaunch: the launch's word, not a failure.
	bool stoppedForGame() const { return mStoppedForGame; }
	// Stopped by stopByPlayer: the player's word, not a failure.
	bool stoppedByPlayer() const { return mStoppedByPlayer; }

private:
	friend class GuiCloudTransfer;

	CloudTransferJob(const std::string& command, const std::string& title, int itemsExpected, int itemsAfterContent);
	// The thread that runs it holds the job (start()), so nothing here does.
	void run();
	void handleLine(const std::string& line);
	void refreshPercent();
	void foldUnit();
	static std::string cleanLine(const std::string& raw);
	static int parsePercent(const std::string& body);
	// Since the run began, frozen once it has ended. With mMutex held.
	int elapsedMsLocked() const;

	std::string mCommand;
	std::string mTitle;
	int mItemsExpected, mItemsAfterContent;

	mutable std::mutex mMutex;
	std::string mCurrent;       // "name.zip" -- the head of the current block
	std::string mFileProgress;  // "45% /2.5Mi, 300Ki/s, 5s" -- that file's own line
	std::string mTotals;        // "1.4 GiB / 2.0 GiB, 70%, 2.5 MiB/s, ETA 3m2s"
	std::string mFilesTotals;   // "12 / 45, 27%" -- the count line of the same block
	std::string mUnitLabel;     // ">>> unit nes|2|5" from the script: what is being copied
	std::string mDoing;         // ">>> doing archive": what the item is busy with before rclone runs
	// ITEM i OF n across the whole run (D-UI-026). One run chains several
	// scripts and each numbers only its own units, so the run counts: a
	// ">>> unit" whose label differs from the current one is the next item,
	// the same label again is a re-announcement and is not. mItemCount
	// starts as the page's estimate (0 = unknown) and is corrected by a
	// script that announces its own count: items counted before that
	// script's first announcement (mScriptBase) + its count + the
	// single-item phases chained after it (mTrailing). mLastScriptIndex is
	// the last announcement's own index, 0 when it carried none, so a count
	// that starts over is a new script.
	int mItemIndex, mItemCount, mTrailing, mScriptBase, mLastScriptIndex;
	// What this unit's rclone has moved so far, as its last "Transferred:"
	// pair read: the bytes line's first field and the count line's first
	// number. Folded into the run's totals when the next unit starts and
	// when the command exits (foldUnit), so the done page can say what the
	// whole run moved rather than what its last unit did. mRunSized records
	// that a byte line was parsed at all: without one there is no number to
	// show, and the page says so rather than inventing a zero.
	long mUnitBytes, mUnitFiles;
	long mRunBytes, mRunFiles;
	bool mRunSized;
	// ">>> removed 14|314572800|snes:12:300000000,gb:2:14572800" -- a match's
	// summary, rendered on the last screen in place of rclone's totals, which
	// for a deletion read "0 B / 0 B" (maintainer, 2026-09-07).
	long mRemovedFiles, mRemovedBytes;
	std::vector<std::string> mRemovedDetail;   // "SNES 12 FILES · 300 MB", per system
	bool mAnyTransferred;       // some block moved bytes: the device's ROMs changed
	int mFilesThisBlock;
	bool mSeenBlock;
	// "Checks:  12 / 45, 27%, Listed 300" -- what rclone compared rather than
	// moved. A run where everything is already in the cloud is nothing but
	// checks: no bytes, no " * file" lines, and without these it looked hung.
	long mChecksDone, mChecksTotal, mListed;
	std::string mChecking;      // " * name: checking" -- the file being compared, if rclone caught one in flight
	int mChecksThisBlock;
	// One block of rclone's output carries up to three percentages: bytes,
	// files transferred, files checked. Each is that line's last word (-1
	// when it printed no number) so the one shown (mPercent) is chosen in
	// refreshPercent(), not by whichever line came last.
	int mBytePercent, mFilePercent, mCheckPercent;
	int mPercent;               // -1 until a block has produced a real number
	bool mFinished;
	int mExit;
	std::vector<Tier> mTiers;
	std::vector<Failed> mFailed;
	std::vector<Failed> mPendingWhys;          // since the last tier line; label "" before any unit
	std::vector<std::string> mUnitsSinceTier;  // items announced since the last tier line
	std::string mWhy;                          // the last why of the run, for a command with no tiers
	// ">>> offer create-saves-folder|<folder>[|<near>]" -- a question a
	// script asked us to put to the player, and the fields it carries.
	// Raised when the page is dismissed, not when the line arrives: the
	// page ends when the player has read the outcome, and a dialog over a
	// run still going has nothing to do with the run (#145).
	std::string mOffer;
	std::vector<std::string> mOfferArgs;
	// Elapsed is the run's, not a page's: while the page could be closed
	// and opened again over a run (#187, before D-UI-078) a counter that
	// ticked only while a page was up read 0:05 after ten minutes; the
	// run's own clock is right in either design.
	std::chrono::steady_clock::time_point mStarted;
	time_t mStartedAt;   // wall clock, for a stamp the snapshot below cannot place
	// The script stamps as they were before the command ran
	// (ThreadedCloudSync::readStamps): a stop restamps only the files
	// written since, not what the clock calls this run's (orchestrator
	// finding G-E2-O2; E1's G-E1-04 on the card, 54d5699b2).
	std::vector<CloudText::StampText> mStampsBefore;

	// Stopped for a game or by the player: the scripts' trap stamped each
	// part it was inside with 130 and no token; say what happened in their
	// place (#203), with the token for who stopped it.
	void restampStoppedParts(const char* token);
	int mElapsedMs;             // frozen when the run ends
	time_t mFinishedAt;
	// The command's process group (run() starts it under setsid and reads
	// its ">>> pid N" first line), for stopForLaunch; and whether that
	// happened. Atomics: written by the reader thread and the launch gate,
	// read by the page.
	std::atomic<pid_t> mPid{0};
	std::atomic<bool> mStoppedForGame{false};
	std::atomic<bool> mStoppedByPlayer{false};
	// The signal a stop asked for (0 for none), and the last one sent: a
	// stop before the pid line is kept and sent when the line arrives.
	std::atomic<int> mStopSignal{0};
	std::atomic<int> mSignalSent{0};
	void requestStop(int sig);
	void deliverStop();

	static std::mutex sMutex;
	static std::shared_ptr<CloudTransferJob> sCurrent;
};

#endif // ES_APP_CLOUD_TRANSFER_JOB_H
