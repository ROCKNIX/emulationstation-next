#pragma once
// Double for es-core/src/Log.h: every line is kept, so a test can read the
// journal the code under test would have written.
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };
namespace FakeLog
{
	std::vector<std::string>& lines();
	std::mutex& lock();
	struct Line
	{
		std::ostringstream out;
		template <class T> Line& operator<<(const T& v) { out << v; return *this; }
		~Line() { std::lock_guard<std::mutex> g(lock()); lines().push_back(out.str()); }
	};
}
#define LOG(level) FakeLog::Line()
