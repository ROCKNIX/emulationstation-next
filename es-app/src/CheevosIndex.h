#pragma once
#ifndef ES_APP_CHEEVOS_INDEX_H
#define ES_APP_CHEEVOS_INDEX_H

// What the RetroAchievements index (ThreadedHasher: INDEX NEW GAMES AT
// STARTUP, INDEX GAMES) owes a game on a run, decided from the two fields it
// keeps per game -- the ROM's cheevosHash and the game's cheevosId -- and
// nothing else, so es-unit-tests can hold the rule (es-app/tests/unit).
//
// The rule used to be "a game with a hash is done". It is not: a game can
// carry a hash and no id because the id was decided after the hash was saved
// and never reached the disk (RC-5's recovery files, fork #183, audit #186
// PL-08), or because RetroAchievements did not know the hash then. Such a
// game gets a lookup -- its hash against the library the run has just
// fetched -- and no file read; the ones the library knows gain their id,
// the rest stay as they were. A game with an id is done, and a forced run
// (INDEX GAMES with "all games") hashes everything.

#include <string>

namespace CheevosIndex
{
	enum class Take
	{
		None,     // the game has its id: nothing to do
		Hash,     // read the ROM and look the hash up (the hash is missing, or the run is forced)
		Lookup    // the hash is there and the id is not: look the hash up, read nothing
	};

	// Whether a cheevosId names a game: a whole number above zero, as
	// FileData::hasCheevos reads it. "0", "", "abc" do not.
	bool hasId(const std::string& cheevosId);

	Take take(bool forceAllGames, const std::string& cheevosHash, const std::string& cheevosId);

	// A lookup pass with nothing to hash beside it costs the hash library
	// (two requests, several megabytes, on the interface thread at startup),
	// and most libraries hold a game RetroAchievements does not know -- a
	// hack, a bad dump -- whose hash would otherwise ask for it at every
	// boot. So the silent startup run makes that pass at most once a day:
	// due when none has run, when a day has passed, or when the clock has
	// gone back. lastEpoch 0 is "never".
	const long long LookupIntervalSeconds = 24 * 60 * 60;
	bool lookupDue(long long lastEpoch, long long nowEpoch);
}

#endif // ES_APP_CHEEVOS_INDEX_H
