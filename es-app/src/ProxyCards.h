#pragma once
#ifndef ES_APP_PROXY_CARDS_H
#define ES_APP_PROXY_CARDS_H

class Window;

// The offline-achievements proxy's two automatic jobs that move data over
// the network, shown while they run (fork #292, #293; D-UI-095, D-RA-030):
// the proxy's send of the awards it held while the device was offline, and
// the ctl's top-up of the achievement sets for recently played games. Each
// is a card in the sync card's shape, one at a time (D-UI-093), and a game
// launch over either asks (FileData::launchGame). The proxy sends on its
// own the moment RetroAchievements answers again; the interface's part is
// to show it, and to run the exit sync that was skipped for want of a
// network, which the exit card had promised for "next time you're
// connected" without anything behind the promise.
namespace ProxyCards
{
	// On the link's return (#305, the maintainer: saves first, then the
	// RetroAchievements work as one batch): when the last exit sync was
	// skipped for no network and nothing has synced since, the owed saves
	// sync runs first with its card, and the batch -- the send card when the
	// proxy holds awards or its stamp says a batch went, and the top-up --
	// follows when that card ends; with no saves owed the batch runs at
	// once. Probes on a thread of its own; shows nothing when nothing is
	// owed. Call on the interface thread.
	void linkReturned(Window* window);
	// As an automatic sync card ends (ThreadedCloudSync, startup and exit):
	// the batch the link left owed, or else the send card alone when the
	// proxy holds awards or its stamp says a batch went that no card
	// reported. Never the saves. Call on any thread.
	void afterSync(Window* window);
	// The send card is up: a launch asks PLAY NOW / KEEP WAITING.
	bool sendRunning();

	// The ctl's top-up, started at once as before (it refuses, or exits at
	// once, when it has nothing to do); a card is attached when the ctl's
	// progress file appears and no other card is up, and ends with the
	// ctl's outcome. Nothing is shown for a run that had no work. Call from
	// any thread.
	void topUp(Window* window, bool afterIndex);
	// The ctl's run is under way (its progress file, judged against the
	// clock): a launch asks STOP IT AND PLAY / KEEP WAITING.
	bool topUpRunning();
	// Ask the run to stop, through the pid in its lock file; the ctl's own
	// TERM trap writes its stamp. True when a signal was sent.
	bool stopTopUp();

	// The hasher had games to identify and RetroAchievements' hash library
	// did not come (fork #299, D-UI-104): with the toggle on, the card --
	// RETROACHIEVEMENTS (OFFLINE) over NEWLY ADDED GAMES WILL BE ENABLED ONCE
	// YOU RECONNECT (D-UI-106) -- and the ctl's index-pending marker, written
	// from a thread of its own, so the link's return lists the library once
	// for them. Nothing when the toggle is off. Call from any thread.
	void indexRanOffline(Window* window, int games);
}

#endif // ES_APP_PROXY_CARDS_H
