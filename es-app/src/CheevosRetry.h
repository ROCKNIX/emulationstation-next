#pragma once

// When the RetroAchievements token check tries again (#175).
//
// Pure: a schedule and nothing else, so es-app/tests/unit can hold it to
// its word. The shell that runs the check and hears about the link is
// CheckCheevosTokenComponent in NetworkThread.cpp.
namespace CheevosRetry
{
	// A check that could not reach the server while the network is up is
	// followed by another IntervalMs later, up to Attempts of them -- ninety
	// seconds in all. A resolver that answers within a minute of the link is
	// the common case (systemd-resolved lagged the address by five seconds
	// on the run that found this); anything longer waits for the regular
	// schedule, ScheduledMs.
	const int IntervalMs  = 10 * 1000;
	const int Attempts    = 9;
	const int ScheduledMs = 120 * 60 * 1000;

	// How long to wait before the next check.
	//   unreachable:       the last check could not reach the server (a
	//                      refusal and a success both reached it)
	//   online:            the network watcher's view at that moment
	//   unreachableInARow: checks since the last success, refusal or link-up
	//                      that could not reach the server, the last one
	//                      included -- so 1 on the first failure
	// Anything but "unreachable, online, within the window" is the regular
	// schedule: a refusal is the account's problem and no retry fixes it,
	// and a device with no link has nothing to retry against.
	int nextDelayMs(bool unreachable, bool online, int unreachableInARow);

	// What closing RETROACHIEVEMENTS SETTINGS does about the token
	// (#308 1-raoffline claude F-RA-19). The page's save runs on the
	// interface thread, so a sign-in there is a request the screen waits on.
	//   Now          sign in here: the player changed the switch or the
	//                account, and is told how it went
	//   Offline      the same change with no address to send from: no
	//                request, the "couldn't reach" answer at once, and the
	//                token check asked to run -- which, failing, retries
	//                when the network comes up
	//   InBackground nothing changed and there is no token: the token
	//                check (NetworkThread, on its own thread) is asked to
	//                run now instead
	//   None         nothing to do
	enum class SaveSignIn { None, Now, Offline, InBackground };
	SaveSignIn saveSignIn(bool on, bool accountChanged, bool tokenEmpty, bool hasAddress);
}
