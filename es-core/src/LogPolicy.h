#pragma once
#ifndef ES_CORE_LOG_POLICY_H
#define ES_CORE_LOG_POLICY_H

// The log's pure rules, kept apart from Log.cpp's file, thread and Settings
// so a unit test can hold them (es-app/tests/unit/LogPolicyTests.cpp, fork
// #178). The shape of a line is a contract: tools/vm-qa and the fork's proofs
// grep es_log.txt for the level tags, so a word here is a word nothing else
// may change.

#include "Log.h"

#include <string>

namespace LogPolicy
{
	// The word a line carries for its level -- byte for byte what the file
	// has always held.
	inline const char* levelTag(LogLevel level)
	{
		switch (level)
		{
		case LogError:   return "ERROR";
		case LogWarning: return "WARNING";
		case LogDebug:   return "DEBUG";
		default:         return "INFO";
		}
	}

	// The LogLevel setting as Log::init reads it: "debug", "information",
	// "warning" (Settings.cpp's default, what a device runs at), "error";
	// no setting at all is "error"; any other word turns the log off (-1).
	// The Debug switch overrides all of this, but that is Settings'
	// business, not this rule's.
	inline int parseLogLevel(const std::string& setting)
	{
		if (setting == "debug")       return LogDebug;
		if (setting == "information") return LogInfo;
		if (setting == "warning")     return LogWarning;
		if (setting == "error")       return LogError;
		if (setting.empty())          return LogError;
		return -1;
	}

	// Which lines also go to stderr -- on a device, the journal: ERROR and
	// WARNING always (fork #178: a warning used to have the file alone, and
	// the file used to be eight batches late), and every line when the level
	// is debug, as before.
	inline bool mirrorToStderr(LogLevel level, bool debugToStderr)
	{
		return level <= LogWarning || debugToStderr;
	}
}

#endif // ES_CORE_LOG_POLICY_H
