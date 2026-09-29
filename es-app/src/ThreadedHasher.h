#pragma once

#include <thread>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>
#include "components/AsyncNotificationComponent.h"

class FileData;

class ThreadedHasher
{
public:
	enum HasherType : unsigned int
	{
		HASH_NETPLAY_CRC = 1,
		HASH_CHEEVOS_MD5 = 2,
		HASH_ALL = HASH_NETPLAY_CRC | HASH_CHEEVOS_MD5,
	};

	static void start(Window* window, HasherType type, bool forceAllGames=false, bool silent=false, std::set<std::string>* systems = nullptr);
	static void stop();
	static bool isRunning() { return mInstance != nullptr; }
	// Whether a run this session got RetroAchievements' hash library. The
	// network watcher starts the startup index again at link-up until one has
	// (fork #183); a run without the library indexes nothing.
	static bool cheevosLibraryCameThisSession() { return sCheevosLibraryCame; }
	static bool checkCloseIfRunning(Window* window);

	static void pause() { mPaused = true; }
	static void resume() { mPaused = false; }

private:
	// lookupOnly: the games with a cheevosHash and no cheevosId (CheevosIndex::Take::Lookup).
	// Those whose hash the library knows join the queue once it has come; none is read.
	ThreadedHasher(Window* window, HasherType type, std::queue<FileData*> searchQueue, const std::vector<FileData*>& lookupOnly, bool forceAllGames = false);
	~ThreadedHasher();

	void updateUI(const std::string label);
	static std::string formatGameName(FileData* game);

	std::queue<FileData*> mSearchQueue;
	// The queued games that are here for a lookup alone: no hash, no CRC is
	// read for them. Filled in the constructor, read by the threads, never
	// written again, so it needs no lock.
	std::unordered_set<FileData*> mLookupOnly;

	Window* mWindow;
	AsyncNotificationComponent* mWndNotification;
	std::string		mCurrentAction;

	std::vector<std::string> mErrors;
	std::map<std::string, std::string>    mCheevosHashes;

	HasherType mType;

	void run();

	//std::thread* mHandle;
	std::vector<std::thread*>	mThreads;
	int							mThreadCount;

	int mTotal;
	bool mExit;
	bool mForce;
	// Whether this run identified games at all: RetroAchievements' hash
	// library came and there was something to hash. Only then does the
	// offline cache have a reason to follow it (fork #184, D-RA-013).
	bool mCheevosIndexed;

	static bool mPaused;
	static ThreadedHasher* mInstance;
	static bool sCheevosLibraryCame;
};

