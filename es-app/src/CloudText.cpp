#include "CloudText.h"

#include "CloudExit.h"
#include "LocaleES.h"
#include "utils/StringUtil.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace CloudText
{

const std::vector<std::pair<std::string, std::string>>& recommendedProviders()
{
	static const std::vector<std::pair<std::string, std::string>> recommended = {
		{ "dropbox",     "DROPBOX" },
		{ "drive",       "GOOGLE DRIVE" },
		{ "onedrive",    "MICROSOFT ONEDRIVE" },
		{ "box",         "BOX" },
		{ "pcloud",      "PCLOUD" },
		{ "mega",        "MEGA" },
		{ "protondrive", "PROTON DRIVE" },
		{ "koofr",       "KOOFR" },
		{ "webdav",      "WEBDAV" },
		{ "sftp",        "SSH / SFTP" },
		{ "smb",         "WINDOWS SHARE (SMB)" },
		{ "s3",          "AMAZON S3 AND COMPATIBLE" },
		{ "b2",          "BACKBLAZE B2" },
		{ "storj",       "STORJ" },
	};
	return recommended;
}

std::string providerLabel(const std::string& type)
{
	if (type.empty())
		return "";
	for (auto& p : recommendedProviders())
		if (p.first == type)
			return p.second;
	return Utils::String::toUpper(type);
}

std::string providerSubtitle(const std::string& type, const std::string& label)
{
	const std::string text = Utils::String::trim(label);
	// One line's worth at 640 wide in the subtitle font, and no list: a
	// label that enumerates is a description, not a name.
	if (!text.empty() && text.size() <= 40 && text.find(',') == std::string::npos)
		return Utils::String::toUpper(text);
	return providerLabel(type);
}

std::string fieldLabel(const std::string& rcloneName)
{
	// The fields the recommended providers (recommendedProviders above)
	// put in front of a player, as rclone 1.75 names them. Anything a
	// provider adds later falls through to the spaced name below, which
	// is readable if not chosen.
	static const std::vector<std::pair<std::string, std::string>> words = {
		// where
		{ "url",                        "SERVER ADDRESS" },
		{ "host",                       "SERVER ADDRESS" },
		{ "endpoint",                   "ENDPOINT ADDRESS" },
		{ "port",                       "PORT" },
		{ "vendor",                     "SERVER TYPE" },
		{ "provider",                   "PROVIDER" },
		{ "region",                     "REGION" },
		{ "location_constraint",        "LOCATION" },
		{ "tenant",                     "TENANT" },
		{ "domain",                     "DOMAIN" },
		{ "spn",                        "SERVICE NAME" },
		// who
		{ "user",                       "USERNAME" },
		{ "pass",                       "PASSWORD" },
		{ "2fa",                        "TWO-FACTOR CODE" },
		{ "bearer_token",               "ACCESS TOKEN" },
		{ "access_token",               "ACCESS TOKEN" },
		{ "token",                      "SIGN-IN TOKEN" },
		{ "access_key_id",              "ACCESS KEY ID" },
		{ "secret_access_key",          "SECRET ACCESS KEY" },
		{ "env_auth",                   "KEYS FROM THE SYSTEM" },
		{ "account",                    "ACCOUNT ID" },
		{ "key",                        "APPLICATION KEY" },
		{ "client_id",                  "APP ID" },
		{ "client_secret",              "APP SECRET" },
		{ "scope",                      "ACCESS SCOPE" },
		{ "service_account_file",       "SERVICE ACCOUNT FILE" },
		{ "box_config_file",            "BOX CONFIG FILE" },
		{ "box_sub_type",               "ACCOUNT TYPE" },
		{ "drive_type",                 "DRIVE TYPE" },
		{ "use_kerberos",               "USE KERBEROS" },
		// ssh
		{ "key_pem",                    "PRIVATE KEY (PASTED)" },
		{ "key_file",                   "PRIVATE KEY FILE" },
		{ "key_file_pass",              "PRIVATE KEY PASSWORD" },
		{ "pubkey",                     "PUBLIC KEY (PASTED)" },
		{ "pubkey_file",                "PUBLIC KEY FILE" },
		{ "key_use_agent",              "USE SSH AGENT" },
		{ "use_insecure_cipher",        "ALLOW OLD CIPHERS" },
		{ "disable_hashcheck",          "SKIP CHECKSUMS" },
		{ "ssh",                        "SSH COMMAND" },
		// how
		{ "tls",                        "SECURE (IMPLICIT TLS)" },
		{ "explicit_tls",               "SECURE (EXPLICIT TLS)" },
		{ "acl",                        "ACCESS PERMISSIONS" },
		{ "storage_class",              "STORAGE CLASS" },
		{ "bucket_object_lock_enabled", "OBJECT LOCK ON BUCKET" },
	};
	const std::string name = Utils::String::toLower(Utils::String::trim(rcloneName));
	if (name.empty())
		return "";
	for (auto& w : words)
		if (w.first == name)
			return w.second;
	return Utils::String::toUpper(Utils::String::replace(name, "_", " "));
}

std::string cleanHostname(const std::string& in)
{
	std::string out;
	bool gap = false;
	for (char c : in)
	{
		const bool keep = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		if (keep)
		{
			if (gap && !out.empty())
				out += '-';
			gap = false;
			out += c;
		}
		else
			gap = true;
	}
	if (out.size() > 63)
	{
		out.resize(63);
		while (!out.empty() && out.back() == '-')
			out.pop_back();
	}
	return out;
}

bool isOutcomeToken(const std::string& token)
{
	static const std::set<std::string> ourTokens = {
		"completed", "gaps", "no-network", "lock-held", "cancelled", "player-cancelled", "stopped",
		"folder-missing", "cloud-stopped", "cloud-refused", "unknown" };
	return ourTokens.find(token) != ourTokens.cend();
}

// A field that is a whole number and nothing else -- an optional minus,
// then digits -- within [lo, hi]. atoi read "garbage" as 0, and 0 is
// success; a status that cannot be read is not one (#308 F-CS-27).
static bool wholeNumber(const std::string& field, long long lo, long long hi, long long& out)
{
	const std::string f = Utils::String::trim(field);
	size_t i = (!f.empty() && f[0] == '-') ? 1 : 0;
	if (i >= f.size() || f.size() - i > 12)
		return false;
	for (size_t j = i; j < f.size(); j++)
		if (f[j] < '0' || f[j] > '9')
			return false;
	const long long value = atoll(f.c_str());
	if (value < lo || value > hi)
		return false;
	out = value;
	return true;
}

// Every stamp each script can write, whichever flags it ran with: a
// composed command runs cloud_backup twice (saves, then --system-only
// settings), so which stamp a part wrote is the stamp's to say, not the
// command line's. The table used to map cloud_backup to last-backup alone,
// and a stopped settings part left its trap's 130 under the settings row.
std::vector<std::string> scriptStampNames(const std::string& command)
{
	struct Part { const char* script; std::vector<const char*> stamps; };
	static const std::vector<Part> PARTS = {
		{ "cloud_content_backup",  { "last-content-backup" } },
		{ "cloud_content_restore", { "last-content-restore", "last-content-match" } },
		{ "cloud_restore",         { "last-restore", "last-settings-restore" } },
		{ "cloud_backup",          { "last-backup", "last-settings-backup" } },
	};
	std::vector<std::string> names;
	for (const Part& part : PARTS)
		if (command.find(part.script) != std::string::npos)
			for (const char* stamp : part.stamps)
				names.push_back(stamp);
	return names;
}

// The part the stop interrupted, and nothing else: a stamp this run wrote
// with the trap's code. A part that finished before the stop keeps its
// outcome, one that failed on its own keeps its why -- the table and a
// time test used to restamp every stamp the run had written, so a saves
// part that completed read as stopped once the settings part after it
// was. A part that never started keeps its last real run's stamp.
std::vector<std::string> stampsToRestamp(const std::vector<StampText>& stamps, time_t runStarted,
	const std::vector<StampText>* before)
{
	std::vector<std::string> names;
	for (auto& s : stamps)
	{
		const LastRun r = parseLastRun(s.text);
		// The trap's stop: 130, and none of this interface's tokens -- a
		// stamp that carries one was restamped after an earlier stop, and is
		// that run's (audit of the fixes G-E1-06: by the time alone, one
		// written in the second this run began qualified).
		if (!r.ran || r.code != CloudExit::Stopped || r.knownToken)
			continue;
		if (before != nullptr)
		{
			// Written since the run began: the file is not the one read then
			// (the scripts write a new file and rename it), whatever the
			// clock -- a device booting with its clock behind made
			// yesterday's stop look like today's (G-E1-04, the claude seat).
			std::string was;
			for (auto& b : *before)
				if (b.name == s.name)
					was = b.version;
			if (!s.version.empty() && s.version != was)
				names.push_back(s.name);
		}
		else if (r.when >= runStarted)
			names.push_back(s.name);
	}
	return names;
}

std::string offlinePartWayWhy(const std::vector<StampText>& stamps, const std::vector<StampText>& before)
{
	for (auto& s : stamps)
	{
		const LastRun r = parseLastRun(s.text);
		if (!r.ran || r.code != CloudExit::NoNetwork || r.token != "gaps")
			continue;
		// Written since the run began, as stampsToRestamp tells one: the
		// scripts write each stamp to a new file and rename it into place.
		std::string was;
		for (auto& b : before)
			if (b.name == s.name)
				was = b.version;
		if (s.version.empty() || s.version == was)
			continue;
		// The scripts write the sentence with spaces after the token; one
		// written with the underscores of their other whys reads the same.
		const std::string why = Utils::String::toUpper(Utils::String::replace(r.why, "_", " "));
		return why.empty() ? std::string("YOU WENT OFFLINE PART-WAY THROUGH") : why;
	}
	return "";
}

bool exitSyncOwed(const std::string& exitStamp, const std::string& startupStamp, const std::string& backupStamp)
{
	// Skipped for no network; or cut by the network part-way, the card's
	// stamp carrying gaps with the 69 since the audit of the fix round
	// (claude G2-A-03): the rest of the saves are owed all the same.
	const LastRun e = parseLastRun(exitStamp);
	if (!e.ran || !(e.token == "no-network" || (e.token == "gaps" && e.code == CloudExit::NoNetwork)))
		return false;
	const LastRun s = parseLastRun(startupStamp);
	if (s.ran && s.when >= e.when && s.outcome == Outcome::Completed)
		return false;
	const LastRun b = parseLastRun(backupStamp);
	if (b.ran && b.when >= e.when && b.code == 0)
		return false;
	return true;
}

LastRun parseLastRun(const std::string& text)
{
	LastRun r;
	auto parts = Utils::String::split(Utils::String::trim(text), ' ', true);
	if (parts.size() < 2)
		return r;
	// Both fields whole numbers, or this is not a stamp: the epoch positive,
	// the code an exit status (-1 where there was none) (#308 F-CS-27).
	long long epoch = 0, status = 0;
	if (!wholeNumber(parts[0], 1, 99999999999LL, epoch) || !wholeNumber(parts[1], -1, 255, status))
		return r;
	time_t when = (time_t) epoch;
	r.ran = true;
	r.when = when;
	// "<epoch> <rc>[ <token>[ <why...>]]" (D-UI-028). The first two fields
	// are what every stamp has always carried; the token is one word for
	// the outcome where the code alone cannot say it (a 130 that was a
	// launch cancel; a composed run whose parts disagreed), and the why
	// is the scripts' own sentence when they printed one. A stamp with two
	// fields -- one written before this reader -- is read from its code.
	//
	// The four words, with commas rather than dashes inside the outcome:
	// the line already uses a dash to separate the date from it. LockHeld
	// and NoNetwork are the scripts' "another sync holds the lock" and "no
	// network" (CloudExit.h): the scripts write no stamp for those (nothing
	// ran), but the whole-run stamps EmulationStation keeps per cause
	// (last-sync-startup, -exit, -manual; fork #94) do, because under SYNC
	// SAVES DURING STARTUP the player's question is what happened this
	// morning, and "nothing, there was no network" answers it.
	const int code = (int) status;
	r.code = code;
	//
	// Two writers, two shapes of third field. EmulationStation's stamps
	// (ThreadedCloudSync::recordOutcome) carry one of its tokens, with the
	// scripts' sentence after it when they printed one; the scripts' own
	// stamps (last-backup, last-restore, last-content-*) carry the sentence
	// itself as the third field, spaces turned into underscores
	// ("1789000000 5 YOUR_CLOUD_STOPPED_ANSWERING"), and only for a code
	// that is not 0, 9, 69 or 75. So a third field that is not one of our
	// tokens is the why, read back with its underscores as spaces.
	const std::string token = parts.size() > 2 ? parts[2] : "";
	const bool ours = isOutcomeToken(token);
	std::string why;
	for (size_t i = ours ? 3 : 2; i < parts.size(); i++)
		why += (why.empty() ? "" : " ") + parts[i];
	if (!ours)
		why = Utils::String::toUpper(Utils::String::replace(why, "_", " "));
	while (!why.empty() && why.back() == '.')
		why.pop_back();

	r.token = token;
	r.knownToken = ours;
	if (code == 0 || code == 9 || token == "completed")
	{
		r.outcome = Outcome::Completed;
		r.finished = true;
	}
	else if (token == "gaps")
	{
		// A run whose parts disagreed. The stamp keeps its own token so a log
		// can tell it from a total failure; the row says what every other
		// failure says (D-UI-030).
		r.outcome = Outcome::Gaps;
		r.why = why;
	}
	else if (code == CloudExit::LockHeld)
		r.outcome = Outcome::SkippedLockHeld;
	else if (code == CloudExit::NoNetwork)
		r.outcome = Outcome::SkippedNoNetwork;
	else if (token == "cancelled")
		r.outcome = Outcome::SkippedGameStarted;
	else if (token == "player-cancelled")
		r.outcome = Outcome::SkippedCancelled;
	else
	{
		r.outcome = Outcome::Failed;
		r.why = why;
	}
	return r;
}

RunOrigin runOrigin(time_t when, time_t exitWhen, time_t startupWhen)
{
	if (when <= 0)
		return RunOrigin::None;
	if (exitWhen > 0 && std::labs((long) (exitWhen - when)) <= 10)
		return RunOrigin::AfterLastGame;
	if (startupWhen > 0 && std::labs((long) (startupWhen - when)) <= 10)
		return RunOrigin::AtStartup;
	return RunOrigin::None;
}

std::string shortenWhy(const std::string& why)
{
	const size_t dash = why.find(" - ");
	if (dash != std::string::npos)
		return Utils::String::trim(why.substr(0, dash));

	const size_t paren = why.find(" (");
	if (paren != std::string::npos)
		return Utils::String::trim(why.substr(0, paren));

	const size_t stop = why.find(". ");
	if (stop != std::string::npos)
		return Utils::String::trim(why.substr(0, stop));

	return "";
}

std::vector<std::string> outcomeCandidates(const std::string& outcome)
{
	std::vector<std::string> candidates;
	candidates.push_back(outcome);

	const size_t dash = outcome.find(" - ");
	if (dash != std::string::npos)
	{
		const std::string head = outcome.substr(0, dash);
		const std::string tail = outcome.substr(dash + 3);
		const std::string shortTail = shortenWhy(tail);
		if (!shortTail.empty() && shortTail != tail)
			candidates.push_back(head + std::string(" - ") + shortTail);
		candidates.push_back(head);
	}

	return candidates;
}

std::vector<std::pair<std::string, std::string>> whySentences()
{
	// Each sentence twice: as the script prints it, to match, and inside
	// _(""), so xgettext carries it into the catalog and the French in
	// locale/lang/fr reaches the card. Built on each call, after the
	// language is chosen. The list is the emitter table in
	// es-app/tests/unit/CloudTextTests.cpp, which fails on a why a script
	// prints that is not here; a sentence a script changes wants its pair
	// changed in the same change, or the card shows it in English again.
	return {
		{ "YOUR CLOUD STORAGE ISN'T SET UP YET", _("YOUR CLOUD STORAGE ISN'T SET UP YET") },
		{ "COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN", _("COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN") },
		{ "YOUR CLOUD STOPPED ANSWERING", _("YOUR CLOUD STOPPED ANSWERING") },
		{ "YOUR CLOUD SYNC SETTINGS COULDN'T BE READ", _("YOUR CLOUD SYNC SETTINGS COULDN'T BE READ") },
		{ "YOUR CLOUD SYNC SETTINGS COULDN'T BE SAVED", _("YOUR CLOUD SYNC SETTINGS COULDN'T BE SAVED") },
		{ "CHECK WHAT WOULD CHANGE FIRST", _("CHECK WHAT WOULD CHANGE FIRST") },
		{ "AN OLD FOLDER SETTING IS IN THE WAY", _("AN OLD FOLDER SETTING IS IN THE WAY") },
		{ "YOUR SAVES FOLDER ISN'T ON THIS DEVICE", _("YOUR SAVES FOLDER ISN'T ON THIS DEVICE") },
		{ "THIS DEVICE'S SETTINGS BACKUP IS DAMAGED", _("THIS DEVICE'S SETTINGS BACKUP IS DAMAGED") },
		{ "THE COPY IN YOUR CLOUD ISN'T COMPLETE", _("THE COPY IN YOUR CLOUD ISN'T COMPLETE") },
		{ "COULDN'T FIND YOUR CLOUD FOLDER", _("COULDN'T FIND YOUR CLOUD FOLDER") },
		{ "SOME FILES DIDN'T FINISH", _("SOME FILES DIDN'T FINISH") },
		{ "YOUR CLOUD WOULDN'T TAKE THE FILES", _("YOUR CLOUD WOULDN'T TAKE THE FILES") },
		{ "IT WAS STOPPED", _("IT WAS STOPPED") },
		{ "THE CLOUD TOOK TOO LONG - IT'LL TRY AGAIN NEXT TIME", _("THE CLOUD TOOK TOO LONG - IT'LL TRY AGAIN NEXT TIME") },
		{ "SOMETHING WENT WRONG", _("SOMETHING WENT WRONG") },
		{ "COULDN'T TELL WHICH CARD YOUR SAVES ARE ON", _("COULDN'T TELL WHICH CARD YOUR SAVES ARE ON") },
		{ "YOUR SAVES ARE ON A DIFFERENT CARD", _("YOUR SAVES ARE ON A DIFFERENT CARD") },
		{ "YOUR SAVES CHANGED CARDS PART-WAY THROUGH", _("YOUR SAVES CHANGED CARDS PART-WAY THROUGH") },
		{ "THERE'S NO SETTINGS BACKUP ON THIS DEVICE YET", _("THERE'S NO SETTINGS BACKUP ON THIS DEVICE YET") },
		{ "COULDN'T KEEP A COPY OF YOUR CURRENT SETTINGS", _("COULDN'T KEEP A COPY OF YOUR CURRENT SETTINGS") },
		{ "THE RESTORE COULDN'T FINISH", _("THE RESTORE COULDN'T FINISH") },
		{ "THIS DEVICE CAN'T MAKE A SETTINGS BACKUP", _("THIS DEVICE CAN'T MAKE A SETTINGS BACKUP") },
		{ "THE BACKUP COULDN'T FINISH WHILE GATHERING YOUR SETTINGS", _("THE BACKUP COULDN'T FINISH WHILE GATHERING YOUR SETTINGS") },
		{ "THE BACKUP COULDN'T FINISH", _("THE BACKUP COULDN'T FINISH") },
		// Added with the table at next 4476f90394 (#308 follow-up): stream
		// A's match, card record and layout migration, the stamp's offline
		// why (a 69 with the gaps token, read by the rows), and backuptool's
		// five since f0f263b8cc.
		{ "SOMETHING CHANGED SINCE YOU CHECKED", _("SOMETHING CHANGED SINCE YOU CHECKED") },
		{ "COULDN'T RECORD WHICH CARD YOUR SAVES ARE ON", _("COULDN'T RECORD WHICH CARD YOUR SAVES ARE ON") },
		{ "THE NEW FOLDER ALREADY HAS FILES IN IT", _("THE NEW FOLDER ALREADY HAS FILES IN IT") },
		{ "YOU WENT OFFLINE PART-WAY THROUGH", _("YOU WENT OFFLINE PART-WAY THROUGH") },
		{ "THIS DEVICE CAN'T RESTORE SETTINGS", _("THIS DEVICE CAN'T RESTORE SETTINGS") },
		{ "A SETTINGS BACKUP OR RESTORE IS ALREADY RUNNING", _("A SETTINGS BACKUP OR RESTORE IS ALREADY RUNNING") },
		{ "THERE'S NOTHING TO BACK UP YET", _("THERE'S NOTHING TO BACK UP YET") },
		{ "A SIGN-IN WAS FOUND IN THE BACKUP", _("A SIGN-IN WAS FOUND IN THE BACKUP") },
		{ "YOUR OWN BACKUP LIST NAMES A FOLDER A BACKUP CAN'T CARRY", _("YOUR OWN BACKUP LIST NAMES A FOLDER A BACKUP CAN'T CARRY") },
	};
}

std::string localizedWhy(const std::string& why)
{
	for (auto& sentence : whySentences())
		if (sentence.first == why)
			return sentence.second;
	return why;
}

bool isKnownWhy(const std::string& why)
{
	for (auto& sentence : whySentences())
		if (sentence.first == why)
			return true;
	return false;
}

std::string cleanLine(const std::string& raw)
{
	// The transfer page's rule (CloudTransferJob::cleanLine, #85), which the
	// card did not share: it kept printable ASCII only, and a folder name in
	// the player's language lost its accented letters on the way to the
	// offer (#308 F-CS-19). A terminal escape is an instruction to a
	// terminal that is not here, and goes whole -- and only itself, by its
	// own grammar (ECMA-48), not "to the next letter", which swallowed the
	// start of a protocol line after a window title or a two-byte escape
	// (audit of the fixes, claude G-E1-07):
	//   ESC [ ...   CSI: parameter and intermediate bytes, then one final
	//               byte in 0x40-0x7E (what rclone prints);
	//   ESC ] ...   OSC (and P, X, ^, _ strings): to BEL, or ESC and a backslash;
	//   ESC <byte>  any other escape: that one byte, when it is ASCII;
	//   ESC         before a UTF-8 byte or at the end: the ESC alone.
	std::string clean;
	for (size_t i = 0; i < raw.size(); ++i)
	{
		const unsigned char c = (unsigned char) raw[i];
		if (c == 0x1B)
		{
			if (i + 1 >= raw.size())
				break;
			const unsigned char kind = (unsigned char) raw[i + 1];
			if (kind == '[')
			{
				size_t j = i + 2;
				while (j < raw.size() && ((unsigned char) raw[j] < 0x40 || (unsigned char) raw[j] > 0x7E)
					&& (unsigned char) raw[j] >= 0x20)
					j++;
				i = j;   // the final byte, skipped by the loop's ++i
			}
			else if (kind == ']' || kind == 'P' || kind == 'X' || kind == '^' || kind == '_')
			{
				size_t j = i + 2;
				while (j < raw.size() && raw[j] != '\x07' && !(raw[j] == 0x1B && j + 1 < raw.size() && raw[j + 1] == '\\'))
					j++;
				// at BEL, or at the backslash of the ESC-backslash that ends it
				i = (j < raw.size() && raw[j] == 0x1B) ? j + 1 : j;
			}
			else if (kind >= 0x20 && kind < 0x7F)
				i++;       // a two-byte escape
			continue;      // before a UTF-8 byte or a control: the ESC alone
		}
		if ((c >= 32 && c < 127) || c >= 0x80)
			clean += (char) c;
	}
	return Utils::String::trim(clean);
}

std::vector<std::string> actionCandidates(const std::string& inPlace,
	const std::vector<std::string>& recoveries, bool keepInPlace)
{
	std::vector<std::string> out;
	if (keepInPlace && !inPlace.empty())
	{
		// The recovery gives way instead (#307 PL-072): each of its forms
		// with the in-place clause, then the clause alone.
		for (auto& r : recoveries)
			out.push_back(inPlace + " " + r);
		out.push_back(inPlace);
		return out;
	}
	if (!inPlace.empty() && !recoveries.empty())
		out.push_back(inPlace + " " + recoveries.front());
	for (auto& r : recoveries)
		out.push_back(r);
	return out;
}

ProtocolLine classifyProtocolLine(const std::string& clean)
{
	ProtocolLine out;
	if (clean.rfind(">>> ", 0) != 0)
		return out;

	if (clean.rfind(">>> pid ", 0) == 0)
	{
		out.kind = ProtocolKind::Pid;
		out.number = atoi(clean.substr(8).c_str());
	}
	else if (clean.rfind(">>> doing ", 0) == 0)
	{
		out.kind = ProtocolKind::Doing;
		out.text = Utils::String::trim(clean.substr(10));
	}
	else if (clean.rfind(">>> why ", 0) == 0)
	{
		out.kind = ProtocolKind::Why;
		std::string why = Utils::String::toUpper(Utils::String::trim(clean.substr(8)));
		// The outcome line supplies its own end; a sentence's
		// full stop after a dash reads as a typo.
		while (!why.empty() && why.back() == '.')
			why.pop_back();
		out.text = why;
	}
	else if (clean.rfind(">>> offer ", 0) == 0)
	{
		out.kind = ProtocolKind::Offer;
		auto parts = Utils::String::split(clean.substr(10), '|', false);
		out.text = parts.size() > 0 ? Utils::String::trim(parts[0]) : "";
		for (size_t i = 1; i < parts.size(); i++)
			out.args.push_back(Utils::String::trim(parts[i]));
	}
	else if (clean.rfind(">>> tier ", 0) == 0)
	{
		out.kind = ProtocolKind::Tier;
		auto parts = Utils::String::split(clean.substr(9), '|', false);
		out.text = parts.size() > 0 ? Utils::String::toUpper(Utils::String::trim(parts[0])) : "";
		// The part's exit status, 0-255; anything else -- nothing, junk -- is
		// -1, which is not a success and not one of rclone's (#308 F-CS-27).
		long long code = -1;
		out.number = (parts.size() > 1 && wholeNumber(parts[1], 0, 255, code)) ? (int) code : -1;
	}
	// ">>> unit nes|2|5" -- an item starts: a system, or a phase whose
	// counts are empty ("SAVES||"). The label keeps the case it came in,
	// because the reader that counts items compares one announcement with
	// the next; the empty fields are zero, which is the reader's "the
	// script did not say".
	else if (clean.rfind(">>> unit ", 0) == 0)
	{
		out.kind = ProtocolKind::Unit;
		auto parts = Utils::String::split(clean.substr(9), '|', false);
		out.text   = parts.size() > 0 ? Utils::String::trim(parts[0]) : "";
		out.number = parts.size() > 1 ? atoi(Utils::String::trim(parts[1]).c_str()) : 0;
		out.count  = parts.size() > 2 ? atoi(Utils::String::trim(parts[2]).c_str()) : 0;
	}
	// ">>> removed 14|314572800|snes:12:300000000,gb:2:14572800" -- what a
	// match took off this device, in total and per system. A match cut off
	// by the network prints it before exiting, so what had already gone can
	// still be said.
	else if (clean.rfind(">>> removed ", 0) == 0)
	{
		out.kind = ProtocolKind::Removed;
		auto parts = Utils::String::split(clean.substr(12), '|', false);
		out.files = parts.size() > 0 ? atol(Utils::String::trim(parts[0]).c_str()) : 0;
		out.bytes = parts.size() > 1 ? atol(Utils::String::trim(parts[1]).c_str()) : 0;
		if (parts.size() > 2)
		{
			for (auto& item : Utils::String::split(Utils::String::trim(parts[2]), ',', true))
			{
				auto f = Utils::String::split(item, ':', false);
				// A system and a count are what makes an item; anything
				// shorter is a torn field, and a system named with no
				// number beside it is worse than one not named at all.
				if (f.size() < 2)
					continue;
				RemovedSystem sys;
				sys.system = Utils::String::toUpper(Utils::String::trim(f[0]));
				sys.files  = Utils::String::trim(f[1]);
				sys.bytes  = f.size() > 2 ? atol(Utils::String::trim(f[2]).c_str()) : 0;
				out.systems.push_back(sys);
			}
		}
	}
	else
		out.kind = ProtocolKind::Unknown;

	return out;
}

Verb verbOf(const std::string& cmd)
{
	const bool restore = cmd.find("cloud_restore") != std::string::npos;
	const bool backup  = cmd.find("cloud_backup")  != std::string::npos || cmd.find("backuptool") != std::string::npos;
	if (restore && backup) return Verb::Sync;
	if (restore) return Verb::Restore;
	if (backup)  return Verb::Backup;
	return Verb::Other;
}

TransferKind transferKind(const std::string& cmd)
{
	if (cmd.find("--match") != std::string::npos)
		return TransferKind::Match;
	const bool restore = cmd.find("cloud_restore") != std::string::npos || cmd.find("cloud_content_restore") != std::string::npos;
	const bool backup  = cmd.find("cloud_backup")  != std::string::npos || cmd.find("cloud_content_backup")  != std::string::npos;
	if (restore && !backup) return TransferKind::Restore;
	if (backup && !restore) return TransferKind::Backup;
	return TransferKind::Other;
}

std::string chooseThatFits(const std::vector<std::string>& candidates, float width,
	const std::function<float(const std::string&)>& measure)
{
	std::string shown;

	for (auto& candidate : candidates)
	{
		shown = candidate;
		if (width <= 0.0f || !measure || measure(candidate) <= width)
			break;
	}

	return shown;
}

const std::vector<RcloneUnit>& rcloneUnits()
{
	static const std::vector<RcloneUnit> units = {
		{ "TiB", "TB",  1024.0 * 1024 * 1024 * 1024 },
		{ "GiB", "GB",  1024.0 * 1024 * 1024 },
		{ "MiB", "MB",  1024.0 * 1024 },
		{ "KiB", "KB",  1024.0 },
		{ "Ti",  " TB", 1024.0 * 1024 * 1024 * 1024 },
		{ "Gi",  " GB", 1024.0 * 1024 * 1024 },
		{ "Mi",  " MB", 1024.0 * 1024 },
		{ "Ki",  " KB", 1024.0 },
		{ "B",   "B",   1.0 },
	};
	return units;
}

long parseBytes(const std::string& field)
{
	const std::string t = Utils::String::trim(field);
	char* end = nullptr;
	const double v = strtod(t.c_str(), &end);
	if (end == t.c_str() || !std::isfinite(v) || v < 0)
		return -1;
	const std::string unit = Utils::String::trim(std::string(end));
	for (const auto& u : rcloneUnits())
		if (unit == u.rclone)
			return (long) (v * u.bytes + 0.5);
	return -1;
}

std::string sizeLabel(unsigned long bytes)
{
	char buf[32];
	const unsigned long kb = (bytes + 1023UL) / 1024UL;
	const double mb = bytes / (1024.0 * 1024.0);
	if (kb < 1024UL)
		snprintf(buf, sizeof(buf), "%lu KB", kb);
	else if (mb < 1023.95)
		snprintf(buf, sizeof(buf), "%.1f MB", mb);
	else
		snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
	return buf;
}

std::string roundSizes(const std::string& f)
{
	std::string out;
	size_t i = 0;
	while (i < f.size())
	{
		// a number starts at a digit that does not continue a token ("3m2s")
		const bool starts = isdigit((unsigned char) f[i]) && (i == 0 || !(isalnum((unsigned char) f[i - 1]) || f[i - 1] == '.'));
		if (!starts)
		{
			out += f[i++];
			continue;
		}
		size_t j = i;
		while (j < f.size() && (isdigit((unsigned char) f[j]) || f[j] == '.'))
			j++;
		size_t k = j;
		while (k < f.size() && f[k] == ' ')
			k++;
		const RcloneUnit* unit = nullptr;
		for (const auto& u : rcloneUnits())
		{
			const std::string spelt = u.rclone;
			if (f.compare(k, spelt.size(), spelt) == 0 && (k + spelt.size() == f.size() || !isalpha((unsigned char) f[k + spelt.size()])))
			{
				unit = &u;
				break;
			}
		}
		const size_t end = unit == nullptr ? j : k + std::string(unit->rclone).size();
		const long bytes = unit == nullptr ? -1 : parseBytes(f.substr(i, end - i));
		out += bytes < 0 ? f.substr(i, end - i) : sizeLabel((unsigned long) bytes);
		i = end;
	}
	return out;
}

namespace
{
	// "<a> / <b>[, <pct>%[, <speed>]]" -- the body of a Transferred: or
	// Checks: line once its label is gone. False when there is no pair.
	bool readPair(const std::string& body, std::string& a, std::string& b, int& percent)
	{
		const auto fields = Utils::String::split(body, ',', true);
		if (fields.empty())
			return false;
		const std::string pair = Utils::String::trim(fields[0]);
		const auto slash = pair.find(" / ");
		if (slash == std::string::npos)
			return false;
		a = Utils::String::trim(pair.substr(0, slash));
		b = Utils::String::trim(pair.substr(slash + 3));
		percent = -1;
		for (size_t i = 1; i < fields.size(); i++)
		{
			const std::string t = Utils::String::trim(fields[i]);
			if (t.empty() || t.back() != '%')
				continue;
			const std::string digits = t.substr(0, t.size() - 1);
			if (!digits.empty() && digits.find_first_not_of("0123456789") == std::string::npos)
			{
				const int value = atoi(digits.c_str());
				if (value >= 0 && value <= 100)
					percent = value;
			}
			break;
		}
		return !a.empty() && !b.empty();
	}

	bool allDigits(const std::string& s)
	{
		return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
	}
}

LiveLine liveLine(const std::string& clean)
{
	LiveLine out;
	static const std::string XFER = "Transferred:";
	static const std::string CHECKS = "Checks:";

	const size_t xfer = clean.rfind(XFER);
	const size_t checks = clean.rfind(CHECKS);

	if (xfer != std::string::npos && (checks == std::string::npos || xfer > checks))
	{
		std::string body = Utils::String::trim(clean.substr(xfer + XFER.size()));
		// speed and ETA are for a terminal; the card has a bar
		const auto eta = body.find(", ETA ");
		if (eta != std::string::npos)
			body = body.substr(0, eta);
		std::string a, b;
		int percent = -1;
		if (!readPair(body, a, b, percent))
			return out;
		if (allDigits(a) && allDigits(b))
		{
			// the count line has no unit: "0 / 3, 0%"
			out.kind = LiveLine::Kind::Files;
			out.sent = atol(a.c_str());
			out.total = atol(b.c_str());
			return out;
		}
		const long sent = parseBytes(a);
		const long total = parseBytes(b);
		if (sent < 0 || total < 0)
			return out;
		out.kind = LiveLine::Kind::Bytes;
		out.sent = sent;
		out.total = total;
		out.percent = percent;
		return out;
	}

	if (checks != std::string::npos)
	{
		// rclone's check counter -- "Checks: 12 / 70, 17%, Listed 313" --
		// is a comparison, not a transfer, and its percentage is not the
		// bar's: drawn as one it reads as seventy uploads.
		std::string a, b;
		int percent = -1;
		if (!readPair(Utils::String::trim(clean.substr(checks + CHECKS.size())), a, b, percent))
			return out;
		if (!allDigits(a) || !allDigits(b))
			return out;
		out.kind = LiveLine::Kind::Checks;
		out.sent = atol(a.c_str());
		out.total = atol(b.c_str());
		return out;
	}

	// A per-file line (" * name: 32% /292.969Ki, 95.996Ki/s, 2s") is
	// written for a log; the card's title already says what is moving.
	if (clean.rfind("* ", 0) == 0)
		return out;

	// Anything else carries progress if it has a percentage or an "x / y"
	// count, and is shown as it came. rclone's headers, the elapsed time
	// and the scripts' banners have neither, and stay off the card.
	if (clean.find('%') != std::string::npos || clean.find(" / ") != std::string::npos)
	{
		out.kind = LiveLine::Kind::Other;
		out.text = clean;
	}
	return out;
}

LiveWords liveWords(const LiveLine& live, bool bytesMoving, bool countShown)
{
	switch (live.kind)
	{
	case LiveLine::Kind::Bytes:
		// A total means rclone has queued something to move; bytes sent mean
		// it is moving. Either way the byte line is the fact. "0 B / 0 B" is
		// the listing and the compare: progress, said as such -- unless the
		// count already says it with a number.
		if (live.sent > 0 || live.total > 0)
			return LiveWords::Bytes;
		return countShown ? LiveWords::Keep : LiveWords::Comparing;
	case LiveLine::Kind::Checks:
		// Not once this half's bytes are moving: the count follows the bytes
		// in every block, so it used to hold the words for the whole second
		// until the next block while the bar moved underneath. And not before
		// rclone has a total to count against: "0 / 0, -, Listed 40" is a
		// listing still under way (#157).
		return (!bytesMoving && live.total > 0) ? LiveWords::ComparingCount : LiveWords::Keep;
	case LiveLine::Kind::Other:
		return LiveWords::Other;
	case LiveLine::Kind::Files:
	case LiveLine::Kind::None:
	default:
		return LiveWords::Keep;
	}
}

long fileInFlight(long filesDone, long filesTotal)
{
	if (filesTotal <= 0)
		return 0;
	const long next = (filesDone < 0 ? 0 : filesDone) + 1;
	return next > filesTotal ? filesTotal : next;
}

Phase phaseOf(const std::string& doingWord)
{
	const std::string word = Utils::String::toLower(Utils::String::trim(doingWord));
	if (word == "receive")
		return Phase::Receiving;
	if (word == "send")
		return Phase::Sending;
	return Phase::None;
}

int phaseBar(Phase phase, int percentInPhase)
{
	const int p = percentInPhase > 100 ? 100 : percentInPhase;
	switch (phase)
	{
		case Phase::Receiving: return p < 0 ? 0 : p / 2;
		case Phase::Sending:   return p < 0 ? 50 : 50 + p / 2;
		default:               return p < 0 ? -1 : p;
	}
}

int forwardOnly(int shown, int proposed)
{
	return proposed > shown ? proposed : shown;
}

NetworkStepChoice networkStep(bool linkUp, const std::string& command)
{
	NetworkStepChoice choice;
	choice.step = linkUp ? NetworkStep::Checking : NetworkStep::Waiting;
	choice.waitSeconds = NETWORK_WAIT_DEFAULT_S;

	// The first --wait is the script's: the startup command names it once,
	// on the cloud_net_ready line, and nothing after it is ours.
	static const std::string FLAG = "--wait";
	const size_t at = command.find(FLAG);
	if (at == std::string::npos)
		return choice;
	size_t i = at + FLAG.size();
	if (i < command.size() && command[i] == '=')
		i++;
	else
		while (i < command.size() && command[i] == ' ')
			i++;
	size_t end = i;
	while (end < command.size() && command[end] >= '0' && command[end] <= '9')
		end++;
	// A number of the size a wait can be, and nothing glued to it -- the
	// script's own parser refuses anything else (exit 64), so the default
	// stands rather than a bound nobody set.
	const size_t digits = end - i;
	if (digits == 0 || digits > 6)
		return choice;
	if (end < command.size() && (isalnum((unsigned char) command[end]) || command[end] == '_' || command[end] == '-'))
		return choice;
	choice.waitSeconds = atoi(command.substr(i, digits).c_str());
	return choice;
}

} // namespace CloudText

// ---------------------------------------------------------------- offline achievements

int CloudText::parsePendingCount(const std::string& text)
{
	const std::string t = Utils::String::trim(text);
	if (t.empty() || t.size() > 9)
		return -1;
	for (char c : t)
		if (c < '0' || c > '9')
			return -1;
	return std::stoi(t);
}

CloudText::FlushStamp CloudText::parseFlushStamp(const std::string& text)
{
	FlushStamp stamp;

	// One line, two fields, both whole numbers, the count above zero.
	std::string line = text;
	const size_t newline = line.find('\n');
	if (newline != std::string::npos)
		line = line.substr(0, newline);
	const std::vector<std::string> fields = Utils::String::split(Utils::String::trim(line), ' ', true);
	if (fields.size() != 2)
		return stamp;
	for (const std::string& f : fields)
	{
		if (f.empty() || f.size() > 12)
			return stamp;
		for (char c : f)
			if (c < '0' || c > '9')
				return stamp;
	}
	const long long when = std::stoll(fields[0]);
	const int flushed = fields[1].size() > 9 ? 0 : std::stoi(fields[1]);
	if (when <= 0 || flushed <= 0)
		return stamp;

	stamp.ok = true;
	stamp.when = (time_t) when;
	stamp.flushed = flushed;
	return stamp;
}

// Moved from OfflineAchievements::scanWhy and ProxyCards' topUpWhy, so the
// table has a case (#308 follow-up): those two now ask here.
std::string CloudText::scanWhy(const std::string& token)
{
	if (token == "TOGGLE_OFF")
		return _("TURN ON OFFLINE ACHIEVEMENTS FIRST.");
	if (token == "NO_ACCOUNT")
		return _("SIGN IN TO RETROACHIEVEMENTS FIRST.");
	if (token == "SIGN_IN_REFUSED")
		return _("RETROACHIEVEMENTS DIDN'T ACCEPT YOUR SIGN-IN");
	if (token == "RETROACHIEVEMENTS_STOPPED_ANSWERING")
		return _("RETROACHIEVEMENTS STOPPED ANSWERING");
	if (token == "NO_GAMES_FOUND")
		return _("NO GAMES WERE FOUND ON THIS CONSOLE");
	if (token == "LIBRARY_UNREADABLE")
		return _("YOUR GAMES COULDN'T BE READ");
	if (token == "TOOK_TOO_LONG")
		return _("IT TOOK TOO LONG");
	// A fetch failed for some game, even among many that went through
	// (audit #186 PL-24): the run could not finish, and the next scan tries
	// those games again, since nothing marks them cached.
	if (token == "SOME_GAMES_NOT_SAVED")
		return _("SOME GAMES COULDN'T BE SAVED. TRY THE SCAN AGAIN.");
	// The image pass left achievement images behind (#307 PL-060; the ctl
	// stamps it only for a failure that may pass, so a scan can fetch them):
	// the games are cached, and the next scan tries the images again.
	// Proposed words, for the maintainer to approve.
	if (token == "SOME_IMAGES_NOT_SAVED")
		return _("SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED. TRY THE SCAN AGAIN.");
	// The player's CANCEL on the scan page (D-UI-078): the ctl's INT trap
	// stamps it so the row says so; not a failure, and the next scan
	// carries on from what was saved.
	if (token == "CANCELLED")
		return _("YOU CANCELLED IT");
	return _("SOMETHING WENT WRONG");
}

std::string CloudText::topUpWhy(const std::string& token)
{
	if (token == "SOME_GAMES_NOT_SAVED")
		return _("SOME GAMES COULDN'T BE SAVED");
	if (token == "SOME_IMAGES_NOT_SAVED")
		return _("SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED");
	return scanWhy(token);
}

// What a match that did not complete removed. The count is the one
// cloud_content_restore's ">>> removed" reports from rclone's own Deleted
// lines (stream A's 258b5eca38), no longer its plan. Nothing about the
// cloud: a match removes only what the cloud does not have (D-CLOUD-023),
// and the clause that followed -- YOUR CLOUD STILL HAS THEM -- said the
// opposite of what had happened.
std::string CloudText::matchRemovedNote(int removedFiles)
{
	if (removedFiles <= 0)
		return _("NOTHING WAS REMOVED FROM THIS DEVICE.");
	if (removedFiles == 1)
		return _("1 FILE WAS REMOVED FROM THIS DEVICE.");
	return Utils::String::format(_("%d FILES WERE REMOVED FROM THIS DEVICE.").c_str(), removedFiles);
}

// The way on, named as the card names its row (TRY AGAIN: <row>): an apply
// spends its preview's plan (PL-001), so the same command again is refused
// with SOMETHING CHANGED SINCE YOU CHECKED; the match's own row checks
// again first.
std::string CloudText::matchRecovery()
{
	return _("TRY AGAIN: MATCH THIS DEVICE TO THE CLOUD");
}

// The composers' labels (the case in CloudTextTests lists where each is
// printed). The page knew SETTINGS and SAVES alone, and a French page named
// its other parts in English (audit of the fixes, E2 claude G-E2-08).
std::vector<std::pair<std::string, std::string>> CloudText::unitLabels()
{
	return {
		{ "SETTINGS", _("SETTINGS") },
		{ "SAVES", _("SAVES") },
		{ "ROMS AND BIOS", _("ROMS AND BIOS") },
		{ "GAME CONTENT", _("GAME CONTENT") },
		{ "ROMS, BIOS, AND GAME CONTENT", _("ROMS, BIOS, AND GAME CONTENT") },
		{ "RESTORING SAVES", _("RESTORING SAVES") },
		{ "BACKING UP SAVES", _("BACKING UP SAVES") },
		{ "RESTORING ROMS AND BIOS", _("RESTORING ROMS AND BIOS") },
	};
}

std::string CloudText::unitLabel(const std::string& label)
{
	const std::string upper = Utils::String::toUpper(label);
	for (auto& l : unitLabels())
		if (l.first == upper)
			return l.second;
	return upper;
}

bool CloudText::isKnownUnitLabel(const std::string& label)
{
	for (auto& l : unitLabels())
		if (l.first == label)
			return true;
	return false;
}

CloudText::ScanStamp CloudText::parseScanStamp(const std::string& text)
{
	ScanStamp stamp;

	// One line: the time, the exit code and the route, then key=value
	// fields in any order. A whole number is at most twelve digits, as the
	// flush stamp's are.
	std::string line = text;
	const size_t newline = line.find('\n');
	if (newline != std::string::npos)
		line = line.substr(0, newline);
	const std::vector<std::string> fields = Utils::String::split(Utils::String::trim(line), ' ', true);
	if (fields.size() < 3)
		return stamp;

	auto number = [](const std::string& f, long long& out) -> bool
	{
		if (f.empty() || f.size() > 12)
			return false;
		for (char c : f)
			if (c < '0' || c > '9')
				return false;
		out = std::stoll(f);
		return true;
	};

	long long when = 0, code = 0;
	if (!number(fields[0], when) || when <= 0 || !number(fields[1], code))
		return stamp;
	if (fields[2] == "scan")
		stamp.topup = false;
	else if (fields[2] == "topup")
		stamp.topup = true;
	else
		return stamp;

	for (size_t i = 3; i < fields.size(); i++)
	{
		const size_t eq = fields[i].find('=');
		if (eq == std::string::npos)
			continue;
		const std::string key = fields[i].substr(0, eq);
		const std::string value = fields[i].substr(eq + 1);
		if (key == "why")
		{
			// A token and nothing else: the caller maps it to a sentence,
			// and anything it does not know reads as SOMETHING WENT WRONG.
			bool token = !value.empty();
			for (char c : value)
				if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
					token = false;
			if (token)
				stamp.why = value;
			continue;
		}
		// The ctl's word for a count it could not measure (PL-031): an
		// answer of its own, where any other non-number says nothing.
		if (key == "added" && value == "unknown")
		{
			stamp.added = ScanStamp::AddedUnknown;
			continue;
		}
		long long n = 0;
		if (!number(value, n) || n > 1000000)
			continue;
		if (key == "cached")       stamp.cached = (int) n;
		else if (key == "added")   stamp.added = (int) n;
		else if (key == "skipped") stamp.skipped = (int) n;
		else if (key == "ready")   stamp.ready = (int) n;
		else if (key == "limit")   stamp.limit = n != 0;
		else if (key == "indexed") stamp.indexed = (int) n;
		else if (key == "errors")  stamp.errors = (int) n;
		else if (key == "truncated") stamp.truncated = n != 0;
	}

	stamp.ran = true;
	stamp.when = (time_t) when;
	stamp.code = (int) code;
	return stamp;
}

CloudText::RunningProgress CloudText::parseRunningProgress(const std::string& text, long long nowEpoch)
{
	RunningProgress run;

	// One line of key=value fields, in any order. A whole number is at most
	// twelve digits, as the stamps' are; a count is capped as the scan
	// stamp's are.
	std::string line = text;
	const size_t newline = line.find('\n');
	if (newline != std::string::npos)
		line = line.substr(0, newline);
	const std::vector<std::string> fields = Utils::String::split(Utils::String::trim(line), ' ', true);

	auto number = [](const std::string& f, long long& out) -> bool
	{
		if (f.empty() || f.size() > 12)
			return false;
		for (char c : f)
			if (c < '0' || c > '9')
				return false;
		out = std::stoll(f);
		return true;
	};

	bool hasAt = false;
	for (const std::string& field : fields)
	{
		// The name may carry '=' of its own: the first one splits.
		const size_t eq = field.find('=');
		if (eq == std::string::npos)
			continue;
		const std::string key = field.substr(0, eq);
		const std::string value = field.substr(eq + 1);
		if (key == "route")
			run.route = value;
		else if (key == "at")
		{
			long long at = 0;
			if (number(value, at) && at > 0)
			{
				run.at = at;
				hasAt = true;
			}
		}
		else if (key == "index" || key == "total")
		{
			long long n = 0;
			if (!number(value, n) || n > 1000000)
				continue;
			if (key == "index")
				run.index = (int) n;
			else
				run.total = (int) n;
		}
	}

	// A file is never a run on its own: it says when the ctl last wrote it,
	// and the ctl bounds its runs, so a line older than that bound was left
	// by a run that died. A line from the future by more than a day is a
	// clock that jumped, not a run.
	if (!hasAt)
		return run;
	if (nowEpoch - run.at > RUNNING_STALE_AFTER_S || run.at - nowEpoch > RUNNING_AHEAD_LIMIT_S)
		return run;
	run.running = true;
	return run;
}

CloudText::NextTime CloudText::nextTime(bool awardsPending, bool savesPending)
{
	if (awardsPending && savesPending)
		return NextTime::AwardsAndSaves;
	if (awardsPending)
		return NextTime::Awards;
	if (savesPending)
		return NextTime::Saves;
	return NextTime::None;
}
