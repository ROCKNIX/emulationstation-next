#include "ThreadedCloudSync.h"
#include "CloudExit.h"
#include "CloudOffer.h"
#include "CloudText.h"
#include "CloudTransferJob.h"
#include "Log.h"
#include "OfflineAchievements.h"
#include "ProxyCards.h"
#include "Window.h"
#include "components/AsyncNotificationComponent.h"
#include "guis/GuiMsgBox.h"
#include "SystemConf.h"
#include "utils/FileSystemUtil.h"
#include "utils/Platform.h"
#include "utils/StringUtil.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <thread>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "LocaleES.h"

#define ICONINDEX _U("\uF0C2 ")

std::atomic<ThreadedCloudSync*> ThreadedCloudSync::mInstance(nullptr);
std::mutex ThreadedCloudSync::sInstanceLock;

ThreadedCloudSync::ThreadedCloudSync(Window* window, const std::string& command,
	const std::string& title, const std::string& running, Origin origin)
	: mWindow(window), mCommand(command), mTitle(title), mRunning(running), mOrigin(origin),
	  mStartedAt(time(nullptr))
{
	mGameExitSync = SystemConf::getInstance()->get("cloudsaves.gameexit") == "1";
	// The script stamps as they are before anything runs, so a stop can tell
	// the one its trap writes from an earlier run's (G-E1-04/06).
	mStampsBefore = readStamps(mCommand);
	// With the action row. The default is a two-row card (title and text),
	// and this was created with the default, so the recovery clause run()
	// composes -- what is in place, and where to try again (D-CLOUD-077) --
	// was measured, chosen, and never drawn: the card had no row to draw it
	// on (guest d, 2026-09-10). The row is blank while the work runs and
	// carries the clause once the outcome is known.
	mWndNotification = mWindow->createAsyncNotificationComponent(true);
	mWndNotification->updateTitle(ICONINDEX + (mRunning.empty() ? mTitle : mRunning));
	mWndNotification->updateText(_("STARTING..."));
	mWndNotification->updatePercent(-1);

	// Detached: run() ends with `delete this`, so nothing is left to join
	// it, and a joinable thread that has ended keeps its stack and control
	// block until somebody does -- one per sync, two a game, for the life of
	// the interface (#307 PL-069). This was `mHandle = new std::thread(...)`,
	// never joined, detached or deleted.
	std::thread(&ThreadedCloudSync::run, this).detach();
}

ThreadedCloudSync::~ThreadedCloudSync()
{
	mWndNotification->close();
	mWndNotification = nullptr;

	// Only if it is still us. run() clears this as soon as the work ends so
	// another sync can start during the few seconds the card holds its
	// result -- and if one has, the static belongs to that one now. Under
	// the lock, like every other change to it (#308 F-CS-25).
	std::lock_guard<std::mutex> lock(sInstanceLock);
	if (ThreadedCloudSync::mInstance == this)
		ThreadedCloudSync::mInstance = nullptr;
}

// The four outcome words and what follows them (D-UI-028, es-native-ui.md
// "Outcome vocabulary"): a why, what is in place, and how to recover.
// Nothing else -- no FAILED, no log path, no exit code, no rclone.

// The why for an exit code the scripts did not explain with a ">>> why"
// line of their own. rclone's codes; 130 is the scripts' trap, when it was
// not this process that stopped them.
std::string ThreadedCloudSync::whyForCode(int rc)
{
	switch (rc)
	{
		case 3: case 4: return _("COULDN'T FIND YOUR CLOUD FOLDER");
		case 5:         return _("YOUR CLOUD STOPPED ANSWERING");
		case 7: case 8: return _("YOUR CLOUD WOULDN'T TAKE THE FILES");
		case CloudExit::Stopped:   return _("IT WAS STOPPED");
		// The sentinels, for a part that exited one beside a part that did
		// not (the transfer page's line 4): the same words the SKIPPED
		// outcome uses, so one code is never called two things.
		case CloudExit::NoNetwork: return _("YOU'RE NOT ONLINE");
		case CloudExit::LockHeld:  return _("A SYNC IS ALREADY RUNNING");
		default:        return _("SOMETHING WENT WRONG");
	}
}

// The short forms of a why sentence, for a panel the whole one does not fit
// on (#115), are CloudText::shortenWhy and CloudText::outcomeCandidates --
// pure string work, checked by es-app/tests/unit.

// The stamp's one-word token for the same code, for the row's reader.
std::string ThreadedCloudSync::tokenForCode(int rc)
{
	switch (rc)
	{
		case 0: case 9: return "completed";
		case 3: case 4: return "folder-missing";
		case 5:         return "cloud-stopped";
		case 7: case 8: return "cloud-refused";
		case CloudExit::Stopped:   return "stopped";
		case CloudExit::NoNetwork: return "no-network";
		case CloudExit::LockHeld:  return "lock-held";
		default:        return "unknown";
	}
}

// The token read back: the phrase for one the table above wrote, "" for
// one it did not (the stamp's code decides then).
std::string ThreadedCloudSync::whyForToken(const std::string& token)
{
	if (token == "folder-missing") return whyForCode(3);
	if (token == "cloud-stopped")  return whyForCode(5);
	if (token == "cloud-refused")  return whyForCode(7);
	if (token == "stopped")        return whyForCode(CloudExit::Stopped);
	return "";
}

// Which way the saves moved, read from the command (CloudText::verbOf): the
// in-place clause is one per verb, true because rclone renames each file
// into place when it is complete (D-CLOUD-077).
static std::string inPlaceClause(CloudText::Verb verb, bool moved)
{
	switch (verb)
	{
		case CloudText::Verb::Sync:    return moved ? _("THE SAVES THAT MADE IT ARE ON BOTH SIDES. NOTHING ELSE CHANGED.") : _("DON'T WORRY, NOTHING CHANGED.");
		case CloudText::Verb::Backup:  return moved ? _("WHAT MADE IT IS IN YOUR CLOUD. THE REST IS STILL HERE.") : _("DON'T WORRY, NOTHING CHANGED.");
		case CloudText::Verb::Restore: return moved ? _("WHAT MADE IT IS ON THIS DEVICE. NOTHING ELSE CHANGED.") : _("DON'T WORRY, NOTHING CHANGED.");
		default:            return "";
	}
}

void ThreadedCloudSync::run()
{
	// Stream the backend's output into the notification card so the user
	// sees live status (rclone --stats-one-line lines, phase banners, ...).
	int ret = -1;

	// What the card says and where its bar stands go through one pair of
	// hands, so that a half of a composed sync is named in front of every
	// line and the bar never moves back within it (D-UI-052, #157). The
	// half's word first, then the line -- RECEIVING . COMPARING SAVES . 113
	// OF 113 -- so the second compare count is visibly a different step
	// from the first. A command that announces no halves (the after-a-game
	// backup, the manual rows) reads and draws exactly as it did.
	auto say = [this](const std::string& line)
	{
		if (mWndNotification == nullptr)
			return;
		std::string half;
		if (mPhase == CloudText::Phase::Receiving)
			half = _("RECEIVING");
		else if (mPhase == CloudText::Phase::Sending)
			half = _("SENDING");
		mWndNotification->updateText(half.empty() ? line : half + std::string(" \xC2\xB7 ") + line);
	};
	// The same line offered in more than one length: the card measures the
	// candidates in its own font on the interface thread and shows the first
	// that fits (AsyncNotificationComponent), as the outcome line does.
	auto sayAny = [this](const std::vector<std::string>& candidates)
	{
		if (mWndNotification == nullptr)
			return;
		std::string half;
		if (mPhase == CloudText::Phase::Receiving)
			half = _("RECEIVING");
		else if (mPhase == CloudText::Phase::Sending)
			half = _("SENDING");
		std::vector<std::string> lines;
		for (auto& line : candidates)
			lines.push_back(half.empty() ? line : half + std::string(" \xC2\xB7 ") + line);
		mWndNotification->updateText(lines, std::vector<std::string>());
	};
	auto bar = [this](int percentInPhase)
	{
		int next = CloudText::phaseBar(mPhase, percentInPhase);
		if (mPhase != CloudText::Phase::None)
			next = CloudText::forwardOnly(mBar, next);
		if (next < 0)
			return;
		mBar = next;
		if (mWndNotification != nullptr)
			mWndNotification->updatePercent(next);
	};

	// Every command runs in a session of its own and says so on its first
	// line: setsid makes the shell a process group leader, and ">>> pid N"
	// tells cancelForLaunch which group to signal, so the shell, the
	// scripts and their rclone children go together. Done here rather than
	// by each caller because the one caller that wrapped its own command
	// (the startup sync) was the only one that could be cancelled -- the
	// after-a-game backup ran bare, with no group to send a signal to.
	// shellQuote, so a command with a quote in it survives the trip.
	const std::string wrapped = "setsid sh -c "
		+ Utils::String::shellQuote("echo \">>> pid $$\"; " + mCommand) + " 2>&1";
	FILE* pipe = popen(wrapped.c_str(), "r");
	if (pipe != nullptr)
	{
		char line[512];
		while (fgets(line, sizeof(line), pipe) != nullptr)
		{
			// One line, without controls or a terminal's escapes, its UTF-8
			// kept (CloudText::cleanLine, #308 F-CS-19).
			const std::string clean = CloudText::cleanLine(line);

			// ">>> " lines are the scripts talking to the UI, not to the
			// player. ">>> pid N" is the wrapper above saying which process
			// group the command is (for cancelForLaunch); ">>> doing network"
			// says cloud_net_ready is waiting for a settled connection, the
			// one wait this card shows -- the startup sync (fork #94) gives
			// the network up to a minute to come up after boot, and a card
			// reading "Working..." for that minute says nothing about why.
			// Its words follow the link (fork #192): CHECKING THE
			// CONNECTION... when the interface has an address, since the
			// script holds a connection that is up for a short grace and
			// that is a check, not a wait; WAITING FOR A NETWORK with the
			// bound otherwise, then the run goes on or ends SKIPPED as it
			// always did; ">>> doing receive" and
			// ">>> doing send" are a composed sync announcing each of its
			// halves before it starts, which is what gives the bar its two
			// halves (D-UI-052). ">>> why <sentence>" is
			// the scripts saying, at the point of failure and in the
			// player's words, what went wrong -- the last one is the outcome
			// line's why (D-UI-028; it replaced a filter that showed any line
			// containing ERROR, FAILED or WARN, which passed rclone's prose
			// and dropped the scripts' own diagnoses). ">>> tier <label>|<rc>"
			// is a composed command reporting each of its parts as it ends,
			// so a run where one part finished and another did not is
			// reported as a failure with that part's why, rather than as the last
			// part's code. ">>> unit" and anything newer is for the transfer
			// page and never reaches the card -- but each one says the wait
			// is over, as does the first word any script prints.
			//
			// Which line is which is CloudText::classifyProtocolLine, so the
			// shapes can be checked without a pipe (es-app/tests/unit); what
			// each one does to this thread stays here.
			const CloudText::ProtocolLine protocol = CloudText::classifyProtocolLine(clean);
			if (protocol.kind != CloudText::ProtocolKind::NotProtocol)
			{
				switch (protocol.kind)
				{
				case CloudText::ProtocolKind::Pid:
					mPid = protocol.number;
					break;
				case CloudText::ProtocolKind::Doing:
				{
					// The local answer, not the member: cancelForLaunch clears
					// the flag from another thread, and this line is about the
					// line just read.
					const bool network = (protocol.text == "network");
					mWaitingForNetwork = network;
					if (network)
					{
						// Which words is CloudText::networkStep's call (fork
						// #192): the link as the interface sees it now -- the
						// same test NetworkStateWatcher makes, an address on
						// an interface -- and the bound as the command spelt
						// it, so the number on the card cannot drift from the
						// number the shell runs. With a link the step is a
						// check and reads so; without one it is a wait, said
						// with its bound where the panel has room and without
						// it where it has not (D-UI-055: true of what happens).
						const CloudText::NetworkStepChoice choice = CloudText::networkStep(
							!Utils::Platform::queryIPAddress().empty(), mCommand);
						if (choice.step == CloudText::NetworkStep::Checking)
							say(_("CHECKING THE CONNECTION..."));
						else
						{
							std::vector<std::string> lines;
							if (choice.waitSeconds > 0)
								lines.push_back(Utils::String::format(_("WAITING FOR A NETWORK, UP TO %d SECONDS...").c_str(), choice.waitSeconds));
							lines.push_back(_("WAITING FOR A NETWORK..."));
							sayAny(lines);
						}
					}

					// A half of a composed sync starting (">>> doing receive",
					// ">>> doing send"; D-UI-052): the words carry its name from
					// here, the bar parks at its start -- 0, then 50 -- and the
					// last half's bytes are forgotten, so the compare that
					// opens this half is shown as one. Announced before the
					// script runs, so the seconds it spends reaching the cloud
					// before rclone prints anything already read as this
					// half's, rather than as the last half standing still.
					const CloudText::Phase phase = CloudText::phaseOf(protocol.text);
					if (phase != CloudText::Phase::None)
					{
						mPhase = phase;
						mBytesMoving = false;
						mCountShown = false;
						mFilesDone = -1;
						mFilesTotal = -1;
						say(_("STARTING..."));
						bar(-1);
					}
					break;
				}
				case CloudText::ProtocolKind::Why:
					mWaitingForNetwork = false;
					if (!protocol.text.empty())
						mWhy = protocol.text;
					break;
				case CloudText::ProtocolKind::Offer:
					// A script asking for a question to be put to the player
					// once the run is over. The only one today: a cloud that
					// answers with no saves folder in it, which is not a
					// failure but does leave the player with nothing to
					// restore and no obvious way forward (#100, D-CLOUD-085).
					mWaitingForNetwork = false;
					mOffer = protocol.text;
					mOfferArgs = protocol.args;
					break;
				case CloudText::ProtocolKind::Tier:
					mWaitingForNetwork = false;
					if (!protocol.text.empty())
						mTiers.push_back(std::make_pair(protocol.text, protocol.number));
					break;
				default:
					mWaitingForNetwork = false;
					break;
				}
				continue;
			}
			mWaitingForNetwork = false;

			// The card shows progress; the title above it already says what
			// is happening. Everything the backends print used to land here,
			// which meant rules of "====================================" for
			// seconds at a time, and -- during a sync, which runs a restore
			// and then a backup -- banners announcing "CLOUD RESTORE UTILITY"
			// and "CLOUD BACKUP UTILITY" underneath a title reading SYNCING
			// SAVES. Those are written for a log read afterwards, not for
			// somebody watching a handheld.
			//
			// So: transfer progress, and nothing else, in the player's words.
			// What the line carries is CloudText::liveLine's to read (#140;
			// it is what found "Elapsed time: 2.0sTransferred: 0 B / 0 B" on
			// the card -- rclone glues one block's last line to the next
			// block's first when it writes to a pipe). What went wrong
			// arrives on the protocol line above, in the scripts' own words,
			// and is said once at the end.
			//
			// Which words a stats line gets is CloudText::liveWords (#208), so
			// the rule is checked without a pipe: a byte line with a total or
			// bytes sent is the fact of the moment and reads X OF Y; a still one
			// ("0 B / 0 B") is the listing and the compare and reads COMPARING
			// SAVES -- it used to read NOTHING SENT YET / NOTHING RECEIVED YET,
			// an outcome so far said where the player watches for progress, once
			// per half of every startup and exit sync; the count line holds the
			// words with its number until the bytes move, and the still byte
			// line leaves it there rather than flicker the number. The check
			// counter is a comparison, not a transfer: it never moves the bar by
			// its own percentage, or seventy checks read as seventy uploads.
			const CloudText::LiveLine live = CloudText::liveLine(clean);
			if (live.kind == CloudText::LiveLine::Kind::Bytes)
			{
				// The byte line leaving "0 B" is the one fact the in-place
				// clause turns on: something reached the other side. A total
				// means rclone has queued something to move: from here the byte
				// line is this half's fact.
				if (live.sent > 0)
					mMoved = true;
				if (live.total > 0)
					mBytesMoving = true;
			}
			// The count line names no words of its own (liveWords keeps them),
			// but the byte line's words say which file is moving from it.
			if (live.kind == CloudText::LiveLine::Kind::Files)
			{
				mFilesDone = live.sent;
				mFilesTotal = live.total;
			}
			std::string shown;
			switch (CloudText::liveWords(live, mBytesMoving, mCountShown))
			{
			case CloudText::LiveWords::Comparing:
				shown = _("COMPARING SAVES");
				break;
			case CloudText::LiveWords::ComparingCount:
				shown = _("COMPARING SAVES") + std::string(" \xC2\xB7 ")
					+ Utils::String::format(_("%d OF %d").c_str(), (int) live.sent, (int) live.total);
				mCountShown = true;
				break;
			case CloudText::LiveWords::Bytes:
			{
				// The transfer says that it is one, which file of how many, and
				// how many bytes of how many, beside the bar (fork #304,
				// D-UI-108; the maintainer: "transferring: file 1 of n (x KB/y
				// KB)"). The half's verb stands in for TRANSFERRING where the
				// sync has halves; the shorter candidates follow, longest first,
				// and the card shows the first that fits (D-UI-035).
				const std::string x = CloudText::sizeLabel((unsigned long) live.sent);
				const std::string y = CloudText::sizeLabel((unsigned long) live.total);
				const long file = CloudText::fileInFlight(mFilesDone, mFilesTotal);
				const bool sending = mPhase == CloudText::Phase::Sending;
				const bool receiving = mPhase == CloudText::Phase::Receiving;
				std::vector<std::string> lines;
				if (file > 0)
				{
					const std::string verbed = sending ? _("SENDING FILE %d OF %d (%s OF %s)")
						: receiving ? _("RECEIVING FILE %d OF %d (%s OF %s)") : _("TRANSFERRING FILE %d OF %d (%s OF %s)");
					lines.push_back(Utils::String::format(verbed.c_str(), (int) file, (int) mFilesTotal, x.c_str(), y.c_str()));
					lines.push_back(Utils::String::format(_("FILE %d OF %d (%s OF %s)").c_str(), (int) file, (int) mFilesTotal, x.c_str(), y.c_str()));
				}
				else
				{
					const std::string verbed = sending ? _("SENDING %s OF %s")
						: receiving ? _("RECEIVING %s OF %s") : _("TRANSFERRING %s OF %s");
					lines.push_back(Utils::String::format(verbed.c_str(), x.c_str(), y.c_str()));
				}
				lines.push_back(Utils::String::format(_("%s OF %s").c_str(), x.c_str(), y.c_str()));
				if (mWndNotification != nullptr)
					mWndNotification->updateText(lines, std::vector<std::string>());
				break;   // said here with the half in the verb, not through say()
			}
			case CloudText::LiveWords::Other:
				shown = live.text;
				break;
			case CloudText::LiveWords::Keep:
				break;
			}

			if (!shown.empty())
				say(shown);
			// The bar: the byte line's percentage, mapped into the half when
			// there is one; a compare parks at the half's start, and leaves
			// the bar of a run with no halves alone (D-UI-052).
			if (live.kind == CloudText::LiveLine::Kind::Bytes)
				bar(live.percent);
			else if (live.kind == CloudText::LiveLine::Kind::Checks)
				bar(-1);
		}

		int status = pclose(pipe);
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
	}

	// A cancelled run ends by SIGTERM, which pclose reports as a signal or,
	// when a shell sat between us and the group, as 143. Neither is what
	// happened. CloudExit::Stopped is what the scripts' own trap exits with
	// when somebody stops them, and what cloudLastRunDetail already reads as
	// STOPPED: the same word for the same thing, whoever did the stopping.
	const bool cancelled = mCancelled;
	if (cancelled)
		ret = CloudExit::Stopped;

	// The outcome, in the four words every cloud surface uses (D-UI-028).
	//
	// COMPLETED when the whole run did (rclone's 9 -- nothing needed
	// moving -- counts). SKIPPED for the two sentinels the scripts exit
	// before touching anything (CloudExit.h) and for the launch cancel; none
	// of the three is a failure, and FAILED would send somebody to a log to
	// find nothing wrong. COULDN'T FINISH for everything else, with the why:
	// the scripts' own sentence when they printed one, the code's phrase
	// otherwise.
	//
	// A composed run whose parts disagree -- the startup sync's restore
	// finished and its backup did not -- is a failure like any other. It
	// read COMPLETED WITH GAPS for a day, and the maintainer's verdict on
	// seeing one (2026-09-10) is that a half-outcome the player cannot act
	// on is worse than either plain answer: "if you don't know what the gaps
	// are, it's not very helpful ... it just makes you more anxious and
	// trust the system less because it's working kind of. You'd rather just
	// know it couldn't connect or it could connect." So the word is
	// COULDN'T FINISH and the why is the failing part's; the action line
	// below still says truthfully what did move. The stamp keeps the token
	// so a log can still tell a partial run from a total one.
	std::vector<std::string> okTiers, badTiers;
	for (auto& t : mTiers)
		(t.second == 0 || t.second == 9 ? okTiers : badTiers).push_back(t.first);
	const bool completed = !cancelled && (ret == 0 || ret == 9);
	const bool gaps = !cancelled && !completed && !okTiers.empty() && !badTiers.empty();
	// In the player's language: the scripts speak English whatever the
	// interface does (#308 F-CS-31). The stamp below keeps mWhy as printed.
	const std::string why = mWhy.empty() ? whyForCode(ret) : CloudText::localizedWhy(mWhy);

	// A 69 is not always a skip (the audit of the fix round, stream A's
	// lead G2-A-03 claude). The scripts exit 69 whenever the network is the
	// reason -- the retry at the link's return and the offline recovery line
	// read the code -- and when files had moved before the link went, the
	// script's own stamp adds the gaps token and its why ("69 gaps YOU WENT
	// OFFLINE PART-WAY THROUGH"), which the rows and the transfer page read
	// as COULDN'T FINISH. This card read the 69 alone and said SKIPPED -
	// YOU'RE NOT ONLINE, which says nothing moved. A bare 69 is still that.
	std::string offlineWhy;
	if (!cancelled && !gaps && ret == CloudExit::NoNetwork)
		offlineWhy = CloudText::offlinePartWayWhy(readStamps(mCommand), mStampsBefore);
	const bool offlineGaps = !offlineWhy.empty();

	std::string outcome, token;
	if (cancelled)
	{
		outcome = _("SKIPPED - YOU STARTED A GAME");
		token = "cancelled";
	}
	else if (completed)
	{
		outcome = _("COMPLETED");
		token = "completed";
	}
	else if (gaps)
	{
		outcome = _("COULDN'T FINISH") + std::string(" - ") + why;
		token = "gaps";
	}
	else if (ret == CloudExit::LockHeld)
	{
		outcome = _("SKIPPED - A SYNC IS ALREADY RUNNING");
		token = "lock-held";
	}
	else if (offlineGaps)
	{
		outcome = _("COULDN'T FINISH") + std::string(" - ") + CloudText::localizedWhy(offlineWhy);
		token = "gaps";
	}
	else if (ret == CloudExit::NoNetwork)
	{
		outcome = _("SKIPPED - YOU'RE NOT ONLINE");
		token = "no-network";
	}
	else
	{
		outcome = _("COULDN'T FINISH") + std::string(" - ") + why;
		token = tokenForCode(ret);
	}

	// Before the card says anything: the stamp is the answer that outlives
	// the card, so it is written first, and written whether or not there is
	// still a card to say it on.
	//
	// The manual stamp (last-sync-manual) is the SYNC SAVES WITH THE CLOUD
	// row's. A manual backup or restore is stamped by its script (last-backup,
	// last-restore), which the BACK UP and RESTORE rows read; writing
	// last-sync-manual for those too put a backup's outcome under the sync
	// row -- LAST 00:48 - COULDN'T FINISH on a row nobody had pressed (guest
	// d, 2026-09-10). Automatic origins stamp whatever they ran.
	if (mOrigin != Origin::Manual || CloudText::verbOf(mCommand) == CloudText::Verb::Sync)
		recordOutcome(mOrigin, ret, token, offlineGaps && mWhy.empty() ? offlineWhy : mWhy);

	// A run stopped for a game (the player's answer to the launch question,
	// D-CLOUD-129): the script's own stamp -- what the BACK UP and RESTORE
	// rows read -- carries its trap's 130 and no token, which the row reads
	// as COULDN'T FINISH (guest d, 2026-09-15: LAST 18:42 - COULDN'T FINISH
	// for a run nobody saw fail). This process knows why it stopped; the
	// stamp says so, in the shape the reader already knows, written after
	// the trap's since pclose returned after it.
	//
	// Every origin, not the manual backup and restore alone: a startup sync
	// or an exit sync a launch cancelled, and a manual sync, left the same
	// 130 under the same rows (#308 5-cloud-sync-and-saves claude F-CS-05).
	// And only the part the stop interrupted: the stamp its trap wrote this
	// run, whichever script and flags (the settings stamps included).
	if (cancelled)
		restampStoppedParts(mCommand, mStampsBefore, mStartedAt, token);

	// Offline achievements no longer ride this card (fork #292, D-RA-030):
	// the proxy sends them the moment RetroAchievements answers, and that
	// send has a card of its own on the link's return (ProxyCards). The
	// exit card says what happened to the saves and nothing else.

	// One surface for the whole event.
	//
	// The card used to vanish the instant the work ended, and the outcome
	// arrived as a GuiInfoPopup: a different shape, in a different place,
	// at exactly the moment somebody is looking for the answer. Two things
	// appeared where one thing happened. Say it in the card that has been
	// reporting all along, hold it long enough to read, and let that same
	// card fade.
	bool lingerShort = false;   // a completed run with nothing to act on: a shorter linger, below
	if (mWndNotification != nullptr)
	{
		mWndNotification->updateTitle(ICONINDEX + mTitle);

		// The action line, blank until now: what is in place, and how to
		// recover (D-CLOUD-077). The in-place clause is the verb's; "moved"
		// is whether rclone's byte totals ever left zero. The recovery
		// clause names the surface that runs it again: for an automatic
		// sync, when that is; for one the player pressed, the row.
		// A run that ended for want of a network after bytes had left
		// (cloud_backup exits 69 when the link goes part way, the S3 case):
		// its outcome word is SKIPPED, which says nothing moved, so the
		// in-place clause -- THE SAVES THAT MADE IT ARE ON BOTH SIDES -- is
		// kept in every candidate rather than dropped first (#307 PL-072).
		// A run whose script stamped the cut (offlineGaps) says COULDN'T
		// FINISH above and takes the in-place clause as every failure does.
		const bool moved = mMoved || offlineGaps;
		const bool keepInPlace = mMoved && ret == CloudExit::NoNetwork && !cancelled && !gaps && !offlineGaps;
		std::vector<std::string> action;
		if (completed)
		{
			// A completed sync has a blank action line.
		}
		else if (mOrigin == Origin::Exit && ret == CloudExit::NoNetwork)
		{
			// The exit sync could not run for want of a connection: the
			// action line says what happens next to the saves. Candidates
			// longest first, as everywhere on this card: the in-place clause
			// goes first when the line is short of room. And there is a
			// mechanism behind the sentence (D-RA-030): the link's return
			// runs the sync that is owed, with a card (ProxyCards).
			const std::string inPlace = inPlaceClause(CloudText::verbOf(mCommand), moved);
			// When awards earned offline wait in the proxy's queue, the line
			// names them too (fork #298): they go with the saves at the link's
			// return (ProxyCards). The ctl is asked here, off the interface
			// thread; a device without the proxy has no awards to name.
			const bool awardsWaiting = OfflineAchievements::available() && OfflineAchievements::toggleOn()
				&& OfflineAchievements::pendingAwards() > 0;
			if (awardsWaiting)
				LOG(LogInfo) << "ThreadedCloudSync: the exit card names the achievements waiting to be sent";
			// The title already says SYNCING SAVES; the line says when, and
			// nothing the title said (D-UI-107).
			const std::string savesWaiting = awardsWaiting
				? _("THEY'LL GO UP NEXT TIME YOU'RE CONNECTED, WITH YOUR ACHIEVEMENTS.")
				: _("THEY'LL GO UP NEXT TIME YOU'RE CONNECTED.");
			std::vector<std::string> recoveries = { savesWaiting };
			if (awardsWaiting)
				recoveries.push_back(_("THEY GO UP WITH YOUR ACHIEVEMENTS WHEN YOU'RE BACK."));
			// A run that moved saves before the link went keeps saying so
			// in every candidate: SKIPPED above says nothing moved (#307
			// PL-072, CloudText::actionCandidates).
			action = CloudText::actionCandidates(inPlace, recoveries, keepInPlace);
		}
		else
		{
			const CloudText::Verb verb = CloudText::verbOf(mCommand);
			const std::string inPlace = inPlaceClause(verb, moved);

			std::string recover;
			if (cancelled && mGameExitSync)
				recover = _("THEY GO UP WHEN YOU EXIT THE GAME.");
			else if (mOrigin == Origin::Startup)
				recover = _("IT'LL TRY AGAIN AT STARTUP, OR SYNC NOW FROM GAME SETTINGS.");
			else if (mOrigin == Origin::Exit)
				recover = _("IT'LL TRY AGAIN WHEN YOU EXIT A GAME.");
			else if (ret == CloudExit::NoNetwork)
				recover = _("TRY AGAIN WHEN YOU'RE ONLINE.");
			else if (ret == CloudExit::LockHeld)
				recover = _("WAIT FOR IT TO FINISH, THEN TRY AGAIN.");
			else if (mOrigin == Origin::Manual)
				recover = _("TRY AGAIN FROM GAME SETTINGS > ") + std::string(
					verb == CloudText::Verb::Sync ? _("SYNC SAVES WITH THE CLOUD")
					: verb == CloudText::Verb::Restore ? _("RESTORE SAVES FROM THE CLOUD")
					: _("BACK UP SAVES TO THE CLOUD"));
			else if (mCommand.find("cloud_migrate_layout") != std::string::npos)
				recover = _("TRY AGAIN FROM MANAGE CLOUD STORAGE > TIDY UP YOUR CLOUD FOLDERS");
			else
				recover = _("TRY AGAIN FROM MANAGE CLOUD STORAGE > BACK UP TO THE CLOUD");

			// Candidates, longest first; the card measures them in the row's
			// own font on the interface thread and shows the first that
			// fits (AsyncNotificationComponent). If both clauses do not fit
			// on the one line, the in-place clause goes first
			// (es-native-ui.md): the outcome word above already implies
			// it, and the recovery is the part nobody can guess. The
			// startup sentence has a short form for a panel where even it
			// alone does not fit.
			std::vector<std::string> recoveries = { recover };
			if (mOrigin == Origin::Startup)
				recoveries.push_back(_("IT'LL TRY AGAIN NEXT STARTUP."));
			action = CloudText::actionCandidates(inPlace, recoveries, keepInPlace);
		}
		// The outcome line, from candidates too, and for the same reason
		// as the action line (#115): it is composed -- the outcome word,
		// then the why -- and the why is a whole sentence. Longest first:
		// the whole thing, then the why with its trailing clause dropped,
		// then the outcome word alone, which fits any panel this runs on.
		// A translation whose outcome line carries no " - " has no split
		// to make and gets the single candidate it has today.
		mWndNotification->updateText(CloudText::outcomeCandidates(outcome), action);

		// A full bar on success; otherwise the bar goes, because a progress
		// bar left standing under COULDN'T FINISH reads as a measure of how
		// much of the failure has completed.
		mWndNotification->updatePercent(completed ? 100 : -1);
		lingerShort = completed && action.empty();
	}

	// Nothing is running any more, so stop claiming otherwise: somebody who
	// wants to start another sync while the card is still up should not be
	// told one is already going. Under the lock, so a cancelForLaunch that
	// has just taken the pointer finishes with it before it goes -- and the
	// delete below is a linger later. Whether or not there was a card: this
	// used to happen only inside the card's branch, leaving a run with no
	// card to the destructor's unlocked clear (#308 F-CS-25).
	{
		std::lock_guard<std::mutex> lock(sInstanceLock);
		if (ThreadedCloudSync::mInstance == this)
			ThreadedCloudSync::mInstance = nullptr;
	}

	if (mWndNotification != nullptr)
	{
		// Hold the outcome long enough to read, then let the card fade.
		// Success is one word and a full bar, and somebody who just exited
		// a game is standing there watching it, so a second and a half (two
		// lingered -- maintainer, 2026-09-07); anything else is two lines to
		// act on, so five. Five for everything dated from when a sync took
		// 18 seconds -- once the exit sync came down to about five, the card
		// spent as long saying it was done as it had spent working.
		std::this_thread::sleep_for(std::chrono::milliseconds(lingerShort ? 1500 : 5000));
	}

	// A question the run asked us to put to the player, once its card has
	// had its say -- and only when the run completed: an offer to create a
	// folder on top of a failure is one thing too many to read at once. The
	// dialog itself is CloudOffer's, shared with the transfer page, so the
	// two surfaces that run these scripts cannot ask it in different words
	// (#145). It pushes on the interface thread itself.
	if (completed)
		CloudOffer::present(mWindow, mOffer, mOfferArgs);

	// The offline achievements' send card follows an automatic sync's
	// (fork #292, D-RA-030): the proxy sends the moment RetroAchievements
	// answers, and the link's own card stands aside while a sync card is
	// up, so this end is where a batch that went during the sync is said.
	// Not after a launch cancel: the player is on their way into a game.
	// And not after a sync that found no network: the proxy is holding
	// its awards for the link, and a card following a queue that cannot
	// empty would say COULDN'T FINISH over an exit the maintainer asked to
	// leave quiet (the v2 proof, 2026-09-26: the exit card's end started
	// the send card offline, and the link's own card, finding one up,
	// dropped the sync it owed).
	if (!cancelled && ret != CloudExit::NoNetwork && (mOrigin == Origin::Exit || mOrigin == Origin::Startup))
		ProxyCards::afterSync(mWindow);

	delete this;
}

void ThreadedCloudSync::start(Window* window, const std::string& command,
	const std::string& title, const std::string& running, Origin origin)
{
	// Or a back up, restore or match still current on the transfer page
	// (fork #187; the page is sat in since D-UI-078, so this is the seam's
	// guard rather than an everyday path): the scripts' flock would answer
	// this run with CloudExit::LockHeld and the card would say these same
	// words after starting; said here, before, in the words the flock's
	// code reads as.
	//
	// The check and the install are one hold of the lock (#308 F-CS-25): the
	// check used to be made before the lock was taken, so two starts at once
	// could both find nothing running and both install a sync.
	{
		std::lock_guard<std::mutex> lock(sInstanceLock);
		if (ThreadedCloudSync::mInstance == nullptr && !CloudTransferJob::running())
		{
			ThreadedCloudSync::mInstance = new ThreadedCloudSync(window, command, title, running, origin);
			return;
		}
	}
	window->pushGui(new GuiMsgBox(window, _("A SYNC IS ALREADY RUNNING.")));
}

bool ThreadedCloudSync::cancelForLaunch(CancelRefusal* refusal, bool evenIfPlayerStarted)
{
	// Stopping unless we find otherwise: it covers the sync that was
	// signalled and has not gone yet, and the one that had already gone
	// before we took the lock. Both are answered by trying again in a
	// moment; only the player's own sync is answered by waiting.
	if (refusal != nullptr)
		*refusal = CancelRefusal::Stopping;

	ThreadedCloudSync* sync = nullptr;
	pid_t pid = 0;
	{
		std::lock_guard<std::mutex> lock(sInstanceLock);
		sync = ThreadedCloudSync::mInstance;
		if (sync == nullptr)
			return false;
		// The player pressed this one; a launch does not override it on its
		// own -- only their answer to the question does (D-CLOUD-129).
		if (!evenIfPlayerStarted && sync->mOrigin != Origin::Startup && sync->mOrigin != Origin::Exit)
		{
			if (refusal != nullptr)
				*refusal = CancelRefusal::PlayerStarted;
			return false;
		}

		// Cancelled before the signal, so run() finds it set however quickly
		// pclose returns. The whole group: the command runs under setsid, so
		// its pid is its process group, and the scripts, their rclone and any
		// ping or sleep are in it.
		sync->mCancelled = true;
		sync->mWaitingForNetwork = false;
		pid = sync->mPid;
		if (pid > 0)
			::kill(-pid, SIGTERM);
	}

	// Now wait for it to be gone, and only then let the launch go ahead.
	//
	// The signal is not the end of the sync; the process ending is. rclone
	// copy writes each file under a temporary name and renames it into
	// place when complete, and a rename that lands after the emulator has
	// opened that save is the one thing this gate exists to prevent -- a
	// game about to write a save must not have a restore rename over it
	// underneath. So the launch waits for run() to report the process gone:
	// it clears mInstance under the lock once pclose has returned, which
	// is once every writer to the pipe has exited. Two seconds is the
	// budget; at one and a half the group is sent SIGKILL, for an rclone
	// that is slow to act on SIGTERM. Past the budget the answer is no, and
	// the caller refuses the launch as it always did -- a sync that will
	// not die is not one to start a game over.
	const auto started = std::chrono::steady_clock::now();
	bool killed = false;
	for (;;)
	{
		{
			std::lock_guard<std::mutex> lock(sInstanceLock);
			if (ThreadedCloudSync::mInstance != sync)
				return true;
			// Its first line had not arrived when we looked -- the command
			// was only just started. Signal it as soon as it says who it is.
			// Safe to read: mInstance still names it, so it is not deleted.
			if (pid <= 0)
			{
				pid = sync->mPid;
				if (pid > 0)
					::kill(-pid, SIGTERM);
			}
		}

		const long elapsed = (long) std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - started).count();
		if (elapsed >= 2000)
			return false;
		if (!killed && elapsed >= 1500 && pid > 0)
		{
			::kill(-pid, SIGKILL);
			killed = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

// /storage/.cache/cloud_sync/last-sync-<origin>: one line, "<epoch> <rc>
// <token>[ <why>]" -- the first two fields the shape the scripts give
// last-backup and last-restore, so one reader (GuiMenu's cloudLastRunDetail)
// serves all of them; the third and any after it are additive (D-UI-028):
// the one-word token names the outcome where the code alone cannot (a 130
// that was a launch cancel against one that was not; a composed run that
// completed with gaps), and the why is the scripts' own sentence when they
// printed one, for the row to show in place of the token's phrase. Readers
// split on space and read what is there. Device-local, so under .cache
// rather than .config: a stamp carried in a settings backup onto a second
// device would describe a run that device never made.
//
// Written whole or not at all. A temp file and a rename: the reader is the
// menu, on another thread and possibly at this moment, and a half-written
// line parses as "never" -- the one thing the stamp exists to stop the row
// saying after a run.
void ThreadedCloudSync::recordOutcome(Origin origin, int rc, const std::string& token, const std::string& why)
{
	const char* name = origin == Origin::Startup ? "startup"
		: origin == Origin::Exit ? "exit"
		: origin == Origin::Manual ? "manual" : nullptr;
	if (name == nullptr)
		return;
	writeStamp(std::string("/storage/.cache/cloud_sync/last-sync-") + name, rc, token, why);
}

static const std::string STAMP_DIR = "/storage/.cache/cloud_sync/";

std::vector<CloudText::StampText> ThreadedCloudSync::readStamps(const std::string& command)
{
	std::vector<CloudText::StampText> stamps;
	for (auto& name : CloudText::scriptStampNames(command))
	{
		const std::string path = STAMP_DIR + name;
		std::string version;
		struct stat st;
		if (::stat(path.c_str(), &st) == 0)
			version = std::to_string((unsigned long long) st.st_ino) + ":" + std::to_string((long long) st.st_mtim.tv_sec)
				+ "." + std::to_string((long long) st.st_mtim.tv_nsec);
		stamps.push_back({ name, version.empty() ? std::string() : Utils::FileSystem::readAllText(path), version });
	}
	return stamps;
}

void ThreadedCloudSync::restampStoppedParts(const std::string& command, time_t runStarted, const std::string& token)
{
	for (auto& name : CloudText::stampsToRestamp(readStamps(command), runStarted))
	{
		writeStamp(STAMP_DIR + name, CloudExit::Stopped, token, "");
		LOG(LogInfo) << "ThreadedCloudSync: " << name << " restamped as " << token;
	}
}

void ThreadedCloudSync::restampStoppedParts(const std::string& command, const std::vector<CloudText::StampText>& before,
	time_t runStarted, const std::string& token)
{
	for (auto& name : CloudText::stampsToRestamp(readStamps(command), runStarted, &before))
	{
		writeStamp(STAMP_DIR + name, CloudExit::Stopped, token, "");
		LOG(LogInfo) << "ThreadedCloudSync: " << name << " restamped as " << token;
	}
}

// One stamp file, in the shape above, whichever surface reads it.
void ThreadedCloudSync::writeStamp(const std::string& path, int rc, const std::string& token, const std::string& why)
{
	if (!Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(path)))
		return;

	// One line: the why is kept to printable characters and a single line
	// so the reader's split cannot be confused by it.
	std::string sentence;
	for (char c : why)
		if (c >= 32 && c < 127)
			sentence += c;
	sentence = Utils::String::trim(sentence);

	const std::string tmp = path + ".tmp";
	Utils::FileSystem::writeAllText(tmp,
		std::to_string(static_cast<long long>(time(nullptr))) + " " + std::to_string(rc) + " " + token
		+ (sentence.empty() ? "" : " " + sentence) + "\n");
	if (std::rename(tmp.c_str(), path.c_str()) != 0)
		std::remove(tmp.c_str());
}
