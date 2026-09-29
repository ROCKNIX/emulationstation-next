#pragma once

#include <chrono>
#include <mutex>
#include <thread>
#include "Scraper.h"

class ScraperThread
{
public:
	ScraperThread(int threadId);
	void run(const ScraperSearchParams& params);
	int updateState();

	ScraperSearchParams& getSearchParams() { return mSearch; }
	ScraperSearchResult& getResult() { return mResult; }

	int getError() { return mErrorStatus; }
	std::string getErrorString() { return mStatusString; }

	int mThreadId;

private:
	void acceptResult(ScraperSearchResult& result)
	{
		mResult = result;
		mStatus = ASYNC_DONE;
		mErrorStatus = 0;
	}

	void processError(int status, const std::string statusString)
	{
		mStatus = ASYNC_ERROR;
		mErrorStatus = status;
		mStatusString = statusString;
	}

	
	int mStatus;
	int mErrorStatus;
	std::string mStatusString;

	ScraperSearchResult mResult;
	ScraperSearchParams mSearch;
	std::unique_ptr<ScraperSearchHandle> mSearchHandle;
	std::unique_ptr<MDResolveHandle> mMDResolveHandle;
};

// The scrape runs here, on threads of its own, and reports to a page
// (GuiScraperRun) through progress() -- the fourth surface tier
// (es-native-ui.md, D-UI-078, fork #241): a page that owns the screen for
// the run's length, whose one way out while it runs is CANCEL (stop()).
// Upstream drew the run on a corner card the player could play through and
// ended it with a toast; a scrape is minutes for a library, and a job that
// long gets a page, not a card.
class ThreadedScraper
{
public:
	// The run as a page reads it: what it is on, what it has done, how it
	// ended. Copied out under a lock; the run's own counters stay with it.
	struct Progress
	{
		bool running = false;     // a run is in flight, or ended and not yet let go
		int done = 0;             // games finished, scraped or not
		int total = 0;
		std::string game;         // the one being looked up now
		int errors = 0;           // games the scraper could not do (a miss, a timeout)
		bool finished = false;
		bool cancelled = false;   // stop(): the player's word for the outcome
		bool failed = false;      // the scraper refused the whole run (a bad login, a ban)
		std::string failure;      // its words, when it did
		int elapsedMs = 0;        // since the run began; frozen once finished
	};

	static void start(Window* window, const std::queue<ScraperSearchParams>& searches);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	static Progress progress();
	
	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

	static std::string formatGameName(FileData* game);

private:
	ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches, int threadCount);
	~ThreadedScraper();

	void Process();
	void ProcessNextGame(ScraperThread* thread);

	Window* mWindow;
	
	std::string		mCurrentGame;

	std::vector<std::string> mErrors;

	void run();

	std::thread* mHandle;
	std::queue<ScraperSearchParams> mSearchQueue;

	std::vector<ScraperThread*> mScraperThreads;
	
	void acceptResult(ScraperThread& thread);
	void processError(int status, const std::string statusString);
	void updateUI();

	int mTotal;
	int mThreadCount;
	int mExitCode;

	static bool mPaused;
	static ThreadedScraper* mInstance;

	static std::mutex sProgressMutex;
	static Progress sProgress;
	static std::chrono::steady_clock::time_point sStarted;
};
