#pragma once

#include <atomic>
#include <mutex>
#include <ctime>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <sys/types.h>
#include "CloudText.h"
#include "components/AsyncNotificationComponent.h"

// Runs a headless cloud sync command in the background with a native
// progress card, instead of taking over the screen with a console.
class ThreadedCloudSync
{
public:
	// Who asked for the saves to move. A run that has one leaves a stamp --
	// /storage/.cache/cloud_sync/last-sync-<origin>, "<epoch> <rc>" -- that
	// the cloud settings page reads back as the line under the toggle that
	// caused it (fork #94). The card is gone seconds after the work ends,
	// and somebody who set SYNC SAVES DURING STARTUP needs an answer to "did
	// it run this morning?" from a screen that still exists.
	//
	// The scripts' own stamps (last-backup, last-restore) are per direction
	// and skip the runs that did nothing (CloudExit::LockHeld, NoNetwork);
	// this one is per cause and records the whole run, skips included,
	// because the question under the toggle is "what happened at startup",
	// and "nothing, no network" is an answer to it.
	//
	// None is for jobs that are not a saves sync -- tidying folders, the
	// wizard's first backup -- which have no toggle to report to.
	enum class Origin { None, Startup, Exit, Manual };

	// `running` is what the card says while it works -- "BACKING UP..." --
	// where `title` names the operation for the line it prints when it is
	// done. One string for both read as "Back up all system data syncing
	// with the cloud", which says neither what is happening nor that it is.
	static void start(Window* window, const std::string& command,
	                  const std::string& title, const std::string& running = "",
	                  Origin origin = Origin::None);
	// Read from any thread without the lock -- the launch gate, the save
	// state manager, the proxy's cards -- so the pointer is atomic; only the
	// answer is read here, never the object (#308 F-CS-25).
	static bool isRunning() { return mInstance.load() != nullptr; }

	// A game launch during a sync EmulationStation started on its own -- the
	// startup sync, the after-a-game backup -- cancels the sync in whatever
	// phase it is in and waits for it to be gone before the launch goes
	// ahead (#101, maintainer's decision, 2026-09-09). It used to cancel
	// only while the sync was still waiting for the network (fork #94); a
	// transfer under way was refused, and with rclone's own timeouts as the
	// only bound on a link that had dropped, that refusal could stand for
	// many minutes (#103). A sync the player asked for keeps the refusal:
	// Origin::Manual, and Origin::None -- the wizard's first backup and the
	// folder tidy -- since they pressed it and can wait for it or stop it
	// themselves.
	//
	// True means the launch may proceed: the sync was one of ours and has
	// ended. False means it may not: nothing running, the player's own
	// sync, or one that had not ended within the budget (two seconds, with
	// SIGKILL to the group at one and a half). The caller refuses on false
	// exactly as it always did.
	//
	// Works on the protocol run() gives every command: it runs under setsid,
	// so its pid is its process group, and its first line is ">>> pid N".
	// The group is sent SIGTERM, so the shell, the scripts and their rclone
	// go together; the scripts' trap exits CloudExit::Stopped, the card says
	// SKIPPED - A GAME WAS STARTED and the stamp records the same, as the
	// network-wait cancel always did.
	//
	// `refusal`, where the caller passes one, says which false it is, so
	// the launch gate can tell the player the right thing (#115). Waiting
	// is the answer to their own sync and the wrong answer to one that is
	// already on its way out.
	enum class CancelRefusal
	{
		// The player pressed this sync. It is still running and was not
		// asked to stop; it finishes, or they stop it.
		PlayerStarted,
		// An automatic sync was signalled and had not gone within the
		// budget -- or had already gone by the time we looked. Either way
		// it is not there to wait for; a moment later the launch works.
		Stopping
	};
	// evenIfPlayerStarted: the player has answered STOP IT AND PLAY to the
	// launch question (D-CLOUD-129; since D-CLOUD-130 it is asked over every
	// sync, automatic ones included), so the origin no longer protects a
	// sync they pressed; the card says SKIPPED - YOU STARTED A GAME.
	static bool cancelForLaunch(CancelRefusal* refusal = nullptr, bool evenIfPlayerStarted = false);

	// The outcome vocabulary's why for an exit code the scripts did not
	// explain with a ">>> why" line (D-UI-028): rclone 3/4 YOUR CLOUD FOLDER
	// WASN'T FOUND, 5 YOUR CLOUD STOPPED ANSWERING, 7/8 YOUR CLOUD REFUSED
	// THE TRANSFER, 130 IT WAS STOPPED, else SOMETHING WENT WRONG. The token
	// is the same table as one word for the stamps; whyForToken reads it
	// back. Shared with the transfer page and the rows under the toggles so
	// every surface says the same thing about the same code.
	// One stamp file in the last-sync shape (recordOutcome's comment), at any
	// path: recordOutcome's own, and a script's last-backup / last-restore
	// when this process knows an outcome the script's trap could not name.
	static void writeStamp(const std::string& path, int rc, const std::string& token, const std::string& why);
	// A run of `command` that began at runStarted was stopped: the part its
	// stop interrupted -- the script stamp its trap wrote with 130 this run
	// (CloudText::stampsToRestamp) -- is restamped with `token`, the
	// stopper's word for it. Parts that finished keep their outcome; parts
	// that never started keep their last real run's. The card and the
	// transfer page both end a stopped run through here.
	static void restampStoppedParts(const std::string& command, time_t runStarted, const std::string& token);
	// The same with the stamps as they were when the run began (readStamps
	// at its start): a stamp is this run's when its file was written since,
	// not when the clock says so (audit of the fixes G-E1-04/06). The card
	// takes this snapshot; a caller that has none uses the one above.
	static void restampStoppedParts(const std::string& command, const std::vector<CloudText::StampText>& before,
		time_t runStarted, const std::string& token);
	// Each stamp a command's scripts can write, with its text and the file's
	// identity (inode and change time) now.
	static std::vector<CloudText::StampText> readStamps(const std::string& command);
	static std::string whyForCode(int rc);
	static std::string tokenForCode(int rc);
	static std::string whyForToken(const std::string& token);

private:
	void run();
	// The stamp's fields: "<epoch> <rc> <token>[ <why>]" -- the token is one
	// word for the outcome (completed, gaps, no-network, lock-held,
	// cancelled, stopped, folder-missing, cloud-stopped, cloud-refused,
	// unknown), the why the scripts' own sentence when they printed one
	// (">>> why ..."), for the row to show instead of the token's phrase.
	static void recordOutcome(Origin origin, int rc, const std::string& token, const std::string& why);

	ThreadedCloudSync(Window* window, const std::string& command,
	                  const std::string& title, const std::string& running,
	                  Origin origin);
	~ThreadedCloudSync();

	std::string					mCommand;
	std::string					mTitle;
	std::string					mRunning;
	Origin						mOrigin;
	std::vector<CloudText::StampText> mStampsBefore;   // the script stamps as the run began (G-E1-04/06)
	time_t						mStartedAt;   // wall clock, to tell a stamp this run wrote

	// Set and read across the worker and the main thread; see
	// cancelForLaunch. mWaitingForNetwork records that the network step is
	// under way (CHECKING THE CONNECTION... / WAITING FOR A NETWORK..., fork
	// #192); it no longer gates the cancel.
	std::atomic<pid_t>			mPid{0};
	std::atomic<bool>			mWaitingForNetwork{false};
	std::atomic<bool>			mCancelled{false};

	// Read by run() alone, on the worker thread. mWhy is the last ">>> why
	// <sentence>" the scripts printed, upper-cased, for the outcome line;
	// mMoved records that rclone's byte totals ever left "0 B", which is
	// what decides the in-place clause; mTiers is every ">>> tier
	// <label>|<rc>" a composed command echoed after each of its parts, so a
	// run where one part finished and another did not is COMPLETED WITH
	// GAPS rather than the last part's code (D-UI-028).
	std::string					mWhy;
	std::string					mOffer;   // a question the script asked us to put to the player once the run ends (#100)
	std::vector<std::string>	mOfferArgs;   // what the question is about: the missing folder, then a near name beside it (#127)
	bool						mMoved{false};
	std::vector<std::pair<std::string, int>> mTiers;
	// Whether SYNC SAVES WHEN EXITING A GAME is on, read in the constructor
	// on the interface thread: the recovery clause for a cancelled sync
	// says the saves go when the game exits only if something will send
	// them then.
	bool						mGameExitSync{false};

	// The two-half bar of a composed sync (D-UI-052, #157), read and
	// written by run() alone: the half the command announced last (">>>
	// doing receive" / ">>> doing send"; None for a command that announces
	// no halves, whose bar is the run's own percentage as it always was),
	// the bar as last drawn, so it never moves back once a half is known,
	// whether this half's byte totals have left zero -- from then on the
	// byte line is the fact and the compare count stays off the words --
	// and whether the compare count has been said for this half, so a
	// still byte line leaves it on the words instead of flickering the
	// number on and off (#208; CloudText::liveWords).
	CloudText::Phase			mPhase{CloudText::Phase::None};
	int							mBar{-1};
	bool						mBytesMoving{false};
	bool						mCountShown{false};
	// rclone's file count for this half (done / of), from its count line;
	// the byte line's words name the file in flight from it (fork #304).
	long						mFilesDone{-1};
	long						mFilesTotal{-1};

	Window*						mWindow;
	AsyncNotificationComponent* mWndNotification;

	static std::atomic<ThreadedCloudSync*> mInstance;
	// Every change to mInstance, and every use of the object it names, is
	// made under this lock: start() checks and installs in one hold, run()
	// clears it before the card's linger, the destructor clears it if it is
	// still this one, and cancelForLaunch dereferences it -- so a caller
	// that took the pointer under the lock has an object that outlives the
	// call, and two starts cannot both see "none" (#308 F-CS-25).
	static std::mutex			sInstanceLock;
};
