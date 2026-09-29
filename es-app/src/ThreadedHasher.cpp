#include "ThreadedHasher.h"
#include "Window.h"
#include "FileData.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "Gamelist.h"
#include "RetroAchievements.h"
#include "SystemConf.h"
#include "SystemData.h"
#include "FileData.h"
#include "ApiSystem.h"
#include "OfflineAchievements.h"
#include "ProxyCards.h"
#include "CheevosIndex.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"
#include <ctime>
#include <unordered_set>
#include <queue>

#include "LocaleES.h"

#define ICONINDEX _U("\uF002 ")

ThreadedHasher* ThreadedHasher::mInstance = nullptr;
bool ThreadedHasher::mPaused = false;
bool ThreadedHasher::sCheevosLibraryCame = false;

static std::mutex mLoaderLock;

ThreadedHasher::ThreadedHasher(Window* window, HasherType type, std::queue<FileData*> searchQueue, const std::vector<FileData*>& lookupOnly, bool forceAllGames)
	: mWindow(window), mWndNotification(nullptr)
{
	mForce = forceAllGames;
	mExit = false;
	mType = type;
	mCheevosIndexed = false;
	mThreadCount = 0;

	mSearchQueue = searchQueue;
	mTotal = mSearchQueue.size();

	if ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5)
	{
		try
		{
			mCheevosHashes = RetroAchievements::getCheevosHashes();
			if (mCheevosHashes.size() == 0)
			{
				// Offline, or not yet: nothing can be identified without it. Said
				// once, so a boot that indexed nothing can be told apart from one
				// that never tried (fork #183); the link-up run tries again.
				LOG(LogWarning) << "ThreadedHasher: RetroAchievements' hash library did not come; nothing is indexed this run";
				// The games that waited for it are said to be waiting (fork
				// #299, D-UI-104): the toast, and the control script's marker
				// so the link's return looks them up.
				if (!mSearchQueue.empty())
					ProxyCards::indexRanOffline(mWindow, (int) mSearchQueue.size());
				while (!mSearchQueue.empty())
					mSearchQueue.pop();
				// And nothing is counted: mTotal was taken from the queue
				// above, so it stayed at the games that were waiting. A card
				// and threads were made for an empty queue, the destructor
				// toasted INDEXING COMPLETED beside the offline card, and the
				// threads -- ending at once -- could delete this before
				// start() read it back (#308 1-raoffline claude F-RA-05).
				// With 0, the check below returns before any of that.
				mTotal = 0;
			}
			else
				sCheevosLibraryCame = true;

			// The games with a hash and no id (audit #186 PL-08): the ones
			// the library knows join the queue for a lookup and a save, and
			// nothing else -- their ROM is never read again. RC-5 left every
			// game indexed in a session that ended with a reboot in this
			// state (fork #183): the hash in its recovery file, the id
			// decided afterwards in memory and lost. The ones the library
			// does not know stay as they are, so a library of games without
			// a set costs no card and no toast at every startup.
			for (FileData* game : lookupOnly)
			{
				const std::string hash = Utils::String::toUpper(game->getMetadata(MetaDataId::CheevosHash));
				if (mCheevosHashes.find(hash) == mCheevosHashes.cend())
					continue;
				mSearchQueue.push(game);
				mLookupOnly.insert(game);
				mTotal++;
			}
			mCheevosIndexed = !mCheevosHashes.empty() && !mSearchQueue.empty();
		}
		catch (const std::exception& e)
		{
			// The fetch failed outright -- offline, nearly always -- with the
			// same games waiting: the same word to the player (fork #299).
			if (!mSearchQueue.empty())
				ProxyCards::indexRanOffline(mWindow, (int) mSearchQueue.size());
			mType = (HasherType)0;
			throw e;
		}
	}

	// A run with nothing to do -- lookups alone, and the library knew none
	// of them -- shows no card and starts no thread; start() deletes it.
	if (mTotal == 0)
		return;

	mWndNotification = mWindow->createAsyncNotificationComponent();

	if (mType == HASH_CHEEVOS_MD5)
		mWndNotification->updateTitle(ICONINDEX + _("SEARCHING RETROACHIEVEMENTS"));
	else 
		mWndNotification->updateTitle(ICONINDEX + _("SEARCHING NETPLAY GAMES"));

	int num_threads = std::thread::hardware_concurrency() / 2;
	if (num_threads == 0)
		num_threads = 1;

	mThreadCount = num_threads;
	for (size_t i = 0; i < num_threads; i++)
		mThreads.push_back(new std::thread(&ThreadedHasher::run, this));
}

ThreadedHasher::~ThreadedHasher()
{
	if ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5 && mTotal > 0)
	{
		mWindow->displayNotificationMessage(ICONINDEX + _("INDEXING COMPLETED") + std::string(". ") + _("UPDATE GAMELISTS TO APPLY CHANGES."));

		// The offline cache follows this index (fork #184, D-RA-013): once
		// the games are identified, and while the device is still connected
		// (the hash library just came from RetroAchievements), the games not
		// yet cached for offline play are cached from the ids and hashes
		// written above -- raofflineproxy-ctl topup --after-index, from a
		// thread of its own, with a card once it has work (fork #293). Not
		// after a run the player stopped, and not after one that identified
		// nothing.
		if (!mExit && mCheevosIndexed)
			OfflineAchievements::topUpAfterIndex(mWindow);
	}

	if (mWndNotification != nullptr)
		mWndNotification->close();
	mWndNotification = nullptr;

	ThreadedHasher::mInstance = nullptr;
}

std::string ThreadedHasher::formatGameName(FileData* game)
{
	return "[" + game->getSystemName() + "] " + game->getName();
}

void ThreadedHasher::updateUI(const std::string label)
{
	std::string idx = std::to_string(mTotal + 1 - mSearchQueue.size()) + "/" + std::to_string(mTotal);
	int percent = 100 - (mSearchQueue.size() * 100 / mTotal);
		
	mWndNotification->updateText(label);
	mWndNotification->updatePercent(percent);	
}

void ThreadedHasher::run()
{
	std::unique_lock<std::mutex> lock(mLoaderLock);

	bool cheevos = ((mType & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5);
	bool netplay = ((mType & HASH_NETPLAY_CRC) == HASH_NETPLAY_CRC);

	while (!mExit && !mSearchQueue.empty())
	{
		FileData* game = mSearchQueue.front();

		auto label = formatGameName(game);

		LOG(LogDebug) << "Hashing " << formatGameName(game);
		updateUI(label);

		mSearchQueue.pop();

		lock.unlock();

		if (mPaused)
		{
			while (!mExit && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}		

		// A game here for a lookup alone is not read: not its CRC, not its
		// hash (checkCheevosHash returns on the hash it has; checkCrc32 would
		// not, in a system netplay never asked for).
		const bool lookupOnly = mLookupOnly.count(game) > 0;

		if (netplay && !lookupOnly)
		{
			LOG(LogDebug) << "CheckCrc32 : " << label;
			game->checkCrc32(mForce);
		}

		if (cheevos)
		{
			LOG(LogDebug) << "CheckCheevosHash : " << label;
			if (!lookupOnly)
				game->checkCheevosHash(mForce);

			auto hash = Utils::String::toUpper(game->getMetadata(MetaDataId::CheevosHash));
			if (!hash.empty())
			{
				// The id reaches the disk with the hash. checkCheevosHash
				// saved the game to the recovery folder with the hash and no
				// id, because the id is decided here, afterwards, and until
				// now it lived in memory until the gamelist was written at
				// exit. The offline cache's scan reads that folder for the
				// index (fork #184, D-RA-013), so a changed id is saved the
				// same way.
				const std::string before = game->getMetadata(MetaDataId::CheevosId);
				auto cheevos = mCheevosHashes.find(hash);
				const std::string id = cheevos != mCheevosHashes.cend() ? cheevos->second : std::string();
				game->setMetadata(MetaDataId::CheevosId, id);
				if (id != before)
				{
					saveToGamelistRecovery(game);
					if (lookupOnly)
						LOG(LogInfo) << "ThreadedHasher: id " << id << " for " << label << " from the hash library, no file read (audit #186 PL-08)";
				}
			}

			LOG(LogDebug) << "CheckCheevosHash OK : " << label;;
		}		

		lock.lock();
	}

	mThreadCount--;

	if (mThreadCount == 0)
	{
		lock.unlock();
		delete this;
		ThreadedHasher::mInstance = nullptr;

	}
}

bool ThreadedHasher::checkCloseIfRunning(Window* window)
{
	if (ThreadedHasher::mInstance != nullptr)
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT?"), _("YES"), []
		{
			ThreadedHasher::stop();
		}, _("NO"), nullptr));

		return false;
	}

	return true;
}

void ThreadedHasher::start(Window* window, HasherType type, bool forceAllGames, bool silent, std::set<std::string>* systems)
{
	if (ThreadedHasher::mInstance != nullptr)
	{
		if (silent)
			return;

		if (!checkCloseIfRunning(window))
			return;
	}
	
	std::queue<FileData*> searchQueue;
	// The games with a hash and no id: a lookup against the library once it
	// has come, no read (CheevosIndex, audit #186 PL-08).
	std::vector<FileData*> lookupOnly;
	
	for (auto sys : SystemData::sSystemVector)
	{
		if (sys->isGroupSystem() || sys->isCollection())
			continue;

		bool takeNetplay = ((type & HASH_NETPLAY_CRC) == HASH_NETPLAY_CRC) && sys->isNetplaySupported();
		bool takeCheevos = ((type & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5) && sys->isCheevosSupported();

		if (!takeNetplay && !takeCheevos)
			continue;

		if (systems != nullptr && systems->find(sys->getName()) == systems->cend())
			continue;
		
		if (!sys->isGameSystem() || sys->getRootFolder() == nullptr)
			continue;

		if (sys->isGroupChildSystem() ? sys->isHidden() : !sys->isVisible())
			continue;

		for (auto file : sys->getRootFolder()->getFilesRecursive(GAME))
		{
			bool netPlay = takeNetplay && (forceAllGames || file->getMetadata(MetaDataId::Crc32).empty());
			CheevosIndex::Take take = CheevosIndex::Take::None;
			if (takeCheevos)
				take = CheevosIndex::take(forceAllGames, file->getMetadata(MetaDataId::CheevosHash), file->getMetadata(MetaDataId::CheevosId));
			bool cheevos = take == CheevosIndex::Take::Hash;

			if (cheevos)
			{
				std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(file->getPath()));
				
				if (ext == ".pbp" || ext == ".cso") // Currently unsupported formats
					cheevos = false;
			}

			if (netPlay || cheevos)
				searchQueue.push(file);
			else if (take == CheevosIndex::Take::Lookup)
				lookupOnly.push_back(file);
		}
	}

	if (searchQueue.size() == 0 && lookupOnly.empty())
	{
		if (!silent)
			window->pushGui(new GuiMsgBox(window, _("NO GAMES FIT THAT CRITERIA.")));

		return;
	}

	// Lookups alone, from the silent startup run: once a day at most
	// (CheevosIndex::lookupDue) -- the pass costs the hash library, and
	// nearly every library has a game RetroAchievements does not know whose
	// hash would ask for it at every boot. A run the player asked for, or
	// one with games to hash (the library comes anyway), is not held back.
	// The stamp is written once the library has come, so an offline boot
	// does not spend the day's pass on a fetch that failed.
	const bool lookupsAlone = searchQueue.empty() && silent && ((type & HASH_CHEEVOS_MD5) == HASH_CHEEVOS_MD5);
	const long long now = (long long) time(nullptr);
	if (lookupsAlone)
	{
		const long long last = strtoll(Settings::getInstance()->getString("CheevosLookupOnlyLast").c_str(), nullptr, 10);
		if (!CheevosIndex::lookupDue(last, now))
		{
			LOG(LogInfo) << "ThreadedHasher: " << lookupOnly.size() << " game(s) with a hash and no id; the lookup pass ran within the day, next one later";
			return;
		}
	}

	try
	{
		ThreadedHasher* hasher = new ThreadedHasher(window, type, searchQueue, lookupOnly, forceAllGames);
		// Stamped only when the library came: a boot without it has not spent
		// the day's pass (the comment above promised as much; the code did not).
		if (lookupsAlone && !hasher->mCheevosHashes.empty())
		{
			Settings::getInstance()->setString("CheevosLookupOnlyLast", std::to_string(now));
			Settings::getInstance()->saveFile();
		}
		if (hasher->mTotal == 0)
		{
			// Lookups alone and the library knew none of them: nothing was
			// started, nothing is shown -- as a run with nothing to hash.
			delete hasher;
			if (!silent)
				window->pushGui(new GuiMsgBox(window, _("NO GAMES FIT THAT CRITERIA.")));
			return;
		}
		ThreadedHasher::mInstance = hasher;
	}
	catch (const std::exception& e)
	{
		LOG(LogError) << "Game Hash failed : " << e.what();

		if (!silent)
			window->pushGui(new GuiMsgBox(window, e.what()));
	}	
}

void ThreadedHasher::stop()
{
	auto thread = ThreadedHasher::mInstance;
	if (thread == nullptr)
		return;

	try
	{
		thread->mExit = true;
	}
	catch (...) {}
}

