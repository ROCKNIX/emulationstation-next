// The shipped es-app/src/ProxyCards.cpp against doubles (audit #307 stream
// E2): the send card's outcome (PL-054), one top-up watcher at a time
// (PL-056), and the top-up card's outcome for a run the player stopped or
// that failed (#308 1-raoffline F-RA-09 / F-RA-17). The waits in the code
// under test are real -- ten seconds for a flush stamp, five of outcome --
// so each case takes as long as the card would on a device.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "Log.h"
#include "FileData.h"
#include "SystemConf.h"
#include "ThreadedCloudSync.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "utils/FileSystemUtil.h"
#include "OfflineAchievements.h"
#include "ProxyCards.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---- The doubles' state, set by each case ----------------------------------

namespace
{
	struct Stamp { std::string path; int rc; std::string token; std::string why; };

	struct Fake
	{
		std::mutex m;
		std::deque<int> pending;         // pendingAwards answers, in order; the last repeats
		std::atomic<bool> stampPresent{ false };
		std::atomic<int> takeFlushedCalls{ 0 };
		std::atomic<bool> gameRunning{ false };
		std::atomic<unsigned> gamesStarted{ 0 };   // never reset: FileData's only grows
		std::atomic<bool> syncRunning{ false };
		std::vector<std::unique_ptr<AsyncNotificationComponent>> cards;
		std::vector<Stamp> stamps;

		// The ctl: runTopUp's behaviour, and what it leaves behind.
		std::function<int(bool)> ctl;
		std::atomic<bool> ctlActive{ false };
		std::atomic<int> ctlCalls{ 0 };
		std::vector<bool> ctlAfterIndex;
		CloudText::ScanStamp scan;
		std::atomic<bool> stopSent{ false };

		void reset()
		{
			std::lock_guard<std::mutex> g(m);
			pending.clear();
			pending.push_back(0);
			stampPresent = false;
			takeFlushedCalls = 0;
			gameRunning = false;
			syncRunning = false;
			cards.clear();
			stamps.clear();
			ctl = nullptr;
			ctlActive = false;
			ctlCalls = 0;
			ctlAfterIndex.clear();
			scan = CloudText::ScanStamp();
			stopSent = false;
		}
	};
	Fake fake;
	Window window;

	std::vector<AsyncNotificationComponent*> cardsNow()
	{
		std::lock_guard<std::mutex> g(fake.m);
		std::vector<AsyncNotificationComponent*> out;
		for (auto& c : fake.cards)
			out.push_back(c.get());
		return out;
	}

	int openCards()
	{
		int n = 0;
		for (auto* c : cardsNow())
			if (!c->closed)
				n++;
		return n;
	}

	// Waits, bounded, for a condition; the bound fails the case rather than
	// hanging the run.
	bool waitFor(const std::function<bool()>& done, int seconds)
	{
		const auto start = std::chrono::steady_clock::now();
		while (!done())
		{
			if (std::chrono::steady_clock::now() - start > std::chrono::seconds(seconds))
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		return true;
	}

	// A game starting, as FileData::launchGame makes it: counted, then on
	// the screen.
	void gameStarts()
	{
		fake.gamesStarted++;
		fake.gameRunning = true;
	}

	std::string join(const std::vector<std::string>& v)
	{
		std::string out;
		for (auto& s : v)
			out += (out.empty() ? "" : " | ") + s;
		return out;
	}
}

// ---- Double definitions ------------------------------------------------------

namespace FakeLog
{
	std::vector<std::string>& lines() { static std::vector<std::string> l; return l; }
	std::mutex& lock() { static std::mutex m; return m; }
}

FileData* FileData::GetRunningGame() { return fake.gameRunning ? reinterpret_cast<FileData*>(1) : nullptr; }
unsigned FileData::GetGamesStarted() { return fake.gamesStarted; }

SystemConf* SystemConf::getInstance() { static SystemConf c; return &c; }
std::string SystemConf::get(const std::string&) { return ""; }
bool SystemConf::getBool(const std::string&, bool def) { return def; }

void ThreadedCloudSync::start(Window*, const std::string&, const std::string&, const std::string&, Origin) {}
bool ThreadedCloudSync::isRunning() { return fake.syncRunning; }
void ThreadedCloudSync::writeStamp(const std::string& path, int rc, const std::string& token, const std::string& why)
{
	std::lock_guard<std::mutex> g(fake.m);
	fake.stamps.push_back({ path, rc, token, why });
}

void Window::postToUiThread(const std::function<void()>& func, void*) { func(); }
AsyncNotificationComponent* Window::createAsyncNotificationComponent(bool)
{
	std::lock_guard<std::mutex> g(fake.m);
	fake.cards.emplace_back(new AsyncNotificationComponent());
	return fake.cards.back().get();
}

void AsyncNotificationComponent::updateTitle(const std::string t) { std::lock_guard<std::mutex> g(lock); title = t; }
void AsyncNotificationComponent::updateText(const std::string t, const std::string a)
{
	std::lock_guard<std::mutex> g(lock);
	text = { t };
	action = a.empty() ? std::vector<std::string>() : std::vector<std::string>{ a };
}
void AsyncNotificationComponent::updateText(const std::string t, const std::vector<std::string>& a)
{
	std::lock_guard<std::mutex> g(lock);
	text = { t };
	action = a;
}
void AsyncNotificationComponent::updateText(const std::vector<std::string>& t, const std::vector<std::string>& a)
{
	std::lock_guard<std::mutex> g(lock);
	text = t;
	action = a;
}
void AsyncNotificationComponent::updatePercent(int p) { std::lock_guard<std::mutex> g(lock); percent = p; }
void AsyncNotificationComponent::close() { closed = true; }

namespace Utils { namespace FileSystem {
	bool exists(const std::string&, bool) { return false; }
	std::string readAllText(const std::string&) { return ""; }
}}

bool OfflineAchievements::available() { return true; }
bool OfflineAchievements::toggleOn() { return true; }
int OfflineAchievements::pendingAwards()
{
	std::lock_guard<std::mutex> g(fake.m);
	const int n = fake.pending.front();
	if (fake.pending.size() > 1)
		fake.pending.pop_front();
	return n;
}
bool OfflineAchievements::takeFlushed()
{
	fake.takeFlushedCalls++;
	return fake.stampPresent.exchange(false);
}
bool OfflineAchievements::flushStampPresent() { return fake.stampPresent; }
CloudText::ScanStamp OfflineAchievements::lastScan() { std::lock_guard<std::mutex> g(fake.m); return fake.scan; }
CloudText::RunningProgress OfflineAchievements::runningProgress()
{
	CloudText::RunningProgress p;
	p.running = fake.ctlActive;
	p.route = "topup";
	p.index = 1;
	p.total = 2;
	return p;
}
int OfflineAchievements::readyCount() { return 0; }
void OfflineAchievements::topUpWhenOnline(Window* w) { ProxyCards::topUp(w, false); }
void OfflineAchievements::topUpAfterIndex(Window* w) { ProxyCards::topUp(w, true); }
int OfflineAchievements::runTopUp(bool afterIndex)
{
	{
		std::lock_guard<std::mutex> g(fake.m);
		fake.ctlAfterIndex.push_back(afterIndex);
	}
	fake.ctlCalls++;
	// The ctl's own lock: a second run while one holds it exits 75 at once.
	if (fake.ctlActive.exchange(true))
		return 75;
	const int rc = fake.ctl ? fake.ctl(afterIndex) : 0;
	fake.ctlActive = false;
	return rc;
}
int OfflineAchievements::markIndexOffline() { return 0; }
bool OfflineAchievements::stopRun() { fake.stopSent = true; return true; }

// ---- The cases -------------------------------------------------------------

// PL-054: the queue drained and the proxy's flush stamp never came. Nothing
// says the awards reached the account, so the card may say COMPLETED and
// must not say they are on the account.
TEST_CASE("send card: an empty queue without the flush stamp says COMPLETED alone")
{
	fake.reset();
	{
		std::lock_guard<std::mutex> g(fake.m);
		fake.pending = { 2, 0 };   // the probe sees two held; the card finds the queue empty
	}
	ProxyCards::afterSync(&window);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c[0]->closed; }, 40));
	auto* card = cardsNow()[0];
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COMPLETED");
	CHECK(card->action.empty());
	std::lock_guard<std::mutex> g(fake.m);
	REQUIRE(fake.stamps.size() == 1);
	CHECK(fake.stamps[0].rc == 0);
	CHECK(fake.stamps[0].token == "completed");
}

// The same run with the stamp in hand: the sentence stays.
TEST_CASE("send card: the flush stamp earns the account sentence")
{
	fake.reset();
	{
		std::lock_guard<std::mutex> g(fake.m);
		fake.pending = { 2, 0 };
	}
	fake.stampPresent = true;
	ProxyCards::afterSync(&window);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c[0]->closed; }, 40));
	auto* card = cardsNow()[0];
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COMPLETED");
	REQUIRE(!card->action.empty());
	CHECK(card->action.front() == "WHAT YOU EARNED OFFLINE IS NOW ON YOUR ACCOUNT.");
	std::lock_guard<std::mutex> g(fake.m);
	REQUIRE(fake.stamps.size() == 1);
	CHECK(fake.stamps[0].token == "sent");
}


// PL-056: a second request while a top-up runs raises no second watcher
// over the same progress file, and is not lost either: the index's run
// (--after-index) is the one that lists the games the index just found,
// and nothing else would run it.
TEST_CASE("top-up: two requests, one watcher at a time, and the second still runs")
{
	fake.reset();
	std::atomic<bool> release{ false };
	fake.ctl = [&release](bool afterIndex)
	{
		if (!afterIndex)
			while (!release)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(800));
		return 0;
	};
	std::atomic<int> maxOpen{ 0 };
	std::atomic<bool> watching{ true };
	std::thread watcher([&] { while (watching) { maxOpen = std::max(maxOpen.load(), openCards()); std::this_thread::sleep_for(std::chrono::milliseconds(20)); } });

	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { return !cardsNow().empty(); }, 10));   // the first run's card is up
	ProxyCards::topUp(&window, true);                              // the index's request arrives
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	release = true;
	REQUIRE(waitFor([] { return fake.ctlCalls >= 2 && fake.ctlActive == false && openCards() == 0 && ProxyCards::topUpRunning() == false; }, 60));
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // any late card has come and gone
	watching = false;
	watcher.join();

	CHECK(maxOpen.load() == 1);
	std::lock_guard<std::mutex> g(fake.m);
	INFO("ctl runs: " << fake.ctlAfterIndex.size());
	REQUIRE(fake.ctlAfterIndex.size() == 2);
	CHECK(fake.ctlAfterIndex[0] == false);
	CHECK(fake.ctlAfterIndex[1] == true);
	for (auto& c : fake.cards)
		CHECK(c->text.front().find("COULDN'T FINISH") == std::string::npos);
}

// #308 1-raoffline F-RA-09 (claude) / F-RA-17 (gpt): STOP IT AND PLAY sends
// the ctl TERM, it exits 143 and writes no stamp. The card that follows the
// game says what happened -- the sync card's launch-cancel words -- never a
// failure, nor the last run's why.
TEST_CASE("top-up: a run stopped for a game says SKIPPED - YOU STARTED A GAME")
{
	fake.reset();
	{
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan.ran = true;
		fake.scan.when = time(nullptr) - 3600;   // the last run's stamp, an hour old
		fake.scan.code = 1;
		fake.scan.why = "SOME_GAMES_NOT_SAVED";
	}
	std::atomic<bool> stopped{ false };
	fake.ctl = [&stopped](bool)
	{
		while (!fake.stopSent)
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		stopped = true;
		return 143;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { return !cardsNow().empty(); }, 10));
	gameStarts();                                // the player launches over it
	CHECK(ProxyCards::stopTopUp());
	REQUIRE(waitFor([&stopped] { return stopped.load(); }, 10));
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	fake.gameRunning = false;                    // a short game ends
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	auto* card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "SKIPPED - YOU STARTED A GAME");
}

// Audit of the fixes, E2 claude G-E2-02 / gpt G-E2-04: STOP IT AND PLAY
// over a top-up with a second request queued. The launch waits only for
// the ctl to be gone (topUpRunning), and then starts its game; the watcher
// waited for a game that was already running, found none yet -- the launch
// was still on its way -- and started the queued run under it. The queued
// run waits for the game the stop was for, to start and to end.
TEST_CASE("top-up: a stop for a game holds the queued run until that game has come and gone")
{
	fake.reset();
	fake.ctl = [](bool afterIndex)
	{
		if (!afterIndex)
			while (!fake.stopSent)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		return afterIndex ? 0 : 143;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { return !cardsNow().empty(); }, 10));
	ProxyCards::topUp(&window, true);            // the index's request, queued
	CHECK(ProxyCards::stopTopUp());               // STOP IT AND PLAY; the game has not started yet
	REQUIRE(waitFor([] { return fake.ctlActive == false; }, 10));

	// The stopped run's card has its five seconds; the launch is on its way.
	std::this_thread::sleep_for(std::chrono::milliseconds(7500));
	CHECK_MESSAGE(fake.ctlCalls.load() == 1, "the queued run started before the game the stop was for");

	gameStarts();                                 // the game starts
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK(fake.ctlCalls.load() == 1);             // and nothing runs under it
	fake.gameRunning = false;                     // it ends
	REQUIRE(waitFor([] { return fake.ctlCalls.load() >= 2; }, 15));
	{
		std::lock_guard<std::mutex> g(fake.m);
		REQUIRE(fake.ctlAfterIndex.size() == 2);
		CHECK(fake.ctlAfterIndex[1] == true);
	}
	REQUIRE(waitFor([] { return openCards() == 0 && fake.ctlActive == false; }, 30));
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // the watcher lets go
}

// Audit of the fix round, G2-E-app-05 (claude) / G2-E-app-02 (gpt): the
// hold a stop sets was honoured only by the same watcher's next run. With
// nothing queued behind the stop the watcher let go as the card ended and
// cleared the hold on its way out, and a request that came after -- the
// index ending while the launch was on its way -- started a fresh watcher
// whose first run skipped the wait and went under the game the player had
// been told it would wait for. The hold outlives the watcher's card, and
// every run waits for a game that has the screen.
TEST_CASE("top-up: a request after the stopped run's card still waits for the game")
{
	fake.reset();
	fake.ctl = [](bool afterIndex)
	{
		if (!afterIndex)
			while (!fake.stopSent)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		return afterIndex ? 0 : 143;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { return !cardsNow().empty(); }, 10));
	CHECK(ProxyCards::stopTopUp());               // STOP IT AND PLAY, nothing queued
	REQUIRE(waitFor([] { return fake.ctlActive == false; }, 10));

	// The stopped run's card has its five seconds and ends; the launch is
	// still on its way when the index asks for its run.
	std::this_thread::sleep_for(std::chrono::milliseconds(7500));
	ProxyCards::topUp(&window, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK_MESSAGE(fake.ctlCalls.load() == 1, "a request after the stopped run's card started before the game the stop was for");

	gameStarts();                                 // the game starts
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK(fake.ctlCalls.load() == 1);             // and nothing runs under it
	fake.gameRunning = false;                     // it ends
	REQUIRE(waitFor([] { return fake.ctlCalls.load() >= 2; }, 15));
	{
		std::lock_guard<std::mutex> g(fake.m);
		REQUIRE(fake.ctlAfterIndex.size() == 2);
		CHECK(fake.ctlAfterIndex[1] == true);
	}
	REQUIRE(waitFor([] { return openCards() == 0 && fake.ctlActive == false; }, 30));
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // the watcher lets go
}

// A request made while a game has the screen, with no stop behind it (the
// index ending under a game): the watcher's first run waits for the game
// as every later run does. It used to go at once.
TEST_CASE("top-up: a first request while a game runs waits for the game to end")
{
	fake.reset();
	fake.ctl = [](bool) { std::this_thread::sleep_for(std::chrono::milliseconds(300)); return 0; };
	gameStarts();
	ProxyCards::topUp(&window, true);
	std::this_thread::sleep_for(std::chrono::milliseconds(2500));
	CHECK_MESSAGE(fake.ctlCalls.load() == 0, "the watcher's first run started under a running game");
	fake.gameRunning = false;
	REQUIRE(waitFor([] { return fake.ctlCalls.load() >= 1; }, 15));
	REQUIRE(waitFor([] { return openCards() == 0 && fake.ctlActive == false; }, 30));
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // the watcher lets go
}

// The game a stop was for can start and end while the stopped run's card
// still shows its outcome (a game quit at once). The hold is spent by that
// game; watching for a running game alone would wait out the whole minute
// for one already gone, and hold every request behind it.
TEST_CASE("top-up: a game that came and went during the stopped run's card spends the hold")
{
	fake.reset();
	fake.ctl = [](bool afterIndex)
	{
		if (!afterIndex)
			while (!fake.stopSent)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		return afterIndex ? 0 : 143;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { return !cardsNow().empty(); }, 10));
	CHECK(ProxyCards::stopTopUp());               // STOP IT AND PLAY, nothing queued
	REQUIRE(waitFor([] { return fake.ctlActive == false; }, 10));
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	gameStarts();                                 // the game starts
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	fake.gameRunning = false;                     // and is quit at once
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // the card ends
	ProxyCards::topUp(&window, true);
	CHECK_MESSAGE(waitFor([] { return fake.ctlCalls.load() >= 2; }, 10), "the hold outlived the game it was for");
	REQUIRE(waitFor([] { return openCards() == 0 && fake.ctlActive == false; }, 70));
	std::this_thread::sleep_for(std::chrono::milliseconds(6000));   // the watcher lets go
}

// A failed run's why is this run's stamp's, or none: an old stamp's token
// describes another run.
TEST_CASE("top-up: a failure reads its why only from this run's stamp")
{
	fake.reset();
	{
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan.ran = true;
		fake.scan.when = time(nullptr) - 3600;
		fake.scan.code = 1;
		fake.scan.why = "RETROACHIEVEMENTS_STOPPED_ANSWERING";
	}
	fake.ctl = [](bool) { std::this_thread::sleep_for(std::chrono::milliseconds(1200)); return 1; };
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	auto* card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COULDN'T FINISH - SOMETHING WENT WRONG");
}

// Audit of the fix round PL-031 (the interface's half of stream D's
// ctl): added=unknown is a count nobody measured, where the ctl used to
// write the cache count. The card said "5 MORE GAMES ARE READY." from the
// cached= beside it; it says how many are ready, which is known, or
// COMPLETED alone when that is not either -- never a count of new games.
TEST_CASE("top-up: added=unknown says how many games are ready, never a count of new ones")
{
	fake.reset();
	fake.ctl = [](bool)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan = CloudText::parseScanStamp(std::to_string((long long) time(nullptr))
			+ " 0 topup cached=5 skipped=0 ready=14 limit=0 indexed=0 errors=0 added=unknown");
		return 0;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	auto* card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COMPLETED");
	REQUIRE(!card->action.empty());
	CHECK(card->action.front() == "14 GAMES ARE READY.");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));   // the watcher lets go

	// With no ready total either, the card says COMPLETED and claims
	// nothing -- not EVERYTHING'S UP TO DATE, which a run that cached five
	// games and could not count them cannot know.
	fake.reset();
	fake.ctl = [](bool)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan = CloudText::parseScanStamp(std::to_string((long long) time(nullptr))
			+ " 0 topup cached=5 skipped=0 ready=0 limit=0 indexed=0 errors=0 added=unknown");
		return 0;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COMPLETED");
	CHECK(card->action.empty());

	// A stamp with a number keeps it (an older ctl's, or one that measured).
	fake.reset();
	fake.ctl = [](bool)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan = CloudText::parseScanStamp(std::to_string((long long) time(nullptr))
			+ " 0 topup cached=5 skipped=0 ready=14 limit=0 added=2");
		return 0;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	REQUIRE(!card->action.empty());
	CHECK(card->action.front() == "2 MORE GAMES ARE READY.");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

// A top-up's own failure carries no instruction to run a scan: the card's
// action line already says it will try again.
TEST_CASE("top-up: SOME_GAMES_NOT_SAVED says so without sending the player to a scan")
{
	fake.reset();
	fake.ctl = [](bool)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));
		std::lock_guard<std::mutex> g(fake.m);
		fake.scan.ran = true;
		fake.scan.when = time(nullptr);
		fake.scan.code = 1;
		fake.scan.topup = true;
		fake.scan.errors = 1;
		fake.scan.why = "SOME_GAMES_NOT_SAVED";
		return 1;
	};
	ProxyCards::topUp(&window, false);
	REQUIRE(waitFor([] { auto c = cardsNow(); return !c.empty() && c.back()->closed; }, 30));
	auto* card = cardsNow().back();
	INFO("text: " << join(card->text) << "  action: " << join(card->action));
	CHECK(card->text.front() == "COULDN'T FINISH - SOME GAMES COULDN'T BE SAVED");
	REQUIRE(!card->action.empty());
	CHECK(card->action.front() == "IT'LL TRY AGAIN NEXT TIME YOU'RE CONNECTED.");
}
