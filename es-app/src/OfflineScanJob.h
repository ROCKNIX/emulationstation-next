#pragma once
#ifndef ES_APP_OFFLINE_SCAN_JOB_H
#define ES_APP_OFFLINE_SCAN_JOB_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>

class Window;

// One run of raofflineproxy-ctl scan (fork #179, D-RA-010), apart from the
// page that shows it (GuiOfflineScan). The run is a thread of its own
// reading the ctl's stdout -- ">>> " lines, the header of raofflineproxy-ctl
// is the contract -- into the state below, and it keeps itself alive (the
// thread holds the shared_ptr) until the ctl exits. One run at a time:
// start() hands back the run in flight when there is one, as the ctl itself
// would refuse a second (exit 75).
//
// The page sits on the run for its length (D-UI-078, #241): the one way out
// while it runs is CANCEL, which is cancel() here -- the ctl gets SIGINT,
// traps it as the player's cancel, stamps the run and exits 130; what it
// saved stays saved, and the next scan passes over those games. For a week
// (audit #186 PL-07) the page could be left and the run went on in the
// background, reported only on the row that launched it; the maintainer met
// that prompt on the device and asked what would happen, and the honest
// answer was "nothing tells you".
class OfflineScanJob : public std::enable_shared_from_this<OfflineScanJob>
{
public:
	struct State
	{
		bool listing = false;     // ">>> doing listing": the library is being walked
		int total = 0;            // ">>> total n" / the n of ">>> game i|n|name"
		int index = 0;            // the i
		std::string game;         // the name
		int cached = 0, skipped = 0, ready = -1;
		int errors = 0;           // ">>> errors n": games a fetch failed for (audit #186 PL-24)
		bool limit = false, nothingNew = false;
		bool truncated = false;   // ">>> note TRUNCATED": the walk stopped at the client's cap of files
		std::string why;          // the ctl's token
		bool cancelled = false;   // cancel(): the player's word for the outcome, whatever the exit
		bool finished = false;
		int exit = -1;
		int elapsedMs = 0;        // since the run began; frozen once finished
	};

	// The run in flight or the last one this session; null when none has run.
	static std::shared_ptr<OfflineScanJob> current();
	// Whether a run is in flight.
	static bool running();
	// Start a run of command, or hand back the one in flight.
	static std::shared_ptr<OfflineScanJob> start(Window* window, const std::string& command);

	State state() const;
	const std::string& command() const { return mCommand; }

	// The player's CANCEL (D-UI-078): SIGINT to the run's process group --
	// the shell, the ctl and its helper together -- and the state marked
	// cancelled, so the page words the outcome as the player's choice and
	// not as a failure. The ctl's INT trap writes the stamp. Nothing to do
	// once the run has ended.
	void cancel();

	// Run on the interface thread whenever the run's state changes and once
	// when it ends -- at most one waiting at a time -- so the row that
	// offered the scan can follow it with no page open. The page sets it on
	// attach; the last setter wins.
	void setOnChanged(const std::function<void()>& onChanged);

private:
	OfflineScanJob(Window* window, const std::string& command);
	// The thread that runs it holds the job (start()), so nothing here does.
	void run();
	void handleLine(const std::string& line);
	void changed();
	static std::string cleanLine(const std::string& raw);

	Window* mWindow;
	std::string mCommand;
	mutable std::mutex mMutex;
	State mState;
	// The command's process group (run() starts it under setsid and reads
	// its ">>> pid N" first line), for cancel(). A cancel that arrives before
	// the line has (mCancelWanted) is delivered when it does.
	std::atomic<pid_t> mPid{0};
	bool mCancelWanted = false;
	std::chrono::steady_clock::time_point mStarted;
	std::function<void()> mOnChanged;
	bool mPostPending = false;   // a changed() already waits on the interface thread

	static std::mutex sMutex;
	static std::shared_ptr<OfflineScanJob> sCurrent;
};

#endif // ES_APP_OFFLINE_SCAN_JOB_H
