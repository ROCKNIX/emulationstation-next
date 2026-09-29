#pragma once
// Double for es-app/src/ThreadedCloudSync.h: whether a sync card is up, and
// the stamps the code under test writes.
#include <ctime>
#include <string>
#include <vector>
#include "CloudText.h"
class Window;
class ThreadedCloudSync
{
public:
	enum class Origin { None, Startup, Exit, Manual };
	static void start(Window* window, const std::string& command,
	                  const std::string& title, const std::string& running = "",
	                  Origin origin = Origin::None);
	static bool isRunning();
	static void writeStamp(const std::string& path, int rc, const std::string& token, const std::string& why);
	static std::string whyForCode(int rc);
	static void restampStoppedParts(const std::string& command, time_t runStarted, const std::string& token);
	static void restampStoppedParts(const std::string& command, const std::vector<CloudText::StampText>& before,
		time_t runStarted, const std::string& token);
	static std::vector<CloudText::StampText> readStamps(const std::string& command);
};
