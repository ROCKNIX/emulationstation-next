#include "CaptureRotationText.h"

#include <cstdlib>

namespace
{
	// The record's second line since fork #288: the turn was read from a
	// log that held this game's launch alone.
	// The writer's claim about what it read. "from=own-launch" (fork #288)
	// was written from a launch that failed and from a log with no launch
	// in it, until the reading was checked (187ff9f1c, 190f62550): nothing
	// can tell those records from good ones, so the claim this build
	// trusts is a new one (audit of the fixes, E2 gpt G-E2-06).
	const char* OWN_LAUNCH_LINE = "from=checked-launch";

	// The value of `key = "value"` in a RetroArch config, or "" when absent.
	std::string configValue(const std::string& config, const std::string& key)
	{
		size_t pos = 0;
		while ((pos = config.find(key, pos)) != std::string::npos)
		{
			const bool atLineStart = pos == 0 || config[pos - 1] == '\n' || config[pos - 1] == '\r';
			const size_t after = pos + key.size();
			pos = after;
			if (!atLineStart)
				continue;
			size_t p = after;
			while (p < config.size() && (config[p] == ' ' || config[p] == '\t'))
				p++;
			if (p >= config.size() || config[p] != '=')
				continue;
			p++;
			while (p < config.size() && (config[p] == ' ' || config[p] == '\t'))
				p++;
			if (p < config.size() && config[p] == '"')
			{
				const size_t end = config.find('"', p + 1);
				return end == std::string::npos ? "" : config.substr(p + 1, end - p - 1);
			}
			const size_t end = config.find_first_of("\r\n", p);
			return config.substr(p, end == std::string::npos ? std::string::npos : end - p);
		}
		return "";
	}
}

namespace CaptureRotationText
{
	bool logHasLaunch(const std::string& launchLog)
	{
		return launchLog.find("=== Build ") != std::string::npos;
	}

	bool shouldRecord(const std::string& launchLog, const std::string& existingRecord, int turns, int tableTurns)
	{
		if (!logHasLaunch(launchLog))
			return false;
		if (existingRecord.empty())
			return turns != 0 || tableTurns != 0;
		return !(recordFromOwnLaunch(existingRecord) && parseRecord(existingRecord) == turns);
	}

	int turnsFromLog(const std::string& launchLog)
	{
		// Only the last launch's lines count (fork #280): the file held
		// every launch since it was last removed on a device whose log level
		// was none, and a vertical game's line at its end turned every game
		// exited after it. RetroArch prints its build banner once per
		// process, so the search starts at the last one; a log with none is
		// no launch's. (Not the content line: a core that loads its own
		// content -- fbneo, mame, the rotating ones -- gets "Content loading
		// skipped" instead.)
		const std::string banner = "=== Build ";
		const size_t launch = launchLog.rfind(banner);
		if (launch == std::string::npos)
			return -1;
		const std::string tag = "SET_ROTATION: \"";
		size_t pos = launchLog.rfind(tag);
		if (pos == std::string::npos || pos < launch)
			return -1;
		pos += tag.size();
		if (pos >= launchLog.size() || launchLog[pos] < '0' || launchLog[pos] > '9')
			return -1;
		return (launchLog[pos] - '0') % 4;
	}

	int fold(int coreTurns, const std::string& retroarchConfig)
	{
		int turns = coreTurns < 0 ? 0 : coreTurns % 4;
		const std::string allow = configValue(retroarchConfig, "video_allow_rotate");
		if (allow == "false")
			turns = 0;
		const std::string own = configValue(retroarchConfig, "video_rotation");
		if (!own.empty() && own[0] >= '0' && own[0] <= '9')
			turns = (turns + (own[0] - '0')) % 4;
		return turns;
	}

	int turnsFromTable(const std::string& table, const std::string& romName)
	{
		if (romName.empty())
			return 0;
		size_t pos = 0;
		while (pos < table.size())
		{
			size_t end = table.find('\n', pos);
			if (end == std::string::npos)
				end = table.size();
			const std::string line = table.substr(pos, end - pos);
			pos = end + 1;
			if (line.size() < romName.size() + 2 || line.compare(0, romName.size(), romName) != 0 || line[romName.size()] != ' ')
				continue;
			const char c = line[romName.size() + 1];
			if (c < '0' || c > '3')
				return 0;
			return c - '0';
		}
		return 0;
	}

	std::string recordText(int turns)
	{
		turns = ((turns % 4) + 4) % 4;
		return "turns=" + std::string(1, (char)('0' + turns)) + "\n" + OWN_LAUNCH_LINE + "\n";
	}

	bool recordFromOwnLaunch(const std::string& text)
	{
		// A line of its own, anywhere in the record, spaces and a carriage
		// return around it allowed; never a substring of a longer line.
		const std::string line(OWN_LAUNCH_LINE);
		size_t pos = 0;
		while (pos <= text.size())
		{
			size_t end = text.find('\n', pos);
			if (end == std::string::npos)
				end = text.size();
			size_t from = pos, to = end;
			while (from < to && (text[from] == ' ' || text[from] == '\t' || text[from] == '\r'))
				from++;
			while (to > from && (text[to - 1] == ' ' || text[to - 1] == '\t' || text[to - 1] == '\r'))
				to--;
			if (to - from == line.size() && text.compare(from, line.size(), line) == 0)
				return true;
			if (end >= text.size())
				break;
			pos = end + 1;
		}
		return false;
	}

	int parseRecord(const std::string& text)
	{
		size_t p = 0;
		while (p < text.size() && (text[p] == ' ' || text[p] == '\t' || text[p] == '\r' || text[p] == '\n'))
			p++;
		const std::string key = "turns=";
		if (text.compare(p, key.size(), key) == 0)
			p += key.size();
		if (p >= text.size() || text[p] < '0' || text[p] > '3')
			return 0;
		return text[p] - '0';
	}
}
