#pragma once
#ifndef ES_CORE_UTILS_COMMAND_LINE_UTIL_H
#define ES_CORE_UTILS_COMMAND_LINE_UTIL_H

#include <string>

// A launch command's option, replaced in place (the save state manager's
// "this state was made with another core": SaveState::setupSaveState).
// Header-only and pure, so es-unit-tests reaches it without SaveState's
// FileData and SystemConf (es-app/tests/unit/CommandLineTests.cpp).
namespace Utils
{
	namespace CommandLine
	{
		// Where each shell word of `line` starts and ends, outside quotes:
		// single quotes take everything to the next one, double quotes and a
		// bare word honour a backslash, and unquoted whitespace ends a word.
		// An unterminated quote runs to the end of the line.
		inline size_t wordEnd(const std::string& line, size_t pos)
		{
			char quote = 0;
			for (size_t i = pos; i < line.size(); i++)
			{
				const char c = line[i];
				if (quote == '\'')
				{
					if (c == '\'')
						quote = 0;
					continue;
				}
				if (c == '\\')
				{
					i++;   // the next character, whatever it is
					continue;
				}
				if (quote == '"')
				{
					if (c == '"')
						quote = 0;
					continue;
				}
				if (c == '\'' || c == '"')
					quote = c;
				else if (c == ' ' || c == '\t')
					return i;
			}
			return line.size();
		}

		// `line` with the value of the option `parameter` ("-core",
		// "-emulator") replaced by `value`. ROCKNIX passes it joined --
		// --core=<v> -- and the legacy launchers separate, -core <v>; either
		// is found only as a whole word of the command, never inside a quoted
		// argument such as the ROM's path (#308 F-CS-33: the joined form was
		// the last "--core=" anywhere in the line, its value ending at the
		// next literal space, so a ROM name spelling the same letters after
		// the real option was rewritten and could lose its closing quote).
		// The joined form's last word wins, as before; the separate form's
		// first. A line with neither comes back as it was.
		inline std::string replaceOptionValue(const std::string& line, const std::string& parameter, const std::string& value)
		{
			const std::string joined = "-" + parameter + "=";   // "-core" -> "--core="

			size_t joinedAt = std::string::npos;
			size_t separateAt = std::string::npos, separateValue = std::string::npos, separateEnd = std::string::npos;
			size_t i = 0;
			while (i < line.size())
			{
				if (line[i] == ' ' || line[i] == '\t')
				{
					i++;
					continue;
				}
				const size_t end = wordEnd(line, i);
				if (line.compare(i, joined.size(), joined) == 0)
					joinedAt = i;
				else if (separateAt == std::string::npos && end - i == parameter.size() && line.compare(i, parameter.size(), parameter) == 0)
				{
					size_t v = end;
					while (v < line.size() && (line[v] == ' ' || line[v] == '\t'))
						v++;
					if (v < line.size())
					{
						separateAt = i;
						separateValue = v;
						separateEnd = wordEnd(line, v);
					}
				}
				i = end;
			}

			if (joinedAt != std::string::npos)
			{
				const size_t from = joinedAt + joined.size();
				return line.substr(0, from) + value + line.substr(wordEnd(line, joinedAt));
			}
			if (separateAt != std::string::npos)
				return line.substr(0, separateValue) + value + line.substr(separateEnd);
			return line;
		}
	}
}

#endif // ES_CORE_UTILS_COMMAND_LINE_UTIL_H
