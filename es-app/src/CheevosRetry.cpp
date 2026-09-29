#include "CheevosRetry.h"

namespace CheevosRetry
{
	int nextDelayMs(bool unreachable, bool online, int unreachableInARow)
	{
		if (!unreachable || !online)
			return ScheduledMs;

		// The count is 1 on the first failure. Anything under that is a
		// caller that did not count, and a guard that cannot see its input
		// does not retry.
		if (unreachableInARow < 1 || unreachableInARow > Attempts)
			return ScheduledMs;

		return IntervalMs;
	}

	// The save used to sign in whenever the switch was on and the token
	// empty or the account changed: every close while offline was a
	// request on the interface thread. Only a change the player made is
	// answered here, and only when there is an address to send it from.
	SaveSignIn saveSignIn(bool on, bool accountChanged, bool tokenEmpty, bool hasAddress)
	{
		if (!on)
			return SaveSignIn::None;
		if (accountChanged)
			return hasAddress ? SaveSignIn::Now : SaveSignIn::Offline;
		return tokenEmpty ? SaveSignIn::InBackground : SaveSignIn::None;
	}
}
