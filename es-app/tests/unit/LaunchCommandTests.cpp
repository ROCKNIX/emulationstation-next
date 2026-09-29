// What a finished launch command says it launched (#308 8-es-menus-and-core
// claude F-ES-20): es-app/src/LaunchCommand.h, pure and, until these cases,
// untested. The capture manifest records the system, emulator and core read
// here, and the boot game's are read from its stored command alone.
// runemu.sh reads the same tokens with ${ARGUMENTS##*-P} and ${...%% *};
// `runemu` below is that reading, so where the two part company a case says
// so rather than hiding it.
#include "doctest/doctest.h"
#include "LaunchCommand.h"
#include "utils/CommandLineUtil.h"

#include <string>

namespace
{
	// ${ARGUMENTS##*<prefix>} then ${VALUE%% *}: from after the last
	// occurrence of the prefix anywhere, to the next space.
	std::string runemu(const std::string& args, const std::string& prefix)
	{
		const size_t pos = args.rfind(prefix);
		std::string v = pos == std::string::npos ? args : args.substr(pos + prefix.size());
		const size_t sp = v.find(' ');
		return sp == std::string::npos ? v : v.substr(0, sp);
	}

	// The controllers blob as InputManager::configureEmulators writes it on
	// ROCKNIX: index and GUID per player, no names (Batocera's -p1name, which
	// a name like "Anbernic-Pad" would turn into a second -P, is not ours).
	const std::string ROCKNIX =
		"/usr/bin/runemu.sh /storage/roms/snes/Super\\ Mario\\ World.sfc -Psnes --core=snes9x --emulator=retroarch "
		"--controllers=\" -p1index 0 -p1guid 19000000010000000100000000010000 \"";
}

TEST_CASE("launch command: the shape ES builds")
{
	CHECK(launchToken(ROCKNIX, "-P") == "snes");
	CHECK(launchArgument(ROCKNIX, "--core", "fallback") == "snes9x");
	CHECK(launchArgument(ROCKNIX, "--emulator", "fallback") == "retroarch");
	CHECK(runemu(ROCKNIX, "-P") == "snes");
	CHECK(runemu(ROCKNIX, "--core=") == "snes9x");
}

TEST_CASE("launch command: a ROM whose name holds -P, before the real one")
{
	const std::string cmd = "/usr/bin/runemu.sh /storage/roms/arcade/Super-Pang.zip -Parcade --core=fbneo --emulator=retroarch";
	CHECK(launchToken(cmd, "-P") == "arcade");
	CHECK(runemu(cmd, "-P") == "arcade");
}

TEST_CASE("launch command: a per-system command with no --core= takes the fallback")
{
	const std::string tools = "/usr/bin/run /storage/roms/tools/Start\\ Kodi.sh";
	CHECK(launchArgument(tools, "--core", "kodi-core") == "kodi-core");
	CHECK(launchArgument(tools, "--emulator", "") == "");
	CHECK(launchToken(tools, "-P") == "");
}

TEST_CASE("launch command: a command that begins with the prefix")
{
	CHECK(launchToken("-Pgb --core=gambatte", "-P") == "gb");
}

TEST_CASE("launch command: a netplay nick holding -P after the system (where runemu.sh parts company)")
{
	const std::string cmd = ROCKNIX + " --connect 192.168.1.20 --port 55435 --nick 'My-Player'";
	// The header wants " -P" and reads the system the command was built with.
	CHECK(launchToken(cmd, "-P") == "snes");
	// runemu.sh's ##*-P matches inside the nick: the launcher reads a system
	// the command was never built with. Recorded as the launcher's (stream
	// B's runemu.sh), not this header's.
	CHECK(runemu(cmd, "-P") == "layer'");
}

// The rest of the finding's list: a netplay client's nick and a savestate's
// rewrite. A nick is the player's own text, single-quoted as one word
// (fork #275), and reading " -P" or " --core=" inside it recorded a system
// or a core the command was never built with -- the reader took the last
// occurrence of the letters anywhere in the line.
TEST_CASE("launch command: a netplay nick holding the prefixes is one word, not a token")
{
	const std::string cmd = ROCKNIX + " --connect '192.168.1.20' --port 55435 --nick 'Bob -Pro --core=x --emulator=y'";
	CHECK(launchToken(cmd, "-P") == "snes");
	CHECK(launchArgument(cmd, "--core", "fallback") == "snes9x");
	CHECK(launchArgument(cmd, "--emulator", "fallback") == "retroarch");
}

TEST_CASE("launch command: a ROM whose escaped name holds the prefixes")
{
	const std::string cmd = "/usr/bin/runemu.sh /storage/roms/snes/Hack\\ -Pbad\\ --core=bad.sfc -Psnes --core=snes9x --emulator=retroarch";
	CHECK(launchToken(cmd, "-P") == "snes");
	CHECK(launchArgument(cmd, "--core", "fallback") == "snes9x");
}

TEST_CASE("launch command: a save state made with another core reads the core it was rewritten to")
{
	// SaveState::setupSaveState: the option swapped in place, the slot after.
	std::string cmd = Utils::CommandLine::replaceOptionValue(ROCKNIX, "-core", "snes9x2010");
	cmd = Utils::CommandLine::replaceOptionValue(cmd, "-emulator", "retroarch");
	cmd += " -autosave 1 -state_slot 3";
	CHECK(launchArgument(cmd, "--core", "fallback") == "snes9x2010");
	CHECK(launchArgument(cmd, "--emulator", "fallback") == "retroarch");
	CHECK(launchToken(cmd, "-P") == "snes");
}
