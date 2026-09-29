// The launch command's option replacement the save state manager makes
// when a state was saved with another core or emulator (fork #21; #308
// 5-cloud-sync-and-saves gpt F-CS-33), checked on the command line alone.

#include "doctest/doctest.h"

#include "utils/CommandLineUtil.h"

#include <string>

using Utils::CommandLine::replaceOptionValue;

TEST_CASE("the joined option is replaced, not glued (fork #21)")
{
	// ROCKNIX passes --core=<v> / --emulator=<v>; the old find("-core")
	// erased "=value" and wrote "--coremgba".
	CHECK(replaceOptionValue("/usr/bin/runemu.sh '/roms/gba/Game.gba' -Pgba --core=mgba --emulator=retroarch", "-core", "vbam")
		== "/usr/bin/runemu.sh '/roms/gba/Game.gba' -Pgba --core=vbam --emulator=retroarch");
	CHECK(replaceOptionValue("/usr/bin/runemu.sh '/roms/gba/Game.gba' -Pgba --core=mgba --emulator=retroarch", "-emulator", "mednafen")
		== "/usr/bin/runemu.sh '/roms/gba/Game.gba' -Pgba --core=mgba --emulator=mednafen");
	// At the end of the line.
	CHECK(replaceOptionValue("run --emulator=retroarch --core=mgba", "-core", "vbam") == "run --emulator=retroarch --core=vbam");
	// The legacy separate form, and an option that is not there.
	CHECK(replaceOptionValue("emulatorlauncher -system gba -core mgba -rom x", "-core", "vbam") == "emulatorlauncher -system gba -core vbam -rom x");
	CHECK(replaceOptionValue("run -Pgba", "-core", "vbam") == "run -Pgba");
}

TEST_CASE("an option spelled inside a quoted ROM name is the ROM's, not the command's (#308 5 gpt F-CS-33)")
{
	// rfind took the last "--core=" anywhere in the line, and ended the value
	// at the next literal space: a ROM file name carrying the same letters
	// after the real option was rewritten instead of the option, and could
	// lose its closing quote.
	const std::string single = "/usr/bin/runemu.sh -Pgba --core=mgba --emulator=retroarch '/roms/gba/Hack --core=beta.gba'";
	CHECK(replaceOptionValue(single, "-core", "vbam")
		== "/usr/bin/runemu.sh -Pgba --core=vbam --emulator=retroarch '/roms/gba/Hack --core=beta.gba'");

	const std::string doubled = "/usr/bin/runemu.sh -Pgba --core=mgba \"/roms/gba/Hack --core=beta.gba\"";
	CHECK(replaceOptionValue(doubled, "-core", "vbam")
		== "/usr/bin/runemu.sh -Pgba --core=vbam \"/roms/gba/Hack --core=beta.gba\"");

	// A backslash-escaped space keeps the name one word too.
	const std::string escaped = "/usr/bin/runemu.sh -Pgba --core=mgba /roms/gba/Hack\\ --core=beta.gba";
	CHECK(replaceOptionValue(escaped, "-core", "vbam")
		== "/usr/bin/runemu.sh -Pgba --core=vbam /roms/gba/Hack\\ --core=beta.gba");

	// Not a token start: the letters inside a longer word are not the option.
	CHECK(replaceOptionValue("run -Pgba x--core=mgba", "-core", "vbam") == "run -Pgba x--core=mgba");

	// The legacy form inside quotes is the ROM's too.
	CHECK(replaceOptionValue("launcher '/roms/a -core b.gba' -core mgba", "-core", "vbam")
		== "launcher '/roms/a -core b.gba' -core vbam");
}
