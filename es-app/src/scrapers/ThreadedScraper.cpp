#include "ThreadedScraper.h"
#include "Window.h"
#include "FileData.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "Gamelist.h"
#include "Log.h"

ThreadedScraper* ThreadedScraper::mInstance = nullptr;
bool ThreadedScraper::mPaused = false;
std::mutex ThreadedScraper::sProgressMutex;
ThreadedScraper::Progress ThreadedScraper::sProgress;
std::chrono::steady_clock::time_point ThreadedScraper::sStarted;

ThreadedScraper::Progress ThreadedScraper::progress()
{
	std::unique_lock<std::mutex> lock(sProgressMutex);
	Progress p = sProgress;
	if (p.running && !p.finished)
		p.elapsedMs = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sStarted).count();
	return p;
}

ThreadedScraper::ThreadedScraper(Window* window, const std::queue<ScraperSearchParams>& searches, int threadCount)
	: mSearchQueue(searches), mWindow(window)
{
	mExitCode = ASYNC_IN_PROGRESS;
	mTotal = (int) mSearchQueue.size();
	mThreadCount = threadCount;
}

void ThreadedScraper::Process()
{
	{
		// A fresh run: the page reads this from its first frame.
		std::unique_lock<std::mutex> lock(sProgressMutex);
		sProgress = Progress();
		sProgress.running = true;
		sProgress.total = mTotal;
		sStarted = std::chrono::steady_clock::now();
	}

	for (int i = 0; i < mThreadCount; i++)
	{
		if (mSearchQueue.size() == 0)
			break;

		ScraperThread* thread = new ScraperThread(i);
		mScraperThreads.push_back(thread);
		ProcessNextGame(thread);
	}

	mHandle = new std::thread(&ThreadedScraper::run, this);
}

void ThreadedScraper::ProcessNextGame(ScraperThread* thread)
{
	auto item = mSearchQueue.front();
	mSearchQueue.pop();
	mCurrentGame = item.getGameName();

	LOG(LogInfo) << "[Thread " << thread->mThreadId << "] ProcessNextGame : " << mCurrentGame;

	thread->run(item);

	updateUI();
}

ThreadedScraper::~ThreadedScraper()
{
	for (auto scraperThread : mScraperThreads)
		delete scraperThread;

	mScraperThreads.clear();

	ThreadedScraper::mInstance = nullptr;
}

std::string ThreadedScraper::formatGameName(FileData* game)
{
	return "["+game->getSystemName()+"] " + game->getName();
}

ScraperThread::ScraperThread(int threadId)
{
	mThreadId = threadId;
	mErrorStatus = 0;
	mStatus = ASYNC_IN_PROGRESS;
}

void ScraperThread::run(const ScraperSearchParams& params)
{
	mResult = ScraperSearchResult();
	mErrorStatus = 0;
	mStatusString = "";
	mStatus = ASYNC_IN_PROGRESS;
	mSearch = params;
	mMDResolveHandle.reset();

	mSearchHandle = Scraper::getScraper()->search(params);
}

int ScraperThread::updateState()
{
	if (mSearchHandle && mSearchHandle->status() != ASYNC_IN_PROGRESS)
	{
		auto status = mSearchHandle->status();
		auto results = mSearchHandle->getResults();
		auto statusString = mSearchHandle->getStatusString();
		auto httpCode = mSearchHandle->getErrorCode();

		LOG(LogInfo) << "[Thread " << mThreadId << "] ThreadedScraper::SearchResponse : " << httpCode << " " << statusString;

		mSearchHandle.reset();

		if (status == ASYNC_DONE)
		{
			if (results.size() > 0)
			{
				if (results[0].hasMedia())
					mMDResolveHandle = results[0].resolveMetaDataAssets(mSearch);
				else
					acceptResult(results[0]);
			}
			else
			{
				mStatus = ASYNC_DONE;
				mErrorStatus = 0;
			}
		}
		else if (status == ASYNC_ERROR)
			processError(httpCode, statusString);
	}

	if (mMDResolveHandle && mMDResolveHandle->status() != ASYNC_IN_PROGRESS)
	{
		auto status = mMDResolveHandle->status();
		auto result = mMDResolveHandle->getResult();
		auto statusString = mMDResolveHandle->getStatusString();
		auto httpCode = mMDResolveHandle->getErrorCode();

		LOG(LogInfo) << "[Thread " << mThreadId << "] ResolveResponse : " << statusString;

		mMDResolveHandle.reset();

		if (status == ASYNC_DONE)
			acceptResult(result);
		else if (status == ASYNC_ERROR)
			processError(httpCode, statusString);
	}

	return mStatus;
}

void ThreadedScraper::processError(int status, const std::string statusString)
{
	if (status == HttpReq::REQ_430_TOOMANYSCRAPS || status == HttpReq::REQ_430_TOOMANYFAILURES || 
		status == HttpReq::REQ_426_BLACKLISTED || status == HttpReq::REQ_FILESTREAM_ERROR || status == HttpReq::REQ_426_SERVERMAINTENANCE ||
		status == HttpReq::REQ_403_BADLOGIN || status == HttpReq::REQ_401_FORBIDDEN)
	{
		// The whole run is refused: the page's outcome carries the
		// scraper's words (one surface for one event, es-native-ui.md),
		// where upstream raised a dialog over its card.
		mExitCode = ASYNC_ERROR;
		std::unique_lock<std::mutex> lock(sProgressMutex);
		sProgress.failed = true;
		sProgress.failure = statusString;
	}
	else
	{
		mErrors.push_back(statusString);
		std::unique_lock<std::mutex> lock(sProgressMutex);
		sProgress.errors = (int) mErrors.size();
	}
}

void ThreadedScraper::run()
{
	while (mExitCode == ASYNC_IN_PROGRESS)
	{
		if (mPaused)
		{
			while (mExitCode == ASYNC_IN_PROGRESS && mPaused)
			{
				std::this_thread::yield();
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
		
		for (auto iter = mScraperThreads.cbegin(); iter != mScraperThreads.cend(); ++iter)
		{
			if (mExitCode != ASYNC_IN_PROGRESS)
				break;

			auto mScraperThread = *iter;

			int state = mScraperThread->updateState();
			switch (state)
			{
			case ASYNC_DONE:
				acceptResult(*mScraperThread);
				break;

			case ASYNC_ERROR:
				processError(mScraperThread->getError(), mScraperThread->getErrorString());
				break;

			default:
				//std::this_thread::yield();
				// std::this_thread::sleep_for(std::chrono::milliseconds(10));
				break;
			}

			if (mExitCode == ASYNC_IN_PROGRESS && state != ASYNC_IN_PROGRESS)
			{
				if (!mSearchQueue.empty())
					ProcessNextGame(mScraperThread);
				else
				{					
					mScraperThreads.erase(iter);
					if (mScraperThreads.size() == 0)
					{
						mExitCode = ASYNC_DONE;
						LOG(LogDebug) << "ThreadedScraper::finished";
					}
					break;
				}
			}
		}
	}
	
	{
		// How it ended, for the page: the elapsed time freezes here.
		std::unique_lock<std::mutex> lock(sProgressMutex);
		sProgress.finished = true;
		sProgress.elapsedMs = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sStarted).count();
		LOG(LogInfo) << "ThreadedScraper: finished " << sProgress.done << " of " << sProgress.total
			<< " (errors " << sProgress.errors << (sProgress.cancelled ? ", cancelled" : "") << (sProgress.failed ? ", failed" : "") << ")";
	}

	delete this;
	ThreadedScraper::mInstance = nullptr;
}

// Called as each game is handed to a thread: the games finished so far are
// the ones neither queued nor in a thread's hands, and the game named is
// the one just handed out.
void ThreadedScraper::updateUI()
{
	int done = mTotal - (int) mSearchQueue.size() - (int) mScraperThreads.size();
	if (done < 0)
		done = 0;

	std::unique_lock<std::mutex> lock(sProgressMutex);
	sProgress.done = done;
	sProgress.total = mTotal;
	sProgress.game = mCurrentGame;
}

void ThreadedScraper::acceptResult(ScraperThread& thread)
{
	LOG(LogDebug) << "ThreadedScraper::acceptResult >>";

	ScraperSearchResult& result = thread.getResult();
	if (result.mdl.getName().empty())
	{		
		auto scraperName = Scraper::getScraperName(Scraper::getScraper());
		thread.getSearchParams().game->getMetadata().setScrapeDate(scraperName);
		return;
	}

	ScraperSearchParams& search = thread.getSearchParams();
	auto game = search.game;

	mWindow->postToUiThread([game, result]()
	{
		LOG(LogDebug) << "ThreadedScraper::importScrappedMetadata";
		game->importP2k(result.p2k);
		game->getMetadata().importScrappedMetadata(result.mdl);
		game->detectLanguageAndRegion(true);
		game->getMetadata().setScrapeDate(result.scraper);

		LOG(LogDebug) << "ThreadedScraper::saveToGamelistRecovery";
		saveToGamelistRecovery(game);
	});

	LOG(LogDebug) << "ThreadedScraper::acceptResult <<";
}

void ThreadedScraper::start(Window* window, const std::queue<ScraperSearchParams>& searches)
{
	if (ThreadedScraper::mInstance != nullptr)
		return;

	std::string error;
	int threadCount = Scraper::getScraper()->getThreadCount(error);
	if (threadCount < 0)
	{
		window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + std::string(" :\r\n") + error));
		return;
	}

	if (threadCount == 0)
		threadCount = 1;

	ThreadedScraper::mInstance = new ThreadedScraper(window, searches, threadCount);

	try
	{
		ThreadedScraper::mInstance->Process();
	}
	catch (const std::exception& e)
	{
		window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + std::string(" :\r\n") + e.what()));
		delete ThreadedScraper::mInstance;
		ThreadedScraper::mInstance = nullptr;
	}
}

// The player's CANCEL (D-UI-078): the run's loop ends at its next turn and
// what was scraped stays scraped. Marked before the loop is told, so run()
// finds it set however quickly it ends.
void ThreadedScraper::stop()
{
	auto thread = ThreadedScraper::mInstance;
	if (thread == nullptr)
		return;

	{
		std::unique_lock<std::mutex> lock(sProgressMutex);
		sProgress.cancelled = true;
	}
	try
	{
		thread->mExitCode = ASYNC_DONE;
	}
	catch (...) {}
}

