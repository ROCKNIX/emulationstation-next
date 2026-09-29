// The pure cloud text, checked without a device.
//
// Everything here runs against es-app/src/CloudText.cpp alone -- no window,
// no locale, no /storage. What it is worth is what it covers: the shapes a
// script can print and a stamp can hold, including the ones nobody meant to
// write. A parser is judged on its junk, so most of these cases are junk.
//
// #120 box 4. Run: see README.md beside this file.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "CloudText.h"

#include <string>
#include <vector>

using namespace CloudText;

// ---------------------------------------------------------------- hostname

TEST_CASE("cleanHostname keeps letters and digits and nothing else")
{
	// The case the row exists for (#106): a name with a space in it, said
	// back the way a router will show it.
	CHECK(cleanHostname("RG SP") == "RG-SP");
	CHECK(cleanHostname("Max's RG35XX SP") == "Max-s-RG35XX-SP");

	// A run of anything is one hyphen, however long the run.
	CHECK(cleanHostname("RG    SP") == "RG-SP");
	CHECK(cleanHostname("RG _-. SP") == "RG-SP");

	// Never at either end.
	CHECK(cleanHostname("-RG-SP-") == "RG-SP");
	CHECK(cleanHostname("   RG SP   ") == "RG-SP");
	CHECK(cleanHostname("---") == "");
	CHECK(cleanHostname("") == "");

	// Unicode is not "anything else" that survives: the bytes are dropped
	// like any other run, and a name of nothing but them cleans to nothing.
	CHECK(cleanHostname("Max\xe2\x80\x99s RG SP") == "Max-s-RG-SP");
	CHECK(cleanHostname("\xf0\x9f\x8e\xae RG SP") == "RG-SP");
	CHECK(cleanHostname("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82") == "");

	// 63 bytes is the limit the network takes.
	CHECK(cleanHostname(std::string(70, 'a')) == std::string(63, 'a'));
	CHECK(cleanHostname(std::string(63, 'a')) == std::string(63, 'a'));

	// And the truncation cannot leave a trailing hyphen behind.
	const std::string cut = cleanHostname(std::string(62, 'a') + " " + std::string(10, 'b'));
	CHECK(cut == std::string(62, 'a'));
}

// ---------------------------------------------------------------- provider

TEST_CASE("providerLabel says the words the player chose the service by")
{
	CHECK(providerLabel("dropbox") == "DROPBOX");
	CHECK(providerLabel("drive") == "GOOGLE DRIVE");
	CHECK(providerLabel("webdav") == "WEBDAV");
	CHECK(providerLabel("onedrive") == "MICROSOFT ONEDRIVE");

	// A provider set up outside the list falls back to rclone's own word.
	CHECK(providerLabel("nextcloud") == "NEXTCLOUD");
	CHECK(providerLabel("yandex") == "YANDEX");

	// Empty in, empty out: the caller decides what to say when nothing is
	// connected.
	CHECK(providerLabel("") == "");

	// The table is the one both sides read.
	CHECK(recommendedProviders().size() == 14);
	CHECK(recommendedProviders().front().first == "dropbox");
}

// ------------------------------------------------------------------- stamp

TEST_CASE("parseLastRun reads a stamp EmulationStation wrote")
{
	const LastRun r = parseLastRun("1789074505 0 completed");
	CHECK(r.ran);
	CHECK(r.when == 1789074505);
	CHECK(r.code == 0);
	CHECK(r.token == "completed");
	CHECK(r.knownToken);
	CHECK(r.outcome == Outcome::Completed);
	CHECK(r.finished);
	CHECK(r.why == "");
}

TEST_CASE("parseLastRun reads a failure with the scripts' own sentence")
{
	const LastRun r = parseLastRun("1789075371 5 cloud-stopped YOUR CLOUD STOPPED ANSWERING");
	CHECK(r.ran);
	CHECK(r.when == 1789075371);
	CHECK(r.code == 5);
	CHECK(r.token == "cloud-stopped");
	CHECK(r.knownToken);
	CHECK(r.outcome == Outcome::Failed);
	CHECK_FALSE(r.finished);
	CHECK(r.why == "YOUR CLOUD STOPPED ANSWERING");
}

TEST_CASE("parseLastRun reads a stamp a script wrote")
{
	// The scripts' shape: the sentence itself as the third field, spaces as
	// underscores, and no token of ours in front of it.
	const LastRun r = parseLastRun("1789000000 5 YOUR_CLOUD_STOPPED_ANSWERING");
	CHECK(r.ran);
	CHECK(r.code == 5);
	CHECK(r.token == "YOUR_CLOUD_STOPPED_ANSWERING");
	CHECK_FALSE(r.knownToken);
	CHECK(r.outcome == Outcome::Failed);
	CHECK(r.why == "YOUR CLOUD STOPPED ANSWERING");

	// Lower case and a full stop, as a script may well print it.
	const LastRun quiet = parseLastRun("1789000000 5 your_cloud_stopped_answering.");
	CHECK(quiet.why == "YOUR CLOUD STOPPED ANSWERING");
}

TEST_CASE("parseLastRun on the shapes that are not a run")
{
	CHECK_FALSE(parseLastRun("").ran);
	CHECK_FALSE(parseLastRun("   \n").ran);
	CHECK_FALSE(parseLastRun("1789000000").ran);        // one field
	CHECK_FALSE(parseLastRun("not-a-stamp at all").ran);  // no epoch
	CHECK_FALSE(parseLastRun("0 0 completed").ran);     // epoch of zero
	CHECK_FALSE(parseLastRun("-5 0 completed").ran);    // epoch in the past-past
	CHECK_FALSE(parseLastRun("\x01\x02 junk").ran);
}

TEST_CASE("a stamp whose fields are not whole numbers is not a run, and never a completed one (#308 5 gpt F-CS-27)")
{
	// atoi read "garbage" as 0 and 0 is success: a stamp cut or overwritten
	// in its code field said COMPLETED under the row that reads it, and one
	// with junk after the epoch's digits dated a run that never was.
	CHECK_FALSE(parseLastRun("1789000000 garbage").ran);
	CHECK_FALSE(parseLastRun("1789000000 garbage completed").ran);
	CHECK_FALSE(parseLastRun("1789000000 0x0 completed").ran);
	CHECK_FALSE(parseLastRun("1789000000 5abc cloud-stopped").ran);
	CHECK_FALSE(parseLastRun("1789000000xyz 0 completed").ran);
	CHECK_FALSE(parseLastRun("1789000000 99999999999 unknown").ran);
	CHECK_FALSE(parseLastRun("1789000000 - completed").ran);

	// And what the writers do write still reads.
	CHECK(parseLastRun("1789000000 0 completed").outcome == Outcome::Completed);
	CHECK(parseLastRun("1789000000 130 cancelled").outcome == Outcome::SkippedGameStarted);
	CHECK(parseLastRun("1789000000 -1 unknown").ran);   // no exit status at all: -1, and not a success
	CHECK(parseLastRun("1789000000 -1 unknown").outcome == Outcome::Failed);
}

TEST_CASE("a tier whose code is not a whole number is not a success (#308 5 gpt F-CS-27)")
{
	// ">>> tier SETTINGS|nonsense" read as 0 -- a part that finished -- and a
	// composed run whose other part failed was then a run whose parts
	// disagreed rather than a failure of both.
	CHECK(classifyProtocolLine(">>> tier SETTINGS|nonsense").number == -1);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|").number == -1);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|0x0").number == -1);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|7up").number == -1);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|99999").number == -1);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|0").number == 0);
	CHECK(classifyProtocolLine(">>> tier SETTINGS| 9 ").number == 9);
	CHECK(classifyProtocolLine(">>> tier SETTINGS|255").number == 255);
}

TEST_CASE("parseLastRun tolerates how the line is written")
{
	// The stamp is written with a trailing newline, and read after a trim.
	CHECK(parseLastRun("1789074505 0 completed\n").when == 1789074505);
	// Two spaces where there should be one: empty fields are dropped.
	CHECK(parseLastRun("1789074505  0  completed").token == "completed");
	// Two fields is the shape that predates the token, and is read from its
	// code alone.
	const LastRun old = parseLastRun("1789000000 0");
	CHECK(old.ran);
	CHECK(old.outcome == Outcome::Completed);
	CHECK(old.token == "");
	CHECK_FALSE(old.knownToken);
}

TEST_CASE("parseLastRun names each of the four outcomes")
{
	// rclone's 9 -- nothing needed moving -- is a completed run.
	CHECK(parseLastRun("1789000000 9").outcome == Outcome::Completed);
	// A token that says completed outranks a non-zero code.
	CHECK(parseLastRun("1789000000 1 completed").outcome == Outcome::Completed);

	// The two sentinels, by code (CloudExit.h).
	CHECK(parseLastRun("1789000000 75").outcome == Outcome::SkippedLockHeld);
	CHECK(parseLastRun("1789000000 69").outcome == Outcome::SkippedNoNetwork);

	// The launch cancel, by token: a 130 that was not one reads as a
	// failure instead.
	CHECK(parseLastRun("1789000000 130 cancelled").outcome == Outcome::SkippedGameStarted);
	CHECK(parseLastRun("1789000000 130 player-cancelled").outcome == Outcome::SkippedCancelled);
	CHECK(parseLastRun("1789000000 130 stopped").outcome == Outcome::Failed);

	// A composed run whose parts disagreed keeps its own token, and has
	// nothing to fall back on when it carried no sentence.
	const LastRun gaps = parseLastRun("1789000000 1 gaps");
	CHECK(gaps.outcome == Outcome::Gaps);
	CHECK(gaps.why == "");
	CHECK(parseLastRun("1789000000 1 gaps NES DID NOT FINISH").why == "NES DID NOT FINISH");

	// Anything else is a failure with whatever why it carried.
	const LastRun failed = parseLastRun("1789000000 7 cloud-refused");
	CHECK(failed.outcome == Outcome::Failed);
	CHECK(failed.why == "");   // the caller fills this from the token
}

// ------------------------------------------------------------------ origin

TEST_CASE("runOrigin says which automatic run a stamp belongs to")
{
	const time_t when = 1789074505;

	// Within ten seconds either way is the same run.
	CHECK(runOrigin(when, when, 0) == RunOrigin::AfterLastGame);
	CHECK(runOrigin(when, when + 10, 0) == RunOrigin::AfterLastGame);
	CHECK(runOrigin(when, when - 10, 0) == RunOrigin::AfterLastGame);

	// Eleven is a different one.
	CHECK(runOrigin(when, when + 11, 0) == RunOrigin::None);
	CHECK(runOrigin(when, when - 11, 0) == RunOrigin::None);

	// The same window on the startup stamp.
	CHECK(runOrigin(when, 0, when + 10) == RunOrigin::AtStartup);
	CHECK(runOrigin(when, 0, when + 11) == RunOrigin::None);
	CHECK(runOrigin(when, 0, when - 10) == RunOrigin::AtStartup);

	// The exit stamp is preferred where both are in range, and the startup
	// stamp answers where the exit one is out of range.
	CHECK(runOrigin(when, when, when) == RunOrigin::AfterLastGame);
	CHECK(runOrigin(when, when + 11, when) == RunOrigin::AtStartup);

	// A stamp that does not exist is a zero, and answers nothing.
	CHECK(runOrigin(when, 0, 0) == RunOrigin::None);
	CHECK(runOrigin(0, when, when) == RunOrigin::None);
	CHECK(runOrigin(0, 0, 0) == RunOrigin::None);
}

// ------------------------------------------------------------ outcome line

TEST_CASE("shortenWhy drops the part that can go")
{
	// D-UI-035's worked example: a trailing clause after a dash.
	CHECK(shortenWhy("COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN") == "COULDN'T REACH YOUR CLOUD");

	// A parenthetical.
	CHECK(shortenWhy("YOUR CLOUD WOULDN'T TAKE THE FILES (403)") == "YOUR CLOUD WOULDN'T TAKE THE FILES");

	// A second sentence.
	CHECK(shortenWhy("IT WAS STOPPED. TRY AGAIN IN A MOMENT") == "IT WAS STOPPED");

	// A sentence with none of the three has no short form, and the caller
	// falls back to the outcome word on its own.
	CHECK(shortenWhy("YOUR CLOUD STOPPED ANSWERING") == "");
	CHECK(shortenWhy("") == "");

	// A hyphen inside a word is not a trailing clause.
	CHECK(shortenWhy("YOUR SIGN-IN MAY HAVE EXPIRED") == "");

	// The dash wins over the others, and the parenthesis over the stop.
	CHECK(shortenWhy("A - B (C). D") == "A");
	CHECK(shortenWhy("A (B). C") == "A");
}

TEST_CASE("outcomeCandidates offers the forms of a line, longest first")
{
	// The worked example (D-UI-035): three forms, each one shorter.
	const std::vector<std::string> three =
		outcomeCandidates("COULDN'T FINISH - COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN");
	REQUIRE(three.size() == 3);
	CHECK(three[0] == "COULDN'T FINISH - COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN");
	CHECK(three[1] == "COULDN'T FINISH - COULDN'T REACH YOUR CLOUD");
	CHECK(three[2] == "COULDN'T FINISH");

	// A why with no short form gives two: the whole line, and the outcome
	// word, which fits any panel this runs on.
	const std::vector<std::string> two = outcomeCandidates("COULDN'T FINISH - YOUR CLOUD STOPPED ANSWERING");
	REQUIRE(two.size() == 2);
	CHECK(two[0] == "COULDN'T FINISH - YOUR CLOUD STOPPED ANSWERING");
	CHECK(two[1] == "COULDN'T FINISH");

	// A line with no " - " has no split to make.
	const std::vector<std::string> one = outcomeCandidates("COMPLETED");
	REQUIRE(one.size() == 1);
	CHECK(one[0] == "COMPLETED");

	CHECK(outcomeCandidates("").size() == 1);

	// A sentinel reads the same way.
	const std::vector<std::string> skipped = outcomeCandidates("SKIPPED - YOU'RE NOT ONLINE");
	REQUIRE(skipped.size() == 2);
	CHECK(skipped[1] == "SKIPPED");
}

// ---------------------------------------------------------------- protocol

TEST_CASE("classifyProtocolLine reads a pid line")
{
	const ProtocolLine pid = classifyProtocolLine(">>> pid 1234");
	CHECK(pid.kind == ProtocolKind::Pid);
	CHECK(pid.number == 1234);

	// A pid line with nothing after it is still one, and says zero -- which
	// the caller reads as "not known yet" and signals nothing.
	CHECK(classifyProtocolLine(">>> pid ").kind == ProtocolKind::Pid);
	CHECK(classifyProtocolLine(">>> pid ").number == 0);
	CHECK(classifyProtocolLine(">>> pid abc").number == 0);

	// Without the space it is not the pid line at all.
	CHECK(classifyProtocolLine(">>> pid").kind == ProtocolKind::Unknown);
}

TEST_CASE("classifyProtocolLine reads a doing line")
{
	const ProtocolLine doing = classifyProtocolLine(">>> doing network");
	CHECK(doing.kind == ProtocolKind::Doing);
	CHECK(doing.text == "network");

	// Any other keyword is a doing line too; only "network" makes the card
	// say it is waiting, and that is the caller's decision.
	CHECK(classifyProtocolLine(">>> doing archive").text == "archive");
	CHECK(classifyProtocolLine(">>> doing ").text == "");
	CHECK(classifyProtocolLine(">>> doing   network  ").text == "network");

	// The halves of a composed sync are doing lines too (D-UI-052); which
	// half each one names is phaseOf's, below.
	CHECK(classifyProtocolLine(">>> doing receive").kind == ProtocolKind::Doing);
	CHECK(classifyProtocolLine(">>> doing receive").text == "receive");
	CHECK(classifyProtocolLine(">>> doing send").text == "send");
}

TEST_CASE("classifyProtocolLine reads a why line")
{
	const ProtocolLine why = classifyProtocolLine(">>> why your cloud stopped answering");
	CHECK(why.kind == ProtocolKind::Why);
	CHECK(why.text == "YOUR CLOUD STOPPED ANSWERING");

	// The outcome line supplies its own end, so a trailing full stop goes --
	// however many of them there are.
	CHECK(classifyProtocolLine(">>> why your cloud stopped answering.").text == "YOUR CLOUD STOPPED ANSWERING");
	CHECK(classifyProtocolLine(">>> why it was stopped...").text == "IT WAS STOPPED");
	CHECK(classifyProtocolLine(">>> why   it was stopped.  ").text == "IT WAS STOPPED");

	// A stop inside the sentence stays.
	CHECK(classifyProtocolLine(">>> why it stopped. try again").text == "IT STOPPED. TRY AGAIN");

	// An empty why is a why line with nothing in it; the caller keeps the
	// one it had.
	CHECK(classifyProtocolLine(">>> why ").kind == ProtocolKind::Why);
	CHECK(classifyProtocolLine(">>> why ").text == "");
	CHECK(classifyProtocolLine(">>> why .").text == "");
}

TEST_CASE("classifyProtocolLine reads an offer line")
{
	const ProtocolLine offer = classifyProtocolLine(">>> offer create-saves-folder");
	CHECK(offer.kind == ProtocolKind::Offer);
	CHECK(offer.text == "create-saves-folder");
	CHECK(offer.args.empty());
	CHECK(classifyProtocolLine(">>> offer ").text == "");

	// #127: the folder that is missing, then a near name beside it.
	const ProtocolLine near = classifyProtocolLine(">>> offer create-saves-folder|/ROCKNIX/Savez|/ROCKNIX/Saves");
	CHECK(near.kind == ProtocolKind::Offer);
	CHECK(near.text == "create-saves-folder");
	REQUIRE(near.args.size() == 2);
	CHECK(near.args[0] == "/ROCKNIX/Savez");
	CHECK(near.args[1] == "/ROCKNIX/Saves");
	const ProtocolLine alone = classifyProtocolLine(">>> offer create-saves-folder|/Saves");
	REQUIRE(alone.args.size() == 1);
	CHECK(alone.args[0] == "/Saves");
}

TEST_CASE("providerSubtitle keeps a name and drops a paragraph")
{
	// rclone's own description of s3 is a list of sixty services; on a
	// 640x480 panel it filled a third of the form in small text (#128).
	CHECK(providerSubtitle("s3", "Amazon S3 Compliant Storage Providers including AWS, Alibaba, ArvanCloud, Ceph, and others") == "AMAZON S3 AND COMPATIBLE");
	CHECK(providerSubtitle("webdav", "WebDAV") == "WEBDAV");
	CHECK(providerSubtitle("sftp", "SSH/SFTP") == "SSH/SFTP");
	CHECK(providerSubtitle("azureblob", "Microsoft Azure Blob Storage") == "MICROSOFT AZURE BLOB STORAGE");
	CHECK(providerSubtitle("azureblob", "") == "AZUREBLOB");
	CHECK(providerSubtitle("drive", "") == "GOOGLE DRIVE");
}

TEST_CASE("fieldLabel says the player's words for rclone's option names")
{
	// The WebDAV form, framed at 640x480 with rclone's names on it (#123).
	CHECK(fieldLabel("url") == "SERVER ADDRESS");
	CHECK(fieldLabel("vendor") == "SERVER TYPE");
	CHECK(fieldLabel("user") == "USERNAME");
	CHECK(fieldLabel("pass") == "PASSWORD");
	CHECK(fieldLabel("bearer_token") == "ACCESS TOKEN");
	// S3 and SFTP.
	CHECK(fieldLabel("access_key_id") == "ACCESS KEY ID");
	CHECK(fieldLabel("secret_access_key") == "SECRET ACCESS KEY");
	CHECK(fieldLabel("host") == "SERVER ADDRESS");
	CHECK(fieldLabel("key_file_pass") == "PRIVATE KEY PASSWORD");
	// Unmapped: the name in plain words, never SOME_OPTION.
	CHECK(fieldLabel("some_option") == "SOME OPTION");
	CHECK(fieldLabel("SOME_OPTION") == "SOME OPTION");
	CHECK(fieldLabel(" url ") == "SERVER ADDRESS");
	CHECK(fieldLabel("") == "");
	// Nothing wider than the row can take beside a value on a 640 panel.
	for (const char* f : { "url", "vendor", "user", "pass", "bearer_token", "env_auth", "access_key_id",
	                       "secret_access_key", "region", "endpoint", "location_constraint", "acl",
	                       "bucket_object_lock_enabled", "host", "port", "key_pem", "key_file",
	                       "key_file_pass", "pubkey", "pubkey_file", "key_use_agent",
	                       "use_insecure_cipher", "disable_hashcheck", "ssh" })
		CHECK(fieldLabel(f).size() <= 21);
}

TEST_CASE("classifyProtocolLine reads a tier line")
{
	const ProtocolLine tier = classifyProtocolLine(">>> tier saves|0");
	CHECK(tier.kind == ProtocolKind::Tier);
	CHECK(tier.text == "SAVES");
	CHECK(tier.number == 0);

	CHECK(classifyProtocolLine(">>> tier SETTINGS|5").text == "SETTINGS");
	CHECK(classifyProtocolLine(">>> tier SETTINGS|5").number == 5);
	CHECK(classifyProtocolLine(">>> tier  roms and bios | 7 ").text == "ROMS AND BIOS");
	CHECK(classifyProtocolLine(">>> tier  roms and bios | 7 ").number == 7);

	// No code: -1, which is not a success and not one of rclone's.
	CHECK(classifyProtocolLine(">>> tier saves").number == -1);
	// An empty or unreadable code is not a success either (#308 F-CS-27):
	// -1, as for no code at all. Every emitter prints its shell's $?.
	CHECK(classifyProtocolLine(">>> tier saves|").number == -1);
	CHECK(classifyProtocolLine(">>> tier saves|nonsense").number == -1);

	// No label: the caller records nothing.
	CHECK(classifyProtocolLine(">>> tier |5").text == "");
	CHECK(classifyProtocolLine(">>> tier ").text == "");
	CHECK(classifyProtocolLine(">>> tier ").number == -1);
}

TEST_CASE("classifyProtocolLine reads a unit line")
{
	const ProtocolLine unit = classifyProtocolLine(">>> unit nes|2|5");
	CHECK(unit.kind == ProtocolKind::Unit);
	CHECK(unit.text == "nes");
	CHECK(unit.number == 2);
	CHECK(unit.count == 5);

	// A phase announces itself with its counts empty ("SAVES||"): one item,
	// and a script that is not counting. Zero is the reader's "it did not
	// say". The label keeps the case it arrived in, because the page that
	// counts items compares one announcement with the next.
	const ProtocolLine phase = classifyProtocolLine(">>> unit SAVES||");
	CHECK(phase.kind == ProtocolKind::Unit);
	CHECK(phase.text == "SAVES");
	CHECK(phase.number == 0);
	CHECK(phase.count == 0);

	const ProtocolLine spaced = classifyProtocolLine(">>> unit  snes | 2 | 5 ");
	CHECK(spaced.text == "snes");
	CHECK(spaced.number == 2);
	CHECK(spaced.count == 5);

	// The junk. Every one of these is still a unit line -- something
	// started, and the reader decides what to make of a nameless item --
	// and none of them invents a number nobody printed.
	CHECK(classifyProtocolLine(">>> unit ").kind == ProtocolKind::Unit);
	CHECK(classifyProtocolLine(">>> unit ").text == "");
	CHECK(classifyProtocolLine(">>> unit ").number == 0);
	CHECK(classifyProtocolLine(">>> unit ").count == 0);
	CHECK(classifyProtocolLine(">>> unit nes").number == 0);
	CHECK(classifyProtocolLine(">>> unit nes").count == 0);
	CHECK(classifyProtocolLine(">>> unit nes|x|y").number == 0);
	CHECK(classifyProtocolLine(">>> unit nes|x|y").count == 0);
	CHECK(classifyProtocolLine(">>> unit |2|5").text == "");
	CHECK(classifyProtocolLine(">>> unit |2|5").number == 2);

	// Without the space it is not the unit line at all.
	CHECK(classifyProtocolLine(">>> unit").kind == ProtocolKind::Unknown);
}

TEST_CASE("classifyProtocolLine reads a removed line")
{
	// cloud_content_restore's match summary: the total, then the same per
	// system. A match cut off by the network prints it before it exits, so
	// what had already gone can still be said.
	const ProtocolLine rm = classifyProtocolLine(">>> removed 14|314572800|snes:12:300000000,gb:2:14572800");
	CHECK(rm.kind == ProtocolKind::Removed);
	CHECK(rm.files == 14);
	CHECK(rm.bytes == 314572800);
	REQUIRE(rm.systems.size() == 2);
	CHECK(rm.systems[0].system == "SNES");
	CHECK(rm.systems[0].files == "12");
	CHECK(rm.systems[0].bytes == 300000000);
	CHECK(rm.systems[1].system == "GB");
	CHECK(rm.systems[1].files == "2");
	CHECK(rm.systems[1].bytes == 14572800);

	// A match that removed nothing still says so, and says it with no
	// systems under it.
	const ProtocolLine none = classifyProtocolLine(">>> removed 0|0|");
	CHECK(none.kind == ProtocolKind::Removed);
	CHECK(none.files == 0);
	CHECK(none.bytes == 0);
	CHECK(none.systems.empty());

	// A system with no size beside it: the count is what it has.
	const ProtocolLine nosize = classifyProtocolLine(">>> removed 3|0|nes:3");
	REQUIRE(nosize.systems.size() == 1);
	CHECK(nosize.systems[0].system == "NES");
	CHECK(nosize.systems[0].files == "3");
	CHECK(nosize.systems[0].bytes == 0);

	// A system named with no number beside it is worse than one not named:
	// the torn item goes, and the ones around it survive it.
	const ProtocolLine torn = classifyProtocolLine(">>> removed 3|0|nes,gb:2:100,");
	REQUIRE(torn.systems.size() == 1);
	CHECK(torn.systems[0].system == "GB");
	CHECK(torn.systems[0].files == "2");

	// The junk.
	CHECK(classifyProtocolLine(">>> removed ").kind == ProtocolKind::Removed);
	CHECK(classifyProtocolLine(">>> removed ").files == 0);
	CHECK(classifyProtocolLine(">>> removed ").bytes == 0);
	CHECK(classifyProtocolLine(">>> removed ").systems.empty());
	CHECK(classifyProtocolLine(">>> removed x|y").files == 0);
	CHECK(classifyProtocolLine(">>> removed x|y").bytes == 0);
	CHECK(classifyProtocolLine(">>> removed 14").files == 14);
	CHECK(classifyProtocolLine(">>> removed 14").bytes == 0);
	CHECK(classifyProtocolLine(">>> removed  14 | 314572800 ").files == 14);
	CHECK(classifyProtocolLine(">>> removed  14 | 314572800 ").bytes == 314572800);

	// Without the space it is not the removed line at all.
	CHECK(classifyProtocolLine(">>> removed").kind == ProtocolKind::Unknown);
}

// Every distinct ">>> " shape anything in the two repos prints, with where
// it is printed and what the readers must make of it.
//
// The column that matters is the kind. Unknown here is a shape one of the
// two readers drops on the floor -- which is exactly how the empty-cloud
// offer never reached the transfer page: that page's own parser knew
// ">>> unit" and ">>> removed" and had never heard of ">>> offer" (#145).
// A new marker added to a script without a kind here fails this case.
TEST_CASE("a script's line keeps its UTF-8 on its way to the card (#308 5 claude F-CS-19)")
{
	// The card kept printable ASCII only, so a saves folder named in the
	// player's language lost its accented letters between the script's
	// ">>> offer" line and the offer: /Spiele/Spielst\xC3\xA4nde arrived as
	// /Spiele/Spielstnde, a folder that is not the configured one.
	const std::string offer = ">>> offer create-saves-folder|/Spiele/Spielst\xC3\xA4nde\n";
	CHECK(cleanLine(offer) == ">>> offer create-saves-folder|/Spiele/Spielst\xC3\xA4nde");
	const ProtocolLine p = classifyProtocolLine(cleanLine(offer));
	REQUIRE(p.args.size() == 1);
	CHECK(p.args[0] == "/Spiele/Spielst\xC3\xA4nde");

	// rclone's shortening ellipsis (U+2026) stays too.
	CHECK(cleanLine(" * Ikari n\xE2\x80\xA6ge.srm: 40% ") == "* Ikari n\xE2\x80\xA6ge.srm: 40%");

	// What is noise is still dropped: C0 controls, DEL, a terminal's escapes.
	CHECK(cleanLine("\x1B[2K\rTransferred: 1 B\x7F\x01\n") == "Transferred: 1 B");
	CHECK(cleanLine("   ") == "");
}

TEST_CASE("an escape sequence goes, and nothing after it that is not its own (claude G-E1-07)")
{
	// Every escape was taken to end at the first ASCII letter: an OSC (a
	// window title, ESC ] ... BEL), a two-byte escape (ESC 7), or an ESC
	// before a UTF-8 byte swallowed everything up to the next letter --
	// here the start of a protocol line.
	CHECK(cleanLine("\x1B]0;title\x07>>> offer create-saves-folder|/Spiele") == ">>> offer create-saves-folder|/Spiele");
	CHECK(cleanLine("\x1B]0;title\x1B\\>>> why IT WAS STOPPED") == ">>> why IT WAS STOPPED");
	CHECK(cleanLine("\x1B" "7>>> pid 12") == ">>> pid 12");
	CHECK(cleanLine("\x1B\xC3\xA4nde") == "\xC3\xA4nde");
	// CSI, which rclone prints: parameters, then one final byte.
	CHECK(cleanLine("\x1B[1;32m>>> tier SAVES|0\x1B[0m") == ">>> tier SAVES|0");
	CHECK(cleanLine("\x1B[?25lTransferred: 1 B") == "Transferred: 1 B");
	// A lone ESC at the end.
	CHECK(cleanLine("done\x1B") == "done");
}

TEST_CASE("the card's action line drops the in-place clause first, as the house rule says")
{
	const std::string inPlace = "WHAT MADE IT IS IN YOUR CLOUD. THE REST IS STILL HERE.";
	const std::string recover = "TRY AGAIN FROM GAME SETTINGS > BACK UP SAVES TO THE CLOUD";
	const auto c = actionCandidates(inPlace, { recover }, false);
	REQUIRE(c.size() == 2);
	CHECK(c[0] == inPlace + " " + recover);
	CHECK(c[1] == recover);

	// The startup sync's recovery has a short form of its own, last.
	const auto s = actionCandidates("DON'T WORRY, NOTHING CHANGED.",
		{ "IT'LL TRY AGAIN AT STARTUP, OR SYNC NOW FROM GAME SETTINGS.", "IT'LL TRY AGAIN NEXT STARTUP." }, false);
	REQUIRE(s.size() == 3);
	CHECK(s[2] == "IT'LL TRY AGAIN NEXT STARTUP.");

	// A command with no verb has no in-place clause: the recovery alone.
	const auto none = actionCandidates("", { recover }, false);
	REQUIRE(none.size() == 1);
	CHECK(none[0] == recover);
}

TEST_CASE("a sync that moved saves and then lost the network keeps what moved in every candidate (#307 PL-072)")
{
	// cloud_backup ends a run that lost the link part way with 69, whose
	// outcome word is SKIPPED - YOU'RE NOT ONLINE: it says nothing moved.
	// When the byte totals had left zero, the in-place clause is the one
	// true thing the line has to add, and the card used to drop it first --
	// on a small panel the player read SKIPPED over THEY'LL GO UP NEXT TIME
	// YOU'RE CONNECTED, as if the run had never started.
	const std::string inPlace = "THE SAVES THAT MADE IT ARE ON BOTH SIDES. NOTHING ELSE CHANGED.";
	const std::vector<std::string> waiting = {
		"THEY'LL GO UP NEXT TIME YOU'RE CONNECTED, WITH YOUR ACHIEVEMENTS.",
		"THEY GO UP WITH YOUR ACHIEVEMENTS WHEN YOU'RE BACK." };
	const auto c = actionCandidates(inPlace, waiting, true);
	REQUIRE(c.size() >= 2);
	for (auto& candidate : c)
	{
		INFO(candidate);
		CHECK(candidate.find(inPlace) != std::string::npos);
	}
	CHECK(c.front() == inPlace + " " + waiting.front());
	CHECK(c.back() == inPlace);   // the shortest still says what moved
}

TEST_CASE("every protocol shape an emitter prints classifies to a known kind")
{
	struct Shape { const char* line; ProtocolKind kind; const char* from; };

	// projects/ROCKNIX/packages/network/rclone/sources/ and
	// projects/ROCKNIX/packages/rocknix/sources/scripts/, on next.
	static const Shape shapes[] = {
		// Every ">>> why" a script prints, from the scripts at next 4476f90394
		// (regenerated, #308 follow-up: the table was f0f263b8cc's, and since
		// then stream A's scripts print four sentences and backuptool five
		// that it did not list, and ">>> unit everything" is gone -- --all
		// makes each system and BIOS a unit of its own). The why_for tables
		// print the rc-keyed ones; backuptool's why() and cloud_saves_root
		// print theirs with echo.
		{ ">>> why YOUR CLOUD STORAGE ISN'T SET UP YET", ProtocolKind::Why, "cloud_backup:843, cloud_restore:914, cloud_content_backup:95, :119, cloud_content_restore:98, :122" },
		{ ">>> why COULDN'T REACH YOUR CLOUD - CHECK YOUR SIGN-IN", ProtocolKind::Why, "cloud_backup:868, cloud_restore:939" },
		{ ">>> why YOUR CLOUD STOPPED ANSWERING", ProtocolKind::Why, "cloud_backup:933, :740, :751, cloud_restore:1000, :802, :813, cloud_content_backup:183, cloud_content_restore:189, cloud_migrate_layout:98" },
		{ ">>> why YOUR CLOUD SYNC SETTINGS COULDN'T BE READ", ProtocolKind::Why, "cloud_backup:1011, :1017, :1475, cloud_restore:1078, :1084, :1578, cloud_content_backup:112, cloud_content_restore:115" },
		{ ">>> why AN OLD FOLDER SETTING IS IN THE WAY", ProtocolKind::Why, "cloud_backup:1029, cloud_restore:1096" },
		{ ">>> why YOUR CLOUD SYNC SETTINGS COULDN'T BE SAVED", ProtocolKind::Why, "cloud_migrate_layout:307 (stream A follow-up 2, the pointer write read back before the delete)" },
		{ ">>> why CHECK WHAT WOULD CHANGE FIRST", ProtocolKind::Why, "cloud_content_restore:851 (a --match --apply with no plan)" },
		{ ">>> why YOUR SAVES FOLDER ISN'T ON THIS DEVICE", ProtocolKind::Why, "cloud_backup:1379" },
		{ ">>> why THIS DEVICE'S SETTINGS BACKUP IS DAMAGED", ProtocolKind::Why, "cloud_backup:1882, backuptool:1116" },
		{ ">>> why THE COPY IN YOUR CLOUD ISN'T COMPLETE", ProtocolKind::Why, "cloud_backup:1993" },
		{ ">>> why COULDN'T FIND YOUR CLOUD FOLDER", ProtocolKind::Why, "why_for 3|4: cloud_backup:739, cloud_restore:801, cloud_content_backup:182, cloud_content_restore:188" },
		{ ">>> why SOME FILES DIDN'T FINISH", ProtocolKind::Why, "why_for 6: cloud_backup:741, cloud_restore:803, cloud_content_backup:184, cloud_content_restore:190; cloud_migrate_layout:330" },
		{ ">>> why YOUR CLOUD WOULDN'T TAKE THE FILES", ProtocolKind::Why, "why_for 7|8: cloud_backup:742, cloud_restore:804, cloud_content_backup:185, cloud_content_restore:191" },
		{ ">>> why IT WAS STOPPED", ProtocolKind::Why, "why_for 130: cloud_backup:743, cloud_restore:805, cloud_content_backup:186, cloud_content_restore:192" },
		{ ">>> why THE CLOUD TOOK TOO LONG - IT'LL TRY AGAIN NEXT TIME", ProtocolKind::Why, "why_for 10|124 automatic: cloud_backup:749, cloud_restore:811" },
		{ ">>> why SOMETHING WENT WRONG", ProtocolKind::Why, "why_for *: cloud_backup:753, cloud_restore:815, cloud_content_backup:187, cloud_content_restore:193" },
		{ ">>> why SOMETHING CHANGED SINCE YOU CHECKED", ProtocolKind::Why, "cloud_content_restore:781, :824 (a match whose plan no longer matches its preview)" },
		{ ">>> why THE NEW FOLDER ALREADY HAS FILES IN IT", ProtocolKind::Why, "cloud_migrate_layout:320 (--apply)" },
		{ ">>> why COULDN'T TELL WHICH CARD YOUR SAVES ARE ON", ProtocolKind::Why, "cloud_saves_root:137" },
		{ ">>> why YOUR SAVES ARE ON A DIFFERENT CARD", ProtocolKind::Why, "cloud_saves_root:156" },
		{ ">>> why YOUR SAVES CHANGED CARDS PART-WAY THROUGH", ProtocolKind::Why, "cloud_saves_root:177" },
		{ ">>> why COULDN'T RECORD WHICH CARD YOUR SAVES ARE ON", ProtocolKind::Why, "cloud_saves_root:185" },
		{ ">>> why THIS DEVICE CAN'T RESTORE SETTINGS", ProtocolKind::Why, "backuptool:1089" },
		{ ">>> why A SETTINGS BACKUP OR RESTORE IS ALREADY RUNNING", ProtocolKind::Why, "backuptool:1094, :1348" },
		{ ">>> why THERE'S NO SETTINGS BACKUP ON THIS DEVICE YET", ProtocolKind::Why, "backuptool:1101" },
		{ ">>> why COULDN'T KEEP A COPY OF YOUR CURRENT SETTINGS", ProtocolKind::Why, "backuptool:1175, :1206" },
		{ ">>> why THE RESTORE COULDN'T FINISH", ProtocolKind::Why, "backuptool:1300, :1303" },
		{ ">>> why THIS DEVICE CAN'T MAKE A SETTINGS BACKUP", ProtocolKind::Why, "backuptool:1342" },
		{ ">>> why THERE'S NOTHING TO BACK UP YET", ProtocolKind::Why, "backuptool:1379" },
		{ ">>> why THE BACKUP COULDN'T FINISH WHILE GATHERING YOUR SETTINGS", ProtocolKind::Why, "backuptool:1381" },
		{ ">>> why A SIGN-IN WAS FOUND IN THE BACKUP", ProtocolKind::Why, "backuptool:1383" },
		{ ">>> why YOUR OWN BACKUP LIST NAMES A FOLDER A BACKUP CAN'T CARRY", ProtocolKind::Why, "backuptool:1385" },
		{ ">>> why THE BACKUP COULDN'T FINISH", ProtocolKind::Why, "backuptool:1387" },
		{ ">>> doing network", ProtocolKind::Doing, "cloud_net_ready:160, :213" },
		{ ">>> unit SAVES||", ProtocolKind::Unit, "cloud_backup:1349, cloud_restore:1408" },
		{ ">>> unit SETTINGS||", ProtocolKind::Unit, "cloud_backup:1661, cloud_restore:1691" },
		{ ">>> unit snes|2|5", ProtocolKind::Unit, "cloud_content_backup:572, cloud_content_restore:1363 (every run, --all included), :810 (--match --apply)" },
		{ ">>> unit bios|6|6", ProtocolKind::Unit, "cloud_content_restore:1363 (--all: BIOS is a unit of its own)" },
		{ ">>> removed 14|314572800|snes:12:300000000,gb:2:14572800", ProtocolKind::Removed, "cloud_content_restore:844, :915, :957" },
		{ ">>> offer create-saves-folder|/ROCKNIX/Savez|/ROCKNIX/Saves", ProtocolKind::Offer, "cloud_restore:1497" },
		{ ">>> offer create-saves-folder|/ROCKNIX/Saves", ProtocolKind::Offer, "cloud_restore:1506" },

		// EmulationStation's own: the wrapper, and the run compositions
		// that chain several scripts into one page or one card.
		{ ">>> pid 1234", ProtocolKind::Pid, "ThreadedCloudSync.cpp:200, CloudTransferJob.cpp:602, OfflineScanJob.cpp:135" },
		{ ">>> doing network", ProtocolKind::Doing, "main.cpp:672" },
		{ ">>> doing receive", ProtocolKind::Doing, "main.cpp:678" },
		{ ">>> doing send", ProtocolKind::Doing, "main.cpp:681" },
		{ ">>> doing unpack", ProtocolKind::Doing, "GuiMenu.cpp:4755" },
		{ ">>> doing archive", ProtocolKind::Doing, "GuiMenu.cpp:4820" },
		{ ">>> unit SETTINGS||", ProtocolKind::Unit, "GuiMenu.cpp:4754, :4820" },
		{ ">>> tier RESTORING SAVES|0", ProtocolKind::Tier, "main.cpp:680; GuiMenu.cpp:5845; JourneyTiers.h:97" },
		{ ">>> tier BACKING UP SAVES|0", ProtocolKind::Tier, "main.cpp:683, GuiMenu.cpp:5846" },
		{ ">>> tier RESTORING ROMS AND BIOS|5", ProtocolKind::Tier, "JourneyTiers.h:96" },
		{ ">>> tier ROMS AND BIOS|0", ProtocolKind::Tier, "GuiMenu.cpp:4717 (the transfer page's parts, by label)" },
	};

	for (auto& shape : shapes)
	{
		INFO(std::string(shape.from) << "  ->  " << std::string(shape.line));
		const ProtocolLine p = classifyProtocolLine(shape.line);
		CHECK(p.kind == shape.kind);
		// Said twice on purpose: the assertion above is the shape, this one
		// is the rule -- no emitter's line is a line a reader cannot place.
		CHECK(p.kind != ProtocolKind::Unknown);
		CHECK(p.kind != ProtocolKind::NotProtocol);
		// And no why an emitter prints is left in English under a
		// translated outcome word: the card has its translation (#308
		// F-CS-31).
		if (p.kind == ProtocolKind::Why)
			CHECK(isKnownWhy(p.text));
	}
}

TEST_CASE("each why's two spellings agree, so its translation is found (#308 5 gpt F-CS-31)")
{
	// whySentences pairs the English a script prints with _("") of the same
	// English, for xgettext to carry into the catalog. The unit build has no
	// gettext, so _() hands the English back: a pair whose two copies differ
	// by a letter is a sentence the card will never translate.
	const auto sentences = whySentences();
	CHECK(sentences.size() >= 32);
	for (auto& sentence : sentences)
	{
		INFO(sentence.first);
		CHECK(sentence.first == sentence.second);
		CHECK(localizedWhy(sentence.first) == sentence.second);
	}
	// A sentence this build does not list comes back as it came.
	CHECK(localizedWhy("A SENTENCE FROM A NEWER SCRIPT") == "A SENTENCE FROM A NEWER SCRIPT");
	CHECK_FALSE(isKnownWhy("A SENTENCE FROM A NEWER SCRIPT"));
}

// The stamp's why is a sentence too (#308 claude F-CS-24, stream A): a run
// the network ended after files had moved keeps its 69, adds the gaps token
// and says YOU WENT OFFLINE PART-WAY THROUGH (cloud_backup:670,
// cloud_restore:716, cloud_content_backup:229, cloud_content_restore:243 at
// next 4476f90394). The row reads it as COULDN'T FINISH with that sentence
// -- in the player's language -- not the generic one.
TEST_CASE("the stamp's offline why reads as COULDN'T FINISH with its own sentence")
{
	const LastRun r = parseLastRun("1789000000 69 gaps YOU WENT OFFLINE PART-WAY THROUGH");
	CHECK(r.ran);
	CHECK(r.code == 69);
	CHECK(r.outcome == Outcome::Gaps);
	CHECK(r.why == "YOU WENT OFFLINE PART-WAY THROUGH");
	CHECK(isKnownWhy(r.why));
	CHECK(localizedWhy(r.why) == "YOU WENT OFFLINE PART-WAY THROUGH");

	// The four stream A sentences the interface had no entry for.
	for (const char* why : { "SOMETHING CHANGED SINCE YOU CHECKED", "COULDN'T RECORD WHICH CARD YOUR SAVES ARE ON",
		"THE NEW FOLDER ALREADY HAS FILES IN IT", "YOU WENT OFFLINE PART-WAY THROUGH" })
	{
		INFO(why);
		CHECK(isKnownWhy(why));
	}
}

TEST_CASE("classifyProtocolLine on everything else")
{
	// A protocol line this build does not know -- a marker added to a
	// script after this image was made -- still says the wait is over, and
	// is never handed to the parsers that read rclone's own output.
	CHECK(classifyProtocolLine(">>> sometime-later a|b").kind == ProtocolKind::Unknown);
	CHECK(classifyProtocolLine(">>> ").kind == ProtocolKind::Unknown);

	// And a line the scripts printed for the player is not a protocol line.
	CHECK(classifyProtocolLine("Transferred: 12.3 MiB / 45.6 MiB, 27%").kind == ProtocolKind::NotProtocol);
	CHECK(classifyProtocolLine("").kind == ProtocolKind::NotProtocol);
	CHECK(classifyProtocolLine(">>>").kind == ProtocolKind::NotProtocol);
	CHECK(classifyProtocolLine(">> why something").kind == ProtocolKind::NotProtocol);
	CHECK(classifyProtocolLine("  >>> why something").kind == ProtocolKind::NotProtocol);
	CHECK(classifyProtocolLine("echo \">>> pid 1\"").kind == ProtocolKind::NotProtocol);
}

// -------------------------------------------------------------------- verb

TEST_CASE("verbOf reads the direction out of the command")
{
	CHECK(verbOf("/usr/bin/cloud_backup --saves") == Verb::Backup);
	CHECK(verbOf("/usr/bin/backuptool --backup") == Verb::Backup);
	CHECK(verbOf("/usr/bin/cloud_restore --saves") == Verb::Restore);
	CHECK(verbOf("/usr/bin/cloud_restore --saves; /usr/bin/cloud_backup --saves") == Verb::Sync);
	CHECK(verbOf("/usr/bin/cloud_migrate_layout") == Verb::Other);
	CHECK(verbOf("") == Verb::Other);
}

// ------------------------------------------------------------------ fitting

TEST_CASE("transferKind reads which transfer a page's command runs")
{
	// The hub's compositions (GuiMenu::cloudOpenTransfer), in the part that
	// matters: the scripts a command names.
	CHECK(transferKind("rc=0 ; _t=0 ; { /usr/bin/cloud_backup --yes --saves-only ; } || _t=$? ; echo \">>> tier SAVES|$_t\" ; exit $rc") == TransferKind::Backup);
	CHECK(transferKind("rc=0 ; _t=0 ; { /usr/bin/cloud_content_backup --selected --with-media ; } || _t=$? ; exit $rc") == TransferKind::Backup);
	CHECK(transferKind("echo '>>> unit SETTINGS||' ; echo '>>> doing archive' ; /usr/bin/backuptool backup >/dev/null 2>&1 && /usr/bin/cloud_backup --yes --system-only") == TransferKind::Backup);
	CHECK(transferKind("/usr/bin/cloud_restore --yes --saves-only") == TransferKind::Restore);
	CHECK(transferKind("/usr/bin/cloud_content_restore --selected --media-only") == TransferKind::Restore);

	// The settings restore names backuptool too; the cloud script beside it
	// says which way the run goes.
	CHECK(transferKind("echo '>>> unit SETTINGS||' ; /usr/bin/cloud_restore --yes --system-only && { echo '>>> doing unpack' ; /usr/bin/backuptool restore --then-cloud --no-restart ; }") == TransferKind::Restore);

	// The journey's first restore (main.cpp): two restore scripts, one kind.
	CHECK(transferKind("rc=0 ; _t=0 ; { /usr/bin/cloud_content_restore --all ; } || _t=$? ; _t=0 ; { /usr/bin/cloud_restore --yes ; } || _t=$? ; exit $rc") == TransferKind::Restore);

	// A match is the restore script with --match, and a kind of its own: it
	// is the one transfer that deletes.
	CHECK(transferKind("/usr/bin/cloud_content_restore --match --apply") == TransferKind::Match);

	// Both directions is the card's sync, not a page's run; nothing named
	// is nothing known.
	CHECK(transferKind("/usr/bin/cloud_restore --yes --method=copy --update --saves-only; /usr/bin/cloud_backup --yes --method=copy --update --saves-only") == TransferKind::Other);
	CHECK(transferKind("/usr/bin/cloud_migrate_layout --apply") == TransferKind::Other);
	CHECK(transferKind("") == TransferKind::Other);

	// verbOf, the card's reader, does not know the content scripts; that is
	// why this exists.
	CHECK(verbOf("/usr/bin/cloud_content_backup --selected") == Verb::Other);
	CHECK(verbOf("/usr/bin/cloud_backup --yes --saves-only") == Verb::Backup);
}

TEST_CASE("chooseThatFits takes the first candidate that fits")
{
	// One character, one unit of width, so a case reads as its lengths.
	const auto measure = [](const std::string& s) { return (float) s.size(); };

	const std::vector<std::string> candidates = { "0123456789", "01234", "01" };

	CHECK(chooseThatFits(candidates, 10.0f, measure) == "0123456789");
	CHECK(chooseThatFits(candidates, 9.0f, measure) == "01234");
	CHECK(chooseThatFits(candidates, 4.0f, measure) == "01");

	// Nothing fits: the last one offered, which is the shortest thing the
	// caller had. Better a clipped word than a blank row.
	CHECK(chooseThatFits(candidates, 1.0f, measure) == "01");

	// Nothing is known yet -- an unsized row, or a row with no font -- and
	// the full form is the right answer then.
	CHECK(chooseThatFits(candidates, 0.0f, measure) == "0123456789");
	CHECK(chooseThatFits(candidates, -1.0f, measure) == "0123456789");
	CHECK(chooseThatFits(candidates, 4.0f, nullptr) == "0123456789");

	// Nothing offered, nothing shown.
	CHECK(chooseThatFits({}, 100.0f, measure) == "");
}

// ------------------------------------------------------------------- sizes

TEST_CASE("parseBytes reads rclone's size fields")
{
	CHECK(parseBytes("0 B") == 0);
	CHECK(parseBytes("80 KiB") == 81920);
	CHECK(parseBytes("1.4 GiB") == 1503238554);   // 1503238553.6, rounded
	CHECK(parseBytes("  878.906 KiB ") == 900000);
	// the torn units of a per-file line cut at 80 columns
	CHECK(parseBytes("292.969Ki") == 300000);
	CHECK(parseBytes("2.5Mi") == 2621440);

	// no number, an unknown unit, or something no size ever is
	CHECK(parseBytes("") == -1);
	CHECK(parseBytes("KiB") == -1);
	CHECK(parseBytes("12 furlongs") == -1);
	CHECK(parseBytes("inf") == -1);
	CHECK(parseBytes("nan B") == -1);
	CHECK(parseBytes("-5 KiB") == -1);
}

TEST_CASE("sizeLabel prints a size at the precision it has")
{
	CHECK(sizeLabel(0) == "0 KB");
	CHECK(sizeLabel(1) == "1 KB");          // never "0 KB" for something
	CHECK(sizeLabel(204800) == "200 KB");
	CHECK(sizeLabel(900000) == "879 KB");
	CHECK(sizeLabel(1258291) == "1.2 MB");
	CHECK(sizeLabel(1288490189) == "1.20 GB");
	// the unit is chosen from the value as it will print: a byte short of
	// the next unit rounds into it rather than reading "1024 KB" or
	// "1024.0 MB"
	CHECK(sizeLabel(1048575) == "1.0 MB");
	CHECK(sizeLabel(1073741823) == "1.00 GB");
	CHECK(sizeLabel(1073689395) == "1023.9 MB"); // and just under the threshold stays in MB
}

TEST_CASE("roundSizes re-renders every size in a fragment and nothing else")
{
	CHECK(roundSizes("16.521 MiB / 16.521 MiB, 100%, 519.844 KiB/s") == "16.5 MB / 16.5 MB, 100%, 520 KB/s");
	CHECK(roundSizes("0 B / 0 B, -, 0 B/s") == "0 KB / 0 KB, -, 0 KB/s");
	// percentages and times have no unit from the table and pass through
	CHECK(roundSizes("3m2s left, 27%") == "3m2s left, 27%");
	// a number whose unit did not parse is left as it was
	CHECK(roundSizes("12 furlongs") == "12 furlongs");
	CHECK(roundSizes("") == "");
}

// --------------------------------------------------------------- live line

TEST_CASE("liveLine reads the byte line wherever the glue put it")
{
	// The card that produced #140: the block before ended without a
	// newline, so its last line arrived in front of the next block's first.
	auto l = liveLine("Elapsed time:         2.0sTransferred:            0 B / 0 B, -, 0 B/s, ETA -");
	CHECK(l.kind == LiveLine::Kind::Bytes);
	CHECK(l.sent == 0);
	CHECK(l.total == 0);
	CHECK(l.percent == -1);

	// a busier run glues a per-file line on instead
	l = liveLine("* f3.srm: 98% /292.969Ki, 71.998Ki/s, 0sTransferred:         864 KiB / 878.906 KiB, 98%, 223.999 KiB/s, ETA 0s");
	CHECK(l.kind == LiveLine::Kind::Bytes);
	CHECK(l.sent == 884736);
	CHECK(l.total == 900000);
	CHECK(l.percent == 98);

	// and a line that arrived on its own reads the same
	l = liveLine("Transferred:        288 KiB / 878.906 KiB, 33%, 287.998 KiB/s, ETA 2s");
	CHECK(l.kind == LiveLine::Kind::Bytes);
	CHECK(l.sent == 294912);
	CHECK(l.total == 900000);
	CHECK(l.percent == 33);

	// the tab after the label is already gone by the time the card reads
	// it (the pipe loop keeps printable ASCII), but a stray one is harmless
	l = liveLine("Transferred:\t  878.906 KiB / 878.906 KiB, 100%, 217.241 KiB/s, ETA 0s");
	CHECK(l.kind == LiveLine::Kind::Bytes);
	CHECK(l.percent == 100);
}

TEST_CASE("liveLine tells the count line and the check counter apart")
{
	auto l = liveLine("Transferred:            0 / 3, 0%");
	CHECK(l.kind == LiveLine::Kind::Files);
	CHECK(l.sent == 0);
	CHECK(l.total == 3);
	CHECK(l.percent == -1);   // never the bar's

	l = liveLine("Checks:                12 / 70, 17%, Listed 313");
	CHECK(l.kind == LiveLine::Kind::Checks);
	CHECK(l.sent == 12);
	CHECK(l.total == 70);
	CHECK(l.percent == -1);   // a comparison is not a transfer

	// a block with both: the byte line came later, so it is the one read
	l = liveLine("Checks: 1 / 1, 100%Transferred: 4 KiB / 8 KiB, 50%, 4 KiB/s");
	CHECK(l.kind == LiveLine::Kind::Bytes);
	CHECK(l.percent == 50);
}

TEST_CASE("liveWords says progress, never the outcome so far (#208)")
{
	// The line the maintainer saw between the stages of every sync: rclone
	// listing and comparing, its byte line still at zero. It read NOTHING
	// SENT YET; it is a compare, and reads so.
	LiveLine still = liveLine("Transferred:            0 B / 0 B, -, 0 B/s, ETA -");
	CHECK(still.kind == LiveLine::Kind::Bytes);
	CHECK(liveWords(still, false, false) == LiveWords::Comparing);
	// once the count has said it with a number, the still byte line leaves
	// the number on the words rather than flicker it off and on
	CHECK(liveWords(still, false, true) == LiveWords::Keep);

	// the listing before there is anything to count: nothing to say yet
	LiveLine listing = liveLine("Checks:                 0 / 0, -, Listed 40");
	CHECK(listing.kind == LiveLine::Kind::Checks);
	CHECK(liveWords(listing, false, false) == LiveWords::Keep);

	// the compare with a total: the count holds the words while the bytes
	// stand still, and stays off them once they move
	LiveLine count = liveLine("Checks:                12 / 70, 17%, Listed 313");
	CHECK(liveWords(count, false, false) == LiveWords::ComparingCount);
	CHECK(liveWords(count, false, true) == LiveWords::ComparingCount);
	CHECK(liveWords(count, true, true) == LiveWords::Keep);

	// a total queued, nothing sent yet: the byte line is already the fact
	LiveLine queued = liveLine("Transferred:            0 B / 878.906 KiB, 0%, 0 B/s, ETA -");
	CHECK(liveWords(queued, false, true) == LiveWords::Bytes);
	// and moving
	LiveLine moving = liveLine("Transferred:        288 KiB / 878.906 KiB, 33%, 287.998 KiB/s, ETA 2s");
	CHECK(liveWords(moving, true, true) == LiveWords::Bytes);

	// the file count and rclone's own lines never touch the words
	CHECK(liveWords(liveLine("Transferred:            0 / 3, 0%"), false, false) == LiveWords::Keep);
	CHECK(liveWords(liveLine("Elapsed time:         2.0s"), false, false) == LiveWords::Keep);
}

TEST_CASE("liveLine keeps rclone's own lines off the card")
{
	CHECK(liveLine("* f2.srm: 32% /292.969Ki, 95.996Ki/s, 2s").kind == LiveLine::Kind::None);
	CHECK(liveLine("Transferring:").kind == LiveLine::Kind::None);
	CHECK(liveLine("Elapsed time:         2.0s").kind == LiveLine::Kind::None);
	CHECK(liveLine("====================================").kind == LiveLine::Kind::None);
	CHECK(liveLine("CLOUD BACKUP UTILITY").kind == LiveLine::Kind::None);
	CHECK(liveLine("").kind == LiveLine::Kind::None);

	// a Transferred: that carries no pair, or a unit nobody knows, is not
	// guessed at
	CHECK(liveLine("Transferred:").kind == LiveLine::Kind::None);
	CHECK(liveLine("Transferred: 3 furlongs / 9 furlongs, 33%").kind == LiveLine::Kind::None);
	CHECK(liveLine("Checks: many / few").kind == LiveLine::Kind::None);
}

TEST_CASE("liveLine passes any other line with progress in it as it came")
{
	auto l = liveLine("Saves: 2 / 5 sent");
	CHECK(l.kind == LiveLine::Kind::Other);
	CHECK(l.text == "Saves: 2 / 5 sent");
	CHECK(l.percent == -1);

	l = liveLine("Packing 40%");
	CHECK(l.kind == LiveLine::Kind::Other);
	CHECK(l.text == "Packing 40%");
}

TEST_CASE("liveLine refuses a percentage that is not one")
{
	CHECK(liveLine("Transferred: 4 KiB / 8 KiB, 150%").percent == -1);
	CHECK(liveLine("Transferred: 4 KiB / 8 KiB, -%").percent == -1);
	CHECK(liveLine("Transferred: 4 KiB / 8 KiB, -, 0 B/s").percent == -1);
}

// ------------------------------------------------------------------- phase

TEST_CASE("phaseOf names the half a doing word announces")
{
	CHECK(phaseOf("receive") == Phase::Receiving);
	CHECK(phaseOf("send") == Phase::Sending);
	CHECK(phaseOf(" Send ") == Phase::Sending);

	// The other words a doing line carries are not halves: the network
	// wait, the settings archive's two steps, nothing, or a word a newer
	// script made up.
	CHECK(phaseOf("network") == Phase::None);
	CHECK(phaseOf("archive") == Phase::None);
	CHECK(phaseOf("unpack") == Phase::None);
	CHECK(phaseOf("") == Phase::None);
	CHECK(phaseOf("receiving") == Phase::None);
	CHECK(phaseOf("sometime-later") == Phase::None);
}

TEST_CASE("phaseBar maps a half's percentage into the whole bar")
{
	// Receiving is the first half of the bar, sending the second
	// (D-UI-052): a half that ends full leaves the bar half full, or full.
	CHECK(phaseBar(Phase::Receiving, 0) == 0);
	CHECK(phaseBar(Phase::Receiving, 50) == 25);
	CHECK(phaseBar(Phase::Receiving, 100) == 50);
	CHECK(phaseBar(Phase::Sending, 0) == 50);
	CHECK(phaseBar(Phase::Sending, 50) == 75);
	CHECK(phaseBar(Phase::Sending, 100) == 100);

	// A compare has no percentage, and parks at the half's start -- never
	// at zero for the second half, which is the reset the maintainer saw.
	CHECK(phaseBar(Phase::Receiving, -1) == 0);
	CHECK(phaseBar(Phase::Sending, -1) == 50);

	// A run with no halves keeps its own percentage, and a compare leaves
	// its bar alone: the after-a-game backup draws as it always did.
	CHECK(phaseBar(Phase::None, 0) == 0);
	CHECK(phaseBar(Phase::None, 37) == 37);
	CHECK(phaseBar(Phase::None, 100) == 100);
	CHECK(phaseBar(Phase::None, -1) == -1);

	// Nothing past the end of a half, whatever rclone said.
	CHECK(phaseBar(Phase::Receiving, 150) == 50);
	CHECK(phaseBar(Phase::Sending, 150) == 100);
	CHECK(phaseBar(Phase::None, 150) == 100);
}

TEST_CASE("forwardOnly never moves the bar back")
{
	CHECK(forwardOnly(-1, -1) == -1);
	CHECK(forwardOnly(-1, 0) == 0);
	CHECK(forwardOnly(0, 25) == 25);
	CHECK(forwardOnly(25, 25) == 25);
	CHECK(forwardOnly(25, 10) == 25);
	CHECK(forwardOnly(25, -1) == 25);
	CHECK(forwardOnly(50, 100) == 100);

	// The sequence the maintainer saw (#157), through the two halves: a
	// compare in the first half, nothing moved, a compare in the second,
	// nothing moved -- the bar goes 0, 0, 50, 50 and then the outcome
	// fills it, where it used to sit still throughout.
	int bar = -1;
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, -1));  CHECK(bar == 0);   // receive announced
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, -1));  CHECK(bar == 0);   // 113 of 113 compared
	bar = forwardOnly(bar, phaseBar(Phase::Sending, -1));    CHECK(bar == 50);  // send announced
	bar = forwardOnly(bar, phaseBar(Phase::Sending, -1));    CHECK(bar == 50);  // 113 of 113 compared

	// And with saves moving both ways: each half's transfer fills its half
	// and a late compare count cannot pull it back.
	bar = -1;
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, -1));  CHECK(bar == 0);
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, 40));  CHECK(bar == 20);
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, -1));  CHECK(bar == 20);
	bar = forwardOnly(bar, phaseBar(Phase::Receiving, 100)); CHECK(bar == 50);
	bar = forwardOnly(bar, phaseBar(Phase::Sending, -1));    CHECK(bar == 50);
	bar = forwardOnly(bar, phaseBar(Phase::Sending, 30));    CHECK(bar == 65);
	bar = forwardOnly(bar, phaseBar(Phase::Sending, 100));   CHECK(bar == 100);
}

// ---------------------------------------------------------------- the network step (#192)

TEST_CASE("networkStep reads the link, not the script")
{
	// The startup command as main.cpp composes it, in the part that matters.
	const std::string startup = "if [ -x /usr/bin/cloud_net_ready ]; then /usr/bin/cloud_net_ready --wait 60; _w=$?; [ \"$_w\" = 0 ] || exit \"$_w\"; else ip -4 route show default; fi; echo \">>> doing receive\"";

	// cloud_net_ready announces its wait whenever it waits at all, the grace
	// it gives a connection that is already up included. The card's words
	// come from the link the interface sees, not from the announcement: a
	// link makes the step a check (the maintainer's device had been online
	// for fifteen seconds and read WAITING FOR THE NETWORK), no link makes
	// it a wait.
	CHECK(networkStep(true, startup).step == NetworkStep::Checking);
	CHECK(networkStep(false, startup).step == NetworkStep::Waiting);

	// Whatever the command says, or does not say.
	CHECK(networkStep(true, "").step == NetworkStep::Checking);
	CHECK(networkStep(false, "").step == NetworkStep::Waiting);
	CHECK(networkStep(true, "cloud_net_ready --wait 5").step == NetworkStep::Checking);
}

TEST_CASE("networkStep reads the wait's bound out of the command")
{
	// The number the shell runs is the number the card says, so the two
	// cannot drift: --wait N as main.cpp spells it, and --wait=N as the
	// script also takes it.
	CHECK(networkStep(false, "/usr/bin/cloud_net_ready --wait 60; _w=$?").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait 45").waitSeconds == 45);
	CHECK(networkStep(false, "cloud_net_ready --wait=45").waitSeconds == 45);
	CHECK(networkStep(false, "cloud_net_ready --wait   7 ; echo x").waitSeconds == 7);
	CHECK(networkStep(false, "cloud_net_ready --wait 120\"").waitSeconds == 120);

	// No bound named: the script's own default, which is also the fallback
	// loop's minute on an image without the script.
	CHECK(NETWORK_WAIT_DEFAULT_S == 60);
	CHECK(networkStep(false, "cloud_net_ready").waitSeconds == 60);
	CHECK(networkStep(false, "").waitSeconds == 60);
	CHECK(networkStep(false, "while :; do timeout 4 ping -q -c1 -W2 google.com && break; [ $(( $(date +%s) - _t0 )) -lt 60 ] || break; sleep 2; done").waitSeconds == 60);

	// Junk after the flag is not a bound, as the script's own parser would
	// refuse it: the default stands rather than a number nobody set.
	CHECK(networkStep(false, "cloud_net_ready --wait abc").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait=").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait 60abc").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait 12345678").waitSeconds == 60);
	CHECK(networkStep(false, "cloud_net_ready --wait -5").waitSeconds == 60);

	// A zero is handed back as one: the caller then names no number rather
	// than promising "up to 0 seconds".
	CHECK(networkStep(false, "cloud_net_ready --wait 0").waitSeconds == 0);

	// The first --wait is the script's; a later one is somebody else's.
	CHECK(networkStep(false, "cloud_net_ready --wait 30; other --wait 99").waitSeconds == 30);

	// The bound is read whether or not the caller needs it.
	CHECK(networkStep(true, "cloud_net_ready --wait 45").waitSeconds == 45);
}

// ---------------------------------------------------------------- offline achievements (#173)

TEST_CASE("parsePendingCount reads one whole number and nothing else")
{
	// What raofflineproxy-ctl pending prints: the count on one line.
	CHECK(parsePendingCount("0") == 0);
	CHECK(parsePendingCount("1") == 1);
	CHECK(parsePendingCount("12\n") == 12);
	CHECK(parsePendingCount("  3  ") == 3);

	// Anything else is not a count. -1 is the caller's "say nothing": an
	// answer that is missing must never read as awards waiting.
	CHECK(parsePendingCount("") == -1);
	CHECK(parsePendingCount("\n") == -1);
	CHECK(parsePendingCount("-1") == -1);
	CHECK(parsePendingCount("1 2") == -1);
	CHECK(parsePendingCount("hardcore=0") == -1);
	CHECK(parsePendingCount("Usage: raofflineproxy-ctl {enable|disable}") == -1);
	CHECK(parsePendingCount("1.5") == -1);
	CHECK(parsePendingCount("99999999999999") == -1);
}

TEST_CASE("parseFlushStamp reads the proxy's last-flush line")
{
	// "<epoch> <flushed>", as flusher.py writes it and the ctl prints it.
	FlushStamp s = parseFlushStamp("1789337156 2\n");
	CHECK(s.ok);
	CHECK(s.when == 1789337156);
	CHECK(s.flushed == 2);

	s = parseFlushStamp("  1789337156 1  ");
	CHECK(s.ok);
	CHECK(s.flushed == 1);

	// A stamp that says nothing went is not a stamp; neither is any other
	// shape. The ctl already refuses these, and so does the reader, so a
	// stamp reaching the card by another route is judged the same way.
	CHECK_FALSE(parseFlushStamp("1789337156 0").ok);
	CHECK_FALSE(parseFlushStamp("1789337156").ok);
	CHECK_FALSE(parseFlushStamp("1789337156 2 3").ok);
	CHECK_FALSE(parseFlushStamp("0 2").ok);
	CHECK_FALSE(parseFlushStamp("garbage").ok);
	CHECK_FALSE(parseFlushStamp("1789337156 two").ok);
	CHECK_FALSE(parseFlushStamp("-1 2").ok);
	CHECK_FALSE(parseFlushStamp("").ok);
	CHECK_FALSE(parseFlushStamp("\n1789337156 2").ok);
	CHECK_FALSE(parseFlushStamp("1789337156 99999999999").ok);
}

TEST_CASE("nextTime names the sentence the exit card ends on")
{
	// D-RA-004: awards waiting, saves waiting (the exit sync could not run
	// for want of a connection), both, or nothing to say.
	CHECK(nextTime(true, true) == NextTime::AwardsAndSaves);
	CHECK(nextTime(true, false) == NextTime::Awards);
	CHECK(nextTime(false, true) == NextTime::Saves);
	CHECK(nextTime(false, false) == NextTime::None);
}

TEST_CASE("parseScanStamp reads raofflineproxy-ctl's last-scan line")
{
	// As the ctl writes it after a scan the player pressed (fork #179).
	ScanStamp s = parseScanStamp("1789400000 0 scan cached=3 skipped=1 ready=3 limit=0\n");
	CHECK(s.ran);
	CHECK(s.when == 1789400000);
	CHECK(s.code == 0);
	CHECK_FALSE(s.topup);
	CHECK(s.cached == 3);
	CHECK(s.skipped == 1);
	CHECK(s.ready == 3);
	CHECK_FALSE(s.limit);
	CHECK(s.why.empty());

	// An automatic top-up that could not finish, with the ctl's token.
	s = parseScanStamp("1789400100 1 topup cached=0 skipped=0 ready=3 limit=0 why=RETROACHIEVEMENTS_STOPPED_ANSWERING");
	CHECK(s.ran);
	CHECK(s.topup);
	CHECK(s.code == 1);
	CHECK(s.ready == 3);
	CHECK(s.why == "RETROACHIEVEMENTS_STOPPED_ANSWERING");

	// A stamp from before the ctl wrote added= (fork #298) says nothing about it: -1, and the card falls back to cached.
	CHECK(s.added == -1);

	// A top-up that re-read five games and added none: the card says the games are ready, not added.
	s = parseScanStamp("1789400150 0 topup cached=5 added=0 skipped=0 ready=12 limit=0 indexed=0 errors=0");
	CHECK(s.cached == 5);
	CHECK(s.added == 0);
	CHECK(s.ready == 12);
	s = parseScanStamp("1789400160 0 topup cached=5 added=2 skipped=0 ready=14 limit=0");
	CHECK(s.added == 2);

	// The cap.
	s = parseScanStamp("  1789400200 0 scan cached=100 skipped=40 ready=100 limit=1  ");
	CHECK(s.ran);
	CHECK(s.limit);
	CHECK(s.ready == 100);

	// Fields in another order, and a field a newer ctl might add.
	s = parseScanStamp("1789400300 0 scan ready=7 limit=0 skipped=2 cached=5 took=90");
	CHECK(s.ran);
	CHECK(s.cached == 5);
	CHECK(s.skipped == 2);
	CHECK(s.ready == 7);
	// A stamp from before indexed= and errors= reads them as none.
	CHECK(s.indexed == 0);
	CHECK(s.errors == 0);
	CHECK_FALSE(s.truncated);

	// As the ctl writes it since audit #186 PL-24: a fetch that failed for
	// one game among many that went through is rc 1 with the count and the
	// token, and indexed= says how many came from the interface's index.
	s = parseScanStamp("1789400400 1 scan cached=3 skipped=1 ready=12 limit=0 indexed=2 errors=1 why=SOME_GAMES_NOT_SAVED\n");
	CHECK(s.ran);
	CHECK(s.code == 1);
	CHECK(s.cached == 3);
	CHECK(s.indexed == 2);
	CHECK(s.errors == 1);
	CHECK(s.why == "SOME_GAMES_NOT_SAVED");
	CHECK_FALSE(s.truncated);

	// truncated= is read should the ctl ever write it (today it is in the
	// scan log only): the walk stopped at the client's cap of files.
	s = parseScanStamp("1789400500 0 scan cached=40 skipped=0 ready=40 limit=0 indexed=0 errors=0 truncated=1");
	CHECK(s.ran);
	CHECK(s.truncated);
	s = parseScanStamp("1789400500 0 scan cached=40 skipped=0 ready=40 limit=0 truncated=0");
	CHECK_FALSE(s.truncated);
}

// Audit of the fix round PL-031 (stream D's ctl, the interface's half):
// the ctl writes added=unknown when the store could not be read before or
// after a run that cached games, where it used to write the cache count in
// its place. The word is an answer of its own -- nobody counted -- never a
// stamp that does not say, which the card reads as cached.
TEST_CASE("parseScanStamp reads added=unknown as its own answer, not as a stamp that does not say")
{
	ScanStamp s = parseScanStamp("1789400170 0 topup cached=5 skipped=0 ready=14 limit=0 indexed=0 errors=0 added=unknown");
	CHECK(s.ran);
	CHECK(s.cached == 5);
	CHECK(s.ready == 14);
	CHECK(s.added == ScanStamp::AddedUnknown);
	CHECK(ScanStamp::AddedUnknown != -1);
	CHECK(ScanStamp::AddedUnknown < 0);

	// A stamp with a number stays a number, and one without added= is -1.
	CHECK(parseScanStamp("1789400180 0 topup cached=5 added=0 ready=14 limit=0").added == 0);
	CHECK(parseScanStamp("1789400180 0 topup cached=5 added=3 ready=14 limit=0").added == 3);
	CHECK(parseScanStamp("1789400180 0 topup cached=5 ready=14 limit=0").added == -1);
	// Only the ctl's word: another non-number still says nothing.
	CHECK(parseScanStamp("1789400180 0 topup cached=5 added=UNKNOWN ready=14").added == -1);
	CHECK(parseScanStamp("1789400180 0 topup cached=5 added= ready=14").added == -1);
	CHECK(parseScanStamp("1789400180 0 topup cached=5 added=x2 ready=14").added == -1);
}

TEST_CASE("parseScanStamp on the shapes that are not a scan")
{
	CHECK_FALSE(parseScanStamp("").ran);
	CHECK_FALSE(parseScanStamp("\n").ran);
	CHECK_FALSE(parseScanStamp("1789400000 0").ran);
	CHECK_FALSE(parseScanStamp("1789400000 0 sync cached=1").ran);
	CHECK_FALSE(parseScanStamp("0 0 scan cached=1").ran);
	CHECK_FALSE(parseScanStamp("garbage 0 scan").ran);
	CHECK_FALSE(parseScanStamp("1789400000 x scan").ran);
	CHECK_FALSE(parseScanStamp("1789337156 2").ran);   // a flush stamp
	CHECK_FALSE(parseScanStamp("\n1789400000 0 scan").ran);

	// A count that is not one reads as zero, and a why that is not a token
	// reads as none: neither stops the line from being a run.
	ScanStamp s = parseScanStamp("1789400000 0 scan cached=abc skipped=-1 ready=99999999999 why=not a token");
	CHECK(s.ran);
	CHECK(s.cached == 0);
	CHECK(s.skipped == 0);
	CHECK(s.ready == 0);
	CHECK(s.why.empty());
	s = parseScanStamp("1789400000 1 scan why=bad-token!");
	CHECK(s.ran);
	CHECK(s.why.empty());
}

TEST_CASE("parseRunningProgress reads raofflineproxy-ctl's running line")
{
	const long long now = 1789500000;

	// As the ctl writes it as each game finishes (fork #189).
	RunningProgress r = parseRunningProgress("route=topup at=1789499990 index=50 total=147 name=Tobu_Tobu_Girl_Deluxe\n", now);
	CHECK(r.running);
	CHECK(r.route == "topup");
	CHECK(r.at == 1789499990);
	CHECK(r.index == 50);
	CHECK(r.total == 147);

	// Before the first game is known: started, listing.
	r = parseRunningProgress("route=topup at=1789500000", now);
	CHECK(r.running);
	CHECK(r.route == "topup");
	CHECK(r.index == 0);
	CHECK(r.total == 0);

	// A scan; fields in another order, spaces around, a field a newer ctl
	// might add, and a name with '=' of its own.
	r = parseRunningProgress("  total=3 name=A=B index=1 route=scan at=1789499999 pid=123  ", now);
	CHECK(r.running);
	CHECK(r.route == "scan");
	CHECK(r.index == 1);
	CHECK(r.total == 3);

	// The first line only.
	r = parseRunningProgress("route=scan at=1789500000 index=2 total=9\nroute=topup at=1 index=7 total=7\n", now);
	CHECK(r.running);
	CHECK(r.index == 2);
	CHECK(r.total == 9);

	// A count that is not one reads as zero and stops nothing.
	r = parseRunningProgress("route=topup at=1789500000 index=abc total=99999999999", now);
	CHECK(r.running);
	CHECK(r.index == 0);
	CHECK(r.total == 0);
	r = parseRunningProgress("route=topup at=1789500000 index=-1 total=", now);
	CHECK(r.running);
	CHECK(r.index == 0);
	CHECK(r.total == 0);
}

TEST_CASE("parseRunningProgress on the shapes that are not a run")
{
	const long long now = 1789500000;

	// Empty, and no at: a file is never a run on its own.
	CHECK_FALSE(parseRunningProgress("", now).running);
	CHECK_FALSE(parseRunningProgress("\n", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup index=3 total=9", now).running);

	// Malformed.
	CHECK_FALSE(parseRunningProgress("garbage", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=abc", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=0", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=-5", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=1789500000000000", now).running);
	CHECK_FALSE(parseRunningProgress("at", now).running);
	CHECK_FALSE(parseRunningProgress("=1789500000", now).running);
	CHECK_FALSE(parseRunningProgress("1789500000 0 topup cached=1", now).running);   // a scan stamp

	// Stale: the ctl's bound is 900 s. At the bound it still runs; past it
	// the run died and left its file.
	CHECK(parseRunningProgress("route=topup at=1789499100 index=3 total=9", now).running);
	RunningProgress r = parseRunningProgress("route=topup at=1789499099 index=3 total=9", now);
	CHECK_FALSE(r.running);
	// What the line said is still read, so a log can tell a dead run from none.
	CHECK(r.route == "topup");
	CHECK(r.at == 1789499099);
	CHECK(r.index == 3);
	CHECK(r.total == 9);

	// An at ahead of now: a day of skew is still a run; more is a clock
	// that jumped.
	CHECK(parseRunningProgress("route=topup at=1789586400", now).running);
	CHECK_FALSE(parseRunningProgress("route=topup at=1789586401", now).running);
}

// The exit sync skipped for no network is owed until a sync after it ran
// (fork #292, D-RA-030): the reconnect card runs it.
TEST_CASE("exitSyncOwed: skipped for no network, and nothing has synced since")
{
	const std::string skipped = "1790440038 69 no-network";
	CHECK(CloudText::exitSyncOwed(skipped, "", ""));
	CHECK(CloudText::exitSyncOwed(skipped, "1790396323 0 completed", ""));           // a startup sync before it
	CHECK_FALSE(CloudText::exitSyncOwed(skipped, "1790440100 0 completed", ""));     // a startup sync after it
	CHECK(CloudText::exitSyncOwed(skipped, "1790440100 69 no-network", ""));         // one after it that also had no network
	CHECK_FALSE(CloudText::exitSyncOwed(skipped, "", "1790440130 0"));               // the back up row, after it
	CHECK(CloudText::exitSyncOwed(skipped, "", "1790440000 0"));                     // a back up before it
	CHECK_FALSE(CloudText::exitSyncOwed("1790440038 0 completed", "", ""));          // the exit sync went
	CHECK_FALSE(CloudText::exitSyncOwed("", "", ""));
}

TEST_CASE("fileInFlight names the file that is moving from rclone's done count")
{
	CHECK(CloudText::fileInFlight(0, 7) == 1);     // none done yet: the first is moving
	CHECK(CloudText::fileInFlight(3, 7) == 4);     // three done: the fourth
	CHECK(CloudText::fileInFlight(7, 7) == 7);     // all done: never past the total
	CHECK(CloudText::fileInFlight(-1, 7) == 1);    // a count not seen yet reads as none done
	CHECK(CloudText::fileInFlight(0, 0) == 0);     // rclone has not counted: the bytes alone
	CHECK(CloudText::fileInFlight(2, -1) == 0);
}

// A stopped run restamps the part its stop interrupted, and only that part
// (#203; #308 5-cloud-sync-and-saves claude F-CS-05, gpt F-CS-23). The
// commands are the ones the transfer page and the startup sync compose.
namespace
{
	bool has(const std::vector<std::string>& v, const std::string& s)
	{
		for (auto& x : v)
			if (x == s)
				return true;
		return false;
	}
}

TEST_CASE("a stopped run's stamps: the settings parts are among the stamps its scripts write")
{
	const std::string settingsBackup = "echo '>>> unit SETTINGS||' ; echo '>>> doing archive' ; "
		"/usr/bin/backuptool backup >/dev/null 2>&1 && /usr/bin/cloud_backup --yes --system-only";
	const std::string settingsRestore = "echo '>>> unit SETTINGS||' ; /usr/bin/cloud_restore --yes --system-only"
		" && { echo '>>> doing unpack' ; /usr/bin/backuptool restore --no-restart ; }";
	CHECK(has(scriptStampNames(settingsBackup), "last-settings-backup"));
	CHECK(has(scriptStampNames(settingsRestore), "last-settings-restore"));

	const std::string startup = "/usr/bin/cloud_restore --yes --method=copy --update --saves-only --automatic; _r=$?;"
		" /usr/bin/cloud_backup --yes --method=copy --update --saves-only --automatic; _b=$?;";
	CHECK(has(scriptStampNames(startup), "last-restore"));
	CHECK(has(scriptStampNames(startup), "last-backup"));
	CHECK(has(scriptStampNames("/usr/bin/cloud_content_restore --match --apply"), "last-content-match"));
	CHECK(has(scriptStampNames("/usr/bin/cloud_content_backup --selected"), "last-content-backup"));
	// cloud_content_backup is not cloud_backup.
	CHECK_FALSE(has(scriptStampNames("/usr/bin/cloud_content_backup --selected"), "last-backup"));
	CHECK(scriptStampNames("sleep 4").empty());
}

TEST_CASE("a stopped run's stamps: only the part the stop interrupted is restamped")
{
	const time_t started = 1789000000;
	// Saves, then content, then settings -- stopped in the settings part:
	// the saves and content parts finished this run and keep their outcome.
	const std::vector<StampText> stamps = {
		{ "last-backup",          "1789000040 0" },
		{ "last-content-backup",  "1789000090 0" },
		{ "last-settings-backup", "1789000120 130" },
	};
	const auto restamp = stampsToRestamp(stamps, started);
	CHECK(restamp.size() == 1);
	CHECK(has(restamp, "last-settings-backup"));
	CHECK_FALSE(has(restamp, "last-backup"));

	// A part that failed on its own before the stop keeps its why.
	CHECK(stampsToRestamp({ { "last-backup", "1789000040 5 YOUR_CLOUD_STOPPED_ANSWERING" } }, started).empty());
	// A part that never started keeps its last real run, even a stop.
	CHECK(stampsToRestamp({ { "last-restore", "1788999000 130" } }, started).empty());
	// No stamp, or one that is not a stamp.
	CHECK(stampsToRestamp({ { "last-restore", "" }, { "last-backup", "garbage" } }, started).empty());
	// The trap's stamp from this run, with a why the scripts printed first.
	CHECK(has(stampsToRestamp({ { "last-restore", "1789000003 130 IT_WAS_STOPPED" } }, started), "last-restore"));
}

TEST_CASE("a stop stamp this interface already restamped is an earlier run's, even in the same second (G-E1-06)")
{
	// The scripts' trap writes "<epoch> 130[ <WHY>]", never one of this
	// interface's tokens; a stamp that carries one was restamped after an
	// earlier stop. Read by the time alone, one written in the second this
	// run began qualified as this run's, and took this run's word.
	const time_t started = 1789000000;
	CHECK(stampsToRestamp({ { "last-backup", "1789000000 130 cancelled" } }, started).empty());
	CHECK(stampsToRestamp({ { "last-backup", "1789000000 130 player-cancelled" } }, started).empty());
	// The trap's own, the same second: this run's.
	CHECK(has(stampsToRestamp({ { "last-backup", "1789000000 130" } }, started), "last-backup"));
}

TEST_CASE("with the stamps read as the run began, a file not written since is not this run's, whatever the clock says (G-E1-04 claude)")
{
	// A device that boots with its clock behind: yesterday's stop stamp is
	// "not older" than this run's start by the clock, and was restamped with
	// a cancel this run never made -- on a part that never started.
	const time_t started = 1789000000;
	const std::vector<StampText> before = {
		{ "last-restore", "1789500000 130 IT_WAS_STOPPED", "11:1789500000.5" },
		{ "last-backup", "", "" },
	};
	// The restore part never ran: the file is the one read at the start.
	const std::vector<StampText> after = {
		{ "last-restore", "1789500000 130 IT_WAS_STOPPED", "11:1789500000.5" },
		{ "last-backup", "", "" },
	};
	CHECK(stampsToRestamp(after, started, &before).empty());

	// The restore part ran and was stopped: a new file, whatever its epoch.
	const std::vector<StampText> stopped = {
		{ "last-restore", "1788990000 130 IT_WAS_STOPPED", "12:1788990000.1" },
		{ "last-backup", "", "" },
	};
	const auto names = stampsToRestamp(stopped, started, &before);
	CHECK(names.size() == 1);
	CHECK(has(names, "last-restore"));

	// Written this run and finished, or failed on its own: kept.
	const std::vector<StampText> finished = { { "last-restore", "1788990000 0", "13:1788990000.2" } };
	CHECK(stampsToRestamp(finished, started, &before).empty());
	// A stamp that did not exist at the start and does now, stopped: this run's.
	const std::vector<StampText> fresh = { { "last-backup", "1788990001 130", "14:1788990001.0" } };
	CHECK(has(stampsToRestamp(fresh, started, &before), "last-backup"));
}

// The scan's and the top-up's whys (moved from OfflineAchievements::scanWhy
// and ProxyCards' topUpWhy so the table has a case). The ctl ends a scan or
// a top-up with SOME_IMAGES_NOT_SAVED when achievement images could not be
// fetched (#307 PL-060); the interface had no words for it and said
// SOMETHING WENT WRONG.
TEST_CASE("scan and top-up whys: every token the ctl stamps has words")
{
	CHECK(scanWhy("SOME_GAMES_NOT_SAVED") == "SOME GAMES COULDN'T BE SAVED. TRY THE SCAN AGAIN.");
	CHECK(topUpWhy("SOME_GAMES_NOT_SAVED") == "SOME GAMES COULDN'T BE SAVED");
	CHECK(scanWhy("RETROACHIEVEMENTS_STOPPED_ANSWERING") == "RETROACHIEVEMENTS STOPPED ANSWERING");
	CHECK(topUpWhy("RETROACHIEVEMENTS_STOPPED_ANSWERING") == "RETROACHIEVEMENTS STOPPED ANSWERING");
	CHECK(scanWhy("CANCELLED") == "YOU CANCELLED IT");
	CHECK(scanWhy("") == "SOMETHING WENT WRONG");
	CHECK(scanWhy("A_TOKEN_NEWER_THAN_THIS_BUILD") == "SOMETHING WENT WRONG");

	// The images: the scan's sentence sends the player to the scan, the
	// top-up's card already says it tries again by itself.
	CHECK(scanWhy("SOME_IMAGES_NOT_SAVED") == "SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED. TRY THE SCAN AGAIN.");
	CHECK(topUpWhy("SOME_IMAGES_NOT_SAVED") == "SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED");

	// The scan page drops the instruction where the line is short, never a
	// word: at 640x480 the whole sentence is 516 px of a 499 px line.
	const std::string why = scanWhy("SOME_IMAGES_NOT_SAVED");
	CHECK(shortenWhy(why) == "SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED");
	auto measure = [](const std::string& t) { return 8.0f * (float) t.size(); };
	CHECK(chooseThatFits({ why, shortenWhy(why) }, 400.0f, measure) == "SOME ACHIEVEMENT IMAGES COULDN'T BE SAVED");
	CHECK(chooseThatFits({ why, shortenWhy(why) }, 600.0f, measure) == why);
}

// A match's done page (#308 5-cloud-sync-and-saves gpt F-CS-26, the page's
// half; the script's is stream A's 258b5eca38, whose `>>> removed` now counts
// rclone's own Deleted lines, not the plan). A match removes only what the
// cloud does not have (D-CLOUD-023), so the line that followed the count --
// YOUR CLOUD STILL HAS THEM -- said the opposite of what happened. And an
// apply spends its preview's plan (PL-001): the same command again is
// refused with SOMETHING CHANGED SINCE YOU CHECKED, so the way on is the
// match's own row, which checks again first.
TEST_CASE("a match's done page: what it removed, nothing about the cloud having it, and the row to start again from")
{
	CHECK(matchRemovedNote(0) == "NOTHING WAS REMOVED FROM THIS DEVICE.");
	CHECK(matchRemovedNote(1) == "1 FILE WAS REMOVED FROM THIS DEVICE.");
	CHECK(matchRemovedNote(12) == "12 FILES WERE REMOVED FROM THIS DEVICE.");
	for (int n : { 0, 1, 12 })
	{
		INFO(n);
		CHECK(matchRemovedNote(n).find("CLOUD") == std::string::npos);
	}
	CHECK(matchRecovery() == "TRY AGAIN: MATCH THIS DEVICE TO THE CLOUD");
	// And the refusal it answers is a sentence the page can translate.
	CHECK(isKnownWhy("SOMETHING CHANGED SINCE YOU CHECKED"));
}

// Audit of the fixes (#307), E2 claude G-E2-08(c): the labels
// EmulationStation composes for its own parts reach the page as raw English
// (">>> unit"/">>> tier"), and the page named them in the player's language
// only for SETTINGS and SAVES. Every label a composer prints is listed, as
// every why a script prints is (the emitter table above).
TEST_CASE("the page's item names: every label EmulationStation composes has its translation")
{
	struct Label { const char* label; const char* from; };
	static const Label labels[] = {
		{ "SETTINGS", "GuiMenu.cpp cloudOpenTransfer add(\"SETTINGS\"), the scripts' >>> unit SETTINGS||" },
		{ "SAVES", "GuiMenu.cpp cloudOpenTransfer add(\"SAVES\"), the scripts' >>> unit SAVES||; JourneyTiers.h" },
		{ "ROMS AND BIOS", "GuiMenu.cpp cloudOpenTransfer; JourneyTiers.h" },
		{ "GAME CONTENT", "GuiMenu.cpp cloudOpenTransfer; JourneyTiers.h" },
		{ "ROMS, BIOS, AND GAME CONTENT", "GuiMenu.cpp cloudOpenTransfer; JourneyTiers.h" },
		{ "RESTORING SAVES", "main.cpp the startup sync; GuiMenu.cpp SYNC SAVES; JourneyTiers.h" },
		{ "BACKING UP SAVES", "main.cpp the startup sync; GuiMenu.cpp SYNC SAVES" },
		{ "RESTORING ROMS AND BIOS", "JourneyTiers.h, an earlier build's marker" },
	};
	for (auto& l : labels)
	{
		INFO(l.from << "  ->  " << l.label);
		CHECK(isKnownUnitLabel(l.label));
		CHECK(unitLabel(l.label) == l.label);   // the unit build has no gettext: the English back
	}
	for (auto& pair : unitLabels())
		CHECK(pair.first == pair.second);
	// A system's folder is shown as it came, upper-cased.
	CHECK(unitLabel("snes") == "SNES");
	CHECK_FALSE(isKnownUnitLabel("SNES"));
}

// The audit of the fix round, stream A's lead G2-A-03 (claude), handed to
// stream E1: a run the network ended after files had moved keeps its 69 --
// the retry at the link's return and the offline recovery line read it --
// and its script's stamp says so with the gaps token. The rows and the
// transfer page read that as COULDN'T FINISH; the automatic sync's card read
// the 69 alone and said SKIPPED - YOU'RE NOT ONLINE, which says nothing moved.
TEST_CASE("a 69 whose stamp carries gaps is a run the network cut part-way, not a skip (audit of the fix round, claude G2-A-03)")
{
	const std::vector<StampText> before = {
		{ "last-backup", "1789000000 0", "11:1789000000.0" },
		{ "last-settings-backup", "1788990000 0", "12:1788990000.0" },
	};
	// This run's saves part: files moved, then the link went.
	std::vector<StampText> after = before;
	after[0] = { "last-backup", "1789000100 69 gaps YOU WENT OFFLINE PART-WAY THROUGH\n", "13:1789000100.0" };
	CHECK(offlinePartWayWhy(after, before) == "YOU WENT OFFLINE PART-WAY THROUGH");
	// The same line read back from a stamp the scripts wrote with underscores.
	after[0].text = "1789000100 69 gaps YOU_WENT_OFFLINE_PART-WAY_THROUGH";
	CHECK(offlinePartWayWhy(after, before) == "YOU WENT OFFLINE PART-WAY THROUGH");
	// A bare 69 -- nothing moved -- stays a skip.
	after[0].text = "1789000100 69";
	CHECK(offlinePartWayWhy(after, before).empty());
	// A gaps stamp left by an earlier run, not written since: not this run's.
	const std::vector<StampText> earlier = { { "last-backup", "1788000000 69 gaps YOU WENT OFFLINE PART-WAY THROUGH", "9:1788000000.0" } };
	CHECK(offlinePartWayWhy(earlier, earlier).empty());
	// Another code with the token is not the network's.
	after[0].text = "1789000100 5 gaps YOUR CLOUD STOPPED ANSWERING";
	CHECK(offlinePartWayWhy(after, before).empty());
	// No why on the line: the sentence the scripts write for it.
	after[0].text = "1789000100 69 gaps";
	CHECK(offlinePartWayWhy(after, before) == "YOU WENT OFFLINE PART-WAY THROUGH");
}

TEST_CASE("an exit sync the network cut part-way is still owed (audit of the fix round, claude G2-A-03)")
{
	// The card's stamp for that run now carries gaps (the row reads it as the
	// card does, COULDN'T FINISH), and the retry at the link's return must
	// still run: only part of the saves went up.
	CHECK(CloudText::exitSyncOwed("1790440038 69 gaps YOU WENT OFFLINE PART-WAY THROUGH", "", ""));
	CHECK_FALSE(CloudText::exitSyncOwed("1790440038 69 gaps YOU WENT OFFLINE PART-WAY THROUGH", "", "1790440130 0"));
	// A gaps stamp that is not the network's is not owed to the link.
	CHECK_FALSE(CloudText::exitSyncOwed("1790440038 5 gaps YOUR CLOUD STOPPED ANSWERING", "", ""));
}
