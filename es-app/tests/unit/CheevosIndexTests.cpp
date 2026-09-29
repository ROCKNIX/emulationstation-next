// What the RetroAchievements index owes a game (audit #186 PL-08).
//
// Runs against es-app/src/CheevosIndex.cpp alone. The case that made the
// rule: RC-5 wrote recovery files with a cheevosHash and no cheevosId, and
// the startup index took a game only when its hash was empty, so those games
// never gained an id until a forced INDEX GAMES read every ROM again.

#include "doctest/doctest.h"

#include "CheevosIndex.h"

using namespace CheevosIndex;

TEST_CASE("hasId: a whole number above zero names a game, nothing else does")
{
	CHECK(hasId("15738"));
	CHECK(hasId(" 4902 "));
	CHECK_FALSE(hasId(""));
	CHECK_FALSE(hasId("0"));
	CHECK_FALSE(hasId("-1"));
	CHECK_FALSE(hasId("abc"));
	CHECK_FALSE(hasId("15738x"));
	CHECK_FALSE(hasId("1234567890123"));   // longer than any id RetroAchievements issues
}

TEST_CASE("take: no hash means the ROM is read, whatever the id says")
{
	CHECK(take(false, "", "") == Take::Hash);
	CHECK(take(false, "", "15738") == Take::Hash);
	CHECK(take(false, "   ", "") == Take::Hash);
}

TEST_CASE("take: a hash and an id is a game that is done")
{
	CHECK(take(false, "8C5F3A9E1B2D4C6E7F8A9B0C1D2E3F40", "15738") == Take::None);
}

TEST_CASE("take: a hash and no id is a lookup, never a read -- RC-5's recovery files")
{
	CHECK(take(false, "8C5F3A9E1B2D4C6E7F8A9B0C1D2E3F40", "") == Take::Lookup);
	// An id of 0 names no game (FileData::hasCheevos wants > 0): looked up again.
	CHECK(take(false, "8C5F3A9E1B2D4C6E7F8A9B0C1D2E3F40", "0") == Take::Lookup);
}

TEST_CASE("lookupDue: a lookup pass on its own runs once a day, at the first boot, and after a clock that went back")
{
	const long long day = LookupIntervalSeconds;
	CHECK(day == 86400);
	CHECK(lookupDue(0, 1789400000));                    // never ran
	CHECK(lookupDue(-5, 1789400000));                   // an unreadable stamp reads as never
	CHECK_FALSE(lookupDue(1789400000, 1789400000));     // just ran
	CHECK_FALSE(lookupDue(1789400000, 1789400000 + day - 1));
	CHECK(lookupDue(1789400000, 1789400000 + day));
	CHECK(lookupDue(1789400000, 1789400000 - 60));      // the clock went back: run rather than wait a day that may never come
}

TEST_CASE("take: a forced run hashes everything, id or not")
{
	CHECK(take(true, "8C5F3A9E1B2D4C6E7F8A9B0C1D2E3F40", "15738") == Take::Hash);
	CHECK(take(true, "8C5F3A9E1B2D4C6E7F8A9B0C1D2E3F40", "") == Take::Hash);
	CHECK(take(true, "", "") == Take::Hash);
}
