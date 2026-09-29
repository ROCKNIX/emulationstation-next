#pragma once
#ifndef ES_APP_CLOUD_TEXT_H
#define ES_APP_CLOUD_TEXT_H

// The cloud surfaces' pure text: what a stamp line says, what a script's
// protocol line means, which words a provider is known by, and how a
// composed line is shortened when the panel is too narrow for all of it.
//
// Nothing here reads a file, asks Settings or SystemConf anything, touches
// a Window, or measures a font, so all of it can be checked by a test
// binary that links no more of EmulationStation than this file and
// StringUtil (es-app/tests/unit). The callers keep the parts that cannot
// be: the file read, the translation of an outcome into the player's
// language, and the side effects a protocol line asks for.
//
// Translation stays outside on purpose. _() at this level would put the
// player's language inside the thing under test, so the enums below name
// an outcome and the caller says it in words.

#include <ctime>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace CloudText
{
	// The providers we put in front of people, most-likely-to-be-owned
	// first: rclone's word for each service, and the words the player
	// chose it by. One table, read from both sides -- the wizard's
	// recommendation list and providerLabel below.
	//
	// This was split into two groups by how the sign-in works -- one you
	// approve through a provider page, one you type a key into. That is our
	// distinction, not the player's: they are choosing where their saves
	// live, and the group headers made an implementation detail look like
	// the question being asked. It also read as a hierarchy it is not,
	// since the second group was a handful of picks rather than the rest of
	// the world.
	//
	// One list of recommendations, and a complete list behind it. Which
	// kind of sign-in a provider needs is settled after it is chosen, by
	// its own tier. The labels are translated at the point of use: _() at
	// static-init time would run before the locale is loaded.
	const std::vector<std::pair<std::string, std::string>>& recommendedProviders();

	// rclone's word for a service ("drive") in the words the player chose
	// it by ("GOOGLE DRIVE"). A provider set up outside the list above
	// falls back to rclone's own word, uppercased, which is at least the
	// name on the tin. Empty in, empty out -- the caller decides what to
	// say when nothing is connected.
	std::string providerLabel(const std::string& type);

	// rclone's name for a provider option ("bearer_token") in the words a
	// player connecting a NAS or a bucket would use ("ACCESS TOKEN"), for
	// the fields the recommended providers ask for; anything unmapped is
	// the name itself with its underscores as spaces, upper case ("SOME
	// OPTION", never "SOME_OPTION"). The rclone name still goes in the
	// config -- only the row's label changes (#123). Every label fits a
	// 640x480 row beside its value; the longest is SECRET ACCESS KEY.
	std::string fieldLabel(const std::string& rcloneName);

	// The line under a provider form's title: the provider as rclone
	// describes it, unless that description is a paragraph -- s3's names
	// sixty compatible services -- in which case the words the player chose
	// it by (providerLabel). A subtitle is one short line on a 3.5" panel,
	// never a paragraph in small text (#128, D-UI-023).
	std::string providerSubtitle(const std::string& type, const std::string& label);

	// The device name as the network takes it: ASCII letters and digits,
	// any run of anything else as one hyphen, none at either end, at most
	// 63. The same rule as the scripts' clean_hostname (001-functions),
	// which network-base-setup applies at boot. The player's own name is
	// left exactly as they typed it -- this is only what the network shows.
	std::string cleanHostname(const std::string& in);

	// How a run ended, in the four words every cloud surface uses
	// (D-UI-028). Gaps and Failed both read COULDN'T FINISH to the player
	// and differ only in where their why comes from: a composed run whose
	// parts disagreed has the failing part's, and nothing to fall back on
	// from an exit code that describes the whole run.
	enum class Outcome
	{
		Completed,
		Gaps,
		SkippedLockHeld,
		SkippedNoNetwork,
		SkippedGameStarted,
		SkippedCancelled,     // the player's CANCEL on the transfer page (D-UI-078)
		Failed
	};

	// What a stamp line says. The file read and the fallback whys stay with
	// the caller; ran is false for anything this cannot make sense of, and
	// a row then reads NOT DONE ON THIS DEVICE YET rather than inventing a
	// date.
	struct LastRun
	{
		bool ran = false;
		time_t when = 0;
		int code = 0;
		std::string token;
		std::string why;      // upper case, no trailing period; empty when there was none
		Outcome outcome = Outcome::Completed;
		bool finished = false;
		bool knownToken = false;  // one of ThreadedCloudSync's tokens, not a scripts' why
	};

	// Whether a stamp's third field is one of the tokens EmulationStation
	// writes (rather than the scripts' own sentence, underscored).
	bool isOutcomeToken(const std::string& token);

	// "<epoch> <rc>[ <token>[ <why...>]]" (D-UI-028), the whole file's text.
	LastRun parseLastRun(const std::string& text);

	// Whether the exit sync that was skipped for want of a network is still
	// owed (fork #292, D-RA-030): last-sync-exit reads no-network, and
	// neither a startup sync that completed nor a back up the player ran
	// (the script's last-backup, "<epoch> 0") has run since. The stamps'
	// whole texts; an absent stamp is "".
	bool exitSyncOwed(const std::string& exitStamp, const std::string& startupStamp, const std::string& backupStamp);

	// A stopped run's stamps (#203; #308 5-cloud-sync-and-saves claude
	// F-CS-05, gpt F-CS-23). The scripts' INT/TERM trap stamps the part it
	// was inside with 130 and no token, which a row reads as COULDN'T
	// FINISH; the process that stopped the run knows why and restamps it.
	//
	// The stamps the scripts a command names can write, by name under
	// /storage/.cache/cloud_sync.
	std::vector<std::string> scriptStampNames(const std::string& command);
	// Of those (each name with the file's whole text, "" when there is
	// none, and the file's identity when it was read -- inode and change
	// time -- "" when there was no file or it was not asked), the ones to
	// restamp for a run that began at runStarted. With `before`, the same
	// stamps read as the run began, a stamp is this run's when its file was
	// written since -- the scripts write each stamp to a new file and rename
	// it into place -- whatever the clocks said; without it (a caller that
	// took no snapshot), when its epoch is not older than runStarted.
	struct StampText { std::string name; std::string text; std::string version; };
	std::vector<std::string> stampsToRestamp(const std::vector<StampText>& stamps, time_t runStarted,
		const std::vector<StampText>* before = nullptr);

	// A run the scripts ended for want of a network (69) after files had
	// moved (the audit of the fix round, stream A's lead G2-A-03 claude):
	// the script that lost the link stamps "<epoch> 69 gaps YOU WENT OFFLINE
	// PART-WAY THROUGH", which the rows and the transfer page read as
	// COULDN'T FINISH, and the automatic sync's card read the 69 alone as
	// SKIPPED - YOU'RE NOT ONLINE. The why of the first of `stamps` written
	// since `before` was read (a new file, as stampsToRestamp tells one)
	// that carries 69 and the gaps token, in English as the scripts wrote
	// it; "" when none does -- a bare 69, where nothing moved.
	std::string offlinePartWayWhy(const std::vector<StampText>& stamps, const std::vector<StampText>& before);

	// Which run a stamp describes. EmulationStation stamps last-sync-exit
	// and last-sync-startup as each automatic run ends, within a second or
	// two of the script writing its own last-backup or last-restore, so a
	// matching time says which it was. Pass 0 for a stamp that does not
	// exist; the exit stamp wins a tie.
	enum class RunOrigin { None, AfterLastGame, AtStartup };
	RunOrigin runOrigin(time_t when, time_t exitWhen, time_t startupWhen);

	// A shorter form of a why sentence, for a panel the whole one does not
	// fit on (#115). Two shapes appear in the sentences the scripts emit,
	// and both put the part that can go at the end: a trailing clause after
	// a dash (COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN) and a
	// trailing sentence after a full stop. Anything else has no short form
	// and comes back empty; the caller falls back to the outcome word on
	// its own, which fits any panel and is still true.
	std::string shortenWhy(const std::string& why);

	// The forms of an outcome line, longest first (D-UI-035): the whole
	// thing, then the why with its trailing clause dropped, then the
	// outcome word alone. A line with no " - " has no split to make and
	// gets the single candidate it has today.
	std::vector<std::string> outcomeCandidates(const std::string& outcome);

	// The sentences the cloud scripts print as ">>> why <SENTENCE>" -- and
	// the rc-keyed ones their why_for prints -- each paired with its
	// translation in the interface's language (#308 F-CS-31). The scripts
	// speak English whatever the language; the card put their sentence
	// beside a translated outcome word. localizedWhy returns the pair's
	// translation, or the sentence as it came for one this build does not
	// list (a script newer than the interface): never worse than before.
	// The stamps keep the English: their readers translate.
	std::vector<std::pair<std::string, std::string>> whySentences();
	std::string localizedWhy(const std::string& why);
	bool isKnownWhy(const std::string& why);

	// One line of a cloud script's output as the card reads it: ANSI escape
	// sequences and C0 controls and DEL dropped, every other byte kept --
	// UTF-8 included, since a folder name in the player's language arrives
	// in a ">>> offer" line and must reach the offer whole -- then trimmed.
	std::string cleanLine(const std::string& raw);

	// The sync card's action line when a run did not complete (D-CLOUD-077),
	// longest first: what is in place and how to recover, then the recovery
	// alone. `recoveries` are the recovery sentence's own forms, longest
	// first. The in-place clause is the part that goes first when the line
	// is short of room (es-player-text.md) -- unless keepInPlace, when every
	// candidate carries it and the in-place clause alone is the last.
	std::vector<std::string> actionCandidates(const std::string& inPlace,
		const std::vector<std::string>& recoveries, bool keepInPlace);

	// The ">>> " lines are the scripts talking to the interface, not to the
	// player. Classification only: what the line is and what it carries.
	// Acting on it -- the pid to signal, the card's waiting text, the why
	// to keep, the offer to put to the player, the tier to record -- stays
	// with the caller.
	//
	// Every shape any script emits is named here, whether or not the reader
	// asking about it acts on one: the transfer page read ">>> unit" and
	// ">>> removed" with a parser of its own and never knew about
	// ">>> offer", so the empty-cloud question reached one of the two
	// surfaces that run cloud_restore and not the other (#145). One parser,
	// two readers, and Unknown means a marker newer than this build --
	// never a marker this build simply forgot.
	enum class ProtocolKind { NotProtocol, Pid, Doing, Why, Offer, Tier, Unit, Removed, Unknown };

	// removed: one per system, out of the third field of
	// ">>> removed 14|314572800|snes:12:300000000,gb:2:14572800". files is
	// the count as the script spelt it, because the caller prints it and
	// picks FILE or FILES by it; an item with fewer than two fields is not
	// one and is dropped.
	struct RemovedSystem
	{
		std::string system;   // upper case
		std::string files;
		long bytes = 0;
	};

	struct ProtocolLine
	{
		ProtocolKind kind = ProtocolKind::NotProtocol;
		// doing: what is being waited on; why: the sentence, upper case and
		// without its full stop; offer: the question's name; tier: the
		// part's label, upper case; unit: the item's label as the script
		// spelt it -- one announcement is matched against the next, so the
		// case it arrived in is the case it is compared in.
		std::string text;
		// offer: what follows the question's name, '|'-separated -- for
		// create-saves-folder the folder that is missing, then a folder
		// beside it whose name is close to it, when there is one (#127).
		std::vector<std::string> args;
		// pid: the process group; tier: that part's exit code, -1 when the
		// line carried none; unit: the script's own number for this item,
		// 0 when it carried none.
		int number = 0;
		// unit: how many items that script has, 0 when it did not say.
		int count = 0;
		// removed: how many files a match took off this device, and what
		// they came to. A line that carried no number says zero, which is
		// what the field means -- nothing went.
		long files = 0;
		long bytes = 0;
		std::vector<RemovedSystem> systems;
	};

	ProtocolLine classifyProtocolLine(const std::string& clean);

	// Which way the saves moved, read from the command: the in-place clause
	// is one per verb (D-CLOUD-077).
	enum class Verb { Sync, Backup, Restore, Other };
	Verb verbOf(const std::string& cmd);

	// Which transfer a page's command runs (fork #187): a match removes
	// (--match), a restore brings down (cloud_restore, cloud_content_restore),
	// a back up sends up (cloud_backup, cloud_content_backup). verbOf answers
	// the card's question about the saves scripts and reads the content
	// scripts as Other; this reads a whole composed command by the scripts
	// it names, for the hub row that follows the current run's outcome
	// (D-UI-070) and for the launch gate's sentence over a run still
	// current. backuptool decides nothing: it
	// appears in a settings restore (restore --then-cloud) and a settings
	// backup alike, and the cloud script beside it says which. A command
	// naming a restore and a backup script both is the card's sync, not a
	// page's run, and Other.
	enum class TransferKind { Backup, Restore, Match, Other };
	TransferKind transferKind(const std::string& cmd);

	// The first candidate that fits the width, else the last one offered.
	// measure is the row's own font, handed in because a font is a GL
	// resource and this has to stay free of one; an empty measure or a
	// width of zero means nothing is known yet, and the full form is the
	// right answer then.
	std::string chooseThatFits(const std::vector<std::string>& candidates, float width,
		const std::function<float(const std::string&)>& measure);

	// rclone's size units, once: how each is spelt in its output (the torn
	// "Ki"/"Mi"/"Gi" is a per-file line cut at 80 columns), what the pages
	// call it, and how many bytes it is. roundSizes finds a size by this
	// table, parseBytes reads it and sizeLabel re-renders it; the transfer
	// page's rename of a token that did not parse uses the same table. So a
	// unit a page can show is a unit it can add up. Longest spelling first:
	// "GiB" must be matched before "Gi".
	struct RcloneUnit { const char* rclone; const char* shown; double bytes; };
	const std::vector<RcloneUnit>& rcloneUnits();

	// The bytes in one rclone size field: "80 KiB" -> 81920, "1.4 GiB" ->
	// 1503238554, "0 B" -> 0. -1 when the field carries no number or a unit
	// the table does not know: a value that was not printed is never added
	// to a total that will be shown. strtod also reads "inf" and "nan",
	// which no size is and whose cast to long is undefined; rclone never
	// prints them, and they are refused all the same.
	long parseBytes(const std::string& field);

	// A size at the precision it has: "200 KB", "1.2 MB", "1.20 GB" -- a
	// whole KB below a megabyte, one decimal below a gigabyte, two above
	// (#85). The one formatter for every size a cloud surface prints, so no
	// two of them disagree on a number. A size that is not zero rounds up,
	// so one byte reads "1 KB", never "0 KB"; the unit is chosen from the
	// value as it will print, so nothing reads "1024 KB" a byte short of
	// the next unit.
	std::string sizeLabel(unsigned long bytes);

	// Every size and speed in an rclone fragment at sizeLabel's precision:
	// "16.521 MiB / 16.521 MiB, 100%, 519.844 KiB/s" -> "16.5 MB / 16.5 MB,
	// 100%, 520 KB/s". A "/s" after the unit stays: a speed is a size per
	// second. Percentages and times carry no unit from the table and pass
	// through untouched; so does a number whose unit did not parse.
	std::string roundSizes(const std::string& fragment);

	// One line of rclone's output as the sync card should read it (#140).
	//
	// Piped, rclone writes its stats block with no newline after the last
	// line, so the next block's "Transferred:" arrives glued to whatever
	// ended the block before -- "Elapsed time: 2.0sTransferred: 0 B / 0 B"
	// on the card that produced #140, a per-file line on a busier run. The
	// marker's last occurrence is where the line that matters starts, and
	// everything in front of it is the tail of a block already shown.
	//
	// What comes back is the fact, not the words: how many bytes of how
	// many (Bytes), how many files of how many (Files), how far the
	// comparison has got (Checks), or a line that carries progress in some
	// other shape (Other, text as it came). The caller says it in the
	// player's language. None is a line the card should not repeat: a
	// per-file line, rclone's headers, the elapsed time, prose.
	struct LiveLine
	{
		enum class Kind { None, Bytes, Files, Checks, Other };
		Kind kind = Kind::None;
		long sent = -1;      // Bytes: bytes moved so far; Files/Checks: done
		long total = -1;     // Bytes: bytes in all; Files/Checks: of how many
		int percent = -1;    // the byte line's percentage, else -1 (no bar change)
		std::string text;    // Other: the line as it came
	};
	LiveLine liveLine(const std::string& clean);

	// What the card's words do for one stats line of a half (#208).
	//
	// rclone prints a byte line and a check count in every block. While it
	// lists both sides and compares them the byte line reads "0 B / 0 B",
	// and the card used to turn that into NOTHING SENT YET or NOTHING
	// RECEIVED YET -- the outcome so far, said where the player is watching
	// for progress, once per half, at boot and after every game. The line
	// reports progress: a still byte line is a compare and reads COMPARING
	// SAVES; once the count has a total, the count holds the words
	// (COMPARING SAVES - N OF M) and the byte line leaves them alone rather
	// than flicker the count on and off; once this half's bytes move, the
	// byte line is the fact and the count stays off the words. The outcome
	// is said once, at the end, from the scripts' own line.
	//
	//   Keep            leave the words as they are
	//   Comparing       COMPARING SAVES
	//   ComparingCount  COMPARING SAVES - N OF M, from the count line
	//   Bytes           X OF Y, from the byte line
	//   Other           the line as it came
	enum class LiveWords { Keep, Comparing, ComparingCount, Bytes, Other };
	LiveWords liveWords(const LiveLine& live, bool bytesMoving, bool countShown);

	// The file the transfer is on, for the card's line (fork #304, D-UI-108):
	// rclone's count line says how many files are done, and the player is
	// told which one is moving -- done + 1, never past the total. 0 when
	// rclone has not counted yet (no total), so the line falls back to the
	// bytes alone.
	long fileInFlight(long filesDone, long filesTotal);

	// The halves of a composed sync, for the card's bar (D-UI-052, #157).
	//
	// A startup sync is two rclone runs back to back -- a restore, then a
	// backup -- each with a compare of its own and a transfer of its own. A
	// bar that followed each run's own percentage stood still through both
	// compares and reset between them, so the card read "113 OF 113" twice
	// with nothing visibly moving. The command now announces each half
	// before it starts (">>> doing receive", ">>> doing send"), and the bar
	// has two halves: receiving fills 0-50, sending 50-100, a percentage
	// within a half maps into it, a compare parks at the half's start, and
	// the bar only ever moves forward. None is a command that announces no
	// halves -- the after-a-game backup, the manual rows -- whose bar is the
	// run's own percentage, as it always was.
	enum class Phase { None, Receiving, Sending };

	// The half a ">>> doing" word names; None for every other word,
	// "network" included.
	Phase phaseOf(const std::string& doingWord);

	// Where a percentage within a phase lands on the whole bar: 0..100 in
	// Receiving is 0..50, in Sending 50..100, in None itself. A percentage
	// that is not one (-1: a compare, or "0 B / 0 B, -") is the phase's
	// start -- 0, 50 -- so the bar parks there; in None it stays -1, which
	// the caller reads as "leave the bar alone".
	int phaseBar(Phase phase, int percentInPhase);

	// The bar never moves back once a phase is known: the larger of what is
	// drawn and what is proposed, with -1 on either side meaning nothing.
	int forwardOnly(int shown, int proposed);

	// The startup card's network step (fork #192). cloud_net_ready prints
	// ">>> doing network" whenever it has to wait at all -- the three
	// seconds it holds a connection that is already up included -- so the
	// card read WAITING FOR THE NETWORK on a device whose Wi-Fi had been up
	// since fifteen seconds after boot. Maintainer, on the RG SP: "doesn't
	// the device know if it's online by the time it starts up and shows
	// EmulationStation?" It does, and the words follow the link, not the
	// script: with a link the step is the check it is -- whether the
	// connection has settled, a different question from whether the link
	// is up -- and reads CHECKING; without one it is a wait, and the line
	// says how long, from the bound the command hands cloud_net_ready
	// (--wait N, or --wait=N; the script's own default when the command
	// names none, which is also the fallback loop's minute). A zero is
	// handed back as one -- the caller then names no number rather than
	// "up to 0 seconds". The words themselves stay with the caller
	// (D-UI-055: a sentence must be true of what happens).
	enum class NetworkStep { Checking, Waiting };
	struct NetworkStepChoice
	{
		NetworkStep step = NetworkStep::Checking;
		int waitSeconds = 60;
	};
	constexpr int NETWORK_WAIT_DEFAULT_S = 60;
	NetworkStepChoice networkStep(bool linkUp, const std::string& command);

	// The offline RetroAchievements proxy's answers, read as text (fork
	// #173, D-RA-004). raofflineproxy-ctl prints them; OfflineAchievements
	// runs it; what the lines mean is settled here, where it has a test.

	// "N" from raofflineproxy-ctl pending: the count of casual awards
	// waiting for a connection. -1 for anything that is not one whole
	// number on its own -- a missing answer must never read as a count, or
	// the card promises a send on the strength of nothing.
	int parsePendingCount(const std::string& text);

	// "<epoch> <flushed>" from raofflineproxy-ctl flushed: the stamp the
	// proxy leaves after a flush that sent awards. ok is false for a line of
	// any other shape and for a count of zero -- a stamp that says nothing
	// went is not a stamp.
	struct FlushStamp
	{
		bool ok = false;
		time_t when = 0;
		int flushed = 0;
	};
	FlushStamp parseFlushStamp(const std::string& text);

	// Which of the sentences the exit card ends on, from what is waiting
	// (D-RA-004; the sentences themselves are D-RA-017's): awards, awards
	// and saves, saves, or nothing to say. The
	// saves are "pending" when the exit sync could not run for want of a
	// connection; awards when the ctl counted any. Translation stays with
	// the caller.
	enum class NextTime { None, Awards, AwardsAndSaves, Saves };
	NextTime nextTime(bool awardsPending, bool savesPending);

	// "<epoch> <rc> <scan|topup> cached=<n> skipped=<m> ready=<total>
	// limit=<0|1> indexed=<i> errors=<e>[ why=<TOKEN>]" from
	// raofflineproxy-ctl's last-scan (indexed= and errors= since audit #186
	// PL-24; a ctl before them writes neither, and both read 0): how
	// the last scan of the console's games for offline achievements went,
	// or the last automatic top-up when the device came online (fork #179,
	// D-RA-010). ran is false for a line of any other shape, and the row
	// then reads NOT SCANNED YET rather than inventing a date. The why is
	// the ctl's token -- upper case, underscores, nothing else -- and the
	// caller says it in words; a field that is not a count reads as zero.
	struct ScanStamp
	{
		bool ran = false;
		time_t when = 0;
		int code = 0;
		bool topup = false;   // the automatic run, not one the player pressed
		int cached = 0;       // games this run cached, new and re-read alike
		int added = -1;       // games new to the store this run (the ctl's added=, fork #298); -1 when the
		                      // stamp does not say -- an older ctl's, where cached stood for it;
		                      // AddedUnknown when the ctl said added=unknown (PL-031, below)
		int skipped = 0;      // ROMs RetroAchievements does not know
		int ready = 0;        // games cached in all, after the run
		bool limit = false;   // the proxy's cap was reached
		int indexed = 0;      // of cached, how many came from the interface's index (no hash)
		int errors = 0;       // games a fetch failed for: what makes rc 1 with why=SOME_GAMES_NOT_SAVED
		bool truncated = false; // the walk stopped at the client's cap of files; a second run reaches the rest
		std::string why;
		// added=unknown (audit of the fix round PL-031): the ctl could not
		// read the store before or after a run that cached games, so how
		// many were new was never counted. Not -1: that is an older ctl's
		// stamp, where cached stood for the count; this one has no count
		// to stand in, and the card says how many are ready instead.
		static constexpr int AddedUnknown = -2;   // constexpr: inline, so a use by reference links
	};
	ScanStamp parseScanStamp(const std::string& text);

	// The ctl's why tokens (raofflineproxy-ctl's scan and top-up), in the
	// player's words (es-player-text.md: everyday, not formal): scanWhy for
	// the scan page and the row under SCAN GAMES, topUpWhy for the top-up's
	// card -- the same, except where the scan's sentence sends the player to
	// the scan page, since that card's action line already says it tries
	// again by itself (#308 1-raoffline F-RA-09). A token neither knows is
	// SOMETHING WENT WRONG.
	std::string scanWhy(const std::string& token);
	std::string topUpWhy(const std::string& token);

	// A match that did not complete, on the transfer page's done lines
	// (#308 5-cloud-sync-and-saves gpt F-CS-26, the page's half): what it
	// removed, and the way on.
	std::string matchRemovedNote(int removedFiles);
	std::string matchRecovery();

	// The names the transfer page gives its items, from the ">>> unit" and
	// ">>> tier" labels: the ones EmulationStation composes (the transfer
	// form's parts, the journey's continuation, the startup sync) paired,
	// English and _(""), as whySentences pairs the scripts' whys; a label
	// not listed -- a system's folder -- is shown upper-cased, as it came.
	std::vector<std::pair<std::string, std::string>> unitLabels();
	std::string unitLabel(const std::string& label);
	bool isKnownUnitLabel(const std::string& label);

	// "route=<scan|topup> at=<epoch> index=<i> total=<n> name=<game>" from
	// raofflineproxy-ctl's running file (fork #189): the one line the ctl
	// keeps beside its stamp while a scan or top-up runs its jobs, rewritten
	// as each game finishes and removed when the run ends. It is how the row
	// under SCAN GAMES follows a run nobody pressed -- the top-up at link-up
	// (D-RA-010) or after the startup index (D-RA-013) -- which no job of
	// this process reports. Fields in any order, unknown ones passed over;
	// index, total and name are absent before the first game is known (the
	// ctl is still listing), and name is not kept: the row has no room for
	// it. running is false for an empty line, a missing at, an at the ctl's
	// own bound has passed -- a run that died leaves its file behind, and a
	// file is never a run on its own -- and an at more than a day ahead of
	// now, which is a clock that jumped. The other fields are read whether
	// or not it runs; a count that is not one reads as zero.
	struct RunningProgress
	{
		bool running = false;
		std::string route;    // "scan" or "topup", as the ctl wrote it
		long long at = 0;     // when the ctl last wrote the line, epoch seconds
		int index = 0;        // the ctl's count of games so far; 0 before the first
		int total = 0;        // of how many; 0 until the ctl has counted
	};
	// The ctl's bound on a run (raofflineproxy-ctl), past which its file is
	// a run that died; and how far ahead of now an at may be before it is
	// a clock that jumped rather than one that drifted.
	constexpr long long RUNNING_STALE_AFTER_S = 900;
	constexpr long long RUNNING_AHEAD_LIMIT_S = 86400;
	RunningProgress parseRunningProgress(const std::string& text, long long nowEpoch);
}

#endif // ES_APP_CLOUD_TEXT_H
