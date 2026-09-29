#include "RetroAchievements.h"
#include "HttpReq.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "PlatformId.h"
#include "SystemData.h"
#include "utils/StringUtil.h"
#include "utils/ZipFile.h"
#include "ApiSystem.h"
#include "Log.h"
#include <algorithm>
#include <rapidjson/rapidjson.h>
#include <rapidjson/pointer.h>
#include <libcheevos/cheevos.h>

#include "LocaleES.h"
#include "EmulationStation.h"
#include "OfflineAchievements.h"
#include "OfflineAchievementsText.h"
#include "FileData.h"
#include "MetaData.h"
#include "guis/GuiRetroAchievements.h"
#include <set>

using namespace PlatformIds;

const std::map<PlatformId, unsigned short> cheevosConsoleID
{
	{ ARCADE, RC_CONSOLE_ARCADE },
	{ NEOGEO, RC_CONSOLE_ARCADE },

	{ SEGA_MEGA_DRIVE, RC_CONSOLE_MEGA_DRIVE },
	{ NINTENDO_64, RC_CONSOLE_NINTENDO_64 },
	{ SUPER_NINTENDO, RC_CONSOLE_SUPER_NINTENDO },
	{ GAME_BOY, RC_CONSOLE_GAMEBOY },
	{ GAME_BOY_ADVANCE, RC_CONSOLE_GAMEBOY_ADVANCE },
	{ GAME_BOY_COLOR, RC_CONSOLE_GAMEBOY_COLOR },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, RC_CONSOLE_NINTENDO },
	{ TURBOGRAFX_16, RC_CONSOLE_PC_ENGINE },
	{ SUPERGRAFX, RC_CONSOLE_PC_ENGINE },
	{ SEGA_CD, RC_CONSOLE_SEGA_CD },
	{ SEGA_32X, RC_CONSOLE_SEGA_32X },
	{ SEGA_MASTER_SYSTEM, RC_CONSOLE_MASTER_SYSTEM },
	{ PLAYSTATION, RC_CONSOLE_PLAYSTATION },
	{ ATARI_LYNX, RC_CONSOLE_ATARI_LYNX },
	{ NEOGEO_POCKET, RC_CONSOLE_NEOGEO_POCKET },
	{ SEGA_GAME_GEAR, RC_CONSOLE_GAME_GEAR },
	{ NINTENDO_GAMECUBE, RC_CONSOLE_GAMECUBE },
	{ ATARI_JAGUAR, RC_CONSOLE_ATARI_JAGUAR },
	{ NINTENDO_DS, RC_CONSOLE_NINTENDO_DS },
	{ NINTENDO_WII, RC_CONSOLE_WII },
	{ NINTENDO_WII_U, RC_CONSOLE_WII_U },
	{ PLAYSTATION_2, RC_CONSOLE_PLAYSTATION_2 },
	{ XBOX, RC_CONSOLE_XBOX },
	{ VIDEOPAC_ODYSSEY2, RC_CONSOLE_MAGNAVOX_ODYSSEY2 },
	{ POKEMINI, RC_CONSOLE_POKEMON_MINI },
	{ ATARI_2600, RC_CONSOLE_ATARI_2600 },
	{ PC, RC_CONSOLE_MS_DOS },
	{ NINTENDO_VIRTUAL_BOY, RC_CONSOLE_VIRTUAL_BOY },
	{ MSX, RC_CONSOLE_MSX },
	{ COMMODORE_64, RC_CONSOLE_COMMODORE_64 },
	{ ZX81, RC_CONSOLE_ZX81 },
	{ ORICATMOS, RC_CONSOLE_ORIC },
	{ SEGA_SG1000, RC_CONSOLE_SG1000 },
	{ AMIGA, RC_CONSOLE_AMIGA },
	{ ATARI_ST, RC_CONSOLE_ATARI_ST },
	{ AMSTRAD_CPC, RC_CONSOLE_AMSTRAD_PC },
	{ CREATONIC_MEGA_DUCK, RC_CONSOLE_MEGADUCK },
	{ APPLE_II, RC_CONSOLE_APPLE_II },
	{ SEGA_SATURN, RC_CONSOLE_SATURN },
	{ SEGA_DREAMCAST, RC_CONSOLE_DREAMCAST },
	{ PLAYSTATION_PORTABLE, RC_CONSOLE_PSP },
	{ THREEDO, RC_CONSOLE_3DO },
	{ COLECOVISION, RC_CONSOLE_COLECOVISION },
	{ INTELLIVISION, RC_CONSOLE_INTELLIVISION },
	{ VECTREX, RC_CONSOLE_VECTREX },
	{ PC_88, RC_CONSOLE_PC8800 },
	{ PC_98, RC_CONSOLE_PC9800 },
	{ PCFX, RC_CONSOLE_PCFX },
	{ ATARI_5200, RC_CONSOLE_ATARI_5200 },
	{ ATARI_7800, RC_CONSOLE_ATARI_7800 },
	{ SHARP_X6800, RC_CONSOLE_X68K },
	{ WONDERSWAN, RC_CONSOLE_WONDERSWAN },
	{ WASM4, RC_CONSOLE_WASM4 },
	{ NEOGEO_CD, RC_CONSOLE_NEO_GEO_CD },
	{ CHANNELF, RC_CONSOLE_FAIRCHILD_CHANNEL_F },
	{ ZX_SPECTRUM, RC_CONSOLE_ZX_SPECTRUM },
	{ NINTENDO_GAME_AND_WATCH, RC_CONSOLE_GAME_AND_WATCH },
	{ NINTENDO_3DS, RC_CONSOLE_NINTENDO_3DS },
	{ VIC20, RC_CONSOLE_VIC20 },
	{ SUPER_CASSETTE_VISION, RC_CONSOLE_SUPER_CASSETTEVISION },
	{ FMTOWNS, RC_CONSOLE_FM_TOWNS },
	{ NOKIA_NGAGE, RC_CONSOLE_NOKIA_NGAGE },
	{ PHILIPS_CDI, RC_CONSOLE_CDI },
	{ WATARA_SUPERVISION, RC_CONSOLE_SUPERVISION },
	{ SHARP_X1, RC_CONSOLE_SHARPX1 },
	{ TIC80, RC_CONSOLE_TIC80 },
	{ THOMSON_TO_MO, RC_CONSOLE_THOMSONTO8 },
	{ ARDUBOY, RC_CONSOLE_ARDUBOY },
	{ SUPER_NINTENDO_MSU1, RC_CONSOLE_SUPER_NINTENDO },
	{ EMERSON_ARCADIA_2001, RC_CONSOLE_ARCADIA_2001 },
	{ ATARI_JAGUAR_CD, RC_CONSOLE_ATARI_JAGUAR_CD },
	{ TURBOGRAFX_CD, RC_CONSOLE_PC_ENGINE_CD },
	{ UZEBOX, RC_CONSOLE_UZEBOX }
};

const std::set<unsigned short> consolesWithmd5hashes 
{
	RC_CONSOLE_APPLE_II,
	RC_CONSOLE_ATARI_2600,	
	RC_CONSOLE_ATARI_JAGUAR,
	RC_CONSOLE_COLECOVISION,
	RC_CONSOLE_GAMEBOY,
	RC_CONSOLE_GAMEBOY_ADVANCE,
	RC_CONSOLE_GAMEBOY_COLOR,
	RC_CONSOLE_GAME_GEAR,
	RC_CONSOLE_INTELLIVISION,
	RC_CONSOLE_MAGNAVOX_ODYSSEY2,
	RC_CONSOLE_MASTER_SYSTEM,
	RC_CONSOLE_MEGA_DRIVE,
	RC_CONSOLE_MSX,
	RC_CONSOLE_NEOGEO_POCKET,
	RC_CONSOLE_ORIC,
	RC_CONSOLE_PC8800,
	RC_CONSOLE_POKEMON_MINI,
	RC_CONSOLE_SEGA_32X,
	RC_CONSOLE_SG1000,
	RC_CONSOLE_VECTREX,
	RC_CONSOLE_VIRTUAL_BOY,
	RC_CONSOLE_WONDERSWAN,
	RC_CONSOLE_SUPERVISION
};

// The web API authenticates with a user and that user's web API key
// ("z=<user>&y=<key>"). Upstream compiles a pair in (CHEEVOS_DEV_LOGIN). A
// fork build compiles none in, so the player's own key -- from the account's
// settings page on retroachievements.org, entered under RETROACHIEVEMENTS
// SETTINGS -- goes with their username (#68). Empty means the API cannot be
// asked, and callers say so instead of letting it answer 401.
std::string RetroAchievements::getApiLogin()
{
#ifdef CHEEVOS_DEV_LOGIN
	return CHEEVOS_DEV_LOGIN;
#else
	const std::string user = SystemConf::getInstance()->get("global.retroachievements.username");
	const std::string key = SystemConf::getInstance()->get("global.retroachievements.key");
	if (user.empty() || key.empty())
		return "";
	return "z=" + HttpReq::urlEncode(user) + "&y=" + HttpReq::urlEncode(key);
#endif
}

std::string RetroAchievements::getMissingLoginMessage()
{
	return _("RETROACHIEVEMENTS NEEDS YOUR WEB API KEY.\nENTER IT UNDER RETROACHIEVEMENTS SETTINGS, NEXT TO YOUR PASSWORD.");
}

// A 401 from the API is the key being wrong; say that in English rather than
// showing the API's JSON.
std::string RetroAchievements::getLoginErrorMessage(HttpReq& req)
{
	if (req.status() == HttpReq::REQ_401_FORBIDDEN || req.status() == HttpReq::REQ_403_BADLOGIN)
		return _("RETROACHIEVEMENTS REJECTED YOUR WEB API KEY.\nCHECK IT AND YOUR USERNAME UNDER RETROACHIEVEMENTS SETTINGS.");

	return req.getErrorMsg();
}

// How long a page waits on the web in all before the device's copy is shown
// instead (fork #190): the connect limit alone (10 s) left a summary on a
// dead link on PLEASE WAIT with no end on the RG SP.
static const long PAGE_REQUEST_MS = 15000L;

// Use empty UserAgent with doRequest.php calls
static HttpReqOptions getHttpOptions()
{
	HttpReqOptions options;

	std::string login = RetroAchievements::getApiLogin();
	if (!login.empty())
	{
		std::string ret = Utils::String::extractString(login, "z=", "&");
		ret =  ret + "/" + Utils::String::replace(RESOURCE_VERSION_STRING, ",", ".");		 
		options.userAgent = ret;
	}

	return options;
}

std::string RetroAchievements::getApiUrl(const std::string& method, const std::string& parameters)
{
	std::string login = getApiLogin();
	if (login.empty())
		return "https://retroachievements.org/API/" + method + ".php?" + parameters;

	return "https://retroachievements.org/API/" + method + ".php?" + login + "&" + parameters;
}

std::string GameInfoAndUserProgress::getImageUrl(const std::string& image)
{
	const std::string& icon = image.empty() ? ImageIcon : image;
	// The proxy hands the icon back as a whole address on itself; the web
	// API hands back a path on the media host.
	if (Utils::String::startsWith(icon, "http://") || Utils::String::startsWith(icon, "https://"))
		return icon;

	return "http://i.retroachievements.org" + icon;
}

bool Achievement::isUnlocked() const
{
	return !DateEarned.empty() || !DateEarnedHardcore.empty() || UnlockedOnDevice;
}

std::string Achievement::getBadgeUrl()
{
	if (isUnlocked())
		return BadgeUrl.empty() ? "http://i.retroachievements.org/Badge/" + BadgeName + ".png" : BadgeUrl;

	return BadgeLockedUrl.empty() ? "http://i.retroachievements.org/Badge/" + BadgeName + "_lock.png" : BadgeLockedUrl;
}


int jsonInt(const rapidjson::Value& val, const std::string& name)
{
	if (!val.HasMember(name.c_str()))
		return 0;

	const rapidjson::Value& value = val[name.c_str()];
	
	if (value.IsInt())
		return value.GetInt();
	
	if (value.IsString())
		return Utils::String::toInteger(value.GetString());

	return 0;
}

std::string jsonString(const rapidjson::Value& val, const std::string& name)
{
	if (!val.HasMember(name.c_str()))
		return "";

	const rapidjson::Value& value = val[name.c_str()];

	if (value.IsInt())
		return std::to_string(value.GetInt());

	if (value.IsString())
		return value.GetString();

	return "";
}

static bool sortAchievements(const Achievement& sys1, const Achievement& sys2)
{
	if (sys1.isUnlocked() != sys2.isUnlocked())
		return sys1.isUnlocked();

	if (sys1.DateEarned.empty() != sys2.DateEarned.empty())
		return !sys1.DateEarned.empty() && sys2.DateEarned.empty();

	if (sys1.DateEarnedHardcore.empty() != sys2.DateEarnedHardcore.empty())
		return !sys1.DateEarnedHardcore.empty() && sys2.DateEarnedHardcore.empty();

	return sys1.DisplayOrder < sys2.DisplayOrder;
}

// The offline proxy's cache, as the page's model (fork #180, D-RA-009). Two
// shapes, because the proxy holds two: patch, keyed by game id, for a game
// the scan cached; achievementsets, keyed by the ROM's hash, for a game
// started once through RetroArch (a launch alone leaves no patch row --
// seen on a guest's store, 2026-09-14). The unlocks come merged with the
// awards still queued, and the queued ones are marked from the ctl's list.
// What the cache cannot say is left unsaid: no unlock dates (the proxy's
// session answer stamps every unlock with now), no hardcore counts (the
// proxy is casual-only), so the page reads those as unknown, not as zero
// dates.
GameInfoAndUserProgress RetroAchievements::getGameInfoFromDevice(int gameId, const std::string& cheevosHash, const std::vector<OfflineAchievementsText::PendingAward>* pending)
{
	GameInfoAndUserProgress ret;
	ret.ID = 0;
	ret.ConsoleID = 0;
	ret.ForumTopicID = 0;
	ret.Flags = 0;
	ret.IsFinal = false;
	ret.NumAchievements = 0;
	ret.NumAwardedToUser = 0;
	ret.NumAwardedToUserHardcore = 0;

	const std::string user = OfflineAchievements::username();
	if (user.empty())
		return ret;

	std::string body, error;
	OfflineAchievementsText::Game game;
	bool notCached = false;

	if (gameId > 0)
	{
		if (OfflineAchievements::askProxy("r=patch&g=" + std::to_string(gameId) + "&u=" + HttpReq::urlEncode(user), body, error))
			game = OfflineAchievementsText::parsePatch(body);
		else if (OfflineAchievementsText::isNotCached(error))
			notCached = true;
		else
		{
			LOG(LogWarning) << "RetroAchievements: the offline proxy did not answer patch for game " << gameId << ": " << error;
			ret.ProxyDidNotAnswer = true;
			return ret;
		}
	}

	if (!game.ok && !cheevosHash.empty())
	{
		if (OfflineAchievements::askProxy("r=achievementsets&m=" + HttpReq::urlEncode(Utils::String::toLower(cheevosHash)) + "&u=" + HttpReq::urlEncode(user), body, error))
			game = OfflineAchievementsText::parseAchievementSets(body);
		else if (OfflineAchievementsText::isNotCached(error))
			notCached = true;
		else
		{
			LOG(LogWarning) << "RetroAchievements: the offline proxy did not answer achievementsets: " << error;
			ret.ProxyDidNotAnswer = true;
			return ret;
		}
	}

	if (!game.ok)
	{
		ret.NotOnDevice = notCached;
		return ret;
	}

	// The unlocks are half the page; without them every badge would read
	// locked, which is a page that lies. No answer, no page: the caller
	// asks the web and says what it says. A 200 whose body is not the
	// unlocks shape is no answer either (audit #186 PL-26): it is not read
	// as "nothing unlocked".
	if (!OfflineAchievements::askProxy("r=unlocks&g=" + std::to_string(game.id) + "&u=" + HttpReq::urlEncode(user), body, error))
	{
		LOG(LogWarning) << "RetroAchievements: the offline proxy did not answer unlocks for game " << game.id << ": " << error;
		ret.ProxyDidNotAnswer = true;
		return ret;
	}
	const OfflineAchievementsText::Unlocks unlocks = OfflineAchievementsText::parseUnlocks(body);
	if (!unlocks.ok)
	{
		LOG(LogWarning) << "RetroAchievements: the offline proxy's unlocks for game " << game.id << " were not the shape expected; no page from the device";
		return ret;
	}
	std::set<int> unlocked;
	for (int id : unlocks.ids)
		unlocked.insert(id);

	std::set<int> queued;
	if (pending != nullptr)
	{
		for (const auto& award : *pending)
			queued.insert(award.id);
	}
	else
	{
		for (const auto& award : OfflineAchievements::pendingAwardIds())
			queued.insert(award.id);
	}

	ret.ID = game.id;
	ret.Title = game.title;
	ret.ImageIcon = game.imageUrl;
	ret.FromDevice = true;
	ret.NumAchievements = (int)game.achievements.size();

	int order = 0;
	for (const auto& a : game.achievements)
	{
		Achievement item;
		item.ID = std::to_string(a.id);
		item.Title = a.title;
		item.Description = a.description;
		item.Points = std::to_string(a.points);
		item.BadgeName = a.badgeName;
		item.BadgeUrl = a.badgeUrl.empty() ? OfflineAchievementsText::badgeUrl(a.badgeName, true) : a.badgeUrl;
		item.BadgeLockedUrl = a.badgeLockedUrl.empty() ? OfflineAchievementsText::badgeUrl(a.badgeName, false) : a.badgeLockedUrl;
		item.DisplayOrder = order++;
		item.UnlockedOnDevice = unlocked.count(a.id) > 0;
		item.Pending = item.UnlockedOnDevice && queued.count(a.id) > 0;
		if (item.UnlockedOnDevice)
			ret.NumAwardedToUser++;
		ret.Achievements.push_back(item);
	}

	std::sort(ret.Achievements.begin(), ret.Achievements.end(), sortAchievements);
	return ret;
}

// The RETROACHIEVEMENTS page from the device: the games the proxy holds
// (the client's export of cached ids, each looked up as the game page is),
// and the account's points as its cached sign-in last said them. No rank
// and no recently-played order -- the proxy has neither -- so the games
// come sorted by name, and a game the proxy holds but this device's game
// list does not know is still listed, as the web page lists games not on
// the device.
UserSummary RetroAchievements::getUserSummaryFromDevice()
{
	UserSummary ret;
	ret.RecentlyPlayedCount = 0;

	const std::string user = OfflineAchievements::username();
	if (user.empty())
		return ret;

	// One read of the store for the whole page (fork #190): raofflineproxy-ctl
	// summary prints a line a game -- title, icon, the set's size and points,
	// what is unlocked and what is still waiting. Before, the page asked the
	// proxy two requests a game (audit #186 PL-09's walk); on the RG SP with
	// 247 saved games and Wi-Fi just switched off, each of those first tried
	// upstream while the proxy still believed it was online -- hours of
	// PLEASE WAIT. A ctl that cannot answer leaves ret as it stands: no name,
	// no games -- the caller's "no summary" -- and the caller asks the web,
	// whose own request is bounded.
	bool ok = false;
	const auto stored = OfflineAchievements::storeSummary(ok);
	if (!ok)
	{
		LOG(LogWarning) << "RetroAchievements: the store gave no summary (raofflineproxy-ctl summary did not answer); no summary from the device";
		return ret;
	}

	// No console name here: this runs on GuiLoading's worker, and the name
	// is found through the game lists' FileData, which a folder rescan on
	// the interface thread may be replacing at that moment (#308 1-raoffline
	// claude F-RA-18; es-code-traps.md, a rescan that deletes FileData). The
	// page's constructor, on the interface thread, looks each game up anyway
	// for its hash, and fills the name from the same lookup.
	std::vector<std::pair<std::string, RecentGame>> games;
	for (const auto& game : stored)
	{
		RecentGame recent;
		recent.GameID = std::to_string(game.id);
		recent.Title = game.title;
		recent.ImageIcon = game.icon;

		Award award;
		award.NumPossibleAchievements = game.achievements;
		award.PossibleScore = game.points;
		award.NumAchieved = game.unlocked;
		award.NumAchievedHardcore = 0;
		award.ScoreAchieved = game.unlockedPoints;
		award.ScoreAchievedHardcore = 0;
		award.AchievedUnknown = !game.unlockedKnown;
		ret.Awarded[recent.GameID] = award;
		games.push_back(std::make_pair(Utils::String::toUpper(game.title), recent));
	}

	// An empty list with the ctl answering is a device with nothing cached
	// yet, and its empty list is the truth: the ctl lists what the store
	// holds, so there is no export for it to disagree with (audit #186 PL-26).
	std::sort(games.begin(), games.end(), [](const std::pair<std::string, RecentGame>& a, const std::pair<std::string, RecentGame>& b) { return a.first < b.first; });
	for (const auto& game : games)
		ret.RecentlyPlayed.push_back(game.second);

	const auto totals = OfflineAchievements::accountTotals();
	ret.Username = user;
	ret.FromDevice = true;
	ret.RecentlyPlayedCount = (int)ret.RecentlyPlayed.size();
	if (totals.ok)
	{
		ret.TotalPoints = std::to_string(totals.score);
		ret.TotalSoftcorePoints = std::to_string(totals.softcore);
	}
	return ret;
}

// A web answer that is the network's fault rather than the account's: the
// cases where the device's copy is worth asking for instead (fork #180).
static bool networkFailure(HttpReq& req)
{
	return req.status() != HttpReq::REQ_401_FORBIDDEN && req.status() != HttpReq::REQ_403_BADLOGIN;
}

GameInfoAndUserProgress RetroAchievements::getGameInfoAndUserProgress(int gameId, const std::string& userName, const std::string& cheevosHash)
{
	auto usrName = userName;
	if (usrName.empty())
		usrName = SystemConf::getInstance()->get("global.retroachievements.username");

	GameInfoAndUserProgress ret;
	ret.ID = 0;

	// Offline with OFFLINE ACHIEVEMENTS on, the proxy's cache is the source
	// (D-RA-009): what it holds is shown, what it never cached is said, and
	// only a proxy that does not answer sends this on to the web, which
	// will say in its own words that there is no connection. Online, the
	// web stays the source -- it has the unlock dates, the hardcore counts
	// and the rank the cache does not -- with the device's copy as the
	// fallback when the web could not be reached.
	const bool offline = OfflineAchievements::proxyOffline();
	if (offline)
	{
		auto device = getGameInfoFromDevice(gameId, cheevosHash);
		if (device.ID != 0 || device.NotOnDevice)
			return device;
		// The proxy did not answer, and there is no link to ask the web on:
		// the web's answer would be its timeout, ten seconds later, in
		// libcurl's words (fork #242: "Timeout was reached" on the RG35XX
		// SP). Say what happened instead, in the player's.
		LOG(LogWarning) << "RetroAchievements: offline, and the proxy did not answer for game " << gameId << "; not asking the web";
		ret.Title = _("THE OFFLINE ACHIEVEMENTS SERVICE DIDN'T ANSWER. TRY AGAIN IN A MOMENT.");
		return ret;
	}

	if (getApiLogin().empty())
	{
		ret.Title = getMissingLoginMessage();
		return ret;
	}

	auto options = getHttpOptions();
	// A page's request ends: the device's copy follows a web that did not
	// answer in time (fork #190; the hash library keeps its unbounded fetch).
	options.timeout = PAGE_REQUEST_MS;
	HttpReq httpreq(getApiUrl("API_GetGameInfoAndUserProgress", "u=" + HttpReq::urlEncode(usrName) + "&g=" + std::to_string(gameId)), &options);
	if (httpreq.wait())
	{
		rapidjson::Document doc;
		doc.Parse(httpreq.getContent().c_str());
		if (doc.HasParseError())
			return ret;

		ret.ID = jsonInt(doc, "ID");
		ret.Title = jsonString(doc, "Title");
		ret.ConsoleID = jsonInt(doc, "ConsoleID");
		ret.ForumTopicID = jsonInt(doc, "ForumTopicID");
		ret.Flags = jsonInt(doc, "Flags");
		ret.ImageIcon = jsonString(doc, "ImageIcon");
		ret.ImageTitle = jsonString(doc, "ImageTitle");
		ret.ImageIngame = jsonString(doc, "ImageIngame");
		ret.ImageBoxArt = jsonString(doc, "ImageBoxArt");
		ret.Publisher = jsonString(doc, "Publisher");
		ret.Developer = jsonString(doc, "Developer");
		ret.Genre = jsonString(doc, "Genre");
		ret.Released = jsonString(doc, "Released");
		ret.ConsoleName = jsonString(doc, "ConsoleName");
		ret.NumDistinctPlayersCasual = jsonString(doc, "NumDistinctPlayersCasual");
		ret.NumDistinctPlayersHardcore = jsonString(doc, "NumDistinctPlayersHardcore");
		ret.NumAchievements = jsonInt(doc, "NumAchievements");
		ret.NumAwardedToUser = jsonInt(doc, "NumAwardedToUser");
		ret.NumAwardedToUserHardcore = jsonInt(doc, "NumAwardedToUserHardcore");
		ret.UserCompletion = jsonString(doc, "UserCompletion");
		ret.UserCompletionHardcore = jsonString(doc, "UserCompletionHardcore");

		if (doc.HasMember("Achievements"))
		{
			const rapidjson::Value& ra = doc["Achievements"];
			for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
			{
				auto& recent = achivId->value;

				Achievement item;
				item.ID = jsonString(recent, "ID");
				item.NumAwarded = jsonString(recent, "NumAwarded");
				item.NumAwardedHardcore = jsonString(recent, "NumAwardedHardcore");
				item.Title = jsonString(recent, "Title");
				item.Description = jsonString(recent, "Description");
				item.Points = jsonString(recent, "Points");
				item.TrueRatio = jsonString(recent, "TrueRatio");
				item.Author = jsonString(recent, "Author");
				item.DateModified = jsonString(recent, "DateModified");
				item.DateCreated = jsonString(recent, "DateCreated");
				item.BadgeName = jsonString(recent, "BadgeName");
				item.DisplayOrder = jsonInt(recent, "DisplayOrder");
				item.DateEarned = jsonString(recent, "DateEarned");
				item.DateEarnedHardcore = jsonString(recent, "DateEarnedHardcore");
				
				ret.Achievements.push_back(item);
			}
		}

		std::sort(ret.Achievements.begin(), ret.Achievements.end(), sortAchievements);
	}
	else
	{
		if (!offline && OfflineAchievements::toggleOn() && networkFailure(httpreq))
		{
			auto device = getGameInfoFromDevice(gameId, cheevosHash);
			if (device.ID != 0 || device.NotOnDevice)
				return device;
		}
		ret.Title = getLoginErrorMessage(httpreq);
	}

	return ret;
}

UserSummary RetroAchievements::getUserSummary(const std::string& userName, int gameCount)
{
	auto usrName = userName;
	if (usrName.empty())
		usrName = SystemConf::getInstance()->get("global.retroachievements.username");

	UserSummary ret;

	// The same switch as the game page (fork #180): offline, the device's
	// copy; online, the web, with the device's copy when it cannot be
	// reached.
	const bool offline = OfflineAchievements::proxyOffline();
	if (offline)
	{
		auto device = getUserSummaryFromDevice();
		if (!device.Username.empty())
			return device;
		// No summary from the device and no link to ask the web on. It used
		// to fall through to the web request here -- a bounded PLEASE WAIT
		// with no route under it (audit #258 PL-028); the game page has said
		// the sentence instead since #242, and so does this. Whatever the web
		// key: it asked for one here, which is the online page's
		// precondition and no use on this path (#308 1-raoffline claude
		// F-RA-15; the game page's offline branch never asked).
		LOG(LogWarning) << "RetroAchievements: offline, and the proxy gave no summary; saying so instead of asking the web";
		ret.Username = usrName;
		ret.Status = _("THE OFFLINE ACHIEVEMENTS SERVICE DIDN'T ANSWER. TRY AGAIN IN A MOMENT.");
		return ret;
	}

	if (getApiLogin().empty())
	{
		ret.Status = getMissingLoginMessage();
		return ret;
	}

	std::string count = std::to_string(gameCount);

	auto options = getHttpOptions();
	options.timeout = PAGE_REQUEST_MS;   // fork #190, as the game page
	HttpReq httpreq(getApiUrl("API_GetUserSummary", "u="+ HttpReq::urlEncode(usrName) +"&g="+ count +"&a="+ count), &options);
	if (httpreq.wait())
	{
		rapidjson::Document doc;
		doc.Parse(httpreq.getContent().c_str());
		if (doc.HasParseError())
		{
			ret.Status = _("INVALID CONTENT");
			return ret;
		}

		ret.Username = usrName;
		ret.RecentlyPlayedCount = jsonInt(doc, "RecentlyPlayedCount");
		ret.MemberSince = jsonString(doc, "MemberSince");
		ret.RichPresenceMsg = jsonString(doc, "RichPresenceMsg");
		ret.LastGameID = jsonString(doc, "LastGameID");
		ret.ContribCount = jsonString(doc, "ContribCount");
		ret.ContribYield = jsonString(doc, "ContribYield");
		ret.TotalTruePoints = jsonString(doc, "TotalTruePoints"); // 
		ret.TotalSoftcorePoints = jsonString(doc, "TotalSoftcorePoints"); // 		
		ret.TotalPoints = jsonString(doc, "TotalPoints");
		ret.Permissions = jsonString(doc, "Permissions");
		ret.Untracked = jsonString(doc, "Untracked");
		ret.ID = jsonString(doc, "ID");
		ret.UserWallActive = jsonString(doc, "UserWallActive");
		ret.Motto = jsonString(doc, "Motto");
		ret.Rank = jsonString(doc, "Rank");
		ret.TotalRanked = jsonString(doc, "TotalRanked");
		ret.Points = jsonString(doc, "Points");
		ret.UserPic = jsonString(doc, "UserPic");
		ret.Status = jsonString(doc, "Status");

		if (doc.HasMember("RecentlyPlayed"))
		{
			for (auto& recent : doc["RecentlyPlayed"].GetArray())
			{
				RecentGame item;
				item.GameID = jsonString(recent, "GameID");
				item.ConsoleID = jsonString(recent, "ConsoleID");
				item.ConsoleName = jsonString(recent, "ConsoleName");
				item.Title = jsonString(recent, "Title");
				item.ImageIcon = jsonString(recent, "ImageIcon");
				item.LastPlayed = jsonString(recent, "LastPlayed");
				item.MyVote = jsonString(recent, "MyVote");

				ret.RecentlyPlayed.push_back(item);
			}
		}

		if (doc.HasMember("Awarded"))
		{
			const rapidjson::Value& ra = doc["Awarded"];
			for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
			{
				std::string gameID = achivId->name.GetString();
				auto& recent = achivId->value;

				Award item;
				item.NumPossibleAchievements = jsonInt(recent, "NumPossibleAchievements");
				item.PossibleScore = jsonInt(recent, "PossibleScore");
				item.NumAchieved = jsonInt(recent, "NumAchieved");
				item.ScoreAchieved = jsonInt(recent, "ScoreAchieved");
				item.NumAchievedHardcore = jsonInt(recent, "NumAchievedHardcore");
				item.ScoreAchievedHardcore = jsonInt(recent, "ScoreAchievedHardcore");

				ret.Awarded[gameID] = item;
			}
		}

		if (doc.HasMember("RecentAchievements"))
		{
			const rapidjson::Value& ra = doc["RecentAchievements"];
			for (auto achivId = ra.MemberBegin(); achivId != ra.MemberEnd(); ++achivId)
			{
				std::string gameID = achivId->name.GetString();

				for (auto itrc = achivId->value.MemberBegin(); itrc != achivId->value.MemberEnd(); ++itrc)
				{
					auto& recent = itrc->value;
					RecentAchievement item;

					item.ID = jsonString(recent, "ID");
					item.GameID = jsonString(recent, "GameID");
					item.GameTitle = jsonString(recent, "GameTitle");
					item.Description = jsonString(recent, "Description");
					item.Points = jsonString(recent, "Points");
					item.BadgeName = jsonString(recent, "BadgeName");
					item.IsAwarded = jsonString(recent, "IsAwarded");
					item.DateAwarded = jsonString(recent, "DateAwarded");
					item.HardcoreAchieved = jsonString(recent, "HardcoreAchieved");
					ret.RecentAchievements[gameID].push_back(item);
				}
			}
		}
	}
	else
	{
		if (!offline && OfflineAchievements::toggleOn() && networkFailure(httpreq))
		{
			auto device = getUserSummaryFromDevice();
			if (!device.Username.empty())
				return device;
		}
		ret.Status = getLoginErrorMessage(httpreq);
	}

	return ret;
}

UserRankAndScore RetroAchievements::getUserRankAndScore(const std::string& userName)
{
	auto usrName = userName;
	if (usrName.empty())
		usrName = SystemConf::getInstance()->get("global.retroachievements.username");

	UserRankAndScore ret;

	if (getApiLogin().empty())
		return ret;

	auto options = getHttpOptions();
	options.timeout = PAGE_REQUEST_MS;   // fork #190

	HttpReq request(getApiUrl("API_GetUserRankAndScore", "u=" + HttpReq::urlEncode(usrName)), &options);
	if (request.wait())
	{
		rapidjson::Document doc;
		doc.Parse(request.getContent().c_str());
		if (doc.HasParseError())
			throw std::domain_error("Error while parsing API GetUserRankAndScore response");

		ret.Score = jsonInt(doc, "Score");
		ret.SoftcoreScore = jsonInt(doc, "SoftcoreScore");
		ret.Rank = jsonString(doc, "Rank");
		ret.TotalRanked = jsonInt(doc, "TotalRanked");
	}
	else
		throw std::domain_error("Error while accessing API GetUserRankAndScore :\n" + request.getErrorMsg());

	return ret;
}

RetroAchievementInfo RetroAchievements::toRetroAchivementInfo(UserSummary& ret)
{
	RetroAchievementInfo info;

	if (ret.Username.empty() && !ret.Status.empty())
	{
		info.error = ret.Status;
		return info;
	}

	info.fromDevice = ret.FromDevice;
	// The proxy caches no picture of the player; a request for one would
	// only fail offline.
	info.userpic = ret.FromDevice ? "" : "https://retroachievements.org" + ret.UserPic;
	info.rank = ret.Rank;

	if (!ret.TotalRanked.empty() && !ret.Rank.empty())
		info.rank = ret.Rank + " / " + ret.TotalRanked;

	info.points = ret.TotalPoints;
	info.totalpoints = ret.TotalTruePoints;

	if (!ret.TotalSoftcorePoints.empty())
		info.softpoints = std::to_string(Utils::String::toInteger(ret.TotalSoftcorePoints));
	else
		info.softpoints = std::to_string(Utils::String::toInteger(info.totalpoints) - Utils::String::toInteger(info.points));

	info.username = ret.Username;
	info.registered = ret.MemberSince;

	for (auto played : ret.RecentlyPlayed)
	{
		RetroAchievementGame rg;
		rg.id = played.GameID;		

		if (Utils::String::startsWith(played.ImageIcon, "http://") || Utils::String::startsWith(played.ImageIcon, "https://"))
			rg.badge = played.ImageIcon;
		else if (!played.ImageIcon.empty())
			rg.badge = "http://i.retroachievements.org" + played.ImageIcon;

		rg.name = played.Title; // +" [" + played.ConsoleName + "]";
		rg.consoleName = played.ConsoleName;
		rg.lastplayed = played.LastPlayed;

		auto aw = ret.Awarded.find(played.GameID);
		if (aw != ret.Awarded.cend())
		{
			// The web page lists what was played and hides what earned
			// nothing; the device page lists what earns offline, and a game
			// with nothing unlocked yet is exactly that.
			if (aw->second.NumAchieved == 0 && aw->second.ScoreAchieved == 0 && !ret.FromDevice)
				continue;

			rg.wonAchievementsSoftcore = aw->second.NumAchieved;
			rg.wonAchievementsHardcore = aw->second.NumAchievedHardcore;
			rg.totalAchievements = aw->second.NumPossibleAchievements;

			rg.achievements = std::to_string(aw->second.NumAchieved) + " of " + std::to_string(aw->second.NumPossibleAchievements);

			rg.scoreSoftcore = aw->second.ScoreAchieved;
			rg.scoreHardcore = aw->second.ScoreAchievedHardcore;
			rg.possibleScore = aw->second.PossibleScore;
			rg.progressUnknown = aw->second.AchievedUnknown;
		}

		info.games.push_back(rg);
	}

	return info;
}

std::map<std::string, std::string> RetroAchievements::getCheevosHashes()
{
	std::map<std::string, std::string> ret;

	try
	{
		std::map<int, std::string> officialGames;

		auto options = getHttpOptions();
		// Long is allowed (the library is megabytes, and #190 keeps its total
		// unbounded); silent is not. With the link gone, a pooled connection
		// to RetroAchievements sends nothing and TCP retries it for a quarter
		// of an hour, and this fetch runs on the interface thread: the game
		// list update with the Wi-Fi down held the screen for twelve minutes
		// on the VM before the link's return failed it (ROCKNIX fork #299).
		// Thirty seconds without a byte ends it, and the hasher then says so.
		options.stallTimeout = 30L;

		HttpReq hashLibrary("https://retroachievements.org/dorequest.php?r=hashlibrary", &options);
		HttpReq officialGamesList("https://retroachievements.org/dorequest.php?r=officialgameslist", &options);

		// Official games
		if (officialGamesList.wait())
		{
			rapidjson::Document ogdoc;
			ogdoc.Parse(officialGamesList.getContent().c_str());
			if (ogdoc.HasParseError())
				return ret;

			if (!ogdoc.HasMember("Response"))
				return ret;

			const rapidjson::Value& response = ogdoc["Response"];
			for (auto it = response.MemberBegin(); it != response.MemberEnd(); ++it)
			{
				int gameId = Utils::String::toInteger(it->name.GetString());

				if (it->value.GetType() == rapidjson::Type::kStringType)
					officialGames[gameId] = it->value.GetString();
				else if (it->value.GetType() == rapidjson::Type::kNumberType)
					officialGames[gameId] = std::to_string(it->value.GetInt());
			}
		}
		else if (officialGamesList.status() != HttpReq::REQ_SUCCESS)
			throw std::domain_error("Error while accessing retroachievements official games list :\n" + officialGamesList.getErrorMsg());

		// Hash library
		if (hashLibrary.wait())
		{
			rapidjson::Document doc;
			doc.Parse(hashLibrary.getContent().c_str());
			if (doc.HasParseError())
				return ret;

			if (!doc.HasMember("MD5List"))
				return ret;

			const rapidjson::Value& mdlist = doc["MD5List"];
			for (auto it = mdlist.MemberBegin(); it != mdlist.MemberEnd(); ++it)
			{
				std::string name = Utils::String::toUpper(it->name.GetString());

				if (!it->value.IsInt())
					continue;

				int gameId = it->value.GetInt();

				if (officialGames.find(gameId) == officialGames.cend())
					continue;

				ret[name] = std::to_string(gameId);
			}
		}
		else if (hashLibrary.status() != HttpReq::REQ_SUCCESS)
			throw std::domain_error("Error while accessing retroachievements hashlibrary :\n" + hashLibrary.getErrorMsg());
	}
	catch (const std::exception& e)
	{
		throw e;
	}
	catch (...)
	{

	}

	return ret;
}

std::string RetroAchievements::getCheevosHashFromFile(int consoleId, const std::string& fileName)
{
	LOG(LogDebug) << "getCheevosHashFromFile : " << fileName;

	try
	{
		char hash[33];
		if (generateHashFromFile(hash, consoleId, fileName.c_str()))
			return hash;
	}
	catch (...)
	{
	}

	LOG(LogWarning) << "cheevos -> Unable to extract hash from file :" << fileName;
	return "00000000000000000000000000000000";	
}

std::string RetroAchievements::getCheevosHash( SystemData* system, const std::string& fileName)
{
	bool fromZipContents = system->shouldExtractHashesFromArchives();

	int consoleId = 0;

	for (auto pid : system->getPlatformIds())
	{
		auto it = cheevosConsoleID.find(pid);
		if (it != cheevosConsoleID.cend())
		{
			consoleId = it->second;
			break;
		}
	}

	if (consoleId == RC_CONSOLE_ARCADE)
		return getCheevosHashFromFile(consoleId, fileName);

	if (consoleId == 0 || consolesWithmd5hashes.find(consoleId) != consolesWithmd5hashes.cend())
		return ApiSystem::getInstance()->getMD5(fileName, fromZipContents);

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(fileName));
	if (ext != ".zip" && ext != ".7z")
		return getCheevosHashFromFile(consoleId, fileName);

	std::string contentFile = fileName;
	std::string ret;
	std::string tmpZipDirectory;

	if (fromZipContents)
	{
		tmpZipDirectory = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), Utils::FileSystem::getStem(fileName));
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory);		

		if (ApiSystem::getInstance()->unzipFile(fileName, tmpZipDirectory))
		{
			auto fileList = Utils::FileSystem::getDirContent(tmpZipDirectory, true);

			std::vector<std::string> res;
			std::copy_if(fileList.cbegin(), fileList.cend(), std::back_inserter(res), [](const std::string file) { return Utils::FileSystem::getExtension(file) != ".txt";  });

			if (res.size() == 1)
				contentFile = *res.cbegin();
		}
	}

	if (consoleId != 0)
		ret = getCheevosHashFromFile(consoleId, contentFile);
	else
		ret = ApiSystem::getInstance()->getMD5(contentFile, false);

	if (!tmpZipDirectory.empty())
		Utils::FileSystem::deleteDirectoryFiles(tmpZipDirectory, true);

	return ret;
}

// refused, when asked for, says whether RetroAchievements itself turned the
// account down -- it answered, and the answer was no, which is a wrong
// username or password -- as against a server that could not be reached or
// answered in a shape this does not read (#175). A caller keeping a switch
// on the player's word needs the difference: a refusal is the account's to
// fix, and anything else is tried again when the network is there.
bool RetroAchievements::testAccount(const std::string& username, const std::string& password, std::string& tokenOrError, bool* refused)
{
	if (refused != nullptr)
		*refused = false;

	if (username.empty() || password.empty())
	{
		// Nothing to sign in with: the account is what is missing, not the network.
		if (refused != nullptr)
			*refused = true;
		tokenOrError = _("A valid account is required. Please register an account on https://retroachievements.org");
		return false;
	}

	std::map<std::string, std::string> ret;

	try
	{
		auto options = getHttpOptions();

		HttpReq request("https://retroachievements.org/dorequest.php?r=login&u=" + HttpReq::urlEncode(username) + "&p=" + HttpReq::urlEncode(password), &options);
		if (!request.wait())
		{						
			tokenOrError = request.getErrorMsg();
			return false;
		}

		rapidjson::Document ogdoc;
		ogdoc.Parse(request.getContent().c_str());
		if (ogdoc.HasParseError() || !ogdoc.HasMember("Success"))
		{
			tokenOrError = "Unable to parse response";
			return false;
		}

		if (ogdoc["Success"].IsTrue())
		{
			if (ogdoc.HasMember("Token"))
				tokenOrError = ogdoc["Token"].GetString();

			return true;
		}

		// The server answered, and the answer is no.
		if (refused != nullptr)
			*refused = true;
		if (ogdoc.HasMember("Error"))
			tokenOrError = ogdoc["Error"].GetString();
	}
	catch (...)
	{
		tokenOrError = "Unknown error";
	}

	return false;
}
