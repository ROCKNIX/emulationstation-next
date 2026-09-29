// The offline proxy's answers, read without a device (#180, D-RA-009).
//
// Runs against es-app/src/OfflineAchievementsText.cpp alone -- no window,
// no socket, no /storage. The bodies below are the ones a guest's proxy
// answered on 2026-09-14 with the link cut (Tobu Tobu Girl Deluxe, game
// 15738), cut down to two achievements and with one unofficial achievement
// and one warning row added by hand, because the filters exist for those.

#include "doctest/doctest.h"

#include "OfflineAchievementsText.h"

#include <string>
#include <vector>

using namespace OfflineAchievementsText;

namespace
{
	// patch, as the proxy served it offline for a game the scan cached.
	// Achievement 100999 (Flags 5, unofficial) and 101000001 (the server's
	// casual-only warning, Flags 3, which the fork's proxy patch already
	// drops but this reader must not depend on) are the additions.
	const char* PATCH = R"({"Success":true,"PatchData":{"ID":15738,"Title":"~Homebrew~ Tobu Tobu Girl Deluxe","ConsoleID":6,
	"ImageIcon":"http://127.0.0.1:8080/Images/031424.png","ImageIconURL":"https://media.retroachievements.org/Images/031424.png",
	"Achievements":[
	 {"ID":100335,"Title":"Up Up and Away!","Description":"Beat the Plains level","MemAddr":"0xH00c0a5=4","Points":5,"Author":"Searo","Modified":1582319626,"Created":1582319626,"BadgeName":"109361","Flags":3,"Type":"progression","Rarity":97.73,"RarityHardcore":67.86,"BadgeURL":"http://127.0.0.1:8080/Badge/109361.png","BadgeLockedURL":"http://127.0.0.1:8080/Badge/109361_lock.png"},
	 {"ID":100999,"Title":"Not Official","Description":"An unofficial one","Points":1,"BadgeName":"000001","Flags":5,"BadgeURL":"http://127.0.0.1:8080/Badge/000001.png","BadgeLockedURL":"http://127.0.0.1:8080/Badge/000001_lock.png"},
	 {"ID":100336,"Title":"Cloudy with a Chance of Aliens","Description":"Beat the Clouds level","Points":5,"Author":"Searo","BadgeName":"109364","Flags":3,"Type":"progression","BadgeURL":"http://127.0.0.1:8080/Badge/109364.png","BadgeLockedURL":"http://127.0.0.1:8080/Badge/109364_lock.png"},
	 {"ID":0,"Title":"No id","Points":1,"Flags":3},
	 {"ID":100359,"Title":"Potato-tan Secret","Description":"Find the hidden song","Points":5,"BadgeName":"109296","Flags":3}
	],"Leaderboards":[{"ID":7495,"Title":"Plains Speedrun"}],"RichPresencePatch":"Lookup:Character\r\n1=Girl","ParentID":15738}})";

	// achievementsets, as the proxy served it offline for a game started
	// once through RetroArch (keyed by the ROM's hash). A second, non-core
	// set is added so the choice of set is exercised.
	const char* SETS = R"({"Success":true,"GameId":15738,"Title":"~Homebrew~ Tobu Tobu Girl Deluxe","ImageIconUrl":"http://127.0.0.1:8080/Images/031424.png",
	"RichPresenceGameId":15738,"RichPresencePatch":"Lookup:Character","ConsoleId":6,"Sets":[
	 {"Title":"Bonus","Type":"bonus","AchievementSetId":9999,"GameId":15738,"Achievements":[{"ID":200001,"Title":"Bonus one","Points":2,"BadgeName":"200001","Flags":3}]},
	 {"Title":null,"Type":"core","AchievementSetId":6292,"GameId":15738,"ImageIconUrl":"http://127.0.0.1:8080/Images/031424.png","Achievements":[
	  {"ID":100335,"MemAddr":"0xH00c0a5=4","Title":"Up Up and Away!","Description":"Beat the Plains level","Points":5,"BadgeName":"109361","Flags":3,"BadgeURL":"http://127.0.0.1:8080/Badge/109361.png","BadgeLockedURL":"http://127.0.0.1:8080/Badge/109361_lock.png"},
	  {"ID":100336,"Title":"Cloudy with a Chance of Aliens","Description":"Beat the Clouds level","Points":5,"BadgeName":"109364","Flags":3,"BadgeURL":"http://127.0.0.1:8080/Badge/109364.png","BadgeLockedURL":"http://127.0.0.1:8080/Badge/109364_lock.png"}
	 ],"Leaderboards":[]}]})";
}

// ------------------------------------------------------------------- patch

TEST_CASE("parsePatch reads the game the scan cached")
{
	const Game g = parsePatch(PATCH);
	REQUIRE(g.ok);
	CHECK(g.id == 15738);
	CHECK(g.title == "~Homebrew~ Tobu Tobu Girl Deluxe");
	CHECK(g.imageUrl == "http://127.0.0.1:8080/Images/031424.png");

	// Core achievements with an id, in the body's order: the unofficial one
	// and the one without an id are not in the set RetroArch shows.
	REQUIRE(g.achievements.size() == 3);
	CHECK(g.achievements[0].id == 100335);
	CHECK(g.achievements[0].title == "Up Up and Away!");
	CHECK(g.achievements[0].description == "Beat the Plains level");
	CHECK(g.achievements[0].points == 5);
	CHECK(g.achievements[0].badgeName == "109361");
	CHECK(g.achievements[0].badgeUrl == "http://127.0.0.1:8080/Badge/109361.png");
	CHECK(g.achievements[0].badgeLockedUrl == "http://127.0.0.1:8080/Badge/109361_lock.png");
	CHECK(g.achievements[1].id == 100336);
	CHECK(g.achievements[2].id == 100359);

	// A body without the badge URLs leaves them empty; the caller builds
	// the proxy's path from the name.
	CHECK(g.achievements[2].badgeUrl == "");
	CHECK(g.achievements[2].badgeName == "109296");
}

TEST_CASE("parsePatch on what is not a cached game")
{
	// The proxy's miss, as HttpReq hands the body back.
	CHECK_FALSE(parsePatch(R"({"Success":false,"Error":"no cached response"})").ok);
	// Junk, nothing, the wrong shape, an id of zero.
	CHECK_FALSE(parsePatch("").ok);
	CHECK_FALSE(parsePatch("not json").ok);
	CHECK_FALSE(parsePatch("[1,2,3]").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true})").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":{"ID":0,"Title":"x"}})").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":"x"})").ok);

	// A 200 with no achievements is not a cached game (audit #186 PL-26):
	// the proxy caches a game for its set, and an empty or missing list
	// must not become a page of "0 of 0". A set the filter empties -- all
	// unofficial -- is still the shape, with nothing to show.
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":{"ID":42,"Title":"Empty","Achievements":[]}})").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":{"ID":42,"Title":"None"}})").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":{"ID":42,"Title":"Odd","Achievements":"x"}})").ok);
	CHECK_FALSE(parsePatch(R"({"Success":true,"PatchData":{"ID":42,"Title":"Odd","Achievements":{"ID":1}}})").ok);
	const Game unofficial = parsePatch(R"({"Success":true,"PatchData":{"ID":42,"Title":"WIP","Achievements":[{"ID":7,"Title":"x","Points":1,"Flags":5}]}})");
	CHECK(unofficial.ok);
	CHECK(unofficial.achievements.empty());
}

// ---------------------------------------------------------- achievementsets

TEST_CASE("parseAchievementSets reads the game RetroArch cached, from its core set")
{
	const Game g = parseAchievementSets(SETS);
	REQUIRE(g.ok);
	CHECK(g.id == 15738);
	CHECK(g.title == "~Homebrew~ Tobu Tobu Girl Deluxe");
	CHECK(g.imageUrl == "http://127.0.0.1:8080/Images/031424.png");
	REQUIRE(g.achievements.size() == 2);
	CHECK(g.achievements[0].id == 100335);
	CHECK(g.achievements[1].id == 100336);
	CHECK(g.achievements[1].badgeLockedUrl == "http://127.0.0.1:8080/Badge/109364_lock.png");
}

TEST_CASE("parseAchievementSets falls back to the first set, and refuses the rest")
{
	const Game first = parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T","Sets":[{"Type":"bonus","Achievements":[{"ID":1,"Title":"a","Points":1}]}]})");
	REQUIRE(first.ok);
	CHECK(first.achievements.size() == 1);

	CHECK_FALSE(parseAchievementSets(R"({"Success":false,"Error":"no cached response"})").ok);
	CHECK_FALSE(parseAchievementSets("").ok);
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":0})").ok);

	// No sets, or a core set with nothing in it: not a cached game (PL-26).
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T"})").ok);
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T","Sets":[]})").ok);
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T","Sets":[{"Type":"core","Achievements":[]}]})").ok);
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T","Sets":[{"Type":"core"}]})").ok);
	CHECK_FALSE(parseAchievementSets(R"({"Success":true,"GameId":7,"Title":"T","Sets":"core"})").ok);

	// The patch shape is not this shape.
	CHECK_FALSE(parseAchievementSets(PATCH).ok);
	CHECK_FALSE(parsePatch(SETS).ok);
}

// ----------------------------------------------------------------- unlocks

TEST_CASE("parseUnlocks reads the merged ids and nothing else")
{
	// As captured: one unlock on the QA account.
	const Unlocks one = parseUnlocks(R"({"Success":true,"GameID":15738,"HardcoreMode":false,"UserUnlocks":[100359]})");
	REQUIRE(one.ok);
	REQUIRE(one.ids.size() == 1);
	CHECK(one.ids[0] == 100359);

	// An unknown game answers 200 with an empty list -- the same as a
	// cached game nobody has unlocked anything in. The shape is right and
	// the list is empty; this says nothing about whether the game is
	// cached, and the caller must not read it so.
	const Unlocks none = parseUnlocks(R"({"Success":true,"UserUnlocks":[]})");
	CHECK(none.ok);
	CHECK(none.ids.empty());

	const Unlocks many = parseUnlocks(R"({"Success":true,"UserUnlocks":[3,"4",0,-1,"x",5]})");
	REQUIRE(many.ok);
	REQUIRE(many.ids.size() == 3);
	CHECK(many.ids[0] == 3);
	CHECK(many.ids[1] == 4);
	CHECK(many.ids[2] == 5);
}

TEST_CASE("parseUnlocks on what is not the unlocks shape: not ok, never an empty list (audit #186 PL-26)")
{
	// Each of these used to read as "nothing unlocked" and lock every badge.
	for (const char* body : {
		"",
		"junk",
		"[]",
		"[100359]",
		R"({"Success":true})",
		R"({"Success":true,"UserUnlocks":"3"})",
		R"({"Success":true,"UserUnlocks":100359})",
		R"({"Success":true,"UserUnlocks":{"a":1}})",
		R"({"Success":true,"UserUnlocks":null})",
		R"({"Success":false,"Error":"offline"})",
		R"({"Success":false,"UserUnlocks":[100359]})",
		R"({"Success":false,"Error":"no cached response"})",
		"Couldn't connect to server" })
	{
		INFO(body);
		const Unlocks u = parseUnlocks(body);
		CHECK_FALSE(u.ok);
		CHECK(u.ids.empty());
	}
}

// ------------------------------------------------------------------- error

TEST_CASE("parseError and isNotCached tell the miss from the rest")
{
	CHECK(parseError(R"({"Success":false,"Error":"no cached response"})") == "no cached response");
	CHECK(isNotCached(R"({"Success":false,"Error":"no cached response"})"));

	// The proxy online with no token to forward, a hardcore request, junk,
	// and curl's own words when nothing listens: none of these is "the
	// game is not cached", and the caller tries the web instead.
	CHECK_FALSE(isNotCached(R"({"Success":false,"Error":"upstream unavailable"})"));
	CHECK(parseError(R"({"Success":false,"Error":"upstream unavailable"})") == "upstream unavailable");
	CHECK_FALSE(isNotCached("Couldn't connect to server"));
	CHECK(parseError("Couldn't connect to server") == "");
	CHECK_FALSE(isNotCached(""));
	CHECK_FALSE(isNotCached(PATCH));
	CHECK(parseError(PATCH) == "");
	CHECK(parseError(R"({"Success":false})") == "");
}

// ----------------------------------------------------------- pending-ids

TEST_CASE("parsePendingIds reads the ctl's lines and drops the rest")
{
	const auto awards = parsePendingIds("100359 1789336962\n100360 1789337000\n");
	REQUIRE(awards.size() == 2);
	CHECK(awards[0].id == 100359);
	CHECK(awards[0].when == 1789336962);
	CHECK(awards[1].id == 100360);

	CHECK(parsePendingIds("").empty());
	CHECK(parsePendingIds("0\n").empty());
	CHECK(parsePendingIds("No pending awards\n").empty());
	CHECK(parsePendingIds("1. Game | Achievement | 2026-09-14 03:14\n").empty());
	CHECK(parsePendingIds("100359\n").empty());
	CHECK(parsePendingIds("100359 abc\n").empty());
	CHECK(parsePendingIds("0 1789336962\n").empty());
	CHECK(parsePendingIds("-5 1789336962\n").empty());

	// A good line among junk still counts; whitespace and CRLF are tolerated.
	const auto mixed = parsePendingIds("junk\r\n  100359 1789336962  \r\nraofflineproxy-ctl: x\n");
	REQUIRE(mixed.size() == 1);
	CHECK(mixed[0].id == 100359);
}

// ----------------------------------------------------------- online state

TEST_CASE("parseOnlineState reads the proxy's file and no more")
{
	bool online = true;
	// As the proxy writes it: indented, a trailing newline.
	CHECK(parseOnlineState("{\n  \"online\": false\n}\n", online));
	CHECK_FALSE(online);
	CHECK(parseOnlineState(R"({"online":true})", online));
	CHECK(online);

	// Unreadable is unknown, and the caller never takes unknown for offline.
	online = false;
	CHECK_FALSE(parseOnlineState("", online));
	CHECK_FALSE(parseOnlineState("{}", online));
	CHECK_FALSE(parseOnlineState(R"({"online":"false"})", online));
	CHECK_FALSE(parseOnlineState(R"({"online":0})", online));
	CHECK_FALSE(parseOnlineState("online: false", online));
	CHECK_FALSE(online);
}

// -------------------------------------------------------------- ready ids

TEST_CASE("parseReadyIds counts the games the client exported")
{
	const auto ids = parseReadyIds("4902\n15738\n31199\n");
	REQUIRE(ids.size() == 3);
	CHECK(ids[0] == 4902);
	CHECK(ids[2] == 31199);

	CHECK(parseReadyIds("").empty());
	CHECK(parseReadyIds("\n\n").empty());
	const auto junk = parseReadyIds("4902\nabc\n0\n-3\n 15738 \n15738x\n");
	REQUIRE(junk.size() == 2);
	CHECK(junk[1] == 15738);
}

// ----------------------------------------------------------------- account

TEST_CASE("parseAccountTotals reads both numbers or none")
{
	const auto t = parseAccountTotals("score=1234 softcore=56\n");
	CHECK(t.ok);
	CHECK(t.score == 1234);
	CHECK(t.softcore == 56);

	const auto zero = parseAccountTotals("score=0 softcore=0");
	CHECK(zero.ok);
	CHECK(zero.score == 0);

	CHECK_FALSE(parseAccountTotals("").ok);
	CHECK_FALSE(parseAccountTotals("score=1234").ok);
	CHECK_FALSE(parseAccountTotals("softcore=56").ok);
	CHECK_FALSE(parseAccountTotals("score=abc softcore=56").ok);
	CHECK_FALSE(parseAccountTotals("score= softcore=56").ok);
	CHECK_FALSE(parseAccountTotals("raofflineproxy-ctl: the store could not be read").ok);
}

// ------------------------------------------------------------------- urls

TEST_CASE("the proxy's addresses are built in one place")
{
	CHECK(requestUrl("r=patch&g=15738&u=qa") == "http://127.0.0.1:8080/dorequest.php?r=patch&g=15738&u=qa");
	CHECK(badgeUrl("109361", true) == "http://127.0.0.1:8080/Badge/109361.png");
	CHECK(badgeUrl("109361", false) == "http://127.0.0.1:8080/Badge/109361_lock.png");
	CHECK(badgeUrl("", true) == "");
}

TEST_CASE("parseStoreGame reads one line of raofflineproxy-ctl summary")
{
	auto g = parseStoreGame(R"({"id":4902,"title":"Bobl","icon":"http://127.0.0.1:8080/Images/000001.png","achievements":12,"points":110,"unlocked":1,"unlockedPoints":5,"pending":1})");
	CHECK(g.ok);
	CHECK(g.id == 4902);
	CHECK(g.title == "Bobl");
	CHECK(g.icon == "http://127.0.0.1:8080/Images/000001.png");
	CHECK(g.achievements == 12);
	CHECK(g.points == 110);
	CHECK(g.unlocked == 1);
	CHECK(g.unlockedPoints == 5);
	CHECK(g.unlockedKnown);
	CHECK(g.pending == 1);

	// The counts the ctl might leave out read as 0; the icon may be empty.
	auto bare = parseStoreGame(R"({"id":7,"title":"T","achievements":3})");
	CHECK(bare.ok);
	CHECK(bare.icon.empty());
	CHECK(bare.points == 0);
	CHECK(bare.unlocked == 0);
	CHECK(bare.pending == 0);
}

// The ctl writes unlocked and unlockedPoints as null when the store's unlock
// row for the game is missing or cannot be read (#307 PL-057). Read as 0,
// that was "nothing earned" on the summary for a damaged cache row -- the
// seat's own complaint. A null is unknown; a 0 is a count.
TEST_CASE("parseStoreGame reads a null unlock count as unknown, not as none")
{
	auto g = parseStoreGame(R"({"id":4902,"title":"Bobl","icon":"","achievements":12,"points":110,"unlocked":null,"unlockedPoints":null,"pending":0})");
	CHECK(g.ok);   // still a game the store holds
	CHECK_FALSE(g.unlockedKnown);
	CHECK(g.achievements == 12);
	CHECK(g.points == 110);

	auto none = parseStoreGame(R"({"id":4902,"title":"Bobl","icon":"","achievements":12,"points":110,"unlocked":0,"unlockedPoints":0,"pending":0})");
	CHECK(none.ok);
	CHECK(none.unlockedKnown);
	CHECK(none.unlocked == 0);
}

TEST_CASE("parseStoreGame refuses what is not a game")
{
	CHECK_FALSE(parseStoreGame("").ok);
	CHECK_FALSE(parseStoreGame("not json").ok);
	CHECK_FALSE(parseStoreGame("[1,2,3]").ok);
	CHECK_FALSE(parseStoreGame(R"({"id":0,"title":"T","achievements":3})").ok);      // no id
	CHECK_FALSE(parseStoreGame(R"({"id":9,"title":"Empty","achievements":0})").ok);  // an empty set is not a cached game (PL-26)
	CHECK_FALSE(parseStoreGame(R"({"title":"no id at all","achievements":3})").ok);
}
