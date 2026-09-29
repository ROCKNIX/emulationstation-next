//EmulationStation, a graphical front-end for ROM browsing. Created by Alec "Aloshi" Lofquist.
//http://www.aloshi.com

#include "services/HttpServerThread.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiCloudTransfer.h"
#include "SystemConf.h"
#include "guis/GuiSettings.h"
#include "utils/FileSystemUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "InputManager.h"
#include "Log.h"
#include "MameNames.h"
#include "Genres.h"
#include "utils/Platform.h"
#include "PowerSaver.h"
#include "Settings.h"
#include "SystemData.h"
#include "SystemScreenSaver.h"
#include <SDL_events.h>
#include <SDL_main.h>
#include <SDL_timer.h>
#include <iostream>
#include <time.h>
#include "LocaleES.h"
#include <SystemConf.h>
#include "ApiSystem.h"
#include "AudioManager.h"
#include "NetworkThread.h"
#include "scrapers/ThreadedScraper.h"
#include "ThreadedHasher.h"
#include "SaveStateBookkeeper.h"
#include <FreeImage.h>
#include "ImageIO.h"
#include "components/VideoVlcComponent.h"
#include <csignal>
#ifdef __GLIBC__
#include <execinfo.h>
#include <unistd.h>
#include <string.h>
#endif
#include "InputConfig.h"
#include "RetroAchievements.h"
#include "TextToSpeech.h"
#include "Paths.h"
#include "resources/TextureData.h"
#include "Scripting.h"
#include "watchers/WatchersManager.h"
#include "HttpReq.h"
#include <thread>
#include "ZaparooSupport.h"
#include "utils/ThreadPool.h"
#include "utils/StringUtil.h"
#include "LaunchCommand.h"
#include "ThreadedCloudSync.h"
#include "CloudExit.h"
#include "JourneyTiers.h"
#include "AppWindow.h"

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#endif

static std::string gPlayVideo;
static int gPlayVideoDuration = 0;
static bool enable_startup_game = true;

bool parseArgs(int argc, char* argv[])
{
	Paths::setExePath(argv[0]);

	// We need to process --home before any call to Settings::getInstance(), because settings are loaded from homepath
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--home") == 0)
		{
			if (i == argc - 1)
				continue;

			std::string arg = argv[i + 1];
			if (arg.find("-") == 0)
				continue;

			Paths::setHomePath(argv[i + 1]);
			break;
		}
	}

	for(int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--videoduration") == 0)
		{
			gPlayVideoDuration = atoi(argv[i + 1]);
			i++; // skip the argument value
		}
		else if (strcmp(argv[i], "--video") == 0)
		{
			gPlayVideo = argv[i + 1];
			i++; // skip the argument value
		}
		else if (strcmp(argv[i], "--monitor") == 0)
		{
			if (i >= argc - 1)
			{
				std::cerr << "Invalid monitor supplied.";
				return false;
			}

			int monitorId = atoi(argv[i + 1]);
			i++; // skip the argument value
			Settings::getInstance()->setInt("MonitorID", monitorId);
		}
		else if(strcmp(argv[i], "--resolution") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid resolution supplied.";
				return false;
			}

			int width = atoi(argv[i + 1]);
			int height = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("WindowWidth", width);
			Settings::getInstance()->setInt("WindowHeight", height);
			Settings::getInstance()->setBool("FullscreenBorderless", false);
		}else if(strcmp(argv[i], "--screensize") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid screensize supplied.";
				return false;
			}

			int width = atoi(argv[i + 1]);
			int height = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("ScreenWidth", width);
			Settings::getInstance()->setInt("ScreenHeight", height);
		}else if(strcmp(argv[i], "--screenoffset") == 0)
		{
			if(i >= argc - 2)
			{
				std::cerr << "Invalid screenoffset supplied.";
				return false;
			}

			int x = atoi(argv[i + 1]);
			int y = atoi(argv[i + 2]);
			i += 2; // skip the argument value
			Settings::getInstance()->setInt("ScreenOffsetX", x);
			Settings::getInstance()->setInt("ScreenOffsetY", y);
		}else if (strcmp(argv[i], "--screenrotate") == 0)
		{
			if (i >= argc - 1)
			{
				std::cerr << "Invalid screenrotate supplied.";
				return false;
			}

			int rotate = atoi(argv[i + 1]);
			++i; // skip the argument value
			Settings::getInstance()->setInt("ScreenRotate", rotate);
		}else if(strcmp(argv[i], "--gamelist-only") == 0)
		{
			Settings::getInstance()->setBool("ParseGamelistOnly", true);
		}else if(strcmp(argv[i], "--ignore-gamelist") == 0)
		{
			Settings::getInstance()->setBool("IgnoreGamelist", true);
		}else if(strcmp(argv[i], "--show-hidden-files") == 0)
		{
			Settings::setShowHiddenFiles(true);
		}else if(strcmp(argv[i], "--draw-framerate") == 0)
		{
			Settings::getInstance()->setBool("DrawFramerate", true);
		}else if(strcmp(argv[i], "--no-exit") == 0)
		{
			Settings::getInstance()->setBool("ShowExit", false);
		}else if(strcmp(argv[i], "--exit-on-reboot-required") == 0)
		{
			Settings::getInstance()->setBool("ExitOnRebootRequired", true);
		}else if(strcmp(argv[i], "--no-startup-game") == 0)
		{
		        enable_startup_game = false;
		}else if(strcmp(argv[i], "--no-splash") == 0)
		{
			Settings::getInstance()->setBool("SplashScreen", false);
		}else if(strcmp(argv[i], "--splash-image") == 0)
		{
		        if (i >= argc - 1)
			{
				std::cerr << "Invalid splash image supplied.";
				return false;
			}
			Settings::getInstance()->setString("AlternateSplashScreen", argv[i+1]);
			++i; // skip the argument value
		}else if(strcmp(argv[i], "--debug") == 0)
		{
			Settings::getInstance()->setBool("Debug", true);
			Settings::getInstance()->setBool("HideConsole", false);
		}
		else if (strcmp(argv[i], "--fullscreen-borderless") == 0)
		{
			Settings::getInstance()->setBool("FullscreenBorderless", true);
		}
		else if (strcmp(argv[i], "--fullscreen") == 0)
		{
		Settings::getInstance()->setBool("FullscreenBorderless", false);
		}
		else if(strcmp(argv[i], "--windowed") == 0)
		{
			Settings::getInstance()->setBool("Windowed", true);
		}else if(strcmp(argv[i], "--vsync") == 0)
		{
			bool vsync = (strcmp(argv[i + 1], "on") == 0 || strcmp(argv[i + 1], "1") == 0) ? true : false;
			Settings::getInstance()->setBool("VSync", vsync);
			i++; // skip vsync value
		}else if(strcmp(argv[i], "--max-vram") == 0)
		{
			int maxVRAM = atoi(argv[i + 1]);
			Settings::getInstance()->setInt("MaxVRAM", maxVRAM);
		}
		else if (strcmp(argv[i], "--anti-aliasing") == 0)
		{
			if (i >= argc - 1)
			{
				std::cerr << "Invalid anti-aliasing supplied.";
				return false;
			}

			int antiAliasing = atoi(argv[i + 1]);
			if (antiAliasing != 0 && antiAliasing != 2 && antiAliasing != 4)
			{
				std::cerr << "Invalid anti-aliasing supplied.";
				return false;
			}

			Settings::getInstance()->setInt("AntiAliasing", antiAliasing);
			i++;
		}
		else if (strcmp(argv[i], "--force-kiosk") == 0)
		{
			Settings::getInstance()->setBool("ForceKiosk", true);
		}
		else if (strcmp(argv[i], "--force-kid") == 0)
		{
			Settings::getInstance()->setBool("ForceKid", true);
		}
		else if (strcmp(argv[i], "--force-disable-filters") == 0)
		{
			Settings::getInstance()->setBool("ForceDisableFilters", true);
		}
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
#ifdef WIN32
			// This is a bit of a hack, but otherwise output will go to nowhere
			// when the application is compiled with the "WINDOWS" subsystem (which we usually are).
			// If you're an experienced Windows programmer and know how to do this
			// the right way, please submit a pull request!
			AttachConsole(ATTACH_PARENT_PROCESS);
			freopen("CONOUT$", "wb", stdout);
#endif
			std::cout <<
				"EmulationStation, a graphical front-end for ROM browsing.\n"
				"Written by Alec \"Aloshi\" Lofquist.\n"
				"Version " << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING << "\n\n"
				"Command line arguments:\n"
				"--resolution [width] [height]	try and force a particular resolution\n"
				"--gamelist-only			skip automatic game search, only read from gamelist.xml\n"
				"--ignore-gamelist		ignore the gamelist (useful for troubleshooting)\n"
				"--draw-framerate		display the framerate\n"
				"--no-exit			don't show the exit option in the menu\n"
				"--no-splash			don't show the splash screen\n"
				"--debug				more logging, show console on Windows\n"				
				"--windowed			not fullscreen, should be used with --resolution\n"
				"--vsync [1/on or 0/off]		turn vsync on or off (default is on)\n"
				"--max-vram [size]		Max VRAM to use in Mb before swapping. 0 for unlimited\n"
				"--anti-aliasing [0, 2 or 4]	set MSAA anti-aliasing to disabled, 2x or 4x\n"
				"--force-kid		Force the UI mode to be Kid\n"
				"--force-kiosk		Force the UI mode to be Kiosk\n"
				"--force-disable-filters		Force the UI to ignore applied filters in gamelist\n"
				"--home [path]		Directory to use as home path\n"
				"--help, -h			summon a sentient, angry tuba\n\n"
				"--monitor [index]			monitor index\n\n"				
				"More information available in README.md.\n";
			return false; //exit after printing help
		}
	}

	return true;
}

bool verifyHomeFolderExists()
{
	//make sure the config directory exists	
	std::string configDir = Paths::getUserEmulationStationPath();
	if(!Utils::FileSystem::exists(configDir))
	{
		std::cout << "Creating config directory \"" << configDir << "\"\n";
		Utils::FileSystem::createDirectory(configDir);
		if(!Utils::FileSystem::exists(configDir))
		{
			std::cerr << "Config directory could not be created!\n";
			return false;
		}
	}

	return true;
}

// Returns true if everything is OK,
bool loadSystemConfigFile(Window* window, const char** errorString)
{
	*errorString = NULL;

	StopWatch stopWatch("loadSystemConfigFile :", LogDebug);

	if(!SystemData::loadConfig(window))
	{
		LOG(LogError) << "Error while parsing systems configuration file!";
		*errorString = "IT LOOKS LIKE YOUR SYSTEMS CONFIGURATION FILE HAS NOT BEEN SET UP OR IS INVALID. YOU'LL NEED TO DO THIS BY HAND, UNFORTUNATELY.\n\n"
			"VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	if(SystemData::sSystemVector.size() == 0)
	{
		LOG(LogError) << "No systems found! Does at least one system have a game present? (check that extensions match!)\n(Also, make sure you've updated your es_systems.cfg for XML!)";
		*errorString = "WE CAN'T FIND ANY SYSTEMS!\n"
			"CHECK THAT YOUR PATHS ARE CORRECT IN THE SYSTEMS CONFIGURATION FILE, "
			"AND YOUR GAME DIRECTORY HAS AT LEAST ONE GAME WITH THE CORRECT EXTENSION.\n\n"
			"VISIT EMULATIONSTATION.ORG FOR MORE INFORMATION.";
		return false;
	}

	return true;
}

//called on exit, assuming we get far enough to have the log initialized
void onExit()
{
	// A deletion still on the worker is finished, then the log closes
	// (D-UI-073, third rule). Idempotent: main's tail has usually done it.
	SaveStateBookkeeper::shutdown();
	Log::close();
}

#ifdef WIN32
#define PATH_MAX MAX_PATH
#include <direct.h>
#endif

int setLocale(char * argv1)
{
#if WIN32
	std::locale::global(std::locale("en-US"));
#else
	if (Utils::FileSystem::exists("./locale/lang")) // for local builds
		EsLocale::init("", "./locale/lang");	
	else
		EsLocale::init("", "/usr/share/locale");	
#endif

	setlocale(LC_TIME, "");

	return 0;
}


// write(2) only: the one call a handler for a fault may make without
// asking what the faulting thread was holding.
static void crashWrite(const char* text)
{
	(void) !write(STDERR_FILENO, text, strlen(text));
}

// A fault (SIGSEGV, SIGFPE, SIGILL): say which, print the frames, die of it.
//
// Nothing here takes a lock or allocates. The handler used to LOG the
// signal and flush the log, and Log's mutex is a plain std::mutex -- so a
// fault raised while a thread was inside Log::write (the logger's own
// buffer, a bad pointer in a message) deadlocked the handler on the lock
// that thread still held, and a crash that essway would have restarted in
// seconds sat as a hang nothing on the device watches (audit #258 PL-003;
// D-SYS-006 is the watchdog question, still open). The signal's name goes
// to stderr with write(2), which is the journal on ROCKNIX; the log file
// says nothing about the fault, and the journal says everything.
//
// The frames, to stderr (the journal, where one log line had been the
// whole record of a crash -- fork #246). glibc's backtrace is not
// async-signal-safe either; a handler that has already decided the
// process is done can afford the risk, and a frame list that prints nine
// times in ten is worth more than a guaranteed silence. The addresses are
// symbolised on the build host with addr2line against the same build's
// unstripped binary (.claude/rules/device-builds.md, "Reading a crash").
//
// Then die of the signal itself, not of exit(): exit() ran the static
// destructors on the faulting thread, so the core the kernel kept
// described the teardown and not the fault, and the exit code hid the
// signal from systemd. The default action dumps at the fault. Installed
// with SA_RESETHAND (below), so a second fault inside this handler ends
// the process by the default action rather than re-entering it.
void signalHandler(int signum)
{
	crashWrite("EmulationStation: fatal signal ");
	crashWrite(signum == SIGSEGV ? "SIGSEGV" : signum == SIGFPE ? "SIGFPE" : signum == SIGILL ? "SIGILL" : "(other)");
	crashWrite(" received.\n");

#ifdef __GLIBC__
	{
		void* frames[64];
		const int count = backtrace(frames, 64);
		crashWrite("EmulationStation crash backtrace (innermost first; symbolise with addr2line):\n");
		backtrace_symbols_fd(frames, count, STDERR_FILENO);
	}
#endif

	signal(signum, SIG_DFL);
	raise(signum);
}

// Ctrl-C is not a fault, and it is not an exit() either. Before #246 every
// signal ended in exit(signum) from the handler; #246 moved SIGINT onto the
// fault path with the rest, which dropped the atexit hooks (the save state
// bookkeeper's join, D-UI-073) without saying so. Putting exit() back was
// tried first (audit #258 PL-003) and died of SIGSEGV in the teardown on
// guest d -- exit() from a handler skips the end of main(), so the static
// destructors met a window and a renderer still in use, which is the very
// thing #246's comment on the faults describes. So the handler only sets a
// flag, and the main loop ends the same way a QUIT event ends it: the
// end of main() runs, the bookkeeper is joined, the log is closed. On a
// device nothing sends SIGINT -- essway stops the unit with SIGTERM -- so
// this is the developer's terminal and the harness's kill.
static volatile sig_atomic_t sInterrupted = 0;

void interruptHandler(int)
{
	sInterrupted = 1;
}

void playVideo()
{
	ApiSystem::getInstance()->setReadyFlag(false);
	Settings::getInstance()->setBool("AlwaysOnTop", true);

	Window window;
	if (!window.init(true))
	{
		LOG(LogError) << "Window failed to initialize!";
		return;
	}

	Settings::getInstance()->setBool("VideoAudio", true);

	bool exitLoop = false;

	VideoVlcComponent vid(&window);
	vid.setVideo(gPlayVideo);
	vid.setOrigin(0.5f, 0.5f);
	vid.setPosition(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f);
	vid.setMaxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	vid.setOnVideoEnded([&exitLoop]()
	{
		exitLoop = true;
		return false;
	});

	window.pushGui(&vid);

	vid.onShow();
	vid.topWindow(true);

	int lastTime = SDL_GetTicks();
	int totalTime = 0;

	while (!exitLoop)
	{
		SDL_Event event;

		if (SDL_PollEvent(&event))
		{
			do
			{
				if (event.type == SDL_QUIT)
					return;
			} 
			while (SDL_PollEvent(&event));
		}

		int curTime = SDL_GetTicks();
		int deltaTime = curTime - lastTime;

		if (vid.isPlaying())
		{
			totalTime += deltaTime;

			if (gPlayVideoDuration > 0 && totalTime > gPlayVideoDuration * 100)
				break;
		}

		Transform4x4f transform = Transform4x4f::Identity();
		vid.update(deltaTime);
		vid.render(transform);

		Renderer::swapBuffers();

		if (ApiSystem::getInstance()->isReadyFlagSet())
			break;
	}

	window.deinit(true);
}

// The startup game's capture could not record: said as a toast once the
// window is up, in the words the exit after any game uses (fork #293).
static bool sStartupCaptureFailed = false;

void launchStartupGame()
{
	auto gamePath = SystemConf::getInstance()->get("global.bootgame.path");
	if (gamePath.empty() || !Utils::FileSystem::exists(gamePath))
		return;
	
	auto command = SystemConf::getInstance()->get("global.bootgame.cmd");
	if (!command.empty())
	{
		InputManager::getInstance()->init();
		command = Utils::String::replace(command, "%CONTROLLERSCONFIG%", InputManager::getInstance()->configureEmulators());

		time_t tstart = time(NULL);
		int exitCode = Utils::Platform::ProcessStartInfo(command).run();

		// A boot-launched session is a game exit too (fork #21 R5), and the
		// one FileData::launchGame never sees: a save it wrote has no entry
		// and an mtime below every later session's --started, so unless it is
		// recorded here nothing ever records it. No system is loaded yet
		// (loadSystemConfigFile runs later in main), so the stored command is
		// the only source -- the -P token is the system, --emulator=/--core=
		// the frozen pair, read the way runemu.sh reads them. A command with
		// no emulator token has nothing that writes a save (tools), so there
		// is nothing to record.
		std::string system = launchToken(command, "-P");
		std::string emulator = launchArgument(command, "--emulator", "");
		if (Utils::FileSystem::exists("/usr/bin/cloud_capture") && !system.empty() && !emulator.empty())
		{
			std::string capture = std::string("/usr/bin/cloud_capture")
				+ " --system "   + Utils::String::shellQuote(system)
				+ " --rom "      + Utils::String::shellQuote(gamePath)
				+ " --emulator " + Utils::String::shellQuote(emulator)
				+ " --core "     + Utils::String::shellQuote(launchArgument(command, "--core", ""))
				+ " --started "  + std::to_string(static_cast<long long>(tstart))
				+ " --exit "     + std::to_string(exitCode);
			int captureCode = ApiSystem::executeScriptLegacy(capture, nullptr).second;
			if (captureCode != 0)
			{
				LOG(LogWarning) << "cloud_capture exited " << captureCode << " after the startup game -- see /var/log/cloud_sync.log and /storage/.cache/cloud_sync/capture-failures";
				// Said once the window exists (fork #293 item 3): this runs
				// before the window is created.
				sStartupCaptureFailed = true;
			}
		}
	}
}

// The startup saves sync (fork #94), run from here so it is seen.
//
// It ran from autostart/102-cloud-saves until now: headless, beside
// EmulationStation's start, writing a log on tmpfs and two stamps and nothing
// on the screen. Maintainer, 2026-09-09: "How do I know if saves were synced
// during startup? It doesn't show any foreground identifier, like when you
// update game lists." The exit sync had a card and an outcome all along
// (ThreadedCloudSync); this gives the startup one the same card, and moves
// the run to the only process that can draw it. The autostart keeps the
// capture pass and drops the sync, so this is the one place it starts.
//
// The transfer is the autostart's: restore, then back up, both
// `copy --update --saves-only`, so the newest copy of every save ends up on
// both sides and nothing is deleted; both halves run whatever the first
// did, and the run's status is the first failure. Around it, what the
// autostart did not do, each because a card and a launch gate now hang on
// this command where nothing hung on the old one:
//
// - No default route, no wait. `ip route show default`, the same test
//   cloud_backup's check_network_link makes, and exit CloudExit::NoNetwork
//   at once -- the scripts' own "no network", so the card says SKIPPED -
//   NO NETWORK CONNECTION within a second and the launch gate is never
//   held on a device booted offline. The wait is for the other case: a
//   link that is up while the connection behind it has not settled.
// - The wait itself is cloud_net_ready's (fork #103): NetworkManager's
//   `connected`, held for a short grace, rather than the first ping that
//   gets through -- which on an SDIO Wi-Fi module is seconds after
//   association and the least stable moment there is (#102). It prints
//   ">>> doing network" once when it starts waiting, so the card can say
//   what that time is -- CHECKING THE CONNECTION... when the interface
//   already has a link, WAITING FOR A NETWORK, UP TO 60 SECONDS... when
//   it has none (fork #192) -- rather than Working..., and
//   gives up at 60 s with the no-network code. An image without it falls
//   back to the route check and probe loop, which prints the same line at
//   the first failed probe and keeps the same 60 s of wall clock, checked
//   before each sleep: the autostart's `seq 1 30` was described as a
//   minute but was not one -- each failed probe is ping's own -W2 plus the
//   2 s sleep, 4 s, so thirty of them are two minutes, longer when the
//   resolver, which -W does not bound, hangs on a link with no DNS behind
//   it. `timeout 4` bounds the probe, the clock bounds the loop.
//
// The command runs under setsid with a ">>> pid" first line, which
// ThreadedCloudSync gives every command (see there). A game launched while
// this sync runs cancels it in whatever phase it is in, waits for it to be
// gone, and goes ahead (#101, maintainer's decision, superseding the
// refusal of D-CLOUD-038/053 and #87 for the syncs EmulationStation starts
// on its own; FileData::launchGame has the reasoning). Behind it, the
// scripts' flock answers CloudExit::LockHeld to any second writer.
static void startStartupSavesSync(Window* window)
{
	if (SystemConf::getInstance()->get("cloudsaves.startup") != "1")
		return;
	if (!Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false)
		|| !Utils::FileSystem::exists("/usr/bin/cloud_restore")
		|| !Utils::FileSystem::exists("/usr/bin/cloud_backup"))
		return;

	// ThreadedCloudSync runs this under setsid and prints the ">>> pid" line
	// itself, for every command; it used to be done here, for this one.
	// The early exits are the scripts' own no-network code, spelled from
	// the constant so this shell cannot drift from what the card reads.
	//
	// Whether the network is ready is cloud_net_ready's question (fork
	// #103). It exits 0 once NetworkManager has reported connected and held
	// it for a short grace, and prints ">>> doing network" once if it has to
	// wait -- the line the probe loop printed, so the card and the launch
	// gate read it unchanged; the card's words for it follow the link the
	// interface sees (fork #192), and the bound it says is read back out of
	// the --wait below, so the two cannot drift. The loop it replaces started the sync on the
	// first ping that got through, which on an SDIO Wi-Fi module is seconds
	// after association and the least stable moment there is (#102); one
	// ping is not a settled connection either, only a packet that once made
	// it. Its exit code passes through: 69 is the no-network sentinel by
	// contract, at once when there is no default route and at the deadline
	// when the connection never settled, and anything else is a failure the
	// card should call one.
	//
	// The route check and probe loop stay as the fallback for an image
	// without cloud_net_ready, so this and the script can ship in either
	// order.
	//
	// Both halves run whatever the first did, and each reports itself to
	// the card as it ends (">>> tier <label>|<rc>"): a restore that finished
	// under a backup that did not is reported as COULDN'T FINISH with that
	// DID NOT FINISH, where the exit code alone read the whole run as
	// failed (D-UI-028). Each half also announces itself before it starts
	// (">>> doing receive", ">>> doing send"), which is what gives the card's
	// bar its two halves and its line the half's name -- RECEIVING, then
	// SENDING, in front of each compare count -- so the second "113 OF 113"
	// is visibly a different step from the first (D-UI-052, #157).
	const std::string noNetwork = std::to_string(CloudExit::NoNetwork);
	const std::string command =
		"if [ -x /usr/bin/cloud_net_ready ]; then"
		" /usr/bin/cloud_net_ready --wait 60; _w=$?; [ \"$_w\" = 0 ] || exit \"$_w\";"
		" else"
		" if ! ip -4 route show default 2>/dev/null | grep -q ."
		" && ! ip -6 route show default 2>/dev/null | grep -q .; then exit " + noNetwork + "; fi;"
		" _t0=$(date +%s); _up=0; _n=0;"
		" while :; do"
		" timeout 4 ping -q -c1 -W2 google.com >/dev/null 2>&1 && _up=1 && break;"
		" _n=$((_n+1)); [ \"$_n\" = 1 ] && echo \">>> doing network\";"
		" [ $(( $(date +%s) - _t0 )) -lt 60 ] || break;"
		" sleep 2;"
		" done;"
		" [ \"$_up\" = 1 ] || exit " + noNetwork + ";"
		" fi;"
		" echo \">>> doing receive\";"
		" /usr/bin/cloud_restore --yes --method=copy --update --saves-only --automatic; _r=$?;"
		" echo \">>> tier RESTORING SAVES|$_r\";"
		" echo \">>> doing send\";"
		" /usr/bin/cloud_backup --yes --method=copy --update --saves-only --automatic; _b=$?;"
		" echo \">>> tier BACKING UP SAVES|$_b\";"
		" [ \"$_r\" != 0 ] && exit \"$_r\"; exit \"$_b\"";

	// SYNC SAVES is the title the manual sync row already prints when it is
	// done; the running line says which sync this is, since the player did
	// not press anything to start it.
	ThreadedCloudSync::start(window, command, _("SYNC SAVES"), _("SYNCING SAVES AT STARTUP"),
		ThreadedCloudSync::Origin::Startup);
}

// #include "utils/MathExpr.h"

int main(int argc, char* argv[])
{
	// Utils::MathExpr::performUnitTests();

#ifdef WIN32
	// Must run before any window/message-queue APIs are touched.
	Utils::Platform::setDpiAwareness();
#endif

	// signal(SIGABRT, signalHandler);
	// The faults through sigaction with SA_RESETHAND: the handler runs once,
	// and a fault inside it -- a bad frame pointer under backtrace, say --
	// meets the default action instead of the handler again (#258 PL-003).
	{
		struct sigaction fault;
		memset(&fault, 0, sizeof(fault));
		fault.sa_handler = signalHandler;
		sigemptyset(&fault.sa_mask);
		fault.sa_flags = SA_RESETHAND | SA_NODEFER;
		sigaction(SIGFPE, &fault, nullptr);
		sigaction(SIGILL, &fault, nullptr);
		sigaction(SIGSEGV, &fault, nullptr);
	}
	signal(SIGINT, interruptHandler);
#ifdef __GLIBC__
	// backtrace() loads libgcc's unwinder on its first call; take that first
	// call here, while nothing is on fire, so the one in the handler does no
	// loading in a process that has just faulted (glibc's own advice).
	{
		void* warm[2];
		backtrace(warm, 2);
	}
#endif
	// signal(SIGTERM, signalHandler);

	srand((unsigned int)time(NULL));

	std::locale::global(std::locale("C"));

	if(!parseArgs(argc, argv))
		return 0;

	// only show the console on Windows if HideConsole is false
#ifdef WIN32
	// MSVC has a "SubSystem" option, with two primary options: "WINDOWS" and "CONSOLE".
	// In "WINDOWS" mode, no console is automatically created for us.  This is good,
	// because we can choose to only create the console window if the user explicitly
	// asks for it, preventing it from flashing open and then closing.
	// In "CONSOLE" mode, a console is always automatically created for us before we
	// enter main. In this case, we can only hide the console after the fact, which
	// will leave a brief flash.
	// TL;DR: You should compile ES under the "WINDOWS" subsystem.
	// I have no idea how this works with non-MSVC compilers.
	if(!Settings::getInstance()->getBool("HideConsole"))
	{
		// we want to show the console
		// if we're compiled in "CONSOLE" mode, this is already done.
		// if we're compiled in "WINDOWS" mode, no console is created for us automatically;
		// the user asked for one, so make one and then hook stdin/stdout/sterr up to it
		if(AllocConsole()) // should only pass in "WINDOWS" mode
		{
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "wb", stdout);
			freopen("CONOUT$", "wb", stderr);
		}
	}else{
		// we want to hide the console
		// if we're compiled with the "WINDOWS" subsystem, this is already done.
		// if we're compiled with the "CONSOLE" subsystem, a console is already created;
		// it'll flash open, but we hide it nearly immediately
		if(GetConsoleWindow()) // should only pass in "CONSOLE" mode
			ShowWindow(GetConsoleWindow(), SW_HIDE);
	}
#endif

	// call this ONLY when linking with FreeImage as a static library
#ifdef FREEIMAGE_LIB
	FreeImage_Initialise();
#endif

	//if ~/.emulationstation doesn't exist and cannot be created, bail
	if(!verifyHomeFolderExists())
		return 1;

	if (!gPlayVideo.empty())
	{
		playVideo();
		return 0;
	}

	//start the logger
	Log::init();	

	LOG(LogInfo) << "EmulationStation - v" << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING;

	//always close the log on exit
	atexit(&onExit);

	// Set locale
	setLocale(argv[0]);	

#if !WIN32
	if(enable_startup_game) {
	  // Run boot game, before Window Create for linux
	  launchStartupGame();
	}
#endif

	// Threaded initializations
	auto threadPool = new Utils::ThreadPool("main()", -3);
	auto vlcInit = threadPool->queueWorkItem([] { VideoVlcComponent::init(); });
	threadPool->queueWorkItem([] { ApiSystem::getInstance()->getIpAddress(); });
	threadPool->queueWorkItem([] { MetaDataList::initMetadata(); });
	threadPool->queueWorkItem([] { MameNames::init(); });
	threadPool->queueWorkItem([] { Genres::init(); });
	threadPool->queueWorkItem([] { HttpReq::resetCookies(); });
	threadPool->start();

	Window window;
	ViewController::init(&window);

	window.setReloadGamelistsCallback([&window] { ViewController::reloadAllGames(&window, true, true); });	
	window.pushGui(ViewController::get());
	if (!window.init(true, false))
	{
		LOG(LogError) << "Window failed to initialize!";
		return 1;
	}

	Renderer::setWindowResizable(false);

	bool splashScreen = Settings::getInstance()->getBool("SplashScreen");
	bool splashScreenProgress = Settings::getInstance()->getBool("SplashScreenProgress");

	if (splashScreen)
		window.renderSplashScreen(splashScreenProgress ? _("Loading system config...") : _("Loading..."));

	Scripting::fireEvent("start");

	SystemScreenSaver screensaver(&window);
	CollectionSystemManager::init(&window);
	
	Zaparoo::checkZaparooEnabledAsync();
	PowerSaver::init();
	InputConfig::AssignActionButtons();

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::PDFEXTRACTION))
		TextureData::PdfHandler = ApiSystem::getInstance();
	
	threadPool->waitAllExcept(vlcInit); // Wait for what's necessary for loadSystemConfigFile

	const char* errorMsg = NULL;
	if (!loadSystemConfigFile(splashScreen && splashScreenProgress ? &window : nullptr, &errorMsg))
	{
		// something went terribly wrong
		if (errorMsg == NULL)
		{
			LOG(LogError) << "Unknown error occured while parsing system config file.";
			Renderer::deinit();
			return 1;
		}

		// we can't handle es_systems.cfg file problems inside ES itself, so display the error message then quit
		window.pushGui(new GuiMsgBox(&window, errorMsg, _("QUIT"), [] { Utils::Platform::quitES(); }));
	}
	else if (!(splashScreen && splashScreenProgress))
	{
		// The loader starts the startup indexes -- INDEX NEW GAMES AT STARTUP,
		// the netplay one -- only when it is handed a window, and it is handed
		// one only for the splash screen's progress. ROCKNIX starts the
		// interface with --no-splash, so on its devices the setting never did
		// anything (fork #183). The window exists either way; start them here.
		SystemData::startIndexesAtStart(&window);
	}

	SystemConf* systemConf = SystemConf::getInstance();

#ifdef _ENABLE_KODI_
	if (systemConf->getBool("kodi.enabled", true) && systemConf->getBool("kodi.atstartup"))
	{
		if (splashScreen)
			window.closeSplashScreen();

		ApiSystem::getInstance()->launchKodi(&window);

		if (splashScreen)
		{
			window.renderSplashScreen("");
			splashScreen = false;
		}
	}
#endif

	// preload what we can right away instead of waiting for the user to select it
	// this makes for no delays when accessing content, but a longer startup time
	ViewController::get()->preload();

	// Initialize input
	InputManager::getInstance()->init();
	SDL_StopTextInput();

	NetworkThread* nthread = new NetworkThread(&window);
	HttpServerThread httpServer(&window);

	// tts
	TextToSpeech::getInstance()->enable(Settings::getInstance()->getBool("TTS"), false);
	
	if (errorMsg == NULL)
	{
		if (splashScreen)
			window.renderSplashScreen(_("Loading theme"));

		ViewController::get()->goToStart(true);
	}

	threadPool->wait();
	delete threadPool;

	window.closeSplashScreen();

	// Check if the device serial number is the same as stored in system.cfg.
	Utils::Platform::runSystemCommand("/usr/bin/serial_number_check", "Check Serial Number Script", &window);

	std::string markerFile = "/storage/serial_number_check_status";
	std::ifstream f(markerFile);
	std::string val;
	if (f.is_open())
	{
		std::getline(f, val);
		f.close();

		if (val == "1")
		window.pushGui(new GuiMsgBox(&window, "ROCKNIX IS FREE SOFTWARE.\n\n IF YOU PAID FOR ROCKNIX YOU HAVE BEEN SCAMMED.\n\n PLEASE REQUEST A REFUND FROM THE SELLER!", _("AGREE")));

		std::remove(markerFile.c_str());
	}

	// Two one-shot markers can both be waiting after a one-touch restore.
	// Order matters and is deliberate: a settings restore ships a
	// sanitized system.cfg with `wifi.key` removed, so it leaves the
	// device without Wi-Fi - and the journey continuation below downloads
	// from the cloud. Credentials are therefore pushed LAST so they land
	// on top and are dealt with first; only then does the player reach
	// the download prompt, by which time the network is back.
	//
	// The marker is backuptool's now: `backuptool restore --then-cloud`
	// touches it only after its extract has been verified (D-CLOUD-078), where
	// GuiMenu used to touch it before running the restore -- so a restore
	// that failed, or never ran, still produced YOUR SETTINGS WERE RESTORED
	// at the next boot. And it is consumed by the choice, not by the display:
	// removed on YES as the download starts and on LATER as the player
	// declines, so a crash or a power cut while the prompt is on screen
	// leaves it for the next boot rather than losing the continuation.
	std::string journeyMarker = "/storage/.config/.cloud-journey-pending";
	bool journeyPending = Utils::FileSystem::exists(journeyMarker);
	// What else the restore form had ticked (JourneyTiers, audit #307
	// PL-029): the continuation is built from it, and the prompt names it.
	// Read uncached, like every file another process leaves. A record with
	// no marker is a settings restore that failed or never ran, and goes.
	// A marker with no record is one an earlier build left, and keeps the
	// continuation and the prompt it always had (D-WORKFLOW-050).
	const std::string journeyRecord = JourneyTiers::PATH;
	JourneyTiers::Tiers journeyTiers;
	if (Utils::FileSystem::exists(journeyRecord, false))
	{
		if (journeyPending)
			journeyTiers = JourneyTiers::parse(Utils::FileSystem::readAllText(journeyRecord));
		else
			Utils::FileSystem::removeFile(journeyRecord);
	}
	if (journeyPending && journeyTiers.known && !journeyTiers.any())
	{
		// Nothing else was ticked: nothing to offer. This build's form asks
		// for no marker then; one left anyway is consumed here.
		LOG(LogInfo) << "journey: the marker's record names nothing to restore; consumed";
		std::remove(journeyMarker.c_str());
		Utils::FileSystem::removeFile(journeyRecord);
		journeyPending = false;
	}
	if (journeyPending)
	{
		std::string question;
		if (!journeyTiers.known)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nDOWNLOAD YOUR GAMES, BIOS FILES, AND SAVES FROM THE CLOUD NOW?");
		else if (journeyTiers.saves && journeyTiers.content && journeyTiers.media)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR SAVES, ROMS, BIOS, AND GAME CONTENT FROM THE CLOUD NOW?");
		else if (journeyTiers.saves && journeyTiers.content)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR SAVES, ROMS, AND BIOS FROM THE CLOUD NOW?");
		else if (journeyTiers.saves && journeyTiers.media)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR SAVES AND GAME CONTENT FROM THE CLOUD NOW?");
		else if (journeyTiers.content && journeyTiers.media)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR ROMS, BIOS, AND GAME CONTENT FROM THE CLOUD NOW?");
		else if (journeyTiers.saves)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR SAVES FROM THE CLOUD NOW?");
		else if (journeyTiers.content)
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR ROMS AND BIOS FROM THE CLOUD NOW?");
		else
			question = _("YOUR SETTINGS WERE RESTORED.\n\nRESTORE YOUR GAME CONTENT FROM THE CLOUD NOW?");
		window.pushGui(new GuiMsgBox(&window, question, _("YES"),
			[&window, journeyMarker, journeyRecord, journeyTiers] {
			std::remove(journeyMarker.c_str());
			Utils::FileSystem::removeFile(journeyRecord);
			// The first thing a new device does, on the transfer page every
			// other cloud run of this size uses (#114). It was a fullscreen
			// console until now: raw script output, no outcome, and nothing
			// to press when it went wrong.
			//
			// Each part reports itself as it ends and the run's status is
			// accumulated rather than taken from the last part
			// (JourneyTiers::command, the restore form's composition): ROMs
			// that could not be reached used to skip the saves silently, which
			// on a device with nothing on it is the half that matters most.
			const std::string cmd = JourneyTiers::command(journeyTiers);
			// How many items the page counts from before a script says. An
			// earlier build's continuation: the content script announces how
			// many systems it has, and the two single-item phases after it --
			// the saves restore and the settings-archive phase inside the same
			// script -- are what the count keeps room for. This build's: one
			// for saves, one per system the picker's selection names.
			int items = 2, itemsAfterContent = 2;
			if (journeyTiers.known)
			{
				items = journeyTiers.saves ? 1 : 0;
				itemsAfterContent = 0;
				if (journeyTiers.content || journeyTiers.media)
					for (auto& line : Utils::String::split(Utils::FileSystem::readAllText("/storage/.cache/cloud_sync/content-systems"), '\n', true))
						if (!Utils::String::trim(line).empty())
							items++;
			}
			LOG(LogInfo) << "journey: the continuation starts (" << (journeyTiers.known ? "the ticked tiers" : "an earlier build's marker: everything") << ")";
			window.pushGui(new GuiCloudTransfer(&window, cmd, _("RESTORING FROM THE CLOUD"), items, itemsAfterContent));
			}, _("LATER"), [journeyMarker, journeyRecord] {
			std::remove(journeyMarker.c_str());
			Utils::FileSystem::removeFile(journeyRecord);
			}));
	}

	// Either configuration file was found missing, empty or damaged at this
	// start and its last-known-good record was loaded and written back in its
	// place (Settings::loadFile, SystemConf::loadSystemConf; D-CLOUD-079).
	// Said once, here, where the interface is up to say it: the alternative
	// was the RG SP's morning -- every setting back at its default and no
	// word why (fork #102). Pushed before the one-shot prompts above so it
	// sits under them and is read after they are dealt with.
	if (Settings::wasRecovered() || SystemConf::wasRecovered())
		window.pushGui(new GuiMsgBox(&window, _("YOUR SETTINGS FILE WAS DAMAGED. THE LAST GOOD COPY WAS RESTORED."), _("OK")));

	// A settings restore that was cut off -- the power gone while the archive
	// was being written over the live tree -- was undone at this boot by
	// chksysconfig from the copy backuptool had taken aside, or could not be;
	// the marker says which (D-CLOUD-078, KILL18). Consumed on OK, not on
	// display, so a power cut with the message up leaves it for the next
	// boot. Read uncached: another process wrote it.
	const std::string revertedMarker = "/storage/.config/.restore-reverted";
	if (Utils::FileSystem::exists(revertedMarker, false))
	{
		const std::string how = Utils::String::trim(Utils::FileSystem::readAllText(revertedMarker));
		window.pushGui(new GuiMsgBox(&window, how == "reverted"
			? _("YOUR SETTINGS RESTORE WAS INTERRUPTED. YOUR PREVIOUS SETTINGS WERE PUT BACK. TRY THE RESTORE AGAIN.")
			: _("YOUR SETTINGS RESTORE WAS INTERRUPTED AND COULDN'T BE UNDONE. RESTORE YOUR SETTINGS AGAIN."),
			_("OK"), [revertedMarker] { std::remove(revertedMarker.c_str()); }));
	}

	// A finished backup restore leaves a one-shot marker (see backuptool).
	// The page itself clears it on FINISH, not here: consuming it on
	// display would lose the flow for good if the device crashed or the
	// player walked away mid-way.
	if (Utils::FileSystem::exists("/storage/.config/.restore-finish-pending"))
		GuiMenu::openRestoreRelink(&window, true);

	// A credential can go missing without a marker to say so (#109).
	//
	// Settings backups strip secrets on purpose, so a restore leaves the
	// RetroAchievements username behind with no password and no token.
	// `backuptool restore` writes the marker above and the page opens; a
	// restore done by hand over SSH -- which is how the RG SP was put back
	// together on 2026-09-09 -- writes no marker, and the account was
	// simply signed out for a day with nothing on any screen saying why.
	//
	// So notice the shape instead of waiting to be told about it: a
	// username with neither a password nor a token is an account that
	// cannot sign in, whatever lost the credential. The token is what an
	// earlier sign-in leaves behind, so a device that still has one is
	// still signed in and is not asked anything.
	//
	// Not while the marker's own flow is running -- that page is already
	// on the stack and covers this and every other credential. Once per
	// boot by construction: this runs once, and a NOT NOW is answered by
	// asking again at the next startup, by which time the account is
	// either back or still signed out.
	if (!Utils::FileSystem::exists("/storage/.config/.restore-finish-pending")
		&& !SystemConf::getInstance()->get("global.retroachievements.username").empty()
		&& SystemConf::getInstance()->get("global.retroachievements.password").empty()
		&& SystemConf::getInstance()->get("global.retroachievements.token").empty())
	{
		LOG(LogInfo) << "retroachievements: username set with no password and no token, offering re-entry";
		window.pushGui(new GuiMsgBox(&window,
			_("YOUR RETROACHIEVEMENTS PASSWORD IS MISSING, SO YOU'RE SIGNED OUT.\n\nENTER IT NOW? IF NOT, IT'S IN GAME SETTINGS > RETROACHIEVEMENTS SETTINGS."),
			_("YES"), [&window] { GuiMenu::openRestoreRelink(&window, false); },
			_("NOT NOW"), nullptr));
	}

	// Create a flag in  temporary directory to signal READY state
	ApiSystem::getInstance()->setReadyFlag();

	// Here and not earlier: this is the point where the interface is up --
	// the theme is loaded (goToStart), the splash has closed, the one-shot
	// boot prompts above are on the stack, and the READY flag has just said
	// so to everything outside. The main loop below draws the card from its
	// first frame; a start any earlier would put it over the splash, or on
	// screen before the theme it is styled by had loaded.
	//
	// Not after a one-touch restore. The journey prompt above offers
	// `cloud_content_restore --all && cloud_restore --yes` on this same
	// boot, and both take the sync lock: a startup sync already holding it
	// would turn the player's YES into "Another cloud sync is already
	// running. Skipped." in a console. That restore brings the saves down
	// anyway, so nothing is lost by sitting this boot out.
	if (sStartupCaptureFailed)
		window.displayNotificationMessage(_U("\uF0C2  ") + _("COULDN'T RECORD THIS SESSION'S SAVES. THEY'RE STILL ON THIS DEVICE."));
	if (!journeyPending)
		startStartupSavesSync(&window);

	// Play music
	AudioManager::getInstance()->init();

	if (ViewController::get()->getState().viewing == ViewController::GAME_LIST || ViewController::get()->getState().viewing == ViewController::SYSTEM_SELECT)
		AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme());
	else
		AudioManager::getInstance()->playRandomMusic();


#ifdef WIN32	
	DWORD displayFrequency = 60;

	DEVMODE lpDevMode;
	memset(&lpDevMode, 0, sizeof(DEVMODE));
	lpDevMode.dmSize = sizeof(DEVMODE);
	lpDevMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
	lpDevMode.dmDriverExtra = 0;

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode) != 0) {
		displayFrequency = lpDevMode.dmDisplayFrequency; // default value if cannot retrieve from user settings.
	}

	int timeLimit = (1000 / displayFrequency) - 10;	 // Margin for vsync
	if (timeLimit < 0)
		timeLimit = 0;
#endif

	Renderer::setWindowResizable(true);

	int lastTime = SDL_GetTicks();
	int ps_time = SDL_GetTicks();

	bool running = true;

#ifdef BATOCERA
	bool hotkeyPressed = false;
#endif

	while(running)
	{
		SDL_Event event;

		// SIGINT (interruptHandler): quit through the loop's own end, so
		// the teardown below runs on this thread (#258 PL-003). The waits
		// below are bounded (the screensaver's is 100 ms at most), so the
		// flag is seen promptly.
		if (sInterrupted)
		{
			LOG(LogInfo) << "SIGINT received; quitting";
			running = false;
			continue;
		}

		int screenSaverTimeout = screensaver.getNextUpdateTimeout();
		bool screenSaverWait = screenSaverTimeout > 0;

		bool ps_standby =
			!screenSaverWait &&
			PowerSaver::getState() &&
			(int)SDL_GetTicks() - ps_time > PowerSaver::getMode();

		int eventResult;

		if (screenSaverWait)
			eventResult = SDL_WaitEventTimeout(&event, screenSaverTimeout);
		else if (ps_standby)
			eventResult = SDL_WaitEventTimeout(&event, PowerSaver::getTimeout());
		else
			eventResult = SDL_PollEvent(&event);

		if (eventResult)
		{
			// PowerSaver can push events to exit SDL_WaitEventTimeout immediatly
			// Reset this event's state
			TRYCATCH("resetRefreshEvent", PowerSaver::resetRefreshEvent());

			do
			{
#ifdef BATOCERA
			  // global hotkeys
			  bool eventTaken = false;
			  if(event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP)
			    {
			      InputConfig* config = InputManager::getInstance()->getInputConfigByDevice(event.jbutton.which);
			      if(config)
				{
				  // Find first player controller info
				  auto playerDevices = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();
				  auto playerDevice = playerDevices.find(0);
				  if (playerDevice != playerDevices.cend())
				    {
				      if (config->getDeviceIndex() == playerDevice->second.index)
					{
					  Input input = Input(event.jbutton.which, TYPE_BUTTON, event.jbutton.button, event.jbutton.state == SDL_PRESSED, false);
					  if (config->isMappedTo("hotkey", input))
					    hotkeyPressed = input.value != 0;

					  if(hotkeyPressed && input.value != 0)
					    {
					      std::string hotkey_controlcenter = Settings::getInstance()->getString("HOTKEY_CONTROLCENTER");
					      if (config->isMappedTo(hotkey_controlcenter, input))
						{
						  hotkeyPressed = false;
						  ApiSystem::getInstance()->launchControlcenter();
						  eventTaken = true;
						}
					    }
					}
				    }
				}
			    }
			  //
			  if(eventTaken)
			    continue;
#endif

				TRYCATCH("InputManager::parseEvent", InputManager::getInstance()->parseEvent(event, &window));

				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED && Settings::getInstance()->getBool("Windowed"))
				{
					if (Renderer::onScreenSizeChanged(event.window.data1, event.window.data2))
					{
						Renderer::setWindowResizable(false);

						window.closeSplashScreen();

						while (window.peekGui() && window.peekGui() != ViewController::get())
							delete window.peekGui();

						ViewController::get()->reloadAll(&window);
						window.closeSplashScreen();

						Renderer::setWindowResizable(true);
					}
				}				

				if (event.type == SDL_QUIT)
					running = false;
			} 
			while(SDL_PollEvent(&event));

			// check guns
			InputManager::getInstance()->updateGuns(&window);

			// triggered if exiting from SDL_WaitEvent due to event
			if (ps_standby || (screenSaverWait && !screensaver.isScreenSaverActive()))
				// show as if continuing from last event
				lastTime = SDL_GetTicks();

			// reset counter
			ps_time = SDL_GetTicks();
		}
		else if (ps_standby == false)
		{
		  // check guns
		  InputManager::getInstance()->updateGuns(&window);

		  if (screenSaverWait)
			ps_time = SDL_GetTicks();

		  // If exitting SDL_WaitEventTimeout due to timeout. Trail considering
		  // timeout as an event
		  //	ps_time = SDL_GetTicks();
		}

		if (window.isSleeping())
		{
			lastTime = SDL_GetTicks();
			SDL_Delay(1); // this doesn't need to be accurate, we're just giving up our CPU time until something wakes us up
			continue;
		}

		int curTime = SDL_GetTicks();
		int deltaTime = curTime - lastTime;
		lastTime = curTime;

		// cap deltaTime if it ever goes negative
		if(deltaTime < 0)
			deltaTime = 1000;

		TRYCATCH("Window.update" ,window.update(deltaTime))	
		TRYCATCH("Window.render", window.render())

		int fpsLimit = Settings::FpsLimit();
		if (fpsLimit > 0)
		{
			int frameTime = (1000 + fpsLimit / 2) / fpsLimit;
			int processDuration = SDL_GetTicks() - curTime;
			if (processDuration < frameTime)
			{
				int timeToWait = frameTime - processDuration;
				if (timeToWait > 0 && timeToWait < 100)
					SDL_Delay(timeToWait);
			}
		}

		Renderer::swapBuffers();		
	}

	// The loop has ended: a worker that finishes from here on posts to
	// nothing (AppWindow; #308 8-es claude F-ES-26). Before any teardown, so
	// no post lands in a window on its way out.
	AppWindow::closing();

	if (Utils::Platform::isFastShutdown())
		Settings::getInstance()->setBool("IgnoreGamelist", true);

	WatchersManager::stop();
	ThreadedHasher::stop();
	ThreadedScraper::stop();
	SaveStateBookkeeper::shutdown();

	ApiSystem::getInstance()->deinit();

	while (window.peekGui() != ViewController::get())
		delete window.peekGui();

	if (SystemData::hasDirtySystems())
		window.renderSplashScreen(_("SAVING METADATA. PLEASE WAIT..."));

	MameNames::deinit();
	ViewController::saveState();
	CollectionSystemManager::deinit();
	SystemData::deleteSystems();
	Scripting::exitScriptingEngine();

	// call this ONLY when linking with FreeImage as a static library
#ifdef FREEIMAGE_LIB
	FreeImage_DeInitialise();
#endif
	
	// Delete ViewController
	while (window.peekGui() != nullptr)
		delete window.peekGui();

	window.deinit();

	Utils::Platform::processQuitMode();

	LOG(LogInfo) << "EmulationStation cleanly shutting down.";

	Log::flush();

	return 0;
}

