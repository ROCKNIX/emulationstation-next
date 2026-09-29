#pragma once
#ifndef ES_APP_LAUNCH_COMMAND_H
#define ES_APP_LAUNCH_COMMAND_H

#include <string>

#include "utils/CommandLineUtil.h"

// Readers for a finished launch command (fork #21 R5). The save manifest
// records exactly what the emulator was handed, so these never re-resolve
// anything from SystemConf. Shared by FileData::getlaunchCommand (the
// ordinary launch) and launchStartupGame in main.cpp (the boot game, whose
// command is stored and replayed before any system is loaded, so the
// stored string is the only source).
//
// A token is a whole shell word of the command -- the words
// Utils::CommandLine::replaceOptionValue reads when a save state swaps the
// core, so the two agree -- and the last such word wins, as it does there.
// It was the last occurrence of the letters anywhere in the line, and a
// netplay nick is the player's own text, one quoted word: 'Bob -Pro'
// recorded the system as ro (#308 8-es-menus-and-core claude F-ES-20).
// runemu.sh still reads ${ARGUMENTS##*-P}, which does find it there; that
// is the launcher's to change (es-app/tests/unit/LaunchCommandTests.cpp).

// The value of the last word that begins with `prefix` ("-P" ->
// the system name), to the end of that word; "" when the command carries
// none.
inline std::string launchToken(const std::string& command, const std::string& prefix)
{
	std::string value;
	size_t i = 0;
	while (i < command.size())
	{
		if (command[i] == ' ' || command[i] == '\t')
		{
			i++;
			continue;
		}
		const size_t end = Utils::CommandLine::wordEnd(command, i);
		if (command.compare(i, prefix.size(), prefix) == 0)
			value = command.substr(i + prefix.size(), end - i - prefix.size());
		i = end;
	}
	return value;
}

// The value of a "--key=" word: a ROM path or a --controllers= blob holding
// the literal is part of another word and cannot win.
// `fallback` covers a per-system customCommandLine with no such token
// (config/emulators/tools.conf is "%RUNCOMMAND%").
inline std::string launchArgument(const std::string& command, const std::string& key, const std::string& fallback)
{
	std::string value = launchToken(command, key + "=");
	return value.empty() ? fallback : value;
}

#endif // ES_APP_LAUNCH_COMMAND_H
