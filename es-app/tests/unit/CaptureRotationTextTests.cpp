#include "doctest/doctest.h"

#include "CaptureRotationText.h"

// What RetroArch logged and configured, as the quarter turns the display
// gave a game's frame (fork #245, D-UI-081).
TEST_CASE("the last SET_ROTATION line of a launch log gives the core's turns")
{
	const std::string load = "[INFO] === Build =======================================\n"
	                         "[INFO] [Content] Content loading skipped. Implementation will load it on its own.\n";
	CHECK(CaptureRotationText::turnsFromLog(load + "[INFO] [Environ] SET_ROTATION: \"1\" (90 deg).\n") == 1);
	CHECK(CaptureRotationText::turnsFromLog(load + "[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n") == 3);
	CHECK(CaptureRotationText::turnsFromLog(load + "[INFO] [Environ] SET_ROTATION: \"0\" (0 deg).\n") == 0);
	// the last one counts: a core that changed its mind
	CHECK(CaptureRotationText::turnsFromLog(load + "SET_ROTATION: \"1\" (90 deg).\nsomething\nSET_ROTATION: \"0\" (0 deg).\n") == 0);
	// a log with no request is not a request for 0
	CHECK(CaptureRotationText::turnsFromLog(load + "[INFO] [Core]: Content loaded.\n") == -1);
	CHECK(CaptureRotationText::turnsFromLog("") == -1);
	CHECK(CaptureRotationText::turnsFromLog(load + "SET_ROTATION: \"x\"") == -1);
}

// The file held every launch since it was last removed on a device whose
// log level was none (fork #280): Ms. Pac-Man's four launches left "3" at
// its end, and Aladdin, F-Zero and Dr. Mario were turned by it. A launch
// starts at RetroArch's build banner -- not at "Loading content file",
// which a core that loads its own content (fbneo, mame) never prints.
TEST_CASE("only the last launch's section of the log counts")
{
	const std::string banner = "[INFO] === Build =======================================\n";
	const std::string pacman = banner +
	                           "[INFO] [Content] Content loading skipped. Implementation will load it on its own.\n"
	                           "[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n"
	                           "[INFO] [Runtime] Content ran for a total of: 00 hours, 05 minutes, 03 seconds.\n";
	const std::string drmario = banner +
	                            "[INFO] [Content] Loading content file: \"/storage/roms/nes/Dr. Mario.zip#Dr. Mario.nes\".\n"
	                            "[INFO] [Core] Content loaded.\n";
	// an earlier launch's line is not this launch's
	CHECK(CaptureRotationText::turnsFromLog(pacman + drmario) == -1);
	CHECK(CaptureRotationText::turnsFromLog(pacman + pacman + drmario) == -1);
	// this launch's own line is
	CHECK(CaptureRotationText::turnsFromLog(drmario + pacman) == 3);
	CHECK(CaptureRotationText::turnsFromLog(drmario + drmario + "[INFO] [Environ] SET_ROTATION: \"1\" (90 deg).\n") == 1);
	// a rotation line with no banner before it belongs to no launch
	CHECK(CaptureRotationText::turnsFromLog("[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n") == -1);
	CHECK(CaptureRotationText::turnsFromLog("[INFO] [Content] Loading content file: \"x\".\n[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n") == -1);
}

TEST_CASE("the display's turn is the core's, unless rotation is forbidden, plus the player's own")
{
	CHECK(CaptureRotationText::fold(1, "video_allow_rotate = \"true\"\nvideo_rotation = \"0\"\n") == 1);
	CHECK(CaptureRotationText::fold(1, "video_allow_rotate = \"false\"\n") == 0);
	CHECK(CaptureRotationText::fold(1, "video_rotation = \"1\"\n") == 2);
	CHECK(CaptureRotationText::fold(3, "video_rotation = \"2\"\n") == 1);
	CHECK(CaptureRotationText::fold(0, "video_rotation = \"1\"\n") == 1);
	CHECK(CaptureRotationText::fold(-1, "") == 0);
	CHECK(CaptureRotationText::fold(1, "") == 1);
	// a key that merely contains the name does not count
	CHECK(CaptureRotationText::fold(1, "menu_video_allow_rotate = \"false\"\n") == 1);
	CHECK(CaptureRotationText::fold(1, "# video_allow_rotate = \"false\"\n") == 1);
}

TEST_CASE("the record is one digit and reads back, and anything else reads as none")
{
	CHECK(CaptureRotationText::recordText(1) == "turns=1\nfrom=checked-launch\n");
	CHECK(CaptureRotationText::recordText(5) == "turns=1\nfrom=checked-launch\n");
	CHECK(CaptureRotationText::recordText(-1) == "turns=3\nfrom=checked-launch\n");
	CHECK(CaptureRotationText::parseRecord("turns=2\nfrom=checked-launch\n") == 2);
	CHECK(CaptureRotationText::parseRecord("turns=2\nfrom=own-launch\n") == 2);
	// longer than the three bytes readAllText's byte-order-mark check reads
	CHECK(CaptureRotationText::recordText(0).size() > 3);
	CHECK(CaptureRotationText::parseRecord("turns=1\n") == 1);
	CHECK(CaptureRotationText::parseRecord("turns=3") == 3);
	CHECK(CaptureRotationText::parseRecord("1\n") == 1);
	CHECK(CaptureRotationText::parseRecord("  3") == 3);
	CHECK(CaptureRotationText::parseRecord("turns=x") == 0);
	CHECK(CaptureRotationText::parseRecord("") == 0);
	CHECK(CaptureRotationText::parseRecord("9") == 0);
	CHECK(CaptureRotationText::parseRecord("north") == 0);
}

// A record says whether its turn came from the game's own launch (fork
// #288): the reader before it took the last rotation line of a log holding
// every launch, so a record without the line may carry another game's turn.
TEST_CASE("a record says its turn came from the game's own launch, and one that does not say so is not trusted")
{
	CHECK(CaptureRotationText::recordFromOwnLaunch(CaptureRotationText::recordText(0)));
	CHECK(CaptureRotationText::recordFromOwnLaunch("turns=3\nfrom=checked-launch\n"));
	CHECK(CaptureRotationText::recordFromOwnLaunch("from=checked-launch\nturns=3\n"));
	CHECK(CaptureRotationText::recordFromOwnLaunch("turns=3\r\nfrom=checked-launch\r\n"));
	CHECK(CaptureRotationText::recordFromOwnLaunch("turns=3\n  from=checked-launch"));
	// the records every build before fork #288 wrote
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch("turns=3\n"));
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch("1\n"));
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch(""));
	// a line of its own, never a substring of a longer one
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch("turns=3\n# from=checked-launch\n"));
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch("turns=3\nfrom=checked-launch-maybe\n"));
	CHECK_FALSE(CaptureRotationText::recordFromOwnLaunch("turns=3\nfrom=checked-launchfrom=checked-launch\n"));
}

TEST_CASE("the core's table gives a game its turn by ROM name, and nothing to the rest")
{
	const std::string table = "1942 1\ndkong 3\nmspacman 3\npacman 3\nsome-flipped 2\n";
	CHECK(CaptureRotationText::turnsFromTable(table, "mspacman") == 3);
	CHECK(CaptureRotationText::turnsFromTable(table, "1942") == 1);
	CHECK(CaptureRotationText::turnsFromTable(table, "some-flipped") == 2);
	CHECK(CaptureRotationText::turnsFromTable(table, "dkong") == 3);
	// a prefix is not a match, nor is a name the table lacks
	CHECK(CaptureRotationText::turnsFromTable(table, "mspac") == 0);
	CHECK(CaptureRotationText::turnsFromTable(table, "pac") == 0);
	CHECK(CaptureRotationText::turnsFromTable(table, "sf2") == 0);
	CHECK(CaptureRotationText::turnsFromTable(table, "") == 0);
	CHECK(CaptureRotationText::turnsFromTable("", "mspacman") == 0);
	CHECK(CaptureRotationText::turnsFromTable("mspacman x\n", "mspacman") == 0);
	// the last line needs no newline
	CHECK(CaptureRotationText::turnsFromTable("galaga 3", "galaga") == 3);
}

TEST_CASE("a session's reading is written only from a launch's log, and a zero only where it corrects something (#308 8-es claude F-ES-08, 8a gpt F-ES-08)")
{
	using namespace CaptureRotationText;
	const std::string launch = "[INFO] === Build =======================================\n[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n";
	const std::string noRotation = "[INFO] === Build =======================================\n[INFO] Content loading skipped.\n";
	const std::string noLaunch = "RetroArch: core not found\n";
	const std::string record3 = recordText(3);

	// No banner: no launch in the log, nothing it says is this game's. The
	// record a real session wrote stays -- it used to become turns=0 with
	// the own-launch line, the line that makes it trusted over the table.
	CHECK_FALSE(shouldRecord(noLaunch, record3, fold(turnsFromLog(noLaunch), ""), 0));
	CHECK_FALSE(shouldRecord("", record3, fold(turnsFromLog(""), ""), 3));
	// A launch whose core never asked is a real zero: it corrects a record.
	CHECK(shouldRecord(noRotation, record3, fold(turnsFromLog(noRotation), ""), 0));
	// No record, a real zero, and a table that says 3 (rotation off in
	// RetroArch, say): the zero is written, or the table keeps answering 3.
	CHECK(shouldRecord(noRotation, "", 0, 3));
	// No record, a zero, and a table that says 0 too: nothing to write.
	CHECK_FALSE(shouldRecord(noRotation, "", 0, 0));
	// A turn, no record: written. The same turn again: not rewritten.
	CHECK(shouldRecord(launch, "", 3, 0));
	CHECK_FALSE(shouldRecord(launch, record3, 3, 0));
	// A record from before the own-launch line: rewritten so it says so.
	CHECK(shouldRecord(launch, "turns=3\n", 3, 0));
}

// Audit of the fixes (#307), E2 gpt G-E2-06: builds between fork #288 and
// the checked reading (187ff9f1c, 190f62550) stamped from=own-launch on
// records they read from a launch that failed (the previous game's log) or
// a log with no launch in it (a 0 nobody read). Nothing can tell such a
// record from a good one, so no own-launch record from those builds is
// trusted: the core's table stands in until the game's next exit rewrites
// it, as fork #288 did for the records before the line existed. The claim
// this build writes is a new line, from=checked-launch.
TEST_CASE("a record an earlier build stamped from its own launch is not trusted; the checked claim is")
{
	using namespace CaptureRotationText;
	CHECK_FALSE(recordFromOwnLaunch("turns=3\nfrom=own-launch\n"));
	CHECK(recordFromOwnLaunch(recordText(3)));
	CHECK(recordText(1) == "turns=1\nfrom=checked-launch\n");
	// A session that reads the same turn rewrites the old claim as the new.
	const std::string launch = "[INFO] === Build =======================================\n[INFO] [Environ] SET_ROTATION: \"3\" (270 deg).\n";
	CHECK(shouldRecord(launch, "turns=3\nfrom=own-launch\n", 3, 0));
	CHECK_FALSE(shouldRecord(launch, recordText(3), 3, 0));
}
