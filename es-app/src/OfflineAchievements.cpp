#include "ProxyCards.h"
#include <csignal>
#include <cstdlib>
#include "OfflineAchievements.h"
#include "RunLock.h"
#include "ApiSystem.h"
#include "CloudText.h"
#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "SystemConf.h"
#include "Window.h"
#include "utils/FileSystemUtil.h"
#include "utils/OfflineProxyUrl.h"
#include "utils/Platform.h"
#include "utils/StringUtil.h"
#include <chrono>
#include <ctime>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace
{
	const char* CTL = "/usr/bin/raofflineproxy-ctl";
	// Where the proxy keeps everything it writes (the unit's
	// RAOFFLINEPROXY_CONFIG_DIR), and the two files the scan reads back.
	const char* SCAN_STAMP = "/storage/.config/raofflineproxy/last-scan";
	// The one line the ctl keeps while a scan or top-up runs its jobs,
	// rewritten as each game finishes and removed when the run ends (fork
	// #189); CloudText::parseRunningProgress reads it.
	const char* RUNNING_FILE = "/storage/.config/raofflineproxy/running";
	const char* READY_FILE = "/storage/.config/raofflineproxy/cached_game_ids.txt";
	// The proxy's own answer to "can RetroAchievements be reached", written
	// by its connectivity monitor (state.py save_online_state).
	const char* ONLINE_STATE = "/storage/.config/raofflineproxy/online_state.json";
	// The proxy's flush stamp (the ctl's FLUSH_STAMP): present until a
	// reader takes it. And the ctl's lock, holding the running pid.
	const char* FLUSH_STAMP = "/storage/.config/raofflineproxy/last-flush";
	const char* SCAN_LOCK = "/var/run/raofflineproxy-scan.lock";

	// How long one question to the proxy may take, in all. It answers from
	// its store on the same device in milliseconds; anything longer is a
	// proxy that is not answering, and the page must not wait on it.
	const long PROXY_CONNECT_MS = 2000L;
	const long PROXY_TOTAL_MS = 15000L;

	// The ctl's last stdout line and its exit status. executeScriptLegacy is
	// the public route that hands back both, as the toggle's page uses it.
	std::pair<std::string, int> ask(const char* verb)
	{
		std::string last;
		auto result = ApiSystem::executeScriptLegacy(std::string(CTL) + " " + verb + " 2>/dev/null",
			[&last](const std::string line) { last = line; });
		return std::make_pair(last, result.second);
	}

	// Whether process `pid` has `path` open (audit of the fix round,
	// G2-E-app-05 gpt / G2-E-app-06 claude). RunLock::holder names the pid
	// in the lock file when the lock is held and that pid's command line
	// mentions the ctl -- a mention, not an identity. The ctl writes its pid
	// just after it takes the lock, so for that moment, and after a run
	// killed -9, the file names an earlier run's pid, which may by now be
	// anything whose arguments say raofflineproxy-ctl: a tail of its log, a
	// grep. The run that holds an flock holds the file open, so a pid is
	// signalled only when one of its descriptors is that file (the same
	// device and inode).
	bool holdsOpen(long pid, const std::string& path)
	{
		struct stat want;
		if (::stat(path.c_str(), &want) != 0)
			return false;
		const std::string fds = "/proc/" + std::to_string(pid) + "/fd";
		DIR* dir = ::opendir(fds.c_str());
		if (dir == nullptr)
			return false;
		bool found = false;
		while (struct dirent* entry = ::readdir(dir))
		{
			if (entry->d_name[0] == '.')
				continue;
			struct stat got;
			if (::stat((fds + "/" + entry->d_name).c_str(), &got) == 0 && got.st_dev == want.st_dev && got.st_ino == want.st_ino)
			{
				found = true;
				break;
			}
		}
		::closedir(dir);
		return found;
	}
}

bool OfflineAchievements::available()
{
	// The image's own file: static for the session, so the cache is right.
	return Utils::FileSystem::exists(CTL);
}

int OfflineAchievements::pendingAwards()
{
	if (!available())
		return -1;

	// 0 with the count, 1 with 0; anything else is the ctl saying it could
	// not tell, and prints no number to be mistaken for one.
	const auto answer = ask("pending");
	if (answer.second != 0 && answer.second != 1)
		return -1;
	return CloudText::parsePendingCount(answer.first);
}

bool OfflineAchievements::takeFlushed()
{
	if (!available())
		return false;

	const auto answer = ask("flushed");
	if (answer.second != 0)
		return false;
	return CloudText::parseFlushStamp(answer.first).ok;
}

bool OfflineAchievements::flushStampPresent()
{
	return available() && Utils::FileSystem::exists(FLUSH_STAMP, false);
}

CloudText::ScanStamp OfflineAchievements::lastScan()
{
	// Uncached: the ctl writes this while EmulationStation runs, and
	// Utils::FileSystem::exists remembers a miss for the session otherwise
	// (es-native-ui.md; the cloud rows read NOT DONE ON THIS DEVICE YET
	// after a run had stamped, 2026-09-10).
	if (!Utils::FileSystem::exists(SCAN_STAMP, false))
		return CloudText::ScanStamp();
	return CloudText::parseScanStamp(Utils::FileSystem::readAllText(SCAN_STAMP));
}

CloudText::RunningProgress OfflineAchievements::runningProgress()
{
	// Uncached, as the stamp is. The ctl writes the file whole (a temporary
	// name, then a rename) and removes it as the run ends, so a read finds
	// either a whole line or nothing -- and nothing, should the file go
	// between the two calls here, parses as no run.
	if (!Utils::FileSystem::exists(RUNNING_FILE, false))
		return CloudText::RunningProgress();
	return CloudText::parseRunningProgress(Utils::FileSystem::readAllText(RUNNING_FILE), (long long) time(nullptr));
}

std::vector<int> OfflineAchievements::readyIds()
{
	// Uncached, like the scan stamp: the client rewrites this file while
	// EmulationStation runs.
	if (!Utils::FileSystem::exists(READY_FILE, false))
		return std::vector<int>();
	// One game id per line (es_export.py). Lines that are not a number are
	// not games: a count is never made of what was not read as one.
	return OfflineAchievementsText::parseReadyIds(Utils::FileSystem::readAllText(READY_FILE));
}

int OfflineAchievements::readyCount()
{
	return (int)readyIds().size();
}

bool OfflineAchievements::toggleOn()
{
	return SystemConf::getInstance()->getBool("global.retroachievements.offlineproxy");
}

bool OfflineAchievements::proxyOffline()
{
	if (!available() || !toggleOn())
		return false;
	// The device's own link first (fork #190). The proxy's monitor rewrites
	// its state only when its next probe fails, some time after the link
	// drops, and a player who has just switched Wi-Fi off opens a page
	// before that; asking the web then is a PLEASE WAIT with no route
	// under it. No address on any wired or wireless interface (the same
	// question the network watcher and the scan row ask; a VPN's tunnel
	// does not count) is offline, whatever the file says.
	if (Utils::Platform::queryIPAddress().empty())
		return true;
	// An address and no state file yet: the toggle has just gone on and the
	// proxy's monitor has not probed. That reads as online, on purpose -- a
	// player with a link who has just enabled the feature expects the page
	// they had a minute ago, and the web request is bounded; reading it as
	// offline would show an empty device copy for a store that has cached
	// nothing yet (audit #258 PL-028 asked which; this is the trade).
	if (!Utils::FileSystem::exists(ONLINE_STATE, false))
		return false;
	bool online = true;
	if (!OfflineAchievementsText::parseOnlineState(Utils::FileSystem::readAllText(ONLINE_STATE), online))
		return false;
	return !online;
}

std::string OfflineAchievements::username()
{
	return SystemConf::getInstance()->get("global.retroachievements.username");
}

bool OfflineAchievements::askProxy(const std::string& query, std::string& body, std::string& error)
{
	body.clear();
	error.clear();

	HttpReqOptions options;
	options.connectTimeout = PROXY_CONNECT_MS;
	options.timeout = PROXY_TOTAL_MS;
	// No cookie jar for a local process, and the interface's own name: the
	// proxy remembers the last client's User-Agent but never uses it for
	// its own requests (utils.py self_user_agent), so this changes nothing
	// about how it speaks to RetroAchievements.
	options.useCookieManager = false;
	// The store and nothing else (fork #190): the fork's proxy answers a
	// request carrying this from its store whatever it believes about the
	// link. Without it, a proxy that still believes it is online -- up to its
	// monitor's next probe after the link drops -- tries upstream first, with a
	// fifteen-second timeout, for every one of these; the pages show the
	// device's copy, which is exactly the store. The same line rides every
	// image WebImageComponent asks of the proxy (fork #199).
	options.customHeaders.push_back(Utils::OfflineProxy::StoreOnlyHeader);

	HttpReq req(OfflineAchievementsText::requestUrl(query), &options);
	if (req.wait())
	{
		body = req.getContent();
		return true;
	}
	// For a 4xx or 503 HttpReq keeps the proxy's body as the message, so a
	// miss reads as the proxy's own words and not as a status code.
	error = req.getErrorMsg();
	return false;
}

std::vector<OfflineAchievementsText::StoreGame> OfflineAchievements::storeSummary(bool& ok)
{
	std::vector<OfflineAchievementsText::StoreGame> games;
	ok = false;
	if (!available())
		return games;
	// One process over the store, one line a game -- in place of two
	// requests a game to the proxy (fork #190). The ctl exits 1 with the
	// toggle off, no account or no store, and says a store it could not
	// read; either way the caller has no summary from the device.
	std::vector<std::string> lines;
	auto result = ApiSystem::executeScriptLegacy(std::string(CTL) + " summary 2>/dev/null",
		[&lines](const std::string line) { lines.push_back(line); });
	if (result.second != 0)
		return games;
	for (const auto& line : lines)
	{
		auto game = OfflineAchievementsText::parseStoreGame(line);
		if (game.ok)
			games.push_back(game);
		else if (!line.empty())
			LOG(LogWarning) << "OfflineAchievements: a line of the store's summary was not a game; skipped";
	}
	ok = true;
	return games;
}

std::vector<OfflineAchievementsText::PendingAward> OfflineAchievements::pendingAwardIds()
{
	if (!available() || !toggleOn())
		return std::vector<OfflineAchievementsText::PendingAward>();

	// Every line, not the last: one award per line. Exit 0 with rows, 1 with
	// none; anything else is the ctl saying it could not tell, and the page
	// then marks nothing as waiting rather than guessing.
	std::string all;
	auto result = ApiSystem::executeScriptLegacy(std::string(CTL) + " pending-ids 2>/dev/null",
		[&all](const std::string line) { all += line + "\n"; });
	if (result.second != 0 && result.second != 1)
		return std::vector<OfflineAchievementsText::PendingAward>();
	return OfflineAchievementsText::parsePendingIds(all);
}

OfflineAchievementsText::AccountTotals OfflineAchievements::accountTotals()
{
	if (!available() || !toggleOn())
		return OfflineAchievementsText::AccountTotals();
	const auto answer = ask("account");
	if (answer.second != 0)
		return OfflineAchievementsText::AccountTotals();
	return OfflineAchievementsText::parseAccountTotals(answer.first);
}

// The table is CloudText's, where it has a case (CloudText::scanWhy).
std::string OfflineAchievements::scanWhy(const std::string& token)
{
	return CloudText::scanWhy(token);
}

void OfflineAchievements::topUpWhenOnline(Window* window)
{
	if (!available())
		return;
	// The toggle is read here as well as in the ctl so a device with the
	// feature off never starts a process for it on every link.
	if (!SystemConf::getInstance()->getBool("global.retroachievements.offlineproxy"))
		return;
	ProxyCards::topUp(window, false);
}

void OfflineAchievements::topUpAfterIndex(Window* window)
{
	if (!available() || !toggleOn())
		return;
	ProxyCards::topUp(window, true);
}

int OfflineAchievements::runTopUp(bool afterIndex)
{
	const auto answer = ask(afterIndex ? "topup --after-index" : "topup");
	LOG(LogInfo) << "OfflineAchievements: topup" << (afterIndex ? " --after-index" : "") << " exited " << answer.second;
	return answer.second;
}

int OfflineAchievements::markIndexOffline()
{
	const auto answer = ask("index-offline");
	LOG(LogInfo) << "OfflineAchievements: index-offline exited " << answer.second;
	return answer.second;
}

bool OfflineAchievements::stopRun()
{
	// The pid in the lock file names a run only while the lock is held, and
	// only as the ctl (RunLock): a file a finished or killed run left names a
	// pid that may belong to anything by now. And only a pid that holds the
	// file open is the run (holdsOpen); while the lock is held by a run that
	// has not yet written its pid, the file is asked again for half a
	// second, then nothing is signalled and the launch waits for the run to
	// end on its own (FileData's launchWhenGone).
	for (int attempt = 0; ; attempt++)
	{
		const long pid = RunLock::holder(SCAN_LOCK, "raofflineproxy-ctl");
		if (pid > 1 && holdsOpen(pid, SCAN_LOCK))
			return ::kill((pid_t) pid, SIGTERM) == 0;
		if (!RunLock::held(SCAN_LOCK))
			return false;
		if (attempt >= 10)
		{
			LOG(LogWarning) << "OfflineAchievements: the run lock is held, and the pid it names (" << pid << ") does not hold it; nothing was signalled";
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}
