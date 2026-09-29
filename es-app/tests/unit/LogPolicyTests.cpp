// The log's pure rules, checked without a device.
//
// Log.cpp owns a file, a thread and the Settings; what it decides about a
// line -- the word for its level, which setting means which level, whether
// the line also goes to stderr -- lives in LogPolicy.h so it can be held
// here. The shape of a line is a contract with tools/vm-qa and the fork's
// proofs, which grep es_log.txt for the tags.
//
// Fork #178. Run: see README.md beside this file.

#include "doctest/doctest.h"

#include "LogPolicy.h"

#include <string>

TEST_CASE("levelTag: the four words the file has always carried")
{
	CHECK(std::string(LogPolicy::levelTag(LogError)) == "ERROR");
	CHECK(std::string(LogPolicy::levelTag(LogWarning)) == "WARNING");
	CHECK(std::string(LogPolicy::levelTag(LogInfo)) == "INFO");
	CHECK(std::string(LogPolicy::levelTag(LogDebug)) == "DEBUG");
}

TEST_CASE("parseLogLevel: the LogLevel setting as Log::init reads it")
{
	CHECK(LogPolicy::parseLogLevel("debug") == LogDebug);
	CHECK(LogPolicy::parseLogLevel("information") == LogInfo);
	// Settings.cpp's default: what a device runs at.
	CHECK(LogPolicy::parseLogLevel("warning") == LogWarning);
	CHECK(LogPolicy::parseLogLevel("error") == LogError);
	// No setting at all is "error", not "off".
	CHECK(LogPolicy::parseLogLevel("") == LogError);
	// Any other word turns the log off.
	CHECK(LogPolicy::parseLogLevel("verbose") == -1);
}

TEST_CASE("mirrorToStderr: ERROR and WARNING reach the journal, the rest only at debug")
{
	// The second channel #178 was opened for: a WARNING used to have the
	// file alone, and the file used to be eight batches late.
	CHECK(LogPolicy::mirrorToStderr(LogError, false));
	CHECK(LogPolicy::mirrorToStderr(LogWarning, false));
	CHECK_FALSE(LogPolicy::mirrorToStderr(LogInfo, false));
	CHECK_FALSE(LogPolicy::mirrorToStderr(LogDebug, false));

	// At debug every line goes to stderr, as before.
	CHECK(LogPolicy::mirrorToStderr(LogInfo, true));
	CHECK(LogPolicy::mirrorToStderr(LogDebug, true));
}
