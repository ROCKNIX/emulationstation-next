// The token check's retry schedule, checked without a device (#175).
//
// Runs against es-app/src/CheevosRetry.cpp alone. What matters here is the
// shape the schedule promises -- a bounded run of short retries while the
// link is up, and the regular schedule for everything else -- because the
// one time it was tried on a guest, a re-check that ran before the resolver
// answered was followed by nothing for two hours.

#include "doctest/doctest.h"

#include "CheevosRetry.h"

using namespace CheevosRetry;

TEST_CASE("the schedule is nine tries ten seconds apart, ninety seconds in all")
{
	// The comment in the header makes these three promises; hold it to them.
	CHECK(IntervalMs == 10 * 1000);
	CHECK(Attempts == 9);
	CHECK(Attempts * IntervalMs == 90 * 1000);
	CHECK(ScheduledMs == 120 * 60 * 1000);
}

TEST_CASE("a check that could not reach the server while online is retried within the window")
{
	for (int n = 1; n <= Attempts; n++)
		CHECK(nextDelayMs(true, true, n) == IntervalMs);

	// Past the window, the regular schedule -- and it stays there however
	// many more fail.
	CHECK(nextDelayMs(true, true, Attempts + 1) == ScheduledMs);
	CHECK(nextDelayMs(true, true, Attempts + 2) == ScheduledMs);
	CHECK(nextDelayMs(true, true, 1000) == ScheduledMs);
}

TEST_CASE("a refusal or a success is never retried early")
{
	// Both reached the server; unreachable is false for both. A refusal is
	// the account's problem, and a loop of wrong passwords helps nobody.
	for (int n = 0; n <= Attempts + 1; n++)
	{
		CHECK(nextDelayMs(false, true, n) == ScheduledMs);
		CHECK(nextDelayMs(false, false, n) == ScheduledMs);
	}
}

TEST_CASE("with no link there is nothing to retry against")
{
	// The offline boot: one failure, one line in the log, and quiet until
	// the link comes up -- not ten failures over ninety seconds.
	for (int n = 1; n <= Attempts; n++)
		CHECK(nextDelayMs(true, false, n) == ScheduledMs);
}

TEST_CASE("a count that was not kept does not retry")
{
	// 1 is the first failure. 0 or less means the caller did not count, and
	// a guard that cannot see its input fails closed.
	CHECK(nextDelayMs(true, true, 0) == ScheduledMs);
	CHECK(nextDelayMs(true, true, -1) == ScheduledMs);
}

TEST_CASE("the window, walked the way the component walks it")
{
	// RC-3 cycle 2 (#175): the link comes up, the re-check runs before the
	// resolver answers and fails, and DNS never does answer. Count the short
	// retries the schedule hands out before it gives up.
	int failures = 0;      // the re-check's failure is the first
	int retries = 0;
	int waitedMs = 0;
	for (;;)
	{
		failures++;
		int delay = nextDelayMs(true, true, failures);
		if (delay == ScheduledMs)
			break;
		retries++;
		waitedMs += delay;
	}
	CHECK(retries == Attempts);
	CHECK(waitedMs == 90 * 1000);

	// And the same walk with the resolver answering on the third try: the
	// success is "reached", so the schedule is the regular one from there.
	CHECK(nextDelayMs(true, true, 1) == IntervalMs);
	CHECK(nextDelayMs(true, true, 2) == IntervalMs);
	CHECK(nextDelayMs(false, true, 0) == ScheduledMs);
}

// Closing RETROACHIEVEMENTS SETTINGS (#308 1-raoffline claude F-RA-19). With
// the switch left on and no token -- every close while offline, since the
// fork keeps the switch on when a sign-in fails (#175) -- the save signed in
// on the interface thread, and the screen waited for the request: on a
// hotspot with a route and no DNS (#242), its connect timeout at every close.
// Nothing changed is the token check's to do, on its own thread; a change
// with no address answers at once.
TEST_CASE("closing the settings page signs in on the interface thread only for a change it can send")
{
	// Nothing changed, no token: the token check runs it, offline or not.
	CHECK(saveSignIn(true, false, true, false) == SaveSignIn::InBackground);
	CHECK(saveSignIn(true, false, true, true) == SaveSignIn::InBackground);   // the route-and-no-DNS case
	// The player changed something: the answer is theirs to see.
	CHECK(saveSignIn(true, true, true, true) == SaveSignIn::Now);
	CHECK(saveSignIn(true, true, false, true) == SaveSignIn::Now);
	// ... and with no address to send from, it is given without a request.
	CHECK(saveSignIn(true, true, true, false) == SaveSignIn::Offline);
	// A token and nothing changed, or the switch off: nothing to do here.
	CHECK(saveSignIn(true, false, false, true) == SaveSignIn::None);
	CHECK(saveSignIn(true, false, false, false) == SaveSignIn::None);
	CHECK(saveSignIn(false, true, true, true) == SaveSignIn::None);
	CHECK(saveSignIn(false, false, true, false) == SaveSignIn::None);
}
