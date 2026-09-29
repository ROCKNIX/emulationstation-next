#pragma once
#ifndef ES_CORE_PLATFORM_H
#define ES_CORE_PLATFORM_H

#include <string>
#include <vector>

#ifdef WIN32
#include <Windows.h>
#include <intrin.h>

#define sleep Sleep
#endif

class Window;

namespace Utils
{
	namespace Platform
	{
		enum QuitMode
		{
			QUIT = 0,
			RESTART = 1,
			SHUTDOWN = 2,
			REBOOT = 3,
			FAST_SHUTDOWN = 4,
			FAST_REBOOT = 5
		};

		class ProcessStartInfo
		{
		public:
			ProcessStartInfo();
			ProcessStartInfo(const std::string& cmd);

			int run() const;

			std::string command;			
			bool waitForExit;
			bool showWindow;
			Window* window;
#ifndef WIN32
			std::string stderrFilename;
			std::string stdoutFilename;
#endif
		};

		int quitES(QuitMode mode = QuitMode::QUIT);
		bool isFastShutdown();
		void processQuitMode();

		struct BatteryInformation
		{
			BatteryInformation()
			{
				hasBattery = false;
				level = 0;
				isCharging = false;
			}

			bool hasBattery;
			int  level;
			bool isCharging;
		};

		BatteryInformation queryBatteryInformation();

		// One address on one interface, and whether that interface is a link
		// of the device's own -- up, with a carrier, not loopback, not
		// point-to-point -- as against a tunnel such as tailscale0, which
		// keeps its fixed address whether or not any network carries it
		// (fork #279). queryIPAddresses() is the physical ones, IPv4 first,
		// and is what every "is the device connected" question reads.
		struct InterfaceAddress { std::string address; std::string interface; bool physical; };
		std::vector<InterfaceAddress> queryInterfaceAddresses();
		std::vector<std::string> queryIPAddresses();
		std::string queryIPAddress();
		std::string getArchString();
		unsigned long long getTotalSystemMemory();

#if WIN32
		bool isWindows10();
		bool isWindows11();
		void setDpiAwareness();
#else
		bool isBuildroot();
#endif
		int runSystemCommand(const std::string& cmd_utf8, const std::string& name, Window* window); // run a utf-8 encoded in the shell (requires wstring conversion on Windows)
		std::string GetEnv(const std::string& var);
		std::string GetShOutput(const std::string& mStr);
		// GetShOutput concatenates its input into a single string, dropping the
		// last character of every chunk it reads -- which is the newline. That is
		// what its single-line callers want, and useless for a command that emits
		// one record per line. This returns the lines instead.
		std::vector<std::string> GetShOutputLines(const std::string& mStr);
	}
}

#endif // ES_CORE_PLATFORM_H
