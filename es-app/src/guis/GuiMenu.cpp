#include "guis/GuiMenu.h"

#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiCollectionSystemsOptions.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiGeneralScreensaverOptions.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiHashStart.h"
#include "guis/GuiThemeInstaller.h"
#include "guis/GuiBezelInstaller.h"
#include "guis/GuiBatoceraStore.h"
#include "guis/GuiSettings.h"
#include "guis/GuiRetroAchievements.h"
#include "OfflineAchievements.h"
#include "guis/GuiGamelistOptions.h"
#include "guis/GuiImageViewer.h"
#include "guis/GuiMoonlight.h"
#include "ThreadedCloudSync.h"
#include "CloudExit.h"
#include "CloudText.h"
#include "WifiText.h"
#include "guis/GuiCloudTransfer.h"
#include "CloudTransferJob.h"
#include "math/Misc.h"
#include "guis/GuiLoading.h"
#include "guis/GuiNetPlaySettings.h"
#include "guis/GuiRetroAchievementsSettings.h"
#include "guis/GuiSystemInformation.h"
#include "guis/GuiControllersSettings.h"
#include "guis/GuiAnalogSticksLedControls.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "EmulationStation.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"

#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include "FavoriteMusicManager.h"
#include "guis/GuiFavoriteMusicSelector.h"

#include <LibretroRatio.h>
#include "guis/GuiUpdate.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "guis/GuiBackupStart.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiWifi.h"
#include "guis/GuiBluetoothPair.h"
#include "guis/GuiBluetoothDevices.h"
#include "scrapers/ThreadedScraper.h"
#include "FileSorts.h"
#include "ThreadedHasher.h"
#include "ThreadedBluetooth.h"
#include "views/gamelist/IGameListView.h"
#include "components/MultiLineMenuEntry.h"
#include "components/BatteryIndicatorComponent.h"
#include "GuiLoading.h"
#include "guis/GuiBios.h"
#include "guis/GuiKeyMappingEditor.h"
#include "Gamelist.h"
#include "TextToSpeech.h"
#include "Paths.h"
#include "JourneyTiers.h"
#include "AppWindow.h"
#include "utils/AtomicFileUtil.h"
#include <set> 
#include <fstream>

#if !WIN32
#include <vector>
#include <string>
#include <utility>
#include <array>
#include <map>
#include <memory>
#include <sstream>
#include <cstdio>
#include <thread>
#endif

#if WIN32
#include "Win32ApiSystem.h"
#endif

#define fake_gettext_fade _("fade")
#define fake_gettext_fastfade _("fast fade")
#define fake_gettext_slide _("slide")
#define fake_gettext_fastslide _("fast slide")
#define fake_gettext_fadeslide _("fade & slide")
#define fake_gettext_instant _("instant")

#define fake_gettext_system       _("System")
#define fake_gettext_architecture _("Architecture")
#define fake_gettext_diskformat   _("Disk format")
#define fake_gettext_temperature  _("Temperature")
#define fake_gettext_avail_memory _("Available memory")
#define fake_gettext_battery      _("Battery")
#define fake_gettext_model        _("Model")
#define fake_gettext_cpu_model    _("Cpu model")
#define fake_gettext_cpu_number   _("Cpu number")
#define fake_gettext_cpu_frequency _("Cpu max frequency")
#define fake_gettext_cpu_feature  _("Cpu feature")

#define fake_gettext_available_memory               _("Available Memory")
#define fake_gettext_display_resolution             _("Display Resolution")
#define fake_gettext_display_refresh_rate           _("Display Refresh Rate")
#define fake_gettext_opengl_driver_version          _("OpenGL Driver Version")
#define fake_gettext_vulkan_driver_name             _("Vulkan Driver Name")
#define fake_gettext_vulkan_driver_version          _("Vulkan Driver Version")
#define fake_gettext_data_partition_format          _("Data Partition Format")
#define fake_gettext_data_partition_available_space _("Data Partition Available Space")
#define fake_gettext_network_ip_address             _("Network IP Address")
#define fake_gettext_uefi_boot                      _("UEFI Boot")
#define fake_gettext_secure_boot                    _("Secure Boot")

#define fake_gettext_simple_bilinear_simple	pgettext("game_options", "SHARP-BILINEAR-SIMPLE")
#define fake_gettext_scanlines				pgettext("game_options", "SCANLINES")
#define fake_gettext_retro					pgettext("game_options", "RETRO")
#define fake_gettext_enhanced				pgettext("game_options", "ENHANCED")
#define fake_gettext_curvature				pgettext("game_options", "CURVATURE")
#define fake_gettext_zfast					pgettext("game_options", "ZFAST")
#define fake_gettext_flatten_glow			pgettext("game_options", "FLATTEN-GLOW")
#define fake_gettext_rgascaling				pgettext("game_options", "RGA SCALING")

#define fake_gettext_glvendor		_("VENDOR")
#define fake_gettext_glvrenderer	_("RENDERER")
#define fake_gettext_glversion		_("VERSION")
#define fake_gettext_glslversion	_("SHADERS")

#define fake_gettext_disk_internal _("INTERNAL")
#define fake_gettext_disk_external _("ANY EXTERNAL")

#define fake_gettext_resolution_max_1K  _("maximum 1920x1080")
#define fake_gettext_resolution_max_640 _("maximum 640x480")

GuiMenu::GuiMenu(Window *window, bool animate) : GuiComponent(window), mMenu(window, _("MAIN MENU").c_str())
{
	// MAIN MENU
	bool isFullUI = !UIModeController::getInstance()->isUIModeKid() && !UIModeController::getInstance()->isUIModeKiosk();

	// KODI >
	// GAMES SETTINGS >
	// CONTROLLER & BLUETOOTH >
	// USER INTERFACE SETTINGS >
	// SOUND SETTINGS >
	// NETWORK >
	// SCRAPER >
	// SYSTEM SETTINGS >
	// QUIT >

	// KODI
#ifdef _ENABLE_KODI_
	if (SystemConf::getInstance()->getBool("kodi.enabled", true) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
		addEntry(_("KODI MEDIA CENTER").c_str(), false, [this] 
	{ 
		Window *window = mWindow;
		delete this;
		if (!ApiSystem::getInstance()->launchKodi(window))
			LOG(LogWarning) << "Shutdown terminated with non-zero result!";

	}, "iconKodi");	
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) &&
		SystemConf::getInstance()->getBool("global.retroachievements") &&
		Settings::getInstance()->getBool("RetroachievementsMenuitem") && 
		SystemConf::getInstance()->get("global.retroachievements.username") != "")
		addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] {
				// Offline with OFFLINE ACHIEVEMENTS on, the page reads the
				// proxy's cache on this device (#180), so the network is not
				// a condition of opening it.
				if (!OfflineAchievements::proxyOffline() && !checkNetwork())
					return;
				GuiRetroAchievements::show(mWindow); }, "iconRetroachievements");
	
	if (isFullUI)
	{
#if defined(BATOCERA) || defined(ROCKNIX)
		addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");
		addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");
		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");
		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WIFI))
			addEntry(_("NETWORK SETTINGS").c_str(), true, [this] { openNetworkSettings(); }, "iconNetwork");
#else
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
			addEntry(_("GAME SETTINGS").c_str(), true, [this] { openGamesSettings(); }, "iconGames");

		addEntry(_("USER INTERFACE SETTINGS").c_str(), true, [this] { openUISettings(); }, "iconUI");

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))		
			addEntry(GuiControllersSettings::getControllersSettingsLabel(), true, [window] { GuiControllersSettings::openControllersSettings(window); }, "iconControllers");
		else
			addEntry(_("CONFIGURE INPUT"), true, [this] { openConfigInput(); }, "iconControllers");

		addEntry(_("SOUND SETTINGS").c_str(), true, [this] { openSoundSettings(); }, "iconSound");
		addEntry(_("GAME COLLECTION SETTINGS").c_str(), true, [this] { openCollectionSystemSettings(); }, "iconAdvanced");

		if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || system->getEmulators().size() == 0 || (system->getEmulators().size() == 1 && system->getEmulators().begin()->cores.size() <= 1))
					continue;

				addEntry(_("EMULATOR SETTINGS"), true, [this] { openEmulatorSettings(); }, "iconGames");
				break;
			}
		}
#endif

		addEntry(_("SCRAPER").c_str(), true, [this] { openScraperSettings(); }, "iconScraper");		

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE) || ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER) ||
			(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEBEZELPROJECT) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS)) ||
			ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
			addEntry(_("UPDATES & DOWNLOADS"), true, [this] { openUpdatesSettings(); }, "iconUpdates");

		addEntry(_("SYSTEM SETTINGS").c_str(), true, [this] { openSystemSettings(); }, "iconSystem");
	}
	else
	{
		addEntry(_("INFORMATION").c_str(), true, [this] { openSystemInformations(); }, "iconSystem");
		addEntry(_("UNLOCK USER INTERFACE MODE").c_str(), true, [this] { exitKidMode(); }, "iconAdvanced");
	}

#ifdef WIN32
	addEntry(_("QUIT"), !Settings::getInstance()->getBool("ShowOnlyExit") || !Settings::getInstance()->getBool("ShowExit"), [this] { openQuitMenu(); }, "iconQuit");
#else
	addEntry(_("QUIT").c_str(), true, [this] { openQuitMenu(); }, "iconQuit");
#endif
	
	addChild(&mMenu);
	addVersionInfo();
	setSize(mMenu.getSize());

	if (animate)
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			animateTo(Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2));
		else
			animateTo(Vector2f((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f));
	}
	else
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
		else
			setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
	}
}

// A maintenance script run headlessly, with the outcome on a dialog.
//
// The ten rows on this page -- the two settings-backup rows and the eight
// resets -- used to hand their command to /usr/bin/run: EmulationStation was
// stopped, the script scrolled past on a console for as long as it ran, and
// the UI came back. Nothing carried the result. runSystemCommand double-forks
// and always returns 0, and /usr/bin/run exited 0 on both halves of its
// console branch whatever the command did, so a factory reset that reset
// nothing and one that worked were the same press with the same ending: the
// carousel (#119).
//
// executeScriptLegacy hands back the real status and every line the script
// printed, so the work happens behind a spinner and ends in a message. The
// scripts print the why in the player's words at the point of failure
// (D-UI-028), so the failure sentence is the script's own last line where
// there is one, and the row's sentence only where the script died without
// saying anything.
static std::string maintenancePlainLine(const std::string& line)
{
	// The scripts colour their failure sentence for whoever is reading a
	// console, and the cloud pages' ">>> " protocol lines are not sentences.
	// Neither belongs on a dialog.
	std::string out;
	for (size_t i = 0; i < line.size(); i++)
	{
		if (line[i] == 0x1B)
		{
			while (i < line.size() && line[i] != 'm')
				i++;
			continue;
		}
		out += line[i];
	}
	return Utils::String::trim(out);
}

// The configuration on disk is no longer the one this process holds: a
// settings restore has replaced it and a factory reset has removed it. Re-read
// both before anything can write the old values back over what just happened
// -- ViewController::saveState() saves es_settings.cfg on the way out whenever
// the player has moved to another system since this boot, and that write
// carries every other setting with it (#114).
//
// Re-reading is not enough after a factory reset: there is nothing to read,
// Settings::loadFile leaves the old map as it stood, and the exit's
// ViewController::saveState would write it all back with LastSystem. So the
// exit is told to write nothing (#308 8-es claude F-ES-10). SystemConf needs
// no telling: saveSystemConf refuses when the live file cannot be opened.
static void maintenanceRestart()
{
	Settings::getInstance()->loadFile();
	SystemConf::getInstance()->loadSystemConf();
	ViewController::configurationReplaced();
	Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
}

// What a failed maintenance run's dialog says after COULDN'T FINISH. The
// why is the script's `>>> why` sentence (es-player-text.md, Outcome
// vocabulary); backuptool's fail() prints a fuller one -- the same words,
// then what is in place and how to recover -- as its last line, and that
// one is said when it carries the why. Any other last line is a tool's,
// printed after the why, and the why is said over it (#308 8-es claude
// F-ES-13: the last line used to win whatever it was). A script that
// printed no why is said by its last line, as before.
static std::string maintenanceWhy(const std::string& last, const std::string& why)
{
	if (why.empty())
		return last;
	if (Utils::String::startsWith(last, why))
		return last;
	return why;
}

static void runMaintenanceCommand(Window* window, const std::string& cmd, const std::string& busyTitle,
	const std::string& doneText, const std::string& failText, std::function<void()> onCompleted = nullptr)
{
	window->pushGui(new GuiLoading<std::pair<int, std::string>>(window, busyTitle,
		[cmd](auto gui)
		{
			std::string last;
			std::string why;
			int rc = ApiSystem::executeScriptLegacy(cmd, [&last, &why](const std::string line)
			{
				const std::string clean = maintenancePlainLine(line);
				if (clean.empty())
					return;
				if (clean.rfind(">>> why ", 0) == 0)
				{
					why = Utils::String::trim(clean.substr(8));
					return;
				}
				if (clean.rfind(">>> ", 0) == 0)
					return;
				last = clean;
			}).second;

			if (rc == 0)
				return std::pair<int, std::string>(0, "");
			return std::pair<int, std::string>(rc, maintenanceWhy(last, why));
		},
		[window, doneText, failText, onCompleted](std::pair<int, std::string> result)
		{
			if (result.first == 0)
			{
				window->pushGui(new GuiMsgBox(window, doneText, _("OK"),
					[onCompleted] { if (onCompleted != nullptr) onCompleted(); }));
				return;
			}

			const std::string why = result.second.empty() ? failText : Utils::String::toUpper(result.second);
			window->pushGui(new GuiMsgBox(window, _("COULDN'T FINISH") + " - " + why, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
		}));
}

void GuiMenu::openResetOptions()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("SYSTEM MANAGEMENT AND RESET").c_str());

	s->addGroup(_("DATA MANAGEMENT"));
	s->addEntry(_("BACK UP SETTINGS TO THIS DEVICE"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("BACK UP YOUR SETTINGS TO /storage/roms/backup/?\n\nWI-FI AND ACCOUNT PASSWORDS ARE NOT INCLUDED. COPY THE FILE SOMEWHERE SAFE, OR BACK UP TO THE CLOUD FROM GAME SETTINGS > MANAGE CLOUD STORAGE."), _("YES"),
		[window] {
		// backuptool names the archive it wrote to the system log only, so
		// the dialog says where the backups live rather than which file this
		// one is -- a filename is not something to read off a 3.5-inch panel.
		runMaintenanceCommand(window, "/usr/bin/backuptool backup", _("PACKING UP YOUR SETTINGS..."),
			_("SETTINGS BACKED UP TO THIS DEVICE.\n\nCOPY IT SOMEWHERE SAFE, OR BACK UP TO THE CLOUD FROM GAME SETTINGS > MANAGE CLOUD STORAGE."),
			_("THE BACKUP COULDN'T FINISH. YOUR LAST BACKUP IS UNCHANGED."));
		}, _("NO"), nullptr));
	});

	s->addEntry(_("RESTORE SETTINGS FROM THIS DEVICE"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("RESTORE SETTINGS FROM THE NEWEST BACKUP IN /storage/roms/backup/?\n\nYOUR EXISTING CONFIGURATION WILL BE OVERWRITTEN AND THE DEVICE WILL REBOOT. WI-FI AND ACCOUNT PASSWORDS MUST BE RE-ENTERED AFTERWARDS."), _("YES"),
		[window] {
		// --no-restart, and the restart happens after the message: left to
		// itself backuptool sleeps five seconds and reboots, which takes the
		// screen away at the moment it has the outcome on it (#114). Nothing
		// restarts on a failure.
		runMaintenanceCommand(window, "/usr/bin/backuptool restore --no-restart", _("RESTORING YOUR SETTINGS..."),
			_("SETTINGS RESTORED. THIS DEVICE RESTARTS SO THEY TAKE EFFECT."),
			_("THE RESTORE COULDN'T FINISH."), maintenanceRestart);
		}, _("NO"), nullptr));
	});

	// The cloud copies of these live in GAME SETTINGS > CLOUD SETTINGS, which does the
	// same job with the save data included. The two entries that used to be
	// here backed up settings only and were a second, quieter path to the
	// same operation.


	s->addEntry(_("CLEAN GAMELISTS & REMOVE UNUSED MEDIA"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE?"), _("YES"), [&]
	{
		int idx = 0;
		for (auto system : SystemData::sSystemVector)
		{
			cleanupGamelist(system);
			idx++;
		}
		}, _("NO"), nullptr));
	});

	s->addGroup(_("EMULATOR MANAGEMENT"));
	s->addEntry(_("RESET RETROARCH CONFIG TO DEFAULT"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: RETROARCH CONFIG WILL RESET TO DEFAULT\n\nPER-CORE CONFIGURATIONS WILL NOT BE AFFECTED AND NO BACKUP WILL BE CREATED!\n\nRESET RETROARCH CONFIG TO DEFAULT?"), _("YES"),
		[window] {
		runMaintenanceCommand(window, "/usr/bin/factoryreset retroarch", _("RESETTING RETROARCH..."),
			_("RETROARCH CONFIG RESET TO DEFAULT."),
			_("THE RETROARCH CONFIG COULDN'T BE RESET."));
		}, _("NO"), nullptr));
	});

	s->addEntry(_("RESET OVERLAYS (CORES, CHEATS, JOYPADS, ETC)"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: ALL CUSTOM RETROARCH OVERLAYS WILL BE REMOVED\n\nCUSTOM CORES, JOYSTICKS, CHEATS, ETC. NO BACKUP WILL BE CREATED!\n\nRESET RETROARCH OVERLAYS TO DEFAULT?"), _("YES"),
		[window] {
		// The overlay mounts are remade at boot, so this one has always ended
		// in a restart. The script does it only when nobody asked to be left
		// in charge of it; here the message comes first.
		runMaintenanceCommand(window, "/usr/bin/factoryreset overlays --no-restart", _("RESETTING YOUR CUSTOM FILES..."),
			_("OVERLAYS RESET TO DEFAULT. THIS DEVICE RESTARTS SO THEY TAKE EFFECT."),
			_("THE OVERLAYS COULDN'T BE RESET."), maintenanceRestart);
		}, _("NO"), nullptr));
	});

	s->addEntry(_("FULLY RESET RETROARCH"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: RETROARCH AND ALL USER SAVED CONFIGURATIONS WILL RESET TO DEFAULT\n\nPER-CORE CONFIGURATIONS WILL BE REMOVED AND NO BACKUP WILL BE CREATED!\n\nRESET RETROARCH?"), _("YES"),
		[window] {
		runMaintenanceCommand(window, "/usr/bin/factoryreset retroarch-full && /usr/bin/factoryreset overlays --no-restart",
			_("RESETTING RETROARCH..."),
			_("RETROARCH RESET TO DEFAULT. THIS DEVICE RESTARTS SO IT TAKES EFFECT."),
			_("RETROARCH COULDN'T BE FULLY RESET."), maintenanceRestart);
		}, _("NO"), nullptr));
	});

	s->addEntry(_("RESET MEDNAFEN CONFIG TO DEFAULT"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: MEDNAFEN CONFIG WILL RESET TO DEFAULT\n\nNO BACKUP WILL BE CREATED!\n\nRESET MEDNAFEN CONFIG TO DEFAULT?"), _("YES"),
		[window] {
		// The only reset that removes one file and copies nothing back, so
		// this is the only failure sentence here that can promise the rest of
		// the device is as it was.
		runMaintenanceCommand(window, "/usr/bin/factoryreset mednafen", _("RESETTING MEDNAFEN..."),
			_("MEDNAFEN CONFIG RESET TO DEFAULT."),
			_("THE MEDNAFEN CONFIG COULDN'T BE RESET. NOTHING ELSE WAS CHANGED."));
		}, _("NO"), nullptr));
	});

	s->addEntry(_("RESET STANDALONE EMULATOR CONFIGS TO DEFAULT"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: STANDALONE EMULATOR CONFIGS WILL RESET TO DEFAULT\n\nNO BACKUP WILL BE CREATED!\n\nRESET STANDALONE EMULATOR CONFIGS TO DEFAULT?"), _("YES"),
		[window] {
		runMaintenanceCommand(window, "/usr/bin/factoryreset standalone", _("RESETTING THE STANDALONE EMULATORS..."),
			_("STANDALONE EMULATOR CONFIGS RESET TO DEFAULT."),
			_("THE STANDALONE EMULATOR CONFIGS COULDN'T ALL BE RESET."));
		}, _("NO"), nullptr));
	});

        s->addEntry(_("FULLY RESET PORTMASTER"), true, [window] {
        window->pushGui(new GuiMsgBox(window, _("WARNING: PORTMASTER WILL RESET TO DEFAULT\n\nNO BACKUP WILL BE CREATED!\n\nRESET PORTMASTER TO DEFAULT?"), _("YES"),
                [window] {
                runMaintenanceCommand(window, "/usr/bin/factoryreset portmaster", _("RESETTING PORTMASTER..."),
                        _("PORTMASTER RESET TO DEFAULT."),
                        _("PORTMASTER COULDN'T BE RESET."));
                }, _("NO"), nullptr));
        });

	s->addGroup(_("SYSTEM MANAGEMENT"));

	s->addEntry(_("AUDIO RESET"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: AUDIO SETTINGS WILL BE RESET TO DEFAULTS AND THE SYSTEM WILL REBOOT!\n\nRESET AUDIO AND RESTART?"), _("YES"),
		[window] {
		runMaintenanceCommand(window, "/usr/bin/factoryreset audio --no-restart", _("RESETTING THE AUDIO SETTINGS..."),
			_("AUDIO SETTINGS RESET. THIS DEVICE RESTARTS SO THEY TAKE EFFECT."),
			_("THE AUDIO SETTINGS COULDN'T BE RESET."), maintenanceRestart);
		}, _("NO"), nullptr));
	});

	if (Utils::Platform::GetEnv("DEVICE_RK817_FUEL_GAUGE") == "true") {
		s->addEntry(_("RESET BATTERY FUEL GAUGE"), true, [window] {
		window->pushGui(new GuiMsgBox(window, _("WARNING: THE PMIC FUEL GAUGE WILL BE RESET AND THE SYSTEM WILL POWER OFF!\n\nLEAVE THE CHARGER UNPLUGGED AND POWER THE DEVICE BACK ON AFTER A FEW SECONDS. THE BATTERY LEVEL IS RE-ESTIMATED FROM THE RESTING BATTERY VOLTAGE AND BECOMES ACCURATE AGAIN AFTER A FULL CHARGE.\n\nRESET FUEL GAUGE AND POWER OFF?"), _("YES"),
			[] {
			Utils::Platform::runSystemCommand("/usr/bin/run \"/usr/bin/factoryreset rk817-fuelgauge\"", "", nullptr);
			}, _("NO"), nullptr));
		});
	}

	s->addEntry(_("FACTORY RESET"), true, [window] {
	window->pushGui(new GuiMsgBox(window, _("WARNING: YOUR DATA AND ALL OTHER CONFIGURATIONS WILL BE RESET TO DEFAULTS!\n\nIF YOU WANT TO KEEP YOUR SETTINGS MAKE A BACKUP AND SAVE IT ON AN EXTERNAL DRIVE BEFORE RUNNING THIS OPTION!\n\nEJECT YOUR GAME CARD BEFORE PROCEEDING!\n\nRESET SYSTEM AND RESTART?"), _("YES"),
		[window] {
		// No "nothing else was changed" anywhere on this row: by the time it
		// can fail, most of /storage is gone, and saying otherwise would be a
		// lie at the worst possible moment.
		runMaintenanceCommand(window, "/usr/bin/factoryreset ALL --no-restart", _("RESETTING THIS DEVICE..."),
			_("THIS DEVICE HAS BEEN RESET. IT RESTARTS NOW."),
			_("THE RESET COULDN'T FINISH."), maintenanceRestart);
		}, _("NO"), nullptr));
	});

	mWindow->pushGui(s);
}

void GuiMenu::openScraperSettings()
{		
	mWindow->pushGui(new GuiScraperStart(mWindow));
}

void GuiMenu::openConfigInput()
{
	Window* window = mWindow;
	window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO CONFIGURE THE INPUT?"), 
		_("YES"), [window] { window->pushGui(new GuiDetectDevice(window, false, nullptr)); }, 
		_("NO"), nullptr)
	);
}

void GuiMenu::addVersionInfo()
{
	std::string  buildDate = (Settings::getInstance()->getBool("Debug") ? std::string( "   (" + Utils::String::toUpper(PROGRAM_BUILT_STRING) + ")") : (""));

	auto theme = ThemeData::getMenuTheme();

	mVersion = std::make_shared<TextComponent>(mWindow);
	mVersion->setFont(theme->Footer.font);
	mVersion->setColor(theme->Footer.color);

	mVersion->setLineSpacing(0);

	std::string label;

	if (!ApiSystem::getInstance()->getVersion().empty())
	{
		if (ApiSystem::getInstance()->getApplicationName() == "ROCKNIX")
			label = "ROCKNIX " + ApiSystem::getInstance()->getVersion() + " (" + ApiSystem::getInstance()->getVersion(true) + ")";
		else
		{
			std::string aboutInfo = ApiSystem::getInstance()->getApplicationName() + " V" + ApiSystem::getInstance()->getVersion();
			label = aboutInfo + buildDate;
		}		
	}
		
	if (!label.empty())
	{
		if (Renderer::ScreenSettings::fullScreenMenus())
		{
			mMenu.setSubTitle(label);
			mMenu.addButton(_("BACK"), _("go back"), [&] { delete this; });
		}
		else
		{
			mVersion->setHorizontalAlignment(ALIGN_CENTER);
			mVersion->setVerticalAlignment(ALIGN_CENTER);
			mVersion->setText(label);
			mMenu.setButtonGrid(mVersion);
		}
	}
}

void GuiMenu::openScreensaverOptions() 
{
	mWindow->pushGui(new GuiGeneralScreensaverOptions(mWindow));
}

void GuiMenu::openCollectionSystemSettings() 
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHILE THE SCRAPER IS RUNNING")));
		return;
	}

	mWindow->pushGui(new GuiCollectionSystemsOptions(mWindow));
}

void GuiMenu::addEntry(const std::string& name, bool add_arrow, const std::function<void()>& func, const std::string iconName)
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	ComponentListRow row;

	MenuComponent::addMenuIcon(mWindow, row, iconName);

	auto text = std::make_shared<TextComponent>(mWindow, name, font, color);
	row.addElement(text, true);

	if (EsLocale::isRTL())
		text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);

		if (EsLocale::isRTL())
			bracket->setFlipX(true);

		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);
	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); 
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"))); 
	prompts.push_back(HelpPrompt("start", _("CLOSE"), [&] { delete this; }));
	return prompts;
}

class ExitKidModeMsgBox : public GuiSettings
{
	public: ExitKidModeMsgBox(Window* window, const std::string& title, const std::string& text) : GuiSettings(window, title) { addEntry(text); }

	bool input(InputConfig* config, Input input) override
	{
		Window* window = mWindow;
		if (UIModeController::getInstance()->listen(config, input))
		{
			// window->pushGui(new GuiMsgBox(window, _("THE USER INTERFACE MODE IS NOW UNLOCKED"), _("OK")));
			return true;
		}

		return GuiComponent::input(config, input);
	}
};

void GuiMenu::exitKidMode()
{
	if (Settings::getInstance()->getString("UIMode") == "Basic")
		Settings::getInstance()->setString("UIMode", "Full");
	else
		mWindow->pushGui(new ExitKidModeMsgBox(mWindow, _("UNLOCK USER INTERFACE MODE"), _("ENTER THE CODE NOW TO UNLOCK")));
}

void GuiMenu::openSystemInformations()
{
	mWindow->pushGui(new GuiSystemInformation(mWindow));
}

void GuiMenu::openServicesSettings()
{
	auto s = new GuiSettings(mWindow, _("SERVICES").c_str());

	auto services = ApiSystem::getInstance()->getServices();
	for(unsigned int i = 0; i < services.size(); i++) {
	  auto service_enabled = std::make_shared<SwitchComponent>(mWindow);
	  service_enabled->setState(services[i].enabled);
	  s->addWithLabel(services[i].name, service_enabled);
	  service_enabled->setOnChangedCallback([services, i, service_enabled]()
	  {
	    ApiSystem::getInstance()->enableService(services[i].name, service_enabled->getState());
	  });
	}

	mWindow->pushGui(s);
}

void GuiMenu::openDmdSettings()
{
	auto s = new GuiSettings(mWindow, _("DMD").c_str());
	Window* window = mWindow;

	// server
	auto services = ApiSystem::getInstance()->getServices();
	std::string current_server = "";
	for(unsigned int i = 0; i < services.size(); i++) {
	  if(services[i].enabled) {
	    if(services[i].name == "dmd_real")      current_server = "dmd_real";
	    if(services[i].name == "dmd_simulator") current_server = "dmd_simulator";
	  }
	}
	auto server = std::make_shared< OptionListComponent<std::string> >(window, _("SERVER"), false);
	server->addRange({ { _("DISABLED"), "" }, { _("DMDSERVER (for real dmd)"), "dmd_real" }, { _("SIMULATOR (for web dmd)"), "dmd_simulator" } }, current_server);
	s->addWithDescription(_("SERVER"), _("dmd server"), server);

	// format
	auto format = std::make_shared< OptionListComponent<std::string> >(window, _("FORMAT"), false);
	std::string current_format = SystemConf::getInstance()->get("dmd.format");
	format->addRange({ { _("AUTO"), "" }, { "SD", "sd" }, { "HD", "hd" } }, current_format);
	s->addWithDescription(_("FORMAT"), _("dmd matrix size"), format);

	s->addGroup("ZEDMD");

	// zedmd.brightness
	auto zedmd_brightness = std::make_shared< OptionListComponent<std::string> >(window, _("BRIGHTNESS"), false);
	std::string current_zedmd_brightness = SystemConf::getInstance()->get("dmd.zedmd.brightness");
	zedmd_brightness->addRange({ { _("AUTO"), "" }, { "0", "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" }, { "7", "7" }, { "8", "8" }, { "9", "9" }, { "10", "10" }, { "11", "11" }, { "12", "12" }, { "13", "13" }, { "14", "14" }, { "15", "15" } }, current_zedmd_brightness);
	s->addWithLabel(_("BRIGHTNESS"), zedmd_brightness);

	s->addSaveFunc([window, server, format, zedmd_brightness, current_server, current_format, current_zedmd_brightness] {
	  bool needRestart = false;
	  bool needSave    = false;

	  if(current_format != format->getSelected()) {
	    SystemConf::getInstance()->set("dmd.format", format->getSelected());
	    needSave = true;
	  }
	  if(current_zedmd_brightness != zedmd_brightness->getSelected()) {
	    SystemConf::getInstance()->set("dmd.zedmd.brightness", zedmd_brightness->getSelected());
	    needRestart = true;
	    needSave = true;
	  }

	  if(server->getSelected() != current_server) {
	    needRestart = true;
	  }

	  if(needSave) {
	    SystemConf::getInstance()->saveSystemConf();
	  }

	  if(needRestart) {
	    bool stopped = false;
	    bool started = false;

	    // stop the existing server
	    if(current_server != "") {
	      ApiSystem::getInstance()->enableService(current_server, false);
	      stopped = true;
	    }
	    // start the new server
	    if(server->getSelected() != "") {
	      ApiSystem::getInstance()->enableService(server->getSelected(), true);
	      started = true;
	    }

	    if(stopped && !started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER stopped"));
	    } else if(stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER restarted"));
	    } else if(!stopped && started) {
	      window->displayNotificationMessage(_U("\uF011  ") + _("DMDSERVER started"));
	    }
	  }
	});

	window->pushGui(s);
}

void GuiMenu::openMultiScreensSettings()
{
	auto s = new GuiSettings(mWindow, _("MULTISCREENS").c_str());
	Window* window = mWindow;

#ifdef BATOCERA
	s->addGroup(_("BACKGLASS / INFORMATION SCREEN"));
	
	// video device2
	std::vector<std::string> availableVideo2 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo2.size())
	{
	        if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BACKGLASS)) {
			// theme
			auto themes = ApiSystem::getInstance()->backglassThemes();
			auto selectedTheme = SystemConf::getInstance()->get("backglass.theme");
			auto theme = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME"), false);
			
			std::vector<std::string> themeList;
			for (auto it = themes.begin(); it != themes.end(); it++)
			  themeList.push_back(*it);
			std::sort(themeList.begin(), themeList.end(), [](const std::string& a, const std::string& b) -> bool { return Utils::String::toLower(a).compare(Utils::String::toLower(b)) < 0; });

			theme->add(_("AUTO"), "auto", selectedTheme == "" || selectedTheme == "auto");
			for (auto themeName : themeList)
			  theme->add(themeName, themeName, themeName == selectedTheme);
			
			s->addWithLabel(_("THEME"), theme);
			s->addSaveFunc([theme]
			{
			  std::string oldTheme = SystemConf::getInstance()->get("backglass.theme");
			  if (oldTheme != theme->getSelected()) {
			    SystemConf::getInstance()->set("backglass.theme", theme->getSelected());
			    SystemConf::getInstance()->saveSystemConf();
			    ApiSystem::getInstance()->restartBackglass();
			  }
			});
		}

		auto optionsVideo2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice2 = SystemConf::getInstance()->get("global.videooutput2");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice2.empty()) currentDevice2 = "auto";

		bool vfound = false;
		for (auto it = availableVideo2.begin(); it != availableVideo2.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo2->add((*it), (*it), currentDevice2 == (*it));
			if (currentDevice2 == (*it))
				vfound = true;
		}

		if (!vfound && currentDevice2 != "none")
			optionsVideo2->add(currentDevice2, currentDevice2, true);
		// add the none value
		optionsVideo2->add(_("NONE"), "none", currentDevice2 == "none");

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo2);
		s->addSaveFunc([this, optionsVideo2, currentDevice2, s] 
		{
			if (optionsVideo2->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput2", optionsVideo2->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});

		// video resolution2
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
		  auto videoModeOptionList2 = createVideoResolutionModeOptionList(mWindow, "es", "resolution2", optionsVideo2->getSelected() == "auto" ? "none" : optionsVideo2->getSelected());
		  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList2);
		  s->addSaveFunc([this, videoModeOptionList2, s] {
		    if(videoModeOptionList2->changed()) {
		      SystemConf::getInstance()->set("es.resolution2", videoModeOptionList2->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		  });
		}

		// video rotation2
		auto optionsRotation2 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

		std::string selectedRotation2 = SystemConf::getInstance()->get("display.rotate2");
		if (selectedRotation2.empty())
		  selectedRotation2 = "auto";

		optionsRotation2->add(_("AUTO"),          "auto", selectedRotation2 == "auto");
		optionsRotation2->add(_("0 DEGREES"),        "0", selectedRotation2 == "0");
		optionsRotation2->add(_("90 DEGREES"),       "1", selectedRotation2 == "1");
		optionsRotation2->add(_("180 DEGREES"),      "2", selectedRotation2 == "2");
		optionsRotation2->add(_("270 DEGREES"),      "3", selectedRotation2 == "3");

		s->addWithLabel(_("SCREEN ROTATION"), optionsRotation2);

		s->addSaveFunc([this, optionsRotation2, selectedRotation2, s]
		{
		  if (optionsRotation2->changed()) {
		    SystemConf::getInstance()->set("display.rotate2", optionsRotation2->getSelected());
		    SystemConf::getInstance()->saveSystemConf();
		    s->setVariable("exitreboot", true);
		  }
		});
	}

	s->addGroup(_("DMD SCREEN"));

	// video device3
	std::vector<std::string> availableVideo3 = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo3.size())
	{
		auto optionsVideo3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice3 = SystemConf::getInstance()->get("global.videooutput3");
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice3.empty()) currentDevice3 = "auto";

		bool vfound = false;
		for (auto it = availableVideo3.begin(); it != availableVideo3.end(); it++)
		{
		        if(currentDevice == (*it)) continue; // ignore the device of the first screen
			optionsVideo3->add((*it), (*it), currentDevice3 == (*it));
			if (currentDevice3 == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo3->add(currentDevice3, currentDevice3, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo3);
		s->addSaveFunc([this, optionsVideo3, currentDevice3, s] 
		{
			if (optionsVideo3->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput3", optionsVideo3->getSelected());
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("exitreboot", true);
			}
		});

		// video resolution3
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION)) {
		  auto videoModeOptionList3 = createVideoResolutionModeOptionList(mWindow, "es", "resolution3", optionsVideo3->getSelected() == "auto" ? "none" : optionsVideo3->getSelected());
		  s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution."), videoModeOptionList3);
		  s->addSaveFunc([this, videoModeOptionList3, s] {
		    if(videoModeOptionList3->changed()) {
		      SystemConf::getInstance()->set("es.resolution3", videoModeOptionList3->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		  });
		}

		// video rotation3
		auto optionsRotation3 = std::make_shared<OptionListComponent<std::string> >(mWindow, _("ROTATION"), false);

		std::string selectedRotation3 = SystemConf::getInstance()->get("display.rotate3");
		if (selectedRotation3.empty())
		  selectedRotation3 = "auto";

		optionsRotation3->add(_("AUTO"),          "auto", selectedRotation3 == "auto");
		optionsRotation3->add(_("0 DEGREES"),        "0", selectedRotation3 == "0");
		optionsRotation3->add(_("90 DEGREES"),       "1", selectedRotation3 == "1");
		optionsRotation3->add(_("180 DEGREES"),      "2", selectedRotation3 == "2");
		optionsRotation3->add(_("270 DEGREES"),      "3", selectedRotation3 == "3");

		s->addWithLabel(_("SCREEN ROTATION"), optionsRotation3);

		s->addSaveFunc([this, optionsRotation3, selectedRotation3, s]
		{
		  if (optionsRotation3->changed()) 
		    {
		      SystemConf::getInstance()->set("display.rotate3", optionsRotation3->getSelected());
		      SystemConf::getInstance()->saveSystemConf();
		      s->setVariable("exitreboot", true);
		    }
		});
	}
#endif

	s->onFinalize([s, window]
	{
	  if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
	    {
	      Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
	      return;
	    }
	});

	window->pushGui(s);
}

void GuiMenu::openDeveloperSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FRONTEND DEVELOPER OPTIONS").c_str());
	
	s->addGroup(_("VIDEO OPTIONS"));

	// maximum vram
	auto max_vram = std::make_shared<SliderComponent>(mWindow, 40.f, 1000.f, 10.f, "Mb");
	max_vram->setValue((float)(Settings::getInstance()->getInt("MaxVRAM")));
	s->addWithLabel(_("VRAM LIMIT"), max_vram);
	s->addSaveFunc([max_vram] { Settings::getInstance()->setInt("MaxVRAM", (int)round(max_vram->getValue())); });
	
	s->addSwitch(_("SHOW FRAMERATE"), _("Also turns on the emulator's native FPS counter, if available."), "DrawFramerate", true, nullptr);
	s->addSwitch(_("VSYNC"), "VSync", true, [] { Renderer::setSwapInterval(); });
	
	// Antialiasing (solves flickering issues with AMD on Windows in some cases)
	auto antiAliasing = std::make_shared<OptionListComponent<int>>(mWindow, _("ANTI-ALIASING (MSAA)"), false);
	antiAliasing->add(_("DISABLED"), 0, Settings::getInstance()->getInt("AntiAliasing") == 0);
	antiAliasing->add("2X", 2, Settings::getInstance()->getInt("AntiAliasing") == 2);
	antiAliasing->add("4X", 4, Settings::getInstance()->getInt("AntiAliasing") == 4);
	if (!antiAliasing->hasSelection())
		antiAliasing->selectFirstItem();
	s->addWithLabel(_("ANTI-ALIASING (MSAA)"), antiAliasing);
	s->addSaveFunc([s, antiAliasing]
		{
			if (Settings::getInstance()->setInt("AntiAliasing", antiAliasing->getSelected()))
				s->setVariable("reboot", true);
		});

	auto fpsLimit = std::make_shared<OptionListComponent<int>>(mWindow, _("FPS LIMIT"), false);
	fpsLimit->add(_("NO"), 0, Settings::FpsLimit() == 0);
	fpsLimit->add("25", 25, Settings::FpsLimit() == 25);
	fpsLimit->add("30", 30, Settings::FpsLimit() == 30);
	fpsLimit->add("50", 50, Settings::FpsLimit() == 50);
	fpsLimit->add("60", 60, Settings::FpsLimit() == 60);
	fpsLimit->add("75", 75, Settings::FpsLimit() == 75);
	fpsLimit->add("90", 90, Settings::FpsLimit() == 90);
	fpsLimit->add("100", 100, Settings::FpsLimit() == 100);
	fpsLimit->add("120", 120, Settings::FpsLimit() == 120);
	fpsLimit->add("144", 144, Settings::FpsLimit() == 144);
	s->addWithLabel(_("FPS LIMIT"), fpsLimit);
	s->addSaveFunc([fpsLimit] { Settings::setFpsLimit(fpsLimit->getSelected()); });

#if defined(BATOCERA) || defined(ROCKNIX)
	// overscan
	auto overscan_enabled = std::make_shared<SwitchComponent>(mWindow);
	overscan_enabled->setState(Settings::getInstance()->getBool("Overscan"));
	s->addWithLabel(_("OVERSCAN"), overscan_enabled);
	s->addSaveFunc([overscan_enabled] 
	{
		if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState()) 
		{
			Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
			ApiSystem::getInstance()->setOverscan(overscan_enabled->getState());
		}
	});
#endif

#ifdef _RPI_
	// Video Player - VideoOmxPlayer
	auto omx_player = std::make_shared<SwitchComponent>(mWindow);
	omx_player->setState(Settings::getInstance()->getBool("VideoOmxPlayer"));
	s->addWithLabel(_("USE OMX PLAYER (HARDWARE ACCELERATED)"), omx_player);
	s->addSaveFunc([omx_player, window]
	{
		// need to reload all views to re-create the right video components
		bool needReload = false;
		if (Settings::getInstance()->getBool("VideoOmxPlayer") != omx_player->getState())
			needReload = true;

		Settings::getInstance()->setBool("VideoOmxPlayer", omx_player->getState());

		if (needReload)
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});
#endif

	s->addGroup(_("TOOLS"));	

#ifndef WIN32
	// GAME AT STARTUP
	if (!SystemConf::getInstance()->get("global.bootgame.path").empty())
	{		
		std::string gamelabel = SystemConf::getInstance()->get("global.bootgame.path");			
		gamelabel = Utils::FileSystem::getStem(gamelabel) + " [" + Utils::FileSystem::getStem(Utils::FileSystem::getParent(gamelabel)) + "]";

		s->addWithDescription(_("STOP LAUNCHING THIS GAME AT STARTUP"), gamelabel, nullptr, [s]
		{
			SystemConf::getInstance()->set("global.bootgame.path", "");
			SystemConf::getInstance()->set("global.bootgame.cmd", "");

			s->close();
		});
	}
#endif

	// WEB ACCESS
	auto hostName = Utils::String::toLower(ApiSystem::getInstance()->getHostsName());

	auto webAccess = std::make_shared<SwitchComponent>(mWindow);
	webAccess->setState(Settings::getInstance()->getBool("PublicWebAccess"));
	s->addWithDescription(_("ENABLE PUBLIC WEB API ACCESS"), Utils::String::format(_("Allow public web access API using %s").c_str(), std::string("http://" + hostName + ":1234").c_str()), webAccess);
	s->addSaveFunc([webAccess, window, s]
	{ 
	  if (Settings::getInstance()->setBool("PublicWebAccess", webAccess->getState())) 
	  {
		  s->setVariable("reboot", true);
		  s->setVariable("exitreboot", true);
	  }
	});

	// log level
	auto logLevel = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOG LEVEL"), false);
	std::vector<std::string> modes;
	modes.push_back("default");
	modes.push_back("disabled");
	modes.push_back("warning");
	modes.push_back("error");
	modes.push_back("debug");

	auto level = Settings::getInstance()->getString("LogLevel");
	if (level.empty())
		level = "default";

	for (auto it = modes.cbegin(); it != modes.cend(); it++)
		logLevel->add(_(it->c_str()), *it, level == *it);

	s->addWithLabel(_("LOG LEVEL"), logLevel);
	s->addSaveFunc([this, logLevel]
	{
		if (Settings::getInstance()->setString("LogLevel", logLevel->getSelected() == "default" ? "" : logLevel->getSelected()))
		{
			Log::init();
		}
	});

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUPPORTFILE))
	{
		// support
		s->addEntry(_("CREATE A SUPPORT FILE"), true, [window] 
		{
			window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE? THIS INCLUDES ALL DATA IN YOUR SYSTEM FOLDER."), _("YES"),
				[window] 
				{
					if (ApiSystem::getInstance()->generateSupportFile())
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATED IN SAVES FOLDER"), _("OK")));
					else
						window->pushGui(new GuiMsgBox(window, _("SUPPORT FILE CREATION FAILED"), _("OK")));				
				}, 
				_("NO"), nullptr));
		});
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DISKFORMAT))
		s->addEntry(_("FORMAT A DISK"), true, [this] { openFormatDriveSettings(); });
	
	s->addWithDescription(_("CLEAN GAMELISTS & REMOVE UNUSED MEDIA"), _("Remove unused entries, and clean references to missing medias."), nullptr, [this, s]
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
		{
			int idx = 0;
			for (auto system : SystemData::sSystemVector)
			{
				mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
				cleanupGamelist(system);
				idx++;
			}

			mWindow->closeSplashScreen();
		}, _("NO"), nullptr));
	});

	s->addWithDescription(_("RESET GAMELISTS USAGE DATA"), _("Reset values of GameTime, PlayCount and LastPlayed metadata."), nullptr, [this, s]
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE?"), _("YES"), [&]
				{
					int idx = 0;
					for (auto system : SystemData::sSystemVector)
					{
						mWindow->renderSplashScreen(_("Cleaning") + ": " + system->getFullName(), (float)idx / (float)SystemData::sSystemVector.size());
						resetGamelistUsageData(system);
						idx++;
					}

					mWindow->closeSplashScreen();

					ViewController::reloadAllGames(mWindow, false);
				}, _("NO"), nullptr));
		});

	s->addWithDescription(_("RESET FILE EXTENSIONS"), _("Reset customized file extensions filters to default."), nullptr, [this, s]
	//s->addEntry(_("RESET FILE EXTENSIONS"), false, [this, s]
	{
		for (auto system : SystemData::sSystemVector)
			Settings::getInstance()->setString(system->getName() + ".HiddenExt", "");

		Settings::getInstance()->saveFile();
		ViewController::reloadAllGames(mWindow, false);
	});

	s->addEntry(_("REDETECT ALL GAMES' LANG/REGION"), false, [this]
	{
		Window* window = mWindow;
		window->pushGui(new GuiLoading<int>(window, _("PLEASE WAIT"), [](auto gui)
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

				for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
					game->detectLanguageAndRegion(true);
			}

			return 0;
		}));
	});

	// The same hasher as INDEX GAMES, so with OFFLINE ACHIEVEMENTS on it feeds
	// the offline cache too and says so in one line (fork #184, D-RA-013;
	// the rows under GAME INDEXES on RETROACHIEVEMENTS SETTINGS do the same).
	auto findAllGames = [this] { ThreadedHasher::start(mWindow, ThreadedHasher::HASH_ALL, true); };
#if defined(ROCKNIX)
	if (OfflineAchievements::available() && OfflineAchievements::toggleOn())
		s->addWithDescription(_("FIND ALL GAMES WITH NETPLAY/ACHIEVEMENTS"), _("Also saves their achievement data for offline play."), nullptr, findAllGames);
	else
#endif
	s->addEntry(_("FIND ALL GAMES WITH NETPLAY/ACHIEVEMENTS"), false, findAllGames);

	s->addEntry(_("CLEAR CACHES"), true, [this, s]
		{
			auto rootPath = Utils::FileSystem::getGenericPath(Paths::getUserEmulationStationPath());

			Utils::FileSystem::deleteDirectoryFiles(rootPath + "/tmp/");
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getTempPath());
			Utils::FileSystem::deleteDirectoryFiles(Utils::FileSystem::getPdfTempPath());

			ViewController::reloadAllGames(mWindow, false);
		});

	s->addGroup(_("DISPLAY SETTINGS"));

	auto menuFontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("MENU FONT SCALE"), false);
	menuFontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("MenuFontScale"));
	s->addWithLabel(_("MENU FONT SCALE"), menuFontScale);
	s->addSaveFunc([s, menuFontScale] { if (Settings::getInstance()->setString("MenuFontScale", menuFontScale->getSelected())) s->setVariable("reboot", true); });

	auto fontScale = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FONT SCALE"), false);
	fontScale->addRange({ { _("AUTO"), "" },{ "100%", "1.0" },{ "110%", "1.1" },{ "125%", "1.25" },{ "133%", "1.31" },{ "150%", "1.5" },{ "175%", "1.75" },{ "200%", "2" },{ "75%", "0.75" } ,{ "50%", "0.5" } },
		Settings::getInstance()->getString("FontScale"));
	s->addWithLabel(_("THEME FONT SCALE"), fontScale);
	s->addSaveFunc([s, fontScale] { if (Settings::getInstance()->setString("FontScale", fontScale->getSelected())) s->setVariable("reboot", true); });

	auto fullScreenMenus = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FULL SCREEN MENUS"), false);
	fullScreenMenus->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("FullScreenMenu"));
	s->addWithLabel(_("FULL SCREEN MENUS"), fullScreenMenus);
	s->addSaveFunc([s, fullScreenMenus] { if (Settings::getInstance()->setString("FullScreenMenu", fullScreenMenus->getSelected())) s->setVariable("reboot", true); });

	auto isSmallScreen = std::make_shared< OptionListComponent<std::string> >(mWindow, _("FORCE SMALL SCREEN THEMING"), false);
	isSmallScreen->addRange({ { _("AUTO"), "" },{ "YES", "true" },{ "NO", "false" } }, Settings::getInstance()->getString("ForceSmallScreen"));
	s->addWithLabel(_("FORCE SMALL SCREEN THEMING"), isSmallScreen);
	s->addSaveFunc([s, isSmallScreen] { if (Settings::getInstance()->setString("ForceSmallScreen", isSmallScreen->getSelected())) s->setVariable("reboot", true); });



	s->addGroup(_("DATA MANAGEMENT"));

	// ExcludeMultiDiskContent
	auto excludeMultiDiskContent = std::make_shared<SwitchComponent>(mWindow);
	excludeMultiDiskContent->setState(Settings::getInstance()->getBool("RemoveMultiDiskContent"));
	s->addWithLabel(_("IGNORE MULTI-FILE DISK CONTENT (CUE/GDI/CCD/M3U)"), excludeMultiDiskContent);
	s->addSaveFunc([excludeMultiDiskContent] { Settings::getInstance()->setBool("RemoveMultiDiskContent", excludeMultiDiskContent->getState()); });

	// enable filters (ForceDisableFilters)
	auto enable_filter = std::make_shared<SwitchComponent>(mWindow);
	enable_filter->setState(!Settings::getInstance()->getBool("ForceDisableFilters"));
	s->addWithDescription(_("ENABLE GAME FILTERING"), _("Whether to show or hide game filtering related settings in the view options."), enable_filter);
	s->addSaveFunc([this, enable_filter]
	{
		Settings::getInstance()->setBool("ForceDisableFilters", !enable_filter->getState());
	});

	// gamelist saving
	auto save_gamelists = std::make_shared<SwitchComponent>(mWindow);
	save_gamelists->setState(Settings::getInstance()->getBool("SaveGamelistsOnExit"));
	s->addWithLabel(_("SAVE METADATA ON EXIT"), save_gamelists);
	s->addSaveFunc([save_gamelists] { Settings::getInstance()->setBool("SaveGamelistsOnExit", save_gamelists->getState()); });

	// gamelist
	auto parse_gamelists = std::make_shared<SwitchComponent>(mWindow);
	parse_gamelists->setState(Settings::getInstance()->getBool("ParseGamelistOnly"));
	s->addWithDescription(_("PARSE GAMELISTS ONLY"), _("Debug tool: Don't check if the ROMs actually exist. Can cause problems!"), parse_gamelists);
	s->addSaveFunc([parse_gamelists] { Settings::getInstance()->setBool("ParseGamelistOnly", parse_gamelists->getState()); });

	// Local Art
	auto local_art = std::make_shared<SwitchComponent>(mWindow);
	local_art->setState(Settings::getInstance()->getBool("LocalArt"));
	s->addWithDescription(_("SEARCH FOR LOCAL ART"), _("If no image is specified in the gamelist, try to find media with the same filename to use."), local_art);
	s->addSaveFunc([local_art] { Settings::getInstance()->setBool("LocalArt", local_art->getState()); });

	s->addGroup(_("USER INTERFACE"));

	// carousel transition option
	auto move_carousel = std::make_shared<SwitchComponent>(mWindow);
	move_carousel->setState(Settings::getInstance()->getBool("MoveCarousel"));
	s->addWithLabel(_("CAROUSEL TRANSITIONS"), move_carousel);
	s->addSaveFunc([move_carousel] { Settings::getInstance()->setBool("MoveCarousel", move_carousel->getState()); });

	// quick system select (left/right in game list view)
	auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
	quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
	s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
	s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

	// quick jump next letter (R2/L2 in game list view)
	auto quick_jump_letter = std::make_shared<SwitchComponent>(mWindow);
	quick_jump_letter->setState(Settings::getInstance()->getBool("QuickJumpLetter"));
	s->addWithLabel(_("QUICK JUMP LETTER"), quick_jump_letter);
	s->addSaveFunc([quick_jump_letter] { Settings::getInstance()->setBool("QuickJumpLetter", quick_jump_letter->getState()); });

	// Enable OSK (On-Screen-Keyboard)
	auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
	osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
	s->addWithLabel(_("ON-SCREEN KEYBOARD"), osk_enable);
	s->addSaveFunc([osk_enable] { Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });
	
#if defined(_WIN32) || defined(X86) || defined(X86_64)
	// Hide EmulationStation Window when running a game ( windows only )
	auto hideWindowScreen = std::make_shared<SwitchComponent>(mWindow);
	hideWindowScreen->setState(Settings::getInstance()->getBool("HideWindow"));
	s->addWithLabel(_("HIDE EMULATIONSTATION WHEN RUNNING A GAME"), hideWindowScreen);
	s->addSaveFunc([hideWindowScreen] { Settings::getInstance()->setBool("HideWindow", hideWindowScreen->getState()); });
#endif
	
#if defined(WIN32) && !defined(_DEBUG)
	// full exit
	auto fullExitMenu = std::make_shared<SwitchComponent>(mWindow);
	fullExitMenu->setState(!Settings::getInstance()->getBool("ShowOnlyExit"));
	s->addWithLabel(_("COMPLETE QUIT MENU"), fullExitMenu);
	s->addSaveFunc([fullExitMenu] { Settings::getInstance()->setBool("ShowOnlyExit", !fullExitMenu->getState()); });
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		auto retroarchRgui = std::make_shared< OptionListComponent<std::string> >(mWindow, _("RETROARCH MENU DRIVER"), false);
		retroarchRgui->addRange({ { _("AUTO"), "" },{ "rgui", "rgui" },{ "xmb", "xmb" },{ "ozone", "ozone" },{ "glui", "glui" } }, SystemConf::getInstance()->get("global.retroarch.menu_driver"));
		s->addWithLabel(_("RETROARCH MENU DRIVER"), retroarchRgui);
		s->addSaveFunc([retroarchRgui] { SystemConf::getInstance()->set("global.retroarch.menu_driver", retroarchRgui->getSelected()); });
	}
#if !defined(ROCKNIX)
	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithDescription(_("SWITCH CONFIRM & CANCEL BUTTONS IN EMULATIONSTATION"), _("Switches the South and East buttons' functionality"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			s->setVariable("reloadAll", true);
		}
	});

	auto invertLongPress = std::make_shared<SwitchComponent>(mWindow);
	invertLongPress->setState(Settings::getInstance()->getBool("GameOptionsAtNorth"));
	s->addWithDescription(_("ACCESS GAME OPTIONS WITH NORTH BUTTON"), _("Switches to short-press North for Save states & long-press South button for Game Options"), invertLongPress);
	s->addSaveFunc([this, s, invertLongPress]
	{
		if (Settings::getInstance()->setBool("GameOptionsAtNorth", invertLongPress->getState()))
			s->setVariable("reloadAll", true);
	});
#endif
	auto firstJoystickOnly = std::make_shared<SwitchComponent>(mWindow);
	firstJoystickOnly->setState(Settings::getInstance()->getBool("FirstJoystickOnly"));
	s->addWithLabel(_("CONTROL EMULATIONSTATION WITH FIRST JOYSTICK ONLY"), firstJoystickOnly);
	s->addSaveFunc([this, firstJoystickOnly] { Settings::getInstance()->setBool("FirstJoystickOnly", firstJoystickOnly->getState()); });

  // Keep default behavior, but make it configurable
  s->addSwitch(_("RIGHT ANALOG STICK CHANGES VOLUME"),
             _("Move the right stick up/down to raise or lower the audio volume."),
             "RightStickVolume", true, nullptr);

//#if !defined(WIN32)
	{
	  auto gun_mt = std::make_shared<SliderComponent>(mWindow, 0.f, 10.f, 0.1f, "%");
	  gun_mt->setValue(Settings::getInstance()->getFloat("GunMoveTolerence"));
	  s->addWithLabel(_("GUN MOVE TOLERENCE"), gun_mt);
	  s->addSaveFunc([gun_mt] {
	    Settings::getInstance()->setFloat("GunMoveTolerence", gun_mt->getValue());
	  });
	}
//#endif

#if defined(BATOCERA) || defined(ROCKNIX)
	// PS3 controller enable
	auto enable_ps3 = std::make_shared<SwitchComponent>(mWindow);
	enable_ps3->setState(SystemConf::getInstance()->getBool("controllers.ps3.enabled"));
	s->addWithDescription(_("ENABLE PS3 CONTROLLER SUPPORT"), _("Might have negative impact on security."), enable_ps3);
	s->addSaveFunc([enable_ps3] {
		bool ps3Enabled = enable_ps3->getState();
		if (ps3Enabled != SystemConf::getInstance()->getBool("controllers.ps3.enabled"))
		{
			SystemConf::getInstance()->setBool("controllers.ps3.enabled", ps3Enabled);
			SystemConf::getInstance()->saveSystemConf();
			if(SystemConf::getInstance()->getBool("controllers.bluetooth.enabled"))
				ApiSystem::getInstance()->enableBluetooth();
		}
	});
#endif

#if defined(WIN32)

	auto hidJoysticks = std::make_shared<SwitchComponent>(mWindow);
	hidJoysticks->setState(Settings::getInstance()->getBool("HidJoysticks"));
	s->addWithLabel(_("ENABLE HID JOYSTICK DRIVERS"), hidJoysticks);
	s->addSaveFunc([this, hidJoysticks] { Settings::getInstance()->setBool("HidJoysticks", hidJoysticks->getState()); });
	
	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });
#endif

	s->addGroup(_("OPTIMIZATIONS"));

	// preload UI
	auto preloadUI = std::make_shared<SwitchComponent>(mWindow);
	preloadUI->setState(Settings::getInstance()->getBool("PreloadUI"));
	s->addWithDescription(_("PRELOAD UI ELEMENTS ON BOOT"), _("Reduces lag when entering gamelists from the system menu, increases boot time"), preloadUI);
	s->addSaveFunc([preloadUI] { Settings::getInstance()->setBool("PreloadUI", preloadUI->getState()); });

	// preload Medias
	auto preloadMedias = std::make_shared<SwitchComponent>(mWindow);
	preloadMedias->setState(Settings::getInstance()->getBool("PreloadMedias"));
	s->addWithDescription(_("PRELOAD METADATA MEDIA ON BOOT"), _("Reduces lag when scrolling through a fully scraped gamelist, increases boot time"), preloadMedias);
	s->addSaveFunc([preloadMedias] { Settings::getInstance()->setBool("PreloadMedias", preloadMedias->getState()); });
	
	// threaded loading
	auto threadedLoading = std::make_shared<SwitchComponent>(mWindow);
	threadedLoading->setState(Settings::getInstance()->getBool("ThreadedLoading"));
	s->addWithLabel(_("THREADED LOADING"), threadedLoading);
	s->addSaveFunc([threadedLoading] { Settings::getInstance()->setBool("ThreadedLoading", threadedLoading->getState()); });

	// threaded loading
	auto asyncImages = std::make_shared<SwitchComponent>(mWindow);
	asyncImages->setState(Settings::getInstance()->getBool("AsyncImages"));
	s->addWithLabel(_("ASYNC IMAGE LOADING"), asyncImages);
	s->addSaveFunc([asyncImages] { Settings::getInstance()->setBool("AsyncImages", asyncImages->getState()); });

	// optimizeVram
	auto optimizeVram = std::make_shared<SwitchComponent>(mWindow);
	optimizeVram->setState(Settings::getInstance()->getBool("OptimizeVRAM"));
	s->addWithLabel(_("OPTIMIZE IMAGES VRAM USE"), optimizeVram);
	s->addSaveFunc([optimizeVram] { Settings::getInstance()->setBool("OptimizeVRAM", optimizeVram->getState()); });

	// optimizeVideo
	auto optimizeVideo = std::make_shared<SwitchComponent>(mWindow);
	optimizeVideo->setState(Settings::getInstance()->getBool("OptimizeVideo"));
	s->addWithLabel(_("OPTIMIZE VIDEO VRAM USAGE"), optimizeVideo);
	s->addSaveFunc([optimizeVideo] { Settings::getInstance()->setBool("OptimizeVideo", optimizeVideo->getState()); });

	s->addSwitch(_("USE FILESYSTEM CACHE"), "UseFileCache", true, [s] { Utils::FileSystem::FileSystemCache::reset(); });

	s->onFinalize([s, window]
	{					
		if (s->getVariable("reboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openUpdatesSettings(bool selectTorrentService)
{
	GuiSettings *updateGui = new GuiSettings(mWindow, _("UPDATES & DOWNLOADS").c_str());

	updateGui->addGroup(_("DOWNLOADS"));

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERASTORE))
	{
		updateGui->addEntry(_("CONTENT DOWNLOADER"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiBatoceraStore(mWindow));
		});
	}

	// Themes installer/browser
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEMESDOWNLOADER))
	{
		updateGui->addEntry(_("THEMES"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiThemeInstaller(mWindow));
		});
	}

	// integration with theBezelProject
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::THEBEZELPROJECT))
	{
		updateGui->addEntry(_("THE BEZEL PROJECT"), true, [this]
		{
			if (!checkNetwork())
				return;

			mWindow->pushGui(new GuiBezelInstaller(mWindow));
		});
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADE))
	{
		updateGui->addGroup(_("SOFTWARE UPDATES"));

		// Enable updates
		updateGui->addSwitch(_("CHECK FOR UPDATES"), "updates.enabled", false);

		updateGui->addSwitch(_("FORCE NEXT UPDATE"), "updates.force", false);

		auto updatesBranchList = std::make_shared<OptionListComponent<std::string> >(mWindow, _("UPDATE BRANCH"), false);
		std::string updatesBranch = SystemConf::getInstance()->get("updates.branch");
		updatesBranchList->add("auto", "auto", updatesBranch == "auto");
		updatesBranchList->add("release", "stable", updatesBranch == "stable");
		updatesBranchList->add("nightly", "nightly", updatesBranch == "nightly");
		updateGui->addWithLabel(_("UPDATE BRANCH"), updatesBranchList);
		updatesBranchList->setSelectedChangedCallback([](std::string name)
		{
			if (SystemConf::getInstance()->set("updates.branch", name))
				SystemConf::getInstance()->saveSystemConf();
		});

#if BATOCERA
		std::string update_architecture;
		if (ApiSystem::getInstance()->canArchitectureUpdate(update_architecture)) {
		  auto architecture_update_switch = std::make_shared<SwitchComponent>(mWindow);
		  architecture_update_switch->setState(SystemConf::getInstance()->get("updates.architecture") == "1");
		  updateGui->addWithLabel(_("ARCHITECTURE UPDATE") + " (" + update_architecture + ")", architecture_update_switch);

		  architecture_update_switch->setOnChangedCallback([this, architecture_update_switch]()
		  {
		    SystemConf::getInstance()->set("updates.architecture", architecture_update_switch->getState() ? "1" : "");
		    SystemConf::getInstance()->saveSystemConf();
		  });
		}
#endif

		// Start update
		updateGui->addEntry(GuiUpdate::state == GuiUpdateState::State::UPDATE_READY ? _("APPLY UPDATE") : _("START UPDATE"), true, [this]
		{
			if (GuiUpdate::state == GuiUpdateState::State::UPDATE_READY)
				Utils::Platform::quitES(Utils::Platform::QuitMode::RESTART);
			else if (GuiUpdate::state == GuiUpdateState::State::UPDATER_RUNNING)
				mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATER IS ALREADY RUNNING")));
			else
			{
				if (!checkNetwork())
					return;

				mWindow->pushGui(new GuiUpdate(mWindow));
			}
		});

		// Start manual update
		if (ApiSystem::getInstance()->canLocalUpdate())
		  {
		    updateGui->addEntry(_("START LOCAL MEDIA UPDATE"), false, [this]
		    {
		      mWindow->pushGui(new GuiMsgBox(mWindow, _("REALLY UPDATE FROM LOCAL MEDIA?"),
						     _("YES"), [this] { new ThreadedUpdater(mWindow, "LOCAL"); }, 
						     _("NO"), nullptr));
		    });
		  }
	}

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::UPGRADEVIATORRENT))
	{
	  updateGui->addGroup(_("SOFTWARE UPDATES VIA TORRENT"));

	  // server
	  bool service_torrent_status = false;
	  auto services = ApiSystem::getInstance()->getServices();
	  for(unsigned int i = 0; i < services.size(); i++) {
	    if(services[i].enabled && services[i].name == "batocera_torrent") {
	      service_torrent_status = true;
	    }
	  }
	  auto server_switch = std::make_shared<SwitchComponent>(mWindow);
	  server_switch->setState(service_torrent_status);

	  updateGui->addWithLabel(_("SHARE UPDATES VIA TORRENT"), server_switch, selectTorrentService);

	  server_switch->setOnChangedCallback([this, updateGui, service_torrent_status, server_switch]()
	  {
	    bool service_torrent_btn_enabled = server_switch->getState();
	    if (service_torrent_btn_enabled != service_torrent_status)
	      {
		if(service_torrent_btn_enabled) {
		  ApiSystem::getInstance()->enableService("batocera_torrent", true);
		  mWindow->displayNotificationMessage(_U("\uF011  ") + _("Torrent update service started"));
		} else {
		  ApiSystem::getInstance()->enableService("batocera_torrent", false);
		  mWindow->displayNotificationMessage(_U("\uF011  ") + _("Torrent update service stopped"));
		}

		delete updateGui;
		openUpdatesSettings(true);
	      }
	  });

	  // menu in case the service is up
	  if(service_torrent_status) {
	    std::string torrent_status = ApiSystem::getInstance()->torrentStatus();

	    if (ApiSystem::getInstance()->torrentIsReadyForUpdate())
	      {
		//updateGui->addEntry(_("START UPDATE FROM TORRENT FILE"), false, [this]
		updateGui->addWithLabel(_("START UPDATE FROM TORRENT FILE"),
					std::make_shared<TextComponent>(mWindow, torrent_status, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color),
					false, [this]		
		{
		  mWindow->pushGui(new GuiMsgBox(mWindow, _("REALLY UPDATE FROM TORRENT FILE ?"),
						 _("YES"), [this] { new ThreadedUpdater(mWindow, "TORRENT"); }, 
						 _("NO"), nullptr));
		});
	      } else {
	        updateGui->addWithLabel(_("DOWNLOAD STATUS"), std::make_shared<TextComponent>(mWindow, torrent_status, ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color));
	    }
	  }
	}

	mWindow->pushGui(updateGui);
}

bool GuiMenu::checkNetwork()
{
	if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
		return false;
	}

	return true;
}

// Keyboard helper function that parses output "code Description" into a pair { "code", "Description" }
#if !WIN32
static std::vector<std::pair<std::string, std::string>> getScriptOutput(const std::string& command)
{
	std::vector<std::pair<std::string, std::string>> results;
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
	if (!pipe) return results;

	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}

	std::stringstream ss(result);
	std::string line;
	while (std::getline(ss, line))
	{
		// Trim newline chars
		if (!line.empty() && line.back() == '\n') line.pop_back();
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		// Find first space: split "code" and "Description"
		size_t splitPos = line.find(' ');
		if (splitPos != std::string::npos)
		{
			std::string code = line.substr(0, splitPos);
			std::string name = line.substr(splitPos + 1);
			
			// Trim potential leading whitespace from name
			size_t first = name.find_first_not_of(' ');
			if (first != std::string::npos) name = name.substr(first);

			results.push_back({ code, name });
		}
	}
	return results;
}
#endif

void GuiMenu::openSystemSettings() 
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("SYSTEM SETTINGS").c_str());
	bool isFullUI = UIModeController::getInstance()->isUIModeFull();

	s->addGroup(_("SYSTEM"));

	// System informations
	s->addEntry(_("INFORMATION"), true, [this] { openSystemInformations(); });

	// language choice
	auto language_choice = std::make_shared<OptionListComponent<std::string> >(window, _("LANGUAGE"), false);

	std::string language = SystemConf::getInstance()->get("system.language");
	if (language.empty()) 
		language = "en_US";

	language_choice->add("العربية",               "ar_YE", language == "ar_YE");
	language_choice->add("CATALÀ",               "ca_ES", language == "ca_ES");
	language_choice->add("ČEŠTINA",                "cs_CZ", language == "cs_CZ");
	language_choice->add("CYMRAEG",              "cy_GB", language == "cy_GB");
	language_choice->add("DEUTSCH", 	     "de_DE", language == "de_DE");
	language_choice->add("ΕΛΛΗΝΙΚΆ",                "el_GR", language == "el_GR");
	language_choice->add("ENGLISH (US)", 	     "en_US", language == "en_US" || language == "en");
	language_choice->add("ENGLISH (UK)", 	     "en_GB", language == "en_GB");
	language_choice->add("ESPAÑOL", 	     "es_ES", language == "es_ES" || language == "es");
	language_choice->add("ESPAÑOL MEXICANO",     "es_MX", language == "es_MX");
	language_choice->add("EUSKARA",               "eu_ES", language == "eu_ES");
	language_choice->add("SUOMI",                "fi_FI", language == "fi_FI");
	language_choice->add("FRANÇAIS",             "fr_FR", language == "fr_FR" || language == "fr");
	language_choice->add("GALEGO",               "gl_ES", language == "gl_ES");
	language_choice->add("עברית",                "he_IL", language == "he_IL");
	language_choice->add("HRVATSKI",            "hu_HU", language == "hu_HU");
	language_choice->add("BAHASA INDONESIA",     "id_ID", language == "id_ID");
	language_choice->add("ITALIANO",             "it_IT", language == "it_IT");
	language_choice->add("日本語", 	     "ja_JP", language == "ja_JP");
	language_choice->add("한국어",   	     "ko_KR", language == "ko_KR" || language == "ko");
	language_choice->add("NORSK BOKMÅL",     "nb_NO", language == "nb_NO");
	language_choice->add("NEDERLANDS",                "nl_NL", language == "nl_NL");
	language_choice->add("NORSK NYNORSK",            "nn_NO", language == "nn_NO");
	language_choice->add("OCCITAN",              "oc_FR", language == "oc_FR");
	language_choice->add("POLSKI",               "pl_PL", language == "pl_PL");
	language_choice->add("PORTUGUÊS BRASILEIRO", "pt_BR", language == "pt_BR");
	language_choice->add("PORTUGUÊS",   "pt_PT", language == "pt_PT");
	language_choice->add("ROMÂNĂ",               "ro_RO", language == "ro_RO");
	language_choice->add("РУССКИЙ",              "ru_RU", language == "ru_RU");
	language_choice->add("SLOVENČINA", 	     "sk_SK", language == "sk_SK");
	language_choice->add("SVENSKA", 	     "sv_SE", language == "sv_SE");
	language_choice->add("TÜRKÇE",  	     "tr_TR", language == "tr_TR");
	language_choice->add("УКРАЇНСЬКА",           "uk_UA", language == "uk_UA");
	language_choice->add("TIẾNG VIỆT",           "vi_VN", language == "vi_VN");
	language_choice->add("简体中文", 	     "zh_CN", language == "zh_CN");
	language_choice->add("正體中文", 	     "zh_TW", language == "zh_TW");

	s->addWithLabel(_("LANGUAGE (REGION)"), language_choice);
	s->addSaveFunc([window, language_choice, language, s]
	{
		if (language_choice->changed() && SystemConf::getInstance()->set("system.language", language_choice->getSelected()))
		{
			FileSorts::reset();
			MetaDataList::initMetadata();

#ifdef HAVE_INTL
			s->setVariable("exitreboot", true);
#endif
			s->setVariable("reloadGuiMenu", true);
		}		
	});

	// Keyboard layout & variant
#if !WIN32
	
	std::string curLayout = SystemConf::getInstance()->get("system.kblayout");
	if (curLayout.empty()) curLayout = "us";

	std::string curVariant = SystemConf::getInstance()->get("system.kbvariant");
	if (curVariant.empty()) curVariant = "none";

	auto keyboard_layout = std::make_shared<OptionListComponent<std::string>>(window, _("KEYBOARD LAYOUT"), false);
	auto keyboard_variant = std::make_shared<OptionListComponent<std::string>>(window, _("KEYBOARD VARIANT"), false);

	// Populate Layouts
	auto layouts = getScriptOutput("/usr/bin/rocknix-keyboard list-layouts");
	bool layoutFound = false;
	
	for (const auto& l : layouts)
	{
		bool isSelected = (l.first == curLayout);
		if (isSelected) layoutFound = true;
		keyboard_layout->add(l.second, l.first, isSelected);
	}
	if (!layoutFound) {
		keyboard_layout->add(curLayout, curLayout, true);
	}

	// Populate Variants
	auto populateVariants = [keyboard_variant, curVariant](std::string layoutCode) {
		keyboard_variant->clear();
		bool noneSelected = (curVariant == "none" || curVariant.empty());
		keyboard_variant->add(_("NONE"), "none", noneSelected);

		auto variants = getScriptOutput("/usr/bin/rocknix-keyboard list-variants " + layoutCode);
		bool variantFound = false;
		for (const auto& v : variants)
		{
			bool isSelected = (v.first == curVariant);
			if (isSelected) variantFound = true;
			keyboard_variant->add(v.second, v.first, isSelected);
		}

		if (!variantFound && !noneSelected) {
			keyboard_variant->selectFirstItem(); 
		}
		keyboard_variant->invalidate();
	};

	populateVariants(curLayout);

	// Callback for layout change
	keyboard_layout->setSelectedChangedCallback([populateVariants, keyboard_variant](std::string newLayout) {
		keyboard_variant->clear();
		keyboard_variant->add(_("NONE"), "none", true);
		
		auto variants = getScriptOutput("/usr/bin/rocknix-keyboard list-variants " + newLayout);
		for (const auto& v : variants)
		{
			keyboard_variant->add(v.second, v.first, false);
		}
		keyboard_variant->selectFirstItem();
		keyboard_variant->invalidate();
	});

	std::string kbHelpText = _("Select the physical keyboard layout. A reboot may be required for changes to take full effect.");
	
	s->addWithDescription(_("KEYBOARD LAYOUT"), kbHelpText, keyboard_layout);
	s->addWithDescription(_("KEYBOARD VARIANT"), kbHelpText, keyboard_variant);

	s->addSaveFunc([keyboard_layout, keyboard_variant, s] {
		if (keyboard_layout->changed() || keyboard_variant->changed())
		{
			std::string selLayout = keyboard_layout->getSelected();
			std::string selVariant = keyboard_variant->getSelected();
			
			std::string cmd = "/usr/bin/rocknix-keyboard set \"" + selLayout + "\" \"" + selVariant + "\"";
			if (system(cmd.c_str()) == 0) {
				SystemConf::getInstance()->set("system.kblayout", selLayout);
				SystemConf::getInstance()->set("system.kbvariant", selVariant);
				
				// Trigger the standard "Reboot Required" notification on menu exit
				s->setVariable("reboot", true);
			}
		}
	});
#endif

	// Timezone
#if defined(ROCKNIX)
	auto tzChoices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SELECT YOUR TIME ZONE"), false);
	std::string currentTZ = SystemConf::getInstance()->get("system.timezone");
	if (currentTZ.empty())
		currentTZ = std::string(Utils::Platform::GetShOutput(R"(/usr/bin/timeinfo current_timezone)"));
	std::string tz;
	for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/timeinfo timezones)")); getline(ss, tz, ','); ) {
		tzChoices->add(tz, tz, currentTZ == tz);
	}
	s->addWithLabel(_("TIMEZONE"), tzChoices);
	s->addSaveFunc([tzChoices] {
		if (tzChoices->changed()) {
			std::string selectedTimezone = tzChoices->getSelected();
			Utils::Platform::runSystemCommand("ln -sf /usr/share/zoneinfo/" + selectedTimezone + " $(readlink /etc/localtime)", "", nullptr);
		}
		SystemConf::getInstance()->set("system.timezone", tzChoices->getSelected());
	});
#else
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::TIMEZONES))
	{
		VectorEx<std::string> availableTimezones = ApiSystem::getInstance()->getTimezones();
		if (availableTimezones.size() > 0)
		{
			std::string currentTZ = ApiSystem::getInstance()->getCurrentTimezone();
			if (currentTZ.empty() || !availableTimezones.any([currentTZ](const std::string& tz) { return tz == currentTZ; }))
				currentTZ = "Europe/Paris";

			auto tzChoices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SELECT YOUR TIME ZONE"), false);

			for (auto tz : availableTimezones)
				tzChoices->add(_(Utils::String::toUpper(tz).c_str()), tz, currentTZ == tz);

			s->addWithLabel(_("TIME ZONE"), tzChoices);
			s->addSaveFunc([tzChoices] 
			{
				if (SystemConf::getInstance()->set("system.timezone", tzChoices->getSelected()))
					ApiSystem::getInstance()->setTimezone(tzChoices->getSelected());
			});
		}
	}
#endif

	// Clock time format (14:42 or 2:42 pm)
	s->addSwitch(_("SHOW CLOCK IN 12-HOUR FORMAT"), "ClockMode12", true);

	// power saver
	auto power_saver = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER SAVING MODE"), false);
	power_saver->addRange({ { _("DISABLED"), "disabled" }, { _("DEFAULT"), "default" }, { _("ENHANCED"), "enhanced" }, { _("INSTANT"), "instant" }, }, Settings::PowerSaverMode());
	s->addWithDescription(_("POWER SAVING MODE"), _("Reduces power consumption when idle (useful for handhelds)."), power_saver);
	s->addSaveFunc([this, power_saver] 
	{
		if (Settings::PowerSaverMode() != "instant" && power_saver->getSelected() == "instant")
			Settings::getInstance()->setBool("EnableSounds", false);

		Settings::setPowerSaverMode(power_saver->getSelected());
		PowerSaver::init();
	});

#if defined(_ENABLE_TTS_) || defined(WIN32)
	if (TextToSpeech::getInstance()->isAvailable())
	{
			// tts
		auto tts = std::make_shared<SwitchComponent>(mWindow);
		tts->setState(Settings::getInstance()->getBool("TTS"));
		s->addWithLabel(_("SCREEN READER (TEXT TO SPEECH)"), tts);
		s->addSaveFunc([tts] {
			 if(TextToSpeech::getInstance()->isEnabled() != tts->getState()) {
			   TextToSpeech::getInstance()->enable(tts->getState());
			   Settings::getInstance()->setBool("TTS", tts->getState());
			 }});
	}
#endif

	// UI RESTRICTIONS
	auto UImodeSelection = std::make_shared< OptionListComponent<std::string> >(mWindow, _("USER INTERFACE MODE"), false);
	std::vector<std::string> UImodes = UIModeController::getInstance()->getUIModes();
	for (auto it = UImodes.cbegin(); it != UImodes.cend(); it++)
		UImodeSelection->add(_(it->c_str()), *it, Settings::getInstance()->getString("UIMode") == *it);

	s->addWithDescription(_("USER INTERFACE MODE"), _("Lock down certain config menus for use with guest users/kids."), UImodeSelection);
	s->addSaveFunc([UImodeSelection, window]
	{
		if (UImodeSelection->changed())
		{
			std::string selectedMode = UImodeSelection->getSelected();
			if (selectedMode == "Basic" || selectedMode == "Full")
				Settings::getInstance()->setString("UIMode", selectedMode);
			else
			{
				std::string msg = _("You are changing the user interface to a restricted mode:\nThis will hide most menu options to prevent changes to the system.\nTo unlock and return to the full user interface, enter this code:") + "\n";
				msg += "\"" + UIModeController::getInstance()->getFormattedPassKeyStr() + "\"\n\n";
				msg += _("Do you want to proceed?");
				window->pushGui(new GuiMsgBox(window, msg,
					_("YES"), [selectedMode] {
					LOG(LogDebug) << "Setting user interface mode to " << selectedMode;
					Settings::getInstance()->setString("UIMode", selectedMode);
					Settings::getInstance()->saveFile();
				}, _("NO"), nullptr));
			}
		}
	});
#if defined(ROCKNIX)
      // Add option to toggle mangohud
      if (Utils::Platform::GetEnv("DEVICE_MANGOHUD_SUPPORT") == "true"){
        auto mangohud_toggle = std::make_shared<SwitchComponent>(mWindow);
        bool internalmoduleEnabled = SystemConf::getInstance()->get("rocknix.mangohud.enabled") == "1";
        mangohud_toggle->setState(internalmoduleEnabled);
        s->addWithLabel(_("ENABLE MANGOHUD OVERLAY"), mangohud_toggle);
        mangohud_toggle->setOnChangedCallback([mangohud_toggle] {
                if (mangohud_toggle->getState() == false) {
                        Utils::Platform::runSystemCommand("/usr/bin/mangohud_set disable", "", nullptr);
                } else {
                        Utils::Platform::runSystemCommand("/usr/bin/mangohud_set enable", "", nullptr);
                }
                bool mangohud_state = mangohud_toggle->getState();
                SystemConf::getInstance()->set("rocknix.mangohud.enabled", mangohud_state ? "1" : "0");
        });
      }

      // Add option to toggle touchscreen keyboard
      if (Utils::Platform::GetEnv("DEVICE_HAS_TOUCHSCREEN") == "true"){
        auto touchscreen_keyboard_toggle = std::make_shared<SwitchComponent>(mWindow);
        bool internalmoduleEnabled = SystemConf::getInstance()->get("rocknix.touchscreen-keyboard.enabled") == "1";
        touchscreen_keyboard_toggle->setState(internalmoduleEnabled);
        s->addWithLabel(_("ENABLE TOUCHSCREEN KEYBOARD"), touchscreen_keyboard_toggle);
        touchscreen_keyboard_toggle->setOnChangedCallback([touchscreen_keyboard_toggle] {
                if (touchscreen_keyboard_toggle->getState() == false) {
                        Utils::Platform::runSystemCommand("systemctl stop touchkeyboard", "", nullptr);
                } else {
                        Utils::Platform::runSystemCommand("systemctl start touchkeyboard", "", nullptr);
                }
                bool touchscreen_keyboard_state = touchscreen_keyboard_toggle->getState();
                SystemConf::getInstance()->set("rocknix.touchscreen-keyboard.enabled", touchscreen_keyboard_state ? "1" : "0");
        });
      }

      // Add toggle to enable / disabled ROCKNIX Screenshot
      auto rocknix_screenshot_enabled = std::make_shared<SwitchComponent>(mWindow);
      bool rocknixscreenshotenabled = SystemConf::getInstance()->get("rocknix.screenshot.enabled") == "1";
      rocknix_screenshot_enabled->setState(SystemConf::getInstance()->getBool("rocknix.screenshot.enabled"));
      s->addWithLabel(_("ENABLE ROCKNIX SCREENSHOT"), rocknix_screenshot_enabled);
      rocknix_screenshot_enabled->setOnChangedCallback([rocknix_screenshot_enabled] {
              bool rocknixscreenshotenabled = rocknix_screenshot_enabled->getState();
                     SystemConf::getInstance()->set("rocknix.screenshot.enabled", rocknixscreenshotenabled ? "1" : "0");
      });
#endif
	// KODI SETTINGS
#ifdef _ENABLE_KODI_
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::KODI))
	{
		s->addEntry(_("KODI SETTINGS"), true, [this] 
		{
			GuiSettings* kodiGui = new GuiSettings(mWindow, _("KODI SETTINGS").c_str());
			kodiGui->addSwitch(_("ENABLE KODI"), "kodi.enabled", false);
			kodiGui->addSwitch(_("LAUNCH KODI AT BOOT"), "kodi.atstartup", false);
			mWindow->pushGui(kodiGui);
		});
	}
#endif

#if defined(BATOCERA) || defined(ROCKNIX)
	s->addGroup(_("HARDWARE"));
#endif

	// brightness
	std::vector<BrightnessDevice> brightnesses;
	if (ApiSystem::getInstance()->getBrightness(brightnesses))
	{
	  int n = 0;
	  bool dualScreen = Utils::Platform::GetEnv("DEVICE_HAS_DUAL_SCREEN") == "true";
	  for (auto brightness : brightnesses) {
	    n++;
            if (n > 1 && !dualScreen)
              break;
	    auto brightnessComponent = std::make_shared<SliderComponent>(mWindow, 5.f, 100.f, 5.f, "%");
	    brightnessComponent->setValue(brightness.value);
	    brightnessComponent->setOnValueChanged([n, brightness](const float &newVal)
	    {
	      BrightnessDevice brightness_new = brightness;
	      brightness_new.value = (int)Math::round(newVal);

	      ApiSystem::getInstance()->setBrightness(brightness_new);
#if !WIN32
	      std::string nstr = "";
	      if(n > 1) {
		nstr = std::to_string(n);
	      }
	      SystemConf::getInstance()->set("display.brightness"+nstr, std::to_string((int)Math::round(newVal)));
#endif
	    });

	    std::string lnstr = "";
	    if(n > 1) {
	      lnstr = " " + std::to_string(n);
	    }
	    s->addWithLabel(_("SCREEN BRIGHTNESS") + lnstr, brightnessComponent);
	  }
	}

#if defined(ROCKNIX)
	// Bottom screen content - only visible when 2+ displays detected
  if (Utils::Platform::GetEnv("DEVICE_HAS_DUAL_SCREEN") == "true"){
	if (brightnesses.size() >= 2)
	{    
		auto bottomScreenType = std::make_shared<OptionListComponent<std::string>>(mWindow, _("BOTTOM SCREEN CONTENT"), false);
		std::string selectedBottomScreenType = SystemConf::getInstance()->get("rocknix.bottomscreen.type");
		if (selectedBottomScreenType.empty())
			selectedBottomScreenType = "vc";

		bottomScreenType->add(_("OFF"), "off", selectedBottomScreenType == "off");
		bottomScreenType->add(_("VIRTUAL CONSOLE"), "vc", selectedBottomScreenType == "vc");

		if (!bottomScreenType->hasSelection())
			bottomScreenType->selectFirstItem();

		s->addWithLabel(_("BOTTOM SCREEN CONTENT"), bottomScreenType);
		s->addSaveFunc([bottomScreenType]
		{
			if (bottomScreenType->changed())
			{
				SystemConf::getInstance()->set("rocknix.bottomscreen.type", bottomScreenType->getSelected());
				SystemConf::getInstance()->saveSystemConf();
			}
		});
	}
  }
#endif

    // Default Display mode
    std::vector<std::string> availableDisplayModes = ApiSystem::getInstance()->getAvailableDisplayModes();
    if (availableDisplayModes.size() > 1){
        auto optionsDisplayModes = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DISPLAY MODE"), false);
        std::string selectedDisplayMode = SystemConf::getInstance()->get("system.display_mode");
        for (auto it = availableDisplayModes.begin(); it != availableDisplayModes.end(); it++)
        {
            if (selectedDisplayMode.empty() && ((*it).find("preferred") != std::string::npos))
            {
                (*it) = (*it).substr((*it).find("px, ") + 4);
                (*it) = (*it).substr(0, (*it).find(" Hz") + 3);
                selectedDisplayMode = (*it);
                continue;
            }

            // Remove resolution at start, and any trailing markers, e.g. "preferred" or  "current"
            (*it) = (*it).substr((*it).find("px, ") + 4);
            (*it) = (*it).substr(0, (*it).find(" Hz") + 3);
        }
        bool cfound = false;
        for (auto it = availableDisplayModes.begin(); it != availableDisplayModes.end(); it++)
        {
	        optionsDisplayModes->add((*it), (*it), selectedDisplayMode == (*it));
	        if (selectedDisplayMode == (*it))
		        cfound = true;
        }

        if (!cfound)
            optionsDisplayModes->add(selectedDisplayMode, selectedDisplayMode, true);

        s->addWithLabel(_("DISPLAY MODE"), optionsDisplayModes);
        s->addSaveFunc([selectedDisplayMode, optionsDisplayModes]
        {
            if (optionsDisplayModes->changed())
            {
                SystemConf::getInstance()->set("system.display_mode", optionsDisplayModes->getSelected());
                SystemConf::getInstance()->saveSystemConf();
                Utils::Platform::runSystemCommand("/usr/bin/sh -lc \". /etc/profile.d/010-wlr-randr; set_refresh_rate "+ optionsDisplayModes->getSelected() + "\"", "", nullptr);
            }
        });
    }

	if (Utils::Platform::GetEnv("DEVICE_PWR_LED_CONTROL") == "true") {
		// Disable Power LED
		auto pwr_led_disabled = std::make_shared<SwitchComponent>(mWindow);
		bool pwrleddisabled = SystemConf::getInstance()->get("powerled.disabled") == "1";
		pwr_led_disabled->setState(SystemConf::getInstance()->getBool("powerled.disabled"));
		s->addWithLabel(_("DISABLE POWER LED"), pwr_led_disabled);
		pwr_led_disabled->setOnChangedCallback([pwr_led_disabled] {
			bool pwrleddisabled = pwr_led_disabled->getState();
				SystemConf::getInstance()->set("powerled.disabled", pwrleddisabled ? "1" : "0");
		});
	}

	if (Utils::Platform::GetEnv("DEVICE_LED_CONTROL") == "true"){
		// Provides LED management
		auto optionsColors = std::make_shared<OptionListComponent<std::string> >(mWindow, _("LED COLOR"), false);
		std::vector<std::string> availableColors = ApiSystem::getInstance()->getAvailableColors();
		std::string selectedColors = SystemConf::getInstance()->get("led.color");
		if (selectedColors.empty())
			selectedColors = "default";
		bool lfound = false;
		for (auto it = availableColors.begin(); it != availableColors.end(); it++)
		{
			optionsColors->add((*it), (*it), selectedColors == (*it));
			if (selectedColors == (*it))
			        lfound = true;
		}
		if (!lfound)
			optionsColors->add(selectedColors, selectedColors, true);
		s->addWithLabel(_("LED COLOR"), optionsColors);

		optionsColors->setSelectedChangedCallback([](const std::string& newColor) {
			SystemConf::getInstance()->set("led.color", newColor);
			SystemConf::getInstance()->saveSystemConf();
			Utils::Platform::runSystemCommand("/usr/bin/sh -lc \"/usr/bin/ledcontrol " + newColor + "\"", "", nullptr);
		});

		s->addSaveFunc([optionsColors] {
			SystemConf::getInstance()->set("led.color", optionsColors->getSelected());
		});
	}

	if (Utils::Platform::GetEnv("DEVICE_LED_BRIGHTNESS") == "true"){
	        // Sets LED brightness
	        auto optionsLEDBrightness = std::make_shared<OptionListComponent<std::string> >(mWindow, _("LED BRIGHTNESS"), false);
	        std::string selectedLEDBrightness = SystemConf::getInstance()->get("led.brightness");
	        if (selectedLEDBrightness.empty())
	                selectedLEDBrightness = "max";
	        optionsLEDBrightness->add(_("MAX"),"max", selectedLEDBrightness == "max");
	        optionsLEDBrightness->add(_("MID"),"mid", selectedLEDBrightness == "mid");
	        optionsLEDBrightness->add(_("MIN"),"min", selectedLEDBrightness == "min");
	        s->addWithLabel(_("LED BRIGHTNESS"), optionsLEDBrightness);

		optionsLEDBrightness->setSelectedChangedCallback([](const std::string& newBrightness) {
			SystemConf::getInstance()->set("led.brightness", newBrightness);
			SystemConf::getInstance()->saveSystemConf();
			Utils::Platform::runSystemCommand("/usr/bin/ledcontrol brightness " + newBrightness, "", nullptr);
		});

		s->addSaveFunc([optionsLEDBrightness] {
			SystemConf::getInstance()->set("led.brightness", optionsLEDBrightness->getSelected());
		});
	}

	if (Utils::Platform::GetEnv("DEVICE_ANALOG_STICKS_LED_CONTROL") == "true"){
		s->addEntry(_("ANALOG STICKS LED COLOR"), true, [this] { openAnalogSticksLedControls(); });
	}

	if (Utils::Platform::GetEnv("DEVICE_DTB_SWITCH") == "true"){
		s->addGroup(_("DEVICE"));
		// Switch device dtb between e.g. the R33S & R36S

		const std::string devSwitchScript = "/usr/bin/device-switch";
		auto optionsDeviceSwitch = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THIS DEVICE IS"), false);
		std::string selectedDevice = std::string(Utils::Platform::GetShOutput(R"(/usr/bin/device-switch)"));
		if (selectedDevice.empty())
			selectedDevice = "unknown";

		std::string a;
		for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/device-switch --options)")); getline(ss, a, ' '); ) {
			optionsDeviceSwitch->add(a, a, a == selectedDevice);
		}
		s->addWithLabel(_("THIS DEVICE IS"), optionsDeviceSwitch);

		s->addSaveFunc([this, window, devSwitchScript, optionsDeviceSwitch, selectedDevice] {
			if (optionsDeviceSwitch->changed()) {
				Utils::Platform::runSystemCommand(devSwitchScript + " " + optionsDeviceSwitch->getSelected(), "", nullptr);
				window->pushGui(new GuiMsgBox(window, _("Device will be switched on next reboot"),
					_("Reboot now"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
					_("later"), nullptr)
				);
			}
		});
        }

	if (Utils::Platform::GetEnv("DEVICE_MMC_EJECT") != "false") {
		s->addGroup(_("STORAGE"));
		// Provides a mechanism to disable use of the second device
		bool MountGamesEnabled = SystemConf::getInstance()->getBool("system.automount");
		auto mount_games = std::make_shared<SwitchComponent>(mWindow);
		mount_games->setState(MountGamesEnabled);
		s->addWithLabel(_("AUTODETECT GAMES CARD"), mount_games);
		mount_games->setOnChangedCallback([this, s, mount_games] {
			SystemConf::getInstance()->setBool("system.automount", mount_games->getState());
			Utils::Platform::runSystemCommand("/usr/bin/systemctl restart rocknix-automount", "", nullptr);
		});
		if (Utils::FileSystem::exists("/storage/.ms_supported") && MountGamesEnabled)
		{
			auto overlayState = std::make_shared<SwitchComponent>(mWindow);
			bool overlayStateEnabled = SystemConf::getInstance()->getBool("system.merged.storage");
			overlayState->setState(overlayStateEnabled);
			s->addWithLabel(_("ENABLE MERGED STORAGE"), overlayState);
			overlayState->setOnChangedCallback([this, s, overlayState] {
				bool overlayStateEnabled = overlayState->getState();
				SystemConf::getInstance()->setBool("system.merged.storage", overlayState->getState());
				Utils::Platform::runSystemCommand("/usr/bin/systemctl restart rocknix-automount", "", nullptr);
			});
			auto optionsMSDevice = std::make_shared<OptionListComponent<std::string> >(mWindow, _("MERGED STORAGE PRIMARY CARD"), false);
			std::string selectedMSDevice = SystemConf::getInstance()->get("system.merged.device");
			if (selectedMSDevice.empty())
				selectedMSDevice = "default";
			optionsMSDevice->add(_("DEFAULT"),"default", selectedMSDevice == "default");
			optionsMSDevice->add(_("EXTERNAL"),"external", selectedMSDevice == "external");
			optionsMSDevice->add(_("INTERNAL"),"internal", selectedMSDevice == "internal");
			s->addWithLabel(_("MERGED STORAGE PRIMARY CARD"), optionsMSDevice);
			s->addSaveFunc([this, optionsMSDevice, selectedMSDevice]
			{
				if (optionsMSDevice->changed()) {
					mWindow->pushGui(new GuiMsgBox(mWindow, _("WARNING: CHANGING THE PRIMARY CARD CAN CAUSE ACCESS TO GAMES TO BE LOST, REQUIRING MANUAL INTERVENTION TO CORRECT. CONTINUE?"), _("YES"), [this, optionsMSDevice, selectedMSDevice]
					{
						SystemConf::getInstance()->set("system.merged.device", optionsMSDevice->getSelected());
						Utils::Platform::runSystemCommand("/usr/bin/systemctl restart rocknix-automount " + optionsMSDevice->getSelected(), "", nullptr);
					}, _("NO"), nullptr));
				}
			});
		}
		s->addEntry(_("EJECT MICROSD CARD"), false, [window] {
			if (Utils::FileSystem::exists("/storage/.ms_supported"))
			{
				Utils::Platform::runSystemCommand("/usr/bin/umount -f /storage/roms; /usr/bin/umount -f /storage/games-external", "", nullptr);
			} else {
				Utils::Platform::runSystemCommand("/usr/bin/umount -f /storage/roms", "", nullptr);
			}
			window->pushGui(new GuiMsgBox(window, _("You may now remove the card.")));
		});
	}

	s->addGroup(_("PERFORMANCE"));
	if (Utils::Platform::GetEnv("DEVICE_HAS_FAN") == "true") {
		// Provides cooling profile switching
		auto optionsFanProfile = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COOLING PROFILE"), false);
		std::string selectedFanProfile = SystemConf::getInstance()->get("cooling.profile");
		if (selectedFanProfile.empty())
			selectedFanProfile = "auto";
		optionsFanProfile->add(_("AUTO"),"auto", selectedFanProfile == "auto");
		optionsFanProfile->add(_("QUIET"),"quiet", selectedFanProfile == "quiet");
		optionsFanProfile->add(_("MODERATE"),"moderate", selectedFanProfile == "moderate");
		optionsFanProfile->add(_("AGGRESSIVE"),"aggressive", selectedFanProfile == "aggressive");
		optionsFanProfile->add(_("CUSTOM"),"custom", selectedFanProfile == "custom");
		s->addWithLabel(_("COOLING PROFILE"), optionsFanProfile);
		s->addSaveFunc([this, optionsFanProfile, selectedFanProfile]
		{
			if (optionsFanProfile->changed()) {
				SystemConf::getInstance()->set("cooling.profile", optionsFanProfile->getSelected());
				Utils::Platform::runSystemCommand("systemctl restart fancontrol", "", nullptr);
			}
		});
	}

	// Default Scaling governor
	auto optionsGovernors = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DEFAULT SCALING GOVERNOR"), false);
	std::vector<std::string> availableGovernors = ApiSystem::getInstance()->getAvailableCpuGovernors();
	std::string selectedGovernors = SystemConf::getInstance()->get("system.cpugovernor");
	if (selectedGovernors.empty())
		selectedGovernors = "default";
	bool cfound = false;
	for (auto it = availableGovernors.begin(); it != availableGovernors.end(); it++)
	{
		optionsGovernors->add((*it), (*it), selectedGovernors == (*it));
		if (selectedGovernors == (*it))
			cfound = true;
	}
	if (!cfound)
		optionsGovernors->add(selectedGovernors, selectedGovernors, true);
	s->addWithLabel(_("DEFAULT SCALING GOVERNOR"), optionsGovernors);
	s->addSaveFunc([selectedGovernors, optionsGovernors]
	{
		if (optionsGovernors->changed()) {
			SystemConf::getInstance()->set("system.cpugovernor", optionsGovernors->getSelected());
		}
		Utils::Platform::runSystemCommand("/usr/bin/sh -lc \". /etc/profile.d/099-freqfunctions; "+ optionsGovernors->getSelected() + "\"", "", nullptr);
	});

	// GPU performance mode with enhanced power savings
	auto optionsGpuGovernors = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DEFAULT GPU SCALING GOVERNOR"), false);

	std::vector<std::string> availableGpuGovernors = ApiSystem::getInstance()->getAvailableGpuGovernors();
	std::string selectedGpuGovernor = SystemConf::getInstance()->get("system.gpuperf");
	
	if (selectedGpuGovernor.empty())
		selectedGpuGovernor = "default";
	
	bool selectedGpuGovernorFound = false;
	
	for (auto it = availableGpuGovernors.begin(); it != availableGpuGovernors.end(); it++)
	{
		optionsGpuGovernors->add((*it), (*it), selectedGpuGovernor == (*it));
		if (selectedGpuGovernor == (*it))
			selectedGpuGovernorFound = true;
	}
	
	if (!selectedGpuGovernorFound)
		optionsGpuGovernors->selectFirstItem(); // Preselect 'default' 

	s->addWithLabel(_("DEFAULT GPU SCALING GOVERNOR"), optionsGpuGovernors);
	s->addSaveFunc([selectedGpuGovernor, optionsGpuGovernors]
	{
		if (optionsGpuGovernors->changed()) {
			SystemConf::getInstance()->set("system.gpuperf", optionsGpuGovernors->getSelected());
		}
		Utils::Platform::runSystemCommand("/usr/bin/sh -lc \". /etc/profile.d/099-freqfunctions; gpu_performance_level "+ optionsGpuGovernors->getSelected() + "\"", "", nullptr);
	});

	if (Utils::Platform::GetEnv("DEVICE_TURBO_MODE") == "true"){
		// Add option to enable turbo mode overclocking
		auto turbo_mode = std::make_shared<SwitchComponent>(mWindow);
		bool internalmoduleEnabled = SystemConf::getInstance()->get("enable.turbo-mode") == "1";
		turbo_mode->setState(internalmoduleEnabled);
		s->addWithLabel(_("ENABLE CPU OVERCLOCK"), turbo_mode);
		turbo_mode->setOnChangedCallback([turbo_mode] {
		if (turbo_mode->getState() == false) {
			Utils::Platform::runSystemCommand("/usr/bin/turbomode disable", "", nullptr);
		} else {
			Utils::Platform::runSystemCommand("/usr/bin/turbomode enable", "", nullptr);
                }
		bool turbomode = turbo_mode->getState();
			SystemConf::getInstance()->set("enable.turbo-mode", turbomode ? "1" : "0");
		});
	}

#if defined(SM6115)
	// Add option to set gpu max clock speed
	auto optionsGpuMaxClockSpeed = std::make_shared<OptionListComponent<std::string> >(mWindow, _("GPU MAX CLOCK SPEED"), false);
	std::string selectedGpuSpeed = SystemConf::getInstance()->get("gpu-max-clock-speed");
	if (selectedGpuSpeed.empty())
		selectedGpuSpeed = "1050";
	optionsGpuMaxClockSpeed->add(_("320 MHZ"),"320", selectedGpuSpeed == "320");
	optionsGpuMaxClockSpeed->add(_("465 MHZ"),"465", selectedGpuSpeed == "465");
	optionsGpuMaxClockSpeed->add(_("600 MHZ"),"600", selectedGpuSpeed == "600");
	optionsGpuMaxClockSpeed->add(_("745 MHZ"),"745", selectedGpuSpeed == "745");
	optionsGpuMaxClockSpeed->add(_("820 MHZ"),"820", selectedGpuSpeed == "820");
	optionsGpuMaxClockSpeed->add(_("900 MHZ"),"900", selectedGpuSpeed == "900");
	optionsGpuMaxClockSpeed->add(_("950 MHZ"),"950", selectedGpuSpeed == "950");
	optionsGpuMaxClockSpeed->add(_("980 MHZ"),"980", selectedGpuSpeed == "980");
	optionsGpuMaxClockSpeed->add(_("1050 MHZ"),"1050", selectedGpuSpeed == "1050");
	s->addWithLabel(_("GPU MAX CLOCK SPEED"), optionsGpuMaxClockSpeed);
	s->addSaveFunc([this, optionsGpuMaxClockSpeed, selectedGpuSpeed]
	{
		if (optionsGpuMaxClockSpeed->changed()) {
			SystemConf::getInstance()->set("gpu-max-clock-speed", optionsGpuMaxClockSpeed->getSelected());
			Utils::Platform::runSystemCommand("/usr/lib/autostart/quirks/platforms/SM6115/bin/gpu_max_clock " + optionsGpuMaxClockSpeed->getSelected(), "", nullptr);
		}
	});
#endif

	if (Utils::Platform::GetEnv("DEVICE_GPU_OVERCLOCK") == "true"){
#if defined(SM8250)
		// Add option to set gpu overclock speed
		auto optionsGpuSpeed = std::make_shared<OptionListComponent<std::string> >(mWindow, _("GPU OVERCLOCK SPEED"), false);
		std::string selectedGpuSpeed = SystemConf::getInstance()->get("gpu-overclock-speed");
		if (selectedGpuSpeed.empty())
			selectedGpuSpeed = "0";
		optionsGpuSpeed->add(_("DISABLED"),"0", selectedGpuSpeed == "0");
		optionsGpuSpeed->add(_("650 MHZ"),"650", selectedGpuSpeed == "650");
		optionsGpuSpeed->add(_("700 MHZ"),"700", selectedGpuSpeed == "700");
		optionsGpuSpeed->add(_("725 MHZ"),"725", selectedGpuSpeed == "725");
		optionsGpuSpeed->add(_("750 MHZ"),"750", selectedGpuSpeed == "750");
		optionsGpuSpeed->add(_("800 MHZ"),"800", selectedGpuSpeed == "800");
		optionsGpuSpeed->add(_("855 MHZ"),"855", selectedGpuSpeed == "855");
		optionsGpuSpeed->add(_("905 MHZ"),"905", selectedGpuSpeed == "905");
		optionsGpuSpeed->add(_("925 MHZ"),"925", selectedGpuSpeed == "925");
		s->addWithLabel(_("GPU OVERCLOCK SPEED"), optionsGpuSpeed);
		s->addSaveFunc([this, optionsGpuSpeed, selectedGpuSpeed]
		{
			if (optionsGpuSpeed->changed()) {
				SystemConf::getInstance()->set("gpu-overclock-speed", optionsGpuSpeed->getSelected());
				Utils::Platform::runSystemCommand("/usr/lib/autostart/quirks/platforms/${HW_DEVICE}/bin/gpu_overclock " + optionsGpuSpeed->getSelected(), "", nullptr);
			}
		});
#else
		// Add option to enable gpu overclocking
		auto gpu_overclock = std::make_shared<SwitchComponent>(mWindow);
		bool internalmoduleEnabled = SystemConf::getInstance()->get("enable.gpu-overclock") == "1";
		gpu_overclock->setState(internalmoduleEnabled);
		s->addWithLabel(_("ENABLE GPU OVERCLOCK"), gpu_overclock);
		gpu_overclock->setOnChangedCallback([gpu_overclock] {
			if (gpu_overclock->getState() == false) {
				Utils::Platform::runSystemCommand("/usr/lib/autostart/quirks/platforms/${HW_DEVICE}/bin/gpu_overclock disable", "", nullptr);
			} else {
				Utils::Platform::runSystemCommand("/usr/lib/autostart/quirks/platforms/${HW_DEVICE}/bin/gpu_overclock enable", "", nullptr);
			}
			bool gpuoverclock = gpu_overclock->getState();
			SystemConf::getInstance()->set("enable.gpu-overclock", gpuoverclock ? "1" : "0");
		});
#endif
	}
	const std::string gpuDriverScript = "/usr/bin/gpudriver";
	if (Utils::FileSystem::exists(gpuDriverScript)) {
		auto optionsGpuDriver = std::make_shared<OptionListComponent<std::string> >(mWindow, _("GPU DRIVER"), false);
		std::string selectedGpuDriver = std::string(Utils::Platform::GetShOutput(R"(/usr/bin/gpudriver)"));
		std::string a;
		for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/gpudriver --options)")); getline(ss, a, ' '); ) {
			optionsGpuDriver->add(a, a, a == selectedGpuDriver);
		}
		s->addWithLabel(_("GPU DRIVER"), optionsGpuDriver);
		s->addSaveFunc([this, window, gpuDriverScript, optionsGpuDriver, selectedGpuDriver] {
			if (optionsGpuDriver->changed()) {
				Utils::Platform::runSystemCommand(gpuDriverScript + " " + optionsGpuDriver->getSelected(), "", nullptr);
				window->pushGui(new GuiMsgBox(window, _("GPU driver will be switched on next reboot"),
					_("Reboot now"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
					_("later"), nullptr)
				);
			}
		});
        }

	if (Utils::FileSystem::exists("/usr/bin/dtb_overlay")) {
		s->addGroup(_("TWEAKS"));
		dtbOverlayItem(mWindow, s, "undervolt-cpu");
		dtbOverlayItem(mWindow, s, "custom");
	}

#if defined(AMD64) || defined(RK3326) || defined(RK3566) || defined(RK3588) || defined(RK3399) || defined(SM6115) || defined(SM8250) || defined(H700)
	// Allow user control over how the device sleeps - only show for devices with real suspend enabled
	s->addGroup(_("SUSPEND"));
	auto optionsSleep = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DEVICE SUSPEND MODE"), false);
	std::vector<std::string> availableSleepModes = ApiSystem::getInstance()->getSleepModes();
	std::string selectedSleep = SystemConf::getInstance()->get("system.suspendmode");
	if (selectedSleep.empty())
		selectedSleep = "default";
	bool found = false;
	for (auto it = availableSleepModes.begin(); it != availableSleepModes.end(); it++)
	{
		optionsSleep->add((*it), (*it), selectedSleep == (*it));
		if (selectedSleep == (*it))
			found = true;
	}
	if (!found)
		optionsSleep->add(selectedSleep, selectedSleep, true);
	s->addWithLabel(_("DEVICE SUSPEND MODE"), optionsSleep);
	s->addSaveFunc([this, optionsSleep, selectedSleep]
	{
		if (optionsSleep->changed()) {
			SystemConf::getInstance()->set("system.suspendmode", optionsSleep->getSelected());
			Utils::Platform::runSystemCommand("/usr/bin/suspendmode " + optionsSleep->getSelected(), "", nullptr);
		}
	});
#else
	// Allow user control over a timed shutdown after fake suspend
	s->addGroup(_("SUSPEND"));

	// Fake suspend toggle
	auto optionsEnableSuspend = std::make_shared<SwitchComponent>(mWindow);
	bool suspendEnabled = SystemConf::getInstance()->get("system.suspend.enable") == "1";
	optionsEnableSuspend->setState(suspendEnabled);
	s->addWithLabel(_("ENABLE SUSPEND"), optionsEnableSuspend);
	
	optionsEnableSuspend->setOnChangedCallback([optionsEnableSuspend] {
		bool enableSuspendState = optionsEnableSuspend->getState();
		SystemConf::getInstance()->set("system.suspend.enable", enableSuspendState ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});
	
	// Timed shutdown toggle
	auto optionsEnableTimedShutdown = std::make_shared<SwitchComponent>(mWindow);
	bool timedShutdownEnabled = SystemConf::getInstance()->get("system.suspend.enable_timed_shutdown") == "1";
	optionsEnableTimedShutdown->setState(timedShutdownEnabled);
	s->addWithLabel(_("ENABLE TIMED SHUTDOWN"), optionsEnableTimedShutdown);
	
	optionsEnableTimedShutdown->setOnChangedCallback([optionsEnableTimedShutdown] {
		bool enableTimedShutdownState = optionsEnableTimedShutdown->getState();
		SystemConf::getInstance()->set("system.suspend.enable_timed_shutdown", enableTimedShutdownState ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	// Shutdown delay after suspend
	auto ctlShutdownDelay = std::make_shared<SliderComponent>(mWindow, 0.f, 30.0f, 1.f, "m");

	// Read system config value (seconds)
	std::string shutdownDelayValueString = SystemConf::getInstance()->get("system.shutdown_delay");
	float shutdownDelayValue = 0.f;

	if(shutdownDelayValueString.length() > 0) {
		shutdownDelayValue = (float)atoi(shutdownDelayValueString.c_str());
	}

	// Convert to minutes for UI
	ctlShutdownDelay->setValue((float)(shutdownDelayValue / 60));
	s->addWithLabel(_("SHUTDOWN DELAY"), ctlShutdownDelay);
	s->addSaveFunc([ctlShutdownDelay]
	{
		SystemConf::getInstance()->set("system.shutdown_delay", std::to_string((int)(round(ctlShutdownDelay->getValue()) * 60)));
	});

	// Shutdown delay after suspend (in game)
	auto ctlShutdownInGameDelay = std::make_shared<SliderComponent>(mWindow, 0.f, 30.0f, 1.f, "m");

	// Read system config value (seconds)
	std::string shutdownInGameDelayValueString = SystemConf::getInstance()->get("system.shutdown_delay_running_game");
	float shutdownInGameDelayValue = 0.f;

	if(shutdownInGameDelayValueString.length() > 0) {
		shutdownInGameDelayValue = (float)atoi(shutdownInGameDelayValueString.c_str());
	}

	// Convert to minutes for UI
	ctlShutdownInGameDelay->setValue((float)(shutdownInGameDelayValue / 60));
	s->addWithLabel(_("SHUTDOWN DELAY (IN GAME)"), ctlShutdownInGameDelay);
	s->addSaveFunc([ctlShutdownInGameDelay]
	{
		SystemConf::getInstance()->set("system.shutdown_delay_running_game", std::to_string((int)(round(ctlShutdownInGameDelay->getValue()) * 60)));
	});

	// Add option to park cores on suspend
	auto optionsSuspendParkCores = std::make_shared<SwitchComponent>(mWindow);
	bool suspendParkCoresEnabled = SystemConf::getInstance()->get("system.suspend.park_cores") == "1";
	optionsSuspendParkCores->setState(suspendParkCoresEnabled);
	s->addWithLabel(_("ENABLE CORE PARKING"), optionsSuspendParkCores);
	optionsSuspendParkCores->setOnChangedCallback([optionsSuspendParkCores] {
		bool suspendParkCoresState = optionsSuspendParkCores->getState();
		SystemConf::getInstance()->set("system.suspend.park_cores", suspendParkCoresState ? "1" : "0");
	});

	// Add option to use DPMS on suspend
	auto optionsSuspendDpms = std::make_shared<SwitchComponent>(mWindow);
	bool suspendDpmsEnabled = SystemConf::getInstance()->get("system.suspend.dpms") == "1";
	optionsSuspendDpms->setState(suspendDpmsEnabled);
	s->addWithLabel(_("ENABLE DPMS"), optionsSuspendDpms);
	optionsSuspendDpms->setOnChangedCallback([optionsSuspendDpms] {
		bool suspendDpmsState = optionsSuspendDpms->getState();
		SystemConf::getInstance()->set("system.suspend.dpms", suspendDpmsState ? "1" : "0");
	});
#endif

#if defined(BATTERYPLUS)
	// Allow user control over battery capacity etimates
	s->addGroup(_("BATTERY"));
	auto optionsEnableBatteryplus = std::make_shared<SwitchComponent>(mWindow);
	std::string batteryplusHelpText = _("Enable batteryplus voltage-based capacity estimates.");
	s->addWithDescription(_("ENABLE BATTERYPLUS"), batteryplusHelpText, optionsEnableBatteryplus);
	
	bool batteryplusEnabled = SystemConf::getInstance()->get("system.batteryplus.enabled") == "1";
	optionsEnableBatteryplus->setState(batteryplusEnabled);

	s->addSaveFunc([this, optionsEnableBatteryplus, batteryplusEnabled, s]
	{
		if(optionsEnableBatteryplus->changed()) {
			SystemConf::getInstance()->set("system.batteryplus.enabled", optionsEnableBatteryplus->getState() ? "1" : "0");
			SystemConf::getInstance()->saveSystemConf();
			s->setVariable("exitreboot", true);
		}
	});
#endif

#ifdef BATOCERA
	// video device
	std::vector<std::string> availableVideo = ApiSystem::getInstance()->getAvailableVideoOutputDevices();
	if (availableVideo.size())
	{
		auto optionsVideo = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO OUTPUT"), false);
		std::string currentDevice = SystemConf::getInstance()->get("global.videooutput");
		if (currentDevice.empty()) currentDevice = "auto";

		bool vfound = false;
		for (auto it = availableVideo.begin(); it != availableVideo.end(); it++)
		{
			optionsVideo->add((*it), (*it), currentDevice == (*it));
			if (currentDevice == (*it))
				vfound = true;
		}

		if (!vfound)
			optionsVideo->add(currentDevice, currentDevice, true);

		s->addWithLabel(_("VIDEO OUTPUT"), optionsVideo);
		s->addSaveFunc([this, optionsVideo, currentDevice, s] 
		{
			if (optionsVideo->changed()) 
			{
				SystemConf::getInstance()->set("global.videooutput", optionsVideo->getSelected());
				SystemConf::getInstance()->saveSystemConf();				
				s->setVariable("exitreboot", true);
			}
		});
	}
	// es resolution
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION))
	{
	    auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "es", "resolution");
	    s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution for emulationstation."), videoModeOptionList);
	    s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("es.resolution", videoModeOptionList->getSelected()); });
	}
#endif

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::AUDIODEVICE))
	{
		std::vector<std::string> availableAudio = ApiSystem::getInstance()->getAvailableAudioOutputDevices();
		if (availableAudio.size())
		{
			// audio device
			auto optionsAudio = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO OUTPUT"), false);

			std::string selectedAudio = SystemConf::getInstance()->get("audio.device");
			if (selectedAudio.empty())
				selectedAudio = "auto";

			bool afound = false;
			for (auto it = availableAudio.begin(); it != availableAudio.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, '\t');

				if (selectedAudio == tokens.at(0))
					afound = true;

				if (tokens.size() >= 2)
				{
					// concatenat the ending words
					std::string vname = "";
					for (unsigned int i = 1; i < tokens.size(); i++)
					{
						if (i > 2) vname += " ";
						vname += tokens.at(i);
					}
					optionsAudio->add(vname, tokens.at(0), selectedAudio == tokens.at(0));
				}
				else
					optionsAudio->add((*it), (*it), selectedAudio == tokens.at(0));
			}

			if (!afound)
				optionsAudio->add(selectedAudio, selectedAudio, true);

			s->addWithLabel(_("AUDIO OUTPUT"), optionsAudio);

			s->addSaveFunc([this, optionsAudio, selectedAudio]
			{
				if (optionsAudio->changed())
				{
					SystemConf::getInstance()->set("audio.device", optionsAudio->getSelected());
					ApiSystem::getInstance()->setAudioOutputDevice(optionsAudio->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}

		// audio profile
		std::vector<std::string> availableAudioProfiles = ApiSystem::getInstance()->getAvailableAudioOutputProfiles();
		if (availableAudioProfiles.size())
		{
			auto optionsAudioProfile = std::make_shared<OptionListComponent<std::string> >(mWindow, _("AUDIO PROFILE"), false);

			std::string selectedAudioProfile = SystemConf::getInstance()->get("audio.profile");
			if (selectedAudioProfile.empty())
				selectedAudioProfile = "auto";

			bool afound = false;
			for (auto it = availableAudioProfiles.begin(); it != availableAudioProfiles.end(); it++)
			{
				std::vector<std::string> tokens = Utils::String::split(*it, '\t');

				if (selectedAudioProfile == tokens.at(0))
					afound = true;

				std::string vname = "";
				if (tokens.size() >= 2)
				{
					// Check if the profile contains "bluez_card" and remove it from the display name
					if (tokens.at(1).find("bluez_card") != std::string::npos)
					{
						// Skip the "bluez_card" token and use the remaining tokens for the display name
						for (unsigned int i = 2; i < tokens.size(); i++)
						{
							if (i > 2) vname += " ";
							vname += tokens.at(i);
						}
					}
					else
					{
						// Normal concatenation for other profiles
						for (unsigned int i = 1; i < tokens.size(); i++)
						{
							if (i > 1) vname += " ";
							vname += tokens.at(i);
						}
					}
					optionsAudioProfile->add(vname, tokens.at(0), selectedAudioProfile == tokens.at(0));
				}
				else
					optionsAudioProfile->add((*it), (*it), selectedAudioProfile == tokens.at(0));
			}

			if (afound == false)
				optionsAudioProfile->add(selectedAudioProfile, selectedAudioProfile, true);

			s->addWithDescription(_("AUDIO PROFILE"), _("Available options can change depending on current audio output."), optionsAudioProfile);

			s->addSaveFunc([this, optionsAudioProfile, selectedAudioProfile]
			{
				if (optionsAudioProfile->changed()) {
					SystemConf::getInstance()->set("audio.profile", optionsAudioProfile->getSelected());
					ApiSystem::getInstance()->setAudioOutputProfile(optionsAudioProfile->getSelected());
				}
				SystemConf::getInstance()->saveSystemConf();
			});
		}
	}

#ifdef BATOCERA
	// video rotation
	auto optionsRotation = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SCREEN ROTATION"), false);

	std::string selectedRotation = SystemConf::getInstance()->get("display.rotate");
	if (selectedRotation.empty())
		selectedRotation = "auto";

	optionsRotation->add(_("AUTO"),              "auto", selectedRotation == "auto");
	optionsRotation->add(_("0 DEGREES"),        "0", selectedRotation == "0");
	optionsRotation->add(_("90 DEGREES"),       "1", selectedRotation == "1");
	optionsRotation->add(_("180 DEGREES"),    "2", selectedRotation == "2");
	optionsRotation->add(_("270 DEGREES"),        "3", selectedRotation == "3");

	s->addWithLabel(_("SCREEN ROTATION"), optionsRotation);

	s->addSaveFunc([this, optionsRotation, selectedRotation, s]
	{
	  if (optionsRotation->changed()) 
{
	    SystemConf::getInstance()->set("display.rotate", optionsRotation->getSelected());
	    SystemConf::getInstance()->saveSystemConf();
		s->setVariable("exitreboot", true);
	  }
	});

	// splash
	auto optionsSplash = std::make_shared<OptionListComponent<std::string> >(mWindow, _("BOOT SPLASH"), false);

	std::string enabledSplash = SystemConf::getInstance()->get("splash.screen.enabled");
	std::string soundSplash   = SystemConf::getInstance()->get("splash.screen.sound");

	std::string selectedSplash = "auto";
	if(enabledSplash == "1") {
	  selectedSplash = "splash";
	  if(soundSplash   == "0") selectedSplash = "silentsplash";
	} else {
	  if(enabledSplash == "0") selectedSplash = "nosplash";
	}

	optionsSplash->add(_("AUTO"), "auto", selectedSplash == "auto");
	optionsSplash->add(_("DEFAULT VIDEO/USER SET SPLASH"), "splash",       selectedSplash == "splash");
	optionsSplash->add(_("SILENT VIDEO/USER SET SPLASH"),  "silentsplash", selectedSplash == "silentsplash");
	optionsSplash->add(_("BATOCERA SPLASH IMAGE"),         "nosplash",     selectedSplash == "nosplash");

	s->addWithLabel(_("SPLASH SETTING"), optionsSplash);

	s->addSaveFunc([this, optionsSplash, selectedSplash]
	{
	  if (optionsSplash->changed()) {
	    if(optionsSplash->getSelected() == "auto") {
	      SystemConf::getInstance()->set("splash.screen.enabled", "");
	    } else {
	      if(optionsSplash->getSelected() == "nosplash") {
		SystemConf::getInstance()->set("splash.screen.enabled", "0");
	      } else {
		SystemConf::getInstance()->set("splash.screen.enabled", "1");
		if(optionsSplash->getSelected() == "silentsplash") {
		  SystemConf::getInstance()->set("splash.screen.sound", "0");
		} else {
		  SystemConf::getInstance()->set("splash.screen.sound", "1");
		}
	      }
	    }
	    SystemConf::getInstance()->saveSystemConf();
	  }
	});	
#else
	if (!ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
	{
		// Retroachievements
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
			s->addEntry(_("RETROACHIEVEMENTS SETTINGS"), true, [this] { openRetroachievementsSettings(); });

		if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
			s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");
		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
		{
			s->addEntry(_("BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });
			s->addSwitch(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), "CheckBiosesAtLaunch", true);
		}
	}
#endif

#if ODROIDGOA || GAMEFORCE || RK3326
	// multimedia keys
	auto multimediakeys_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MULTIMEDIA KEYS"));
	multimediakeys_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("system.multimediakeys.enabled") != "0" && SystemConf::getInstance()->get("system.multimediakeys.enabled") != "1");
	multimediakeys_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "1");
	multimediakeys_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("system.multimediakeys.enabled") == "0");
	s->addWithLabel(_("MULTIMEDIA KEYS"), multimediakeys_enabled);
	s->addSaveFunc([this, multimediakeys_enabled, s]
	{
		if (multimediakeys_enabled->changed())
		{
			SystemConf::getInstance()->set("system.multimediakeys.enabled", multimediakeys_enabled->getSelected());
			s->setVariable("reboot", true);
		}
	});
#endif

#if GAMEFORCE
	auto buttonColor_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BUTTON LED COLOR"));
	buttonColor_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("color_rgb") == "off" || SystemConf::getInstance()->get("color_rgb") == "");
	buttonColor_GameForce->add(_("red"), "red", SystemConf::getInstance()->get("color_rgb") == "red");
	buttonColor_GameForce->add(_("green"), "green", SystemConf::getInstance()->get("color_rgb") == "green");
	buttonColor_GameForce->add(_("blue"), "blue", SystemConf::getInstance()->get("color_rgb") == "blue");
	buttonColor_GameForce->add(_("white"), "white", SystemConf::getInstance()->get("color_rgb") == "white");
	buttonColor_GameForce->add(_("purple"), "purple", SystemConf::getInstance()->get("color_rgb") == "purple");
	buttonColor_GameForce->add(_("yellow"), "yellow", SystemConf::getInstance()->get("color_rgb") == "yellow");
	buttonColor_GameForce->add(_("cyan"), "cyan", SystemConf::getInstance()->get("color_rgb") == "cyan");		
	s->addWithLabel(_("BUTTON LED COLOR"), buttonColor_GameForce);
	s->addSaveFunc([buttonColor_GameForce] 
	{
		if (buttonColor_GameForce->changed()) {
			ApiSystem::getInstance()->setButtonColorGameForce(buttonColor_GameForce->getSelected());
			SystemConf::getInstance()->set("color_rgb", buttonColor_GameForce->getSelected());
		}
	});

	auto powerled_GameForce = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER LED COLOR"));
	powerled_GameForce->add(_("heartbeat"), "heartbeat", SystemConf::getInstance()->get("option_powerled") == "heartbeat" || SystemConf::getInstance()->get("option_powerled") == "");
	powerled_GameForce->add(_("off"), "off", SystemConf::getInstance()->get("option_powerled") == "off");
	powerled_GameForce->add(_("on"), "on", SystemConf::getInstance()->get("option_powerled") == "on");	
	s->addWithLabel(_("POWER LED COLOR"), powerled_GameForce);
	s->addSaveFunc([powerled_GameForce] 
	{
		if (powerled_GameForce->changed()) {
			ApiSystem::getInstance()->setPowerLedGameForce(powerled_GameForce->getSelected());
			SystemConf::getInstance()->set("option_powerled", powerled_GameForce->getSelected());
		}
	});
#endif

#if RK3326
	if (Utils::FileSystem::exists("/sys/class/leds/keros::ambient")) {
		auto buttonColor_r36ultra = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BUTTON LED COLOR"));
		buttonColor_r36ultra->add(_("off"), "off", SystemConf::getInstance()->get("color_rgb") == "off" || SystemConf::getInstance()->get("color_rgb") == "");
		buttonColor_r36ultra->add(_("red"), "red", SystemConf::getInstance()->get("color_rgb") == "red");
		buttonColor_r36ultra->add(_("yellow"), "yellow", SystemConf::getInstance()->get("color_rgb") == "yellow");
		buttonColor_r36ultra->add(_("green"), "green", SystemConf::getInstance()->get("color_rgb") == "green");
		buttonColor_r36ultra->add(_("cyan"), "cyan", SystemConf::getInstance()->get("color_rgb") == "cyan");
		buttonColor_r36ultra->add(_("blue"), "blue", SystemConf::getInstance()->get("color_rgb") == "blue");
		buttonColor_r36ultra->add(_("purple"), "purple", SystemConf::getInstance()->get("color_rgb") == "purple");
		buttonColor_r36ultra->add(_("white"), "white", SystemConf::getInstance()->get("color_rgb") == "white");		
		s->addWithLabel(_("BUTTON LED COLOR"), buttonColor_r36ultra);
		s->addSaveFunc([buttonColor_r36ultra] 
		{
			if (buttonColor_r36ultra->changed()) {
				ApiSystem::getInstance()->setButtonColorR36Ultra(buttonColor_r36ultra->getSelected());
				SystemConf::getInstance()->set("color_rgb", buttonColor_r36ultra->getSelected());
			}
		});
	}

	auto powerled_r36 = std::make_shared< OptionListComponent<std::string> >(mWindow, _("POWER LED COLOR"));
	powerled_r36->add(_("on"), "on", SystemConf::getInstance()->get("option_powerled") == "on" || SystemConf::getInstance()->get("option_powerled") == "");
	powerled_r36->add(_("off"), "off", SystemConf::getInstance()->get("option_powerled") == "off");
	s->addWithLabel(_("POWER LED COLOR"), powerled_r36);
	s->addSaveFunc([powerled_r36]
	{
		if (powerled_r36->changed()) {
			ApiSystem::getInstance()->setPowerLedR36(powerled_r36->getSelected());
			SystemConf::getInstance()->set("option_powerled", powerled_r36->getSelected());
		}
	});
#endif

	// Overclock choice
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::OVERCLOCK))
	{
		auto overclock_choice = std::make_shared<OptionListComponent<std::string>>(window, _("OVERCLOCK"), false);

		std::string currentOverclock = Settings::getInstance()->getString("Overclock");
		if (currentOverclock == "")
			currentOverclock = "none";

		std::vector<std::string> availableOverclocking = ApiSystem::getInstance()->getAvailableOverclocking();

		// Overclocking device
		bool isOneSet = false;
		for (auto it = availableOverclocking.begin(); it != availableOverclocking.end(); it++)
		{
			std::vector<std::string> tokens = Utils::String::split(*it, ' ');
			if (tokens.size() >= 2)
			{
				// concatenat the ending words
				std::string vname;
				for (unsigned int i = 1; i < tokens.size(); i++)
				{
					if (i > 1) vname += " ";
					vname += tokens.at(i);
				}
				bool isSet = currentOverclock == std::string(tokens.at(0));
				if (isSet)
					isOneSet = true;

				if (vname == "NONE" || vname == "none")
					vname = _("NONE");

				overclock_choice->add(vname, tokens.at(0), isSet);
			}
		}

		if (isOneSet == false)
		{
			if (currentOverclock == "none")
				overclock_choice->add(_("NONE"), currentOverclock, true);
			else
				overclock_choice->add(currentOverclock, currentOverclock, true);
		}

		// overclocking
		s->addWithLabel(_("OVERCLOCK"), overclock_choice);

		s->addSaveFunc([overclock_choice, window, s]
		{
			if (overclock_choice->changed() && Settings::getInstance()->setString("Overclock", overclock_choice->getSelected()))
			{
				ApiSystem::getInstance()->setOverclock(overclock_choice->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

#ifdef BATOCERA
	s->addEntry(_("DMD"), true, [this] { openDmdSettings(); });
#endif

#ifdef BATOCERA
        s->addEntry(_("MULTISCREENS"), true, [this] { openMultiScreensSettings(); });
#endif

#ifdef BATOCERA
	int red, green, blue;
	bool ledSupported = ApiSystem::getInstance()->getLED(red, green, blue);

	if (ledSupported) {
		s->addGroup(_("LED HARDWARE"));

		auto led_enabled_switch = std::make_shared<SwitchComponent>(mWindow);
		bool isEnabled = ApiSystem::getInstance()->isLEDEnabled();
		led_enabled_switch->setState(isEnabled);
		s->addWithLabel(_("ENABLE LED"), led_enabled_switch);
		
		// Only display RGB color sliders and modes if the hardware is NOT monochrome
		if (!ApiSystem::getInstance()->isLEDMonochrome()) {
			// LED MODE Dropdown Component
			std::string currentMode = SystemConf::getInstance()->get("led.mode");
			if (currentMode.empty())
				currentMode = "static";

			auto ledMode = std::make_shared<OptionListComponent<std::string>>(mWindow, _("LED MODE"), false);
			ledMode->addRange({
				{ _("STATIC"), "static" },
				{ _("RAINBOW"), "rainbow" },
				{ _("CHROMA"), "chroma" },
				{ _("PULSE"), "pulse" }
			}, currentMode);
			s->addWithLabel(_("LED MODE"), ledMode);

			// RGB Configuration Colors
			std::string colourString = SystemConf::getInstance()->get("led.colour");
			if (colourString.empty())
				colourString = "255 0 165";

			std::stringstream ss(colourString);
			ss >> red >> green >> blue; 

			auto redLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
			auto greenLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);
			auto blueLEDComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 255.f, 1.f);

			// Track if sliders were adjusted so we can revert mode back to static
			auto sliderChanged = std::make_shared<bool>(false);

			redLEDComponent->setValue(red);
			redLEDComponent->setOnValueChanged([greenLEDComponent, blueLEDComponent, sliderChanged](const float &newVal) {
				int redInt = static_cast<int>(newVal);
				int greenInt = static_cast<int>(greenLEDComponent->getValue());
				int blueInt = static_cast<int>(blueLEDComponent->getValue());
				*sliderChanged = true;
				ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
				std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
				SystemConf::getInstance()->set("led.colour", colourString);
			});
			s->addWithLabel(_("RED"), redLEDComponent);

			greenLEDComponent->setValue(green);
			greenLEDComponent->setOnValueChanged([redLEDComponent, blueLEDComponent, sliderChanged](const float &newVal) {
				int redInt = static_cast<int>(redLEDComponent->getValue());
				int greenInt = static_cast<int>(newVal);
				int blueInt = static_cast<int>(blueLEDComponent->getValue());
				*sliderChanged = true;
				ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
				std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
				SystemConf::getInstance()->set("led.colour", colourString);
			});
			s->addWithLabel(_("GREEN"), greenLEDComponent);

			blueLEDComponent->setValue(blue);
			blueLEDComponent->setOnValueChanged([redLEDComponent, greenLEDComponent, sliderChanged](const float &newVal) {
				int redInt = static_cast<int>(redLEDComponent->getValue());
				int greenInt = static_cast<int>(greenLEDComponent->getValue());
				int blueInt = static_cast<int>(newVal);
				*sliderChanged = true;
				ApiSystem::getInstance()->setLEDColours(redInt, greenInt, blueInt);
				std::string colourString = std::to_string(redInt) + " " + std::to_string(greenInt) + " " + std::to_string(blueInt);
				SystemConf::getInstance()->set("led.colour", colourString);
			});
			s->addWithLabel(_("BLUE"), blueLEDComponent);

			s->addSaveFunc([led_enabled_switch, ledMode, sliderChanged] {
				bool state = led_enabled_switch->getState();
				if (state != (SystemConf::getInstance()->get("led.enabled") != "0")) {
					ApiSystem::getInstance()->setLEDEnabled(state);
				}

				std::string newMode = ledMode->getSelected();
				if (*sliderChanged) {
					newMode = "static";
				}
				if (newMode != SystemConf::getInstance()->get("led.mode")) {
					ApiSystem::getInstance()->setLEDMode(newMode);
				}
			});
		} else {
			s->addSaveFunc([led_enabled_switch] {
				bool state = led_enabled_switch->getState();
				if (state != (SystemConf::getInstance()->get("led.enabled") != "0")) {
					ApiSystem::getInstance()->setLEDEnabled(state);
				}
			});
		}
	}
	
	// LED brightness - Only display if the hardware is NOT monochrome
	int ledBrightness;
	if (!ApiSystem::getInstance()->isLEDMonochrome() && ApiSystem::getInstance()->getLEDBrightness(ledBrightness)) {
		auto ledBrightnessComponent = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		ledBrightnessComponent->setValue(ledBrightness);
		ledBrightnessComponent->setOnValueChanged([](const float &newVal)
		{
			ApiSystem::getInstance()->setLEDBrightness((int)Math::round(newVal));
			SystemConf::getInstance()->set("led.brightness", std::to_string((int)Math::round(newVal)));
		});

		s->addWithLabel(_("LED BRIGHTNESS"), ledBrightnessComponent);
	}
#endif

#if defined(BATOCERA)
	s->addGroup(_("STORAGE"));

	// Storage device
	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableStorageDevices();
	if (availableStorage.size())
	{		
		std::string selectedStorage = ApiSystem::getInstance()->getCurrentStorage();

		auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("STORAGE DEVICE"), false);
		for (auto it = availableStorage.begin(); it != availableStorage.end(); it++)
		{
				if (Utils::String::startsWith(*it, "DEV"))
				{
					std::vector<std::string> tokens = Utils::String::split(*it, ' ');

					if (tokens.size() >= 3) {
						// concatenat the ending words
						std::string vname = "";
						for (unsigned int i = 2; i < tokens.size(); i++) {
							if (i > 2) vname += " ";
							vname += tokens.at(i);
						}
						optionsStorage->add(vname, (*it), selectedStorage == std::string("DEV " + tokens.at(1)));
					}
				} else {
				  std::vector<std::string> tokens = Utils::String::split(*it, ' ');
				  if (tokens.size() == 1) {
					optionsStorage->add((*it), (*it), selectedStorage == (*it));
				  } else {
				    // concatenat the ending words
				    std::string vname = "";
				    for (unsigned int i = 1; i < tokens.size(); i++) {
				      if (i > 1) vname += " ";
				      vname += tokens.at(i);
				    }
				    optionsStorage->add(_(vname.c_str()), tokens.at(0), selectedStorage == tokens.at(0));
				  }
				}
		}

		s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);
		s->addSaveFunc([optionsStorage, selectedStorage, s]
		{
			if (optionsStorage->changed())
			{
				ApiSystem::getInstance()->setStorage(optionsStorage->getSelected());
				s->setVariable("reboot", true);
			}
		});
	}

	// backup
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BACKUP))
		s->addEntry(_("BACKUP USER DATA"), true, [this] { mWindow->pushGui(new GuiBackupStart(mWindow)); });

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::INSTALL))
		s->addEntry(_("INSTALL ON A NEW DISK"), true, [this] { mWindow->pushGui(new GuiInstallStart(mWindow)); });

	s->addEntry(_("EJECT AN EXTRA DISK"), true, [this] { openUnmountDriveSettings(); });

    auto diskFormat = std::make_shared<OptionListComponent<std::string>>(window, _("EXTRA DRIVE FILESYSTEM TYPE"), false);
    
    // Load saved preference (default to btrfs)
    std::string selectedFormat = SystemConf::getInstance()->get("system.external_disk_format");
    
    if (selectedFormat.empty()) selectedFormat = "btrfs";

    std::vector<std::string> fstypes = ApiSystem::getInstance()->getFormatFileSystems();
    if (fstypes.empty()) fstypes = { "ext4", "btrfs", "exfat" };

    for (const auto& fs : fstypes) {
        diskFormat->add(Utils::String::toUpper(fs), fs, selectedFormat == fs);
    }
    
    if (!diskFormat->hasSelection()) {
         diskFormat->selectFirstItem();
    }

    s->addWithLabel(_("EXTRA DRIVE FILESYSTEM TYPE"), diskFormat);
    
    s->addSaveFunc([diskFormat] {
        if (diskFormat->changed()) {
            SystemConf::getInstance()->set("system.external_disk_format", diskFormat->getSelected());
            SystemConf::getInstance()->saveSystemConf();
        }
    });
	
	s->addGroup(_("ADVANCED"));

	if(ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES)) {
	  // Services
	  if (isFullUI)
	    s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}

	// Security
	s->addEntry(_("SECURITY"), true, [this, s] 
	{
		GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
		auto securityEnabled = std::make_shared<SwitchComponent>(mWindow);
		securityEnabled->setState(SystemConf::getInstance()->get("system.security.enabled") == "1");
		securityGui->addWithDescription(_("ENFORCE SECURITY"), _("Require a password for accessing the network share."), securityEnabled);

		auto rootpassword = std::make_shared<TextComponent>(mWindow, ApiSystem::getInstance()->getRootPassword(), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
		securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

#ifdef BATOCERA
		auto cpuMitigations = std::make_shared<SwitchComponent>(mWindow);
		cpuMitigations->setState(ApiSystem::getInstance()->areCpuMitigationsEnabled());
		securityGui->addWithDescription(
			_("CPU SECURITY MITIGATIONS"),
			_("Disabling mitigations may improve performance on some systems at the cost of reduced protection against certain CPU vulnerabilities."),
			cpuMitigations);
#endif

		securityGui->addSaveFunc([this, securityEnabled, s]
		{
			Window* window = this->mWindow;

			if (securityEnabled->changed()) 
			{
				SystemConf::getInstance()->set("system.security.enabled", securityEnabled->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
				s->setVariable("reboot", true);				
			}
		});

#ifdef BATOCERA
		securityGui->addSaveFunc([cpuMitigations, s]
		{
			if (cpuMitigations->changed())
			{
				if (ApiSystem::getInstance()->setCpuMitigationsEnabled(cpuMitigations->getState()))
					s->setVariable("reboot", true);
			}
		});
#endif

		mWindow->pushGui(securityGui);
	});
#else
	if (isFullUI)
	{
		s->addGroup(_("ADVANCED"));

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SERVICES) && ApiSystem::getInstance()->getServices().size())
			s->addEntry(_("SERVICES"), true, [this] { openServicesSettings(); });
	}
#endif
	
	// Developer options
	if (isFullUI) {
		s->addEntry(_("FRONTEND DEVELOPER OPTIONS"), true, [this] { openDeveloperSettings(); });

		// Security
		s->addEntry(_("SECURITY"), true, [this, s]
		{
			GuiSettings *securityGui = new GuiSettings(mWindow, _("SECURITY").c_str());
			auto rootpassword = std::make_shared<TextComponent>(mWindow, SystemConf::getInstance()->get("root.password"), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color);
			securityGui->addInputTextConfigRow(_("ROOT PASSWORD"), "root.password", false);

			securityGui->addSaveFunc([this, rootpassword] {
				SystemConf::getInstance()->saveSystemConf();
				const std::string rootpass = SystemConf::getInstance()->get("root.password");
				// Quoted, as the Wi-Fi key is: a password with a space, a $ or a
				// quote reached the shell unquoted and was cut or misread (fork #198).
				Utils::Platform::runSystemCommand("setrootpass " + Utils::String::shellQuote(rootpass), "", nullptr);
			});
			mWindow->pushGui(securityGui);
		});

		s->addEntry(_("SYSTEM MANAGEMENT AND RESET"), true, [this] { openResetOptions(); });
	}

	auto pthis = this;
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("exitreboot") && Settings::getInstance()->getBool("ExitOnRebootRequired"))
		{
			Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
			return;
		}
		
		if (s->getVariable("reboot") || s->getVariable("exitreboot"))
			window->displayNotificationMessage(_U("\uF011  ") + _("REBOOT REQUIRED TO APPLY THE NEW CONFIGURATION"));

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window, false));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openLatencyReductionConfiguration(Window* mWindow, std::string configName)
{
	GuiSettings* guiLatency = new GuiSettings(mWindow, _("LATENCY REDUCTION").c_str());

	// run-ahead
	auto runahead_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("RUN-AHEAD FRAMES"));
	runahead_enabled->addRange({ { _("AUTO"), "" }, { _("NONE"), "0" }, { "1", "1" }, { "2", "2" }, { "3", "3" }, { "4", "4" }, { "5", "5" }, { "6", "6" } }, SystemConf::getInstance()->get(configName + ".runahead"));
	guiLatency->addWithDescription(_("RUN-AHEAD FRAMES"), _("High numbers can result in visible jitter."), runahead_enabled);
	guiLatency->addSaveFunc([configName, runahead_enabled] { SystemConf::getInstance()->set(configName + ".runahead", runahead_enabled->getSelected()); });

	// second instance
	auto secondinstance = std::make_shared<OptionListComponent<std::string>>(mWindow, _("USE SECOND INSTANCE FOR RUN-AHEAD"));
	secondinstance->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".secondinstance"));
	guiLatency->addWithDescription(_("USE SECOND INSTANCE FOR RUN-AHEAD"), _("Can prevent audio skips on button presses."), secondinstance);
	guiLatency->addSaveFunc([configName, secondinstance] { SystemConf::getInstance()->set(configName + ".secondinstance", secondinstance->getSelected()); });

	// auto frame delay
	auto video_frame_delay_auto = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOMATIC FRAME DELAY"));
	video_frame_delay_auto->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".video_frame_delay_auto"));
	guiLatency->addWithDescription(_("AUTO FRAME DELAY"), _("Automatically decrease frame delay temporarily to prevent frame drops. Can introduce stuttering."), video_frame_delay_auto);
	guiLatency->addSaveFunc([configName, video_frame_delay_auto] { SystemConf::getInstance()->set(configName + ".video_frame_delay_auto", video_frame_delay_auto->getSelected()); });

	// variable refresh rate (freesync, gsync, etc.)
	auto vrr_runloop_enable = std::make_shared<OptionListComponent<std::string>>(mWindow, _("VARIABLE REFRESH RATE (G-SYNC, FREESYNC)"));
	vrr_runloop_enable->addRange({ { _("AUTO"), "" }, { _("ON"), "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".vrr_runloop_enable"));
	guiLatency->addWithDescription(_("VARIABLE REFRESH RATE"), _("Don't deviate from core requested timing. G-Sync, FreeSync, HDMI 2.1 VRR."), vrr_runloop_enable);
	guiLatency->addSaveFunc([configName, vrr_runloop_enable] { SystemConf::getInstance()->set(configName + ".vrr_runloop_enable", vrr_runloop_enable->getSelected()); });

	mWindow->pushGui(guiLatency);
}

void GuiMenu::dtbOverlayItem(Window* mWindow, GuiSettings *s, const std::string dtb_type)
{
	const std::string overlayScript = "/usr/bin/dtb_overlay";
	const std::string getDtb = overlayScript + " get " + dtb_type;
	const std::string listDtb = overlayScript + " ls " + dtb_type;
	const std::string setDtb = overlayScript + " set " + dtb_type;
	const std::string title = dtb_type + " DTB OVERLAY";

	auto optionsDtb = std::make_shared<OptionListComponent<std::string> >(mWindow, _(title.c_str()), false);
	std::string selectedDtb = std::string(Utils::Platform::GetShOutput(getDtb));
	if (selectedDtb.empty() || selectedDtb.compare("None") == 0)
		optionsDtb->add("None", "None", true);
	else
		optionsDtb->add("None", "None", false);

	std::string option;
	for(std::stringstream ss(Utils::Platform::GetShOutput(listDtb)); getline(ss, option, ' '); ) {
		optionsDtb->add(option, option, option == selectedDtb);
	}

	s->addWithLabel(_(title.c_str()), optionsDtb);

	s->addSaveFunc([this, mWindow, overlayScript, setDtb, optionsDtb] {
	if (optionsDtb->changed()) {
			Utils::Platform::runSystemCommand(setDtb + " " + optionsDtb->getSelected(), "", nullptr);
			mWindow->pushGui(new GuiMsgBox(mWindow, _("WARNING: You are altering "
				"hardware parameters that may yield your system unstable or unbootable. "
				"In this case you will need to recover by manually editing "
				"extlinux/extlinux.conf on the ROCKNIX partition from a PC, by "
				"removing the whole line starting with: FDTOVERLAYS. \n "
				"The changes will be applied on next reboot"),
				_("Reboot now"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
				_("later"), nullptr));
			}
	});
}

void GuiMenu::openSystemOptionsConfiguration(Window* mWindow, std::string configName)
{
	GuiSettings* guiSystemOptions = new GuiSettings(mWindow, _("SYSTEM OPTIONS").c_str());
	bool cfound = false;

#if defined(S922X) || defined(RK3588) || defined(RK3399)|| defined(RK3576)|| defined(SM4450) || defined(SM6115) || defined(SM8250) || defined(SM8550) || defined(SM8650)
	// Core chooser
	auto cores_used = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CORES USED"));
	cores_used->addRange({ {("DEFAULT"), "" }, { _("ALL"), "all" },{ _("BIG") , "big" },{ _("LITTLE") , "little" } }, SystemConf::getInstance()->get(configName + ".cores"));
	guiSystemOptions->addWithLabel(_("CORES USED"), cores_used);
	guiSystemOptions->addSaveFunc([cores_used, configName] { SystemConf::getInstance()->set(configName + ".cores", cores_used->getSelected()); });
#endif

	if (Utils::Platform::GetEnv("DEVICE_HAS_FAN") == "true") {
		// Provides cooling profile switching
		auto optionsFanProfile = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COOLING PROFILE"), false);
		std::string selectedFanProfile = SystemConf::getInstance()->get(configName + ".cooling.profile");
		if (selectedFanProfile.empty())
			selectedFanProfile = "default";

		optionsFanProfile->add(_("DEFAULT"),"default", selectedFanProfile == "default");
		optionsFanProfile->add(_("AUTO"),"auto", selectedFanProfile == "auto");
		optionsFanProfile->add(_("QUIET"),"quiet", selectedFanProfile == "quiet");
		optionsFanProfile->add(_("MODERATE"),"moderate", selectedFanProfile == "moderate");
		optionsFanProfile->add(_("AGGRESSIVE"),"aggressive", selectedFanProfile == "aggressive");
		optionsFanProfile->add(_("CUSTOM"),"custom", selectedFanProfile == "custom");

		guiSystemOptions->addWithLabel(_("COOLING PROFILE"), optionsFanProfile);
		guiSystemOptions->addSaveFunc([optionsFanProfile, selectedFanProfile, configName]
			{
				if (optionsFanProfile->changed()) {
				SystemConf::getInstance()->set(configName + ".cooling.profile", optionsFanProfile->getSelected());
			}
		});
	}

	// Per game/core/emu CPU governor
	auto optionsGovernors = std::make_shared<OptionListComponent<std::string> >(mWindow, _("CPU SCALING GOVERNOR"), false);

	std::vector<std::string> availableGovernors = ApiSystem::getInstance()->getAvailableCpuGovernors();
	std::string selectedGovernors = SystemConf::getInstance()->get(configName + ".cpugovernor");
	if (selectedGovernors.empty())
		selectedGovernors = "default";

	cfound = false;
	for (auto it = availableGovernors.begin(); it != availableGovernors.end(); it++)
	{
		optionsGovernors->add((*it), (*it), selectedGovernors == (*it));
		if (selectedGovernors == (*it))
			cfound = true;
	}
	if (!cfound)
		optionsGovernors->add(selectedGovernors, selectedGovernors, true);

	guiSystemOptions->addWithLabel(_("CPU SCALING GOVERNOR"), optionsGovernors);

	guiSystemOptions->addSaveFunc([configName, selectedGovernors, optionsGovernors]
	{
		if (optionsGovernors->changed()) {
			SystemConf::getInstance()->set(configName + ".cpugovernor", optionsGovernors->getSelected());
		}
	});

	// GPU performance mode with enhanced power savings
	auto optionsGpuGovernors = std::make_shared<OptionListComponent<std::string> >(mWindow, _("GPU SCALING GOVERNOR"), false);

	std::vector<std::string> availableGpuGovernors = ApiSystem::getInstance()->getAvailableGpuGovernors();
	std::string selectedGpuGovernor = SystemConf::getInstance()->get(configName + ".gpuperf");
	
	if (selectedGpuGovernor.empty())
		selectedGpuGovernor = "default";
	
	bool selectedGpuGovernorFound = false;
	
	for (auto it = availableGpuGovernors.begin(); it != availableGpuGovernors.end(); it++)
	{
		optionsGpuGovernors->add((*it), (*it), selectedGpuGovernor == (*it));
		if (selectedGpuGovernor == (*it))
			selectedGpuGovernorFound = true;
	}

	if (!selectedGpuGovernorFound)
		optionsGpuGovernors->selectFirstItem(); // Preselect 'default' 
	
	guiSystemOptions->addWithLabel(_("GPU SCALING GOVERNOR"), optionsGpuGovernors);
	guiSystemOptions->addSaveFunc([configName, selectedGpuGovernor, optionsGpuGovernors]
	{
		if (optionsGpuGovernors->changed()) {
			SystemConf::getInstance()->set(configName + ".gpuperf", optionsGpuGovernors->getSelected());
		}
	});

#if defined(ROCKNIX)
	// Per game/core/emu Mangohud
	if (Utils::Platform::GetEnv("DEVICE_MANGOHUD_SUPPORT") == "true"){
		auto mangohud = std::make_shared<OptionListComponent<std::string>>(mWindow, _("MANGOHUD OVERLAY"));
		mangohud->addRange({ {("DEFAULT"), "" }, { _("ENABLED"), "1" },{ _("DISABLED") , "0" } }, SystemConf::getInstance()->get(configName + ".rocknix.mangohud.enabled"));
		guiSystemOptions->addWithLabel(_("MANGOHUD OVERLAY"), mangohud);
		guiSystemOptions->addSaveFunc([mangohud, configName] { SystemConf::getInstance()->set(configName + ".rocknix.mangohud.enabled", mangohud->getSelected()); });
	}
#endif

    // Per game/core/emu Display mode
    std::vector<std::string> availableDisplayModes = ApiSystem::getInstance()->getAvailableDisplayModes();
    if (availableDisplayModes.size() > 1){
        auto optionsDisplayModes = std::make_shared<OptionListComponent<std::string> >(mWindow, _("DISPLAY MODE"), false);

        std::string selectedDisplayMode = SystemConf::getInstance()->get(configName + ".display_mode");
        for (auto it = availableDisplayModes.begin(); it != availableDisplayModes.end(); it++)
        {
            if (selectedDisplayMode.empty())
            {
                selectedDisplayMode = "default";
                continue;
            }

            // Remove resolution at start, and any trailing markers, e.g. "preferred" or  "current"
            (*it) = (*it).substr((*it).find("px, ") + 4);
            (*it) = (*it).substr(0, (*it).find(" Hz") + 3);
        }
        availableDisplayModes.insert(availableDisplayModes.begin(), "default");

        cfound = false;
        for (auto it = availableDisplayModes.begin(); it != availableDisplayModes.end(); it++)
        {
	        optionsDisplayModes->add((*it), (*it), selectedDisplayMode == (*it));
	        if (selectedDisplayMode == (*it))
		        cfound = true;
        }

        if (!cfound)
            optionsDisplayModes->add(selectedDisplayMode, selectedDisplayMode, true);

        guiSystemOptions->addWithLabel(_("DISPLAY MODE"), optionsDisplayModes);
        guiSystemOptions->addSaveFunc([configName, selectedDisplayMode, optionsDisplayModes]
        {
            if (optionsDisplayModes->changed())
            {
                SystemConf::getInstance()->set(configName + ".display_mode", optionsDisplayModes->getSelected());
                SystemConf::getInstance()->saveSystemConf();
            }
        });
    }

	mWindow->pushGui(guiSystemOptions);
}

void GuiMenu::openRetroachievementsSettings()
{
	mWindow->pushGui(new GuiRetroAchievementsSettings(mWindow));
}

void GuiMenu::openNetplaySettings()
{
	mWindow->pushGui(new GuiNetPlaySettings(mWindow));	
}

void GuiMenu::openAnalogSticksLedControls()
{
	mWindow->pushGui(new GuiAnalogSticksLedControls(mWindow));	
}

void GuiMenu::addDecorationSetOptionListComponent(Window* window, GuiSettings* parentWindow, const std::vector<DecorationSetInfo>& sets, const std::string& configName)
{
	auto decorations = std::make_shared<OptionListComponent<std::string> >(window, _("DECORATION SET"), false);
	decorations->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });

	std::vector<std::string> items = { _("AUTO"), _("NONE") };
	for (auto set : sets)
		items.push_back(set.name);

	std::string bezel = SystemConf::getInstance()->get(configName + ".bezel");

	for (auto item : items)
		decorations->add(item, item, (bezel == item) || (bezel == "none" && item == _("NONE")) || (bezel == "" && item == _("AUTO")));

	if (!decorations->hasSelection())
		decorations->selectFirstItem();

	parentWindow->addWithLabel(_("DECORATION SET"), decorations);
	parentWindow->addSaveFunc([decorations, configName]
	{
		SystemConf::getInstance()->set(configName + ".bezel", decorations->getSelected() == _("NONE") ? "none" : decorations->getSelected() == _("AUTO") ? "" : decorations->getSelected());
	});
}



void GuiMenu::addFeatureItem(Window* window, GuiSettings* settings, const CustomFeature& feat, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core)
{	
	if (feat.preset == "hidden")
		return;

	std::string storageName = configName + "." + feat.value;
	
	if (configName == "global" && Utils::String::startsWith(feat.value, configName + "."))
		storageName = feat.value;
			
	if (feat.preset == "input")
	{
		settings->addInputTextConfigRow(pgettext("game_options", feat.name.c_str()), storageName, false);
		return;
	}
	
	if (feat.preset == "password")
	{
		settings->addInputTextConfigRow(pgettext("game_options", feat.name.c_str()), storageName, true);
		return;
	}
	
	if (feat.preset == "image")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::IMAGES);
		return;
	}

	if (feat.preset == "video")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::VIDEO);
		return;
	}

	if (feat.preset == "folder")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::DIRECTORY);
		return;
	}

	if (feat.preset == "document")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::MANUALS);
		return;
	}

	if (feat.preset == "files")
	{
		settings->addFileBrowser(pgettext("game_options", feat.name.c_str()), storageName, GuiFileBrowser::FILES);
		return;
	}

	std::string storedValue = SystemConf::getInstance()->get(storageName);
	
	std::string inheritedValue;
	if (!Utils::String::startsWith(storageName, "global."))
	{
		std::string systemSetting = storageName;

		bool querySystemSetting = false;

		// Look if we are using a "per-game" setting, then compute the system setting name
		auto gameInfoStart = storageName.find("[\"");
		if (gameInfoStart != std::string::npos)
		{
			auto gameInfoEnd = storageName.find("\"]");
			if (gameInfoEnd != std::string::npos)
			{
				systemSetting = storageName.substr(0, gameInfoStart) + storageName.substr(gameInfoEnd + 2);
				querySystemSetting = true;
			}
		}
	
		// First find the global option
		auto dotPos = systemSetting.find(".");
		if (dotPos != std::string::npos)
		{
			std::string globalSetting = "global." + systemSetting.substr(dotPos + 1);
			std::string globalStoredValue = SystemConf::getInstance()->get(globalSetting);
			if (!globalStoredValue.empty() && globalStoredValue != "auto" && globalStoredValue != storedValue)
				inheritedValue = globalStoredValue;
		}

		// Then take the system option
		if (querySystemSetting)
		{
			std::string systemStoredValue = SystemConf::getInstance()->get(systemSetting);
			if (!systemStoredValue.empty() && systemStoredValue != "auto" && systemStoredValue != storedValue)
				inheritedValue = systemStoredValue;
		}
	}

	if (feat.preset == "switch" || feat.preset == "switch_default_off")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue == "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "1" : ""); });
		return;
	}

	if (feat.preset == "switchauto")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setHasAuto(true);
		switchComponent->setAutoState(storedValue == "");
		switchComponent->setState(storedValue == "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getAutoState() ? "" : (switchComponent->getState() ? "1" : "0")); });
		return;
	}

	if (feat.preset == "switchon" || feat.preset == "switch_default_on")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "0");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "0"); });
		return;
	}

	if (feat.preset == "switchoff" || feat.preset == "switch_default_off_reverse_value")
	{
		auto switchComponent = std::make_shared<SwitchComponent>(window);
		switchComponent->setState(storedValue != "1");

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), switchComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), switchComponent);

		settings->addSaveFunc([storageName, switchComponent] { SystemConf::getInstance()->set(storageName, switchComponent->getState() ? "" : "1"); });
		return;
	}

	if (feat.preset == "slider")
	{
		std::vector<std::string> tokens = Utils::String::split(feat.preset_parameters, ' ');

		float slider_from    =   0.0f;
		float slider_to      = 100.0f;
		float slider_step    =   1.0f;
		float slider_default =   0.0f;
		std::string slider_suffix = "";

		if (tokens.size() >= 1) slider_from    = Utils::String::toFloat(tokens.at(0));
		if (tokens.size() >= 2) slider_to      = Utils::String::toFloat(tokens.at(1));
		if (tokens.size() >= 3) slider_step    = Utils::String::toFloat(tokens.at(2));
		if (tokens.size() >= 4) slider_default = Utils::String::toFloat(tokens.at(3));
		if (tokens.size() >= 5) slider_suffix  = tokens.at(4);

		auto sliderComponent = std::make_shared<SliderComponent>(window, slider_from, slider_to, slider_step, slider_suffix);
		if(storedValue == "") {
		  sliderComponent->setValue(slider_default);
		} else {
		  sliderComponent->setValue(Utils::String::toFloat(storedValue));
		}

		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), sliderComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), sliderComponent);

		settings->addSaveFunc([storageName, sliderComponent] { SystemConf::getInstance()->set(storageName, std::to_string(sliderComponent->getValue())); });
		return;
	}

	if (feat.preset == "sliderauto")
	{
		std::vector<std::string> tokens = Utils::String::split(feat.preset_parameters, ' ');
		float slider_from = 0.0f;
		float slider_to = 100.0f;
		float slider_step = 1.0f;
		std::string slider_suffix = "";

		// Parse parameters from the preset
		if (tokens.size() >= 1) slider_from = Utils::String::toFloat(tokens.at(0));
		if (tokens.size() >= 2) slider_to = Utils::String::toFloat(tokens.at(1));
		if (tokens.size() >= 3) slider_step = Utils::String::toFloat(tokens.at(2));
		if (tokens.size() >= 4) slider_suffix = tokens.at(3);

		auto sliderComponent = std::make_shared<SliderComponent>(window, slider_from, slider_to, slider_step, slider_suffix, true);

		if (storedValue.empty())
		{
			// Set to AUTO if no saved value exists
			sliderComponent->setAuto(true);
		}
		else
		{
			// Set to the stored value
			sliderComponent->setValue(Utils::String::toFloat(storedValue));
		}

		// Add the slider to the settings menu
		if (!feat.description.empty())
			settings->addWithDescription(pgettext("game_options", feat.name.c_str()), pgettext("game_options", feat.description.c_str()), sliderComponent);
		else
			settings->addWithLabel(pgettext("game_options", feat.name.c_str()), sliderComponent);

		// Save the slider value
		settings->addSaveFunc([storageName, sliderComponent] {
			float value = sliderComponent->getValue();

			// If the value is AUTO, save an empty string
			if (sliderComponent->getAuto())
			{
				SystemConf::getInstance()->set(storageName, "");
			}
			else
			{
				// Save the actual slider value
				SystemConf::getInstance()->set(storageName, std::to_string(value));
			}
			});

		return;
	}

	auto item = std::make_shared<OptionListComponent<std::string>>(window, pgettext("game_options", feat.name.c_str()));

	if (feat.preset == "shaders" || feat.preset == "shaderset")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

#if !defined(ROCKNIX)
		auto shaders = ApiSystem::getInstance()->getShaderList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (shaders.size() > 0)
		{
#endif
			item->add(_("NONE"), "none", storedValue == "none");

#if !defined(ROCKNIX)
			for (auto shader : shaders)
			  item->add(pgettext("game_options", Utils::String::toUpper(shader).c_str()), shader, storedValue == shader);
		}
#else
			std::string shader;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getshaders)")); getline(ss, shader, ','); )
				item->add(pgettext("game_options", Utils::String::toUpper(shader).c_str()), shader, storedValue == shader);
#endif
	}
	else if (feat.preset == "videofilters")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

#if !defined(ROCKNIX)
		auto videofilters = ApiSystem::getInstance()->getVideoFilterList(configName != "global" ? system : "", configName != "global" ? emulator : "", configName != "global" ? core : "");
		if (videofilters.size() > 0)
		{
#endif
			item->add(_("NONE"), "none", storedValue == "none");

#if !defined(ROCKNIX)
			for (auto videofilter : videofilters)
				item->add(pgettext("game_options", Utils::String::toUpper(videofilter).c_str()), videofilter, storedValue == videofilter);
		}
#else
			std::string videofilter;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getfilters)")); getline(ss, videofilter, ','); )
				item->add(pgettext("game_options", Utils::String::toUpper(videofilter).c_str()), videofilter, storedValue == videofilter);
#endif
	}
	else if (feat.preset == "decorations" || feat.preset == "bezel")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
			item->setRowTemplate([window, sets](std::string data, ComponentListRow& row) { createDecorationItemTemplate(window, sets, data, row); });
			item->add(_("NONE"), "none", storedValue == "none");

			for (auto set : sets)
				item->add(set.name, set.name, storedValue == set.name);
		}
	}
	else if (feat.preset == "videomodes" || feat.preset == "videomode")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto modes = ApiSystem::getInstance()->getVideoModes();
		for (auto videoMode : modes)
		{
			std::vector<std::string> tokens = Utils::String::split(videoMode, ':');
			if (tokens.size() == 0)
				continue;

			std::string vname;
			for (unsigned int i = 1; i < tokens.size(); i++)
			{
				if (i > 1)
					vname += ":";

				vname += tokens.at(i);
			}

			item->add(_(vname.c_str()), tokens.at(0), storedValue == tokens.at(0));
		}
	}
	else if (feat.preset == "runners")
	{
		item->add(_("AUTO"), "auto", storedValue.empty() || storedValue == "auto");

		auto runners = ApiSystem::getInstance()->getCustomRunners();
		for (auto customRunner : runners)
		{
			item->add(_(customRunner.c_str()), customRunner, storedValue == customRunner);
		}
	}
	else
	{
		item->add(_("AUTO"), "", storedValue.empty() || storedValue == "auto");

		for (auto fval : feat.choices)
			item->add(pgettext("game_options", fval.name.c_str()), fval.value, storedValue == fval.value);
	}

	if (!item->hasSelection())
		item->selectFirstItem();

	std::string desc = pgettext("game_options", feat.description.c_str());

	if (!inheritedValue.empty())
	{
		auto displayName = item->getItemDisplayName(inheritedValue);
		if (!displayName.empty())
		{
			if (desc.empty())
				desc = _("Current setting") + " : " + displayName;
			else
				desc = desc + "\r\n" + _("Current setting") + " : " + displayName;
		}
	}

	if (!desc.empty())
		settings->addWithDescription(pgettext("game_options", feat.name.c_str()), desc, item);
	else
		settings->addWithLabel(pgettext("game_options", feat.name.c_str()), item);

	settings->addSaveFunc([item, storageName] { SystemConf::getInstance()->set(storageName, item->getSelected()); });
}

static bool hasGlobalFeature(const std::string& name)
{
	return CustomFeatures::GlobalFeatures.hasGlobalFeature(name);
}

static std::string getFeatureMenuDescription(const std::string& configName, const VectorEx<CustomFeature>& items)
{
	std::string description;

	for (auto item : items)
	{
		std::string storageName = configName + "." + item.value;
		std::string storedValue = SystemConf::getInstance()->get(storageName);
		if (!storedValue.empty())
		{
			std::string text = pgettext("game_options", item.name.c_str());

			for (auto ch : item.choices)
			{
				if (ch.value == storedValue)
				{
					storedValue = ch.name;
					break;
				}
			}

			if (item.preset == "switchoff" || item.preset == "switch_default_off_reverse_value")
			{
				if (storedValue == "0")
					storedValue = _("ON");
				else if (storedValue == "1")
					storedValue = _("OFF");
			}
			else if (Utils::String::startsWith(item.preset, "switch"))
			{
				if (storedValue == "0")
					storedValue = _("OFF");
				else if (storedValue == "1")
					storedValue = _("ON");
			}

			text += " : " + Utils::String::toUpper(storedValue);

			if (description.empty())
				description = text;
			else
				description = description + "\r\n" + text;
		}
	}

	return description;
}


void GuiMenu::addFeatures(const VectorEx<CustomFeature>& features, Window* window, GuiSettings* settings, const std::string& configName, const std::string& system, const std::string& emulator, const std::string& core, const std::string& defaultGroupName, bool addDefaultGroupOnlyIfNotFirst)
{
	bool firstGroup = true;
	
	auto groups = features.groupBy([](auto x) { return x.group; });
	for (auto group : groups)
	{
		settings->removeLastRowIfGroup();

		if (!group.first.empty())
			settings->addGroup(pgettext("game_options", group.first.c_str()));
		else if (!defaultGroupName.empty())
		{
			if (!addDefaultGroupOnlyIfNotFirst || !firstGroup)
				settings->addGroup(defaultGroupName); // _("DEFAULT GLOBAL SETTINGS")
		}

		firstGroup = false;

		std::set<std::string> processed;

		for (auto feat : group.second)
		{
			if (feat.submenu.empty())
			{
				addFeatureItem(window, settings, feat, configName, system, emulator, core);
				continue;
			}

			if (processed.find(feat.submenu) != processed.cend())
				continue;

			processed.insert(feat.submenu);

			auto items = features.where([feat](auto x) { return x.preset != "hidden" && x.submenu == feat.submenu; });
			if (items.size() > 0)
			{
				std::string label = Utils::String::toUpper(pgettext("game_options", feat.submenu.c_str()));
				std::string description = getFeatureMenuDescription(configName, items);

				std::shared_ptr<MultiLineMenuEntry> entry = std::make_shared<MultiLineMenuEntry>(window, label, description, true);

				ComponentListRow row;
				row.addElement(entry, true);

				auto arrow = makeArrow(window);
				if (EsLocale::isRTL()) arrow->setFlipX(true);
				row.addElement(arrow, false);

				row.makeAcceptInputHandler([window, configName, feat, items, system, emulator, core, settings, entry]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					groupSettings->addSaveFunc([settings, entry, configName, items]
					{
						if (entry != nullptr)
						{
							std::string newDesc = getFeatureMenuDescription(configName, items);
							if (newDesc != entry->getDescription())
							{
								entry->setDescription(newDesc);
								settings->updateSize();
							}
						}
					});

					window->pushGui(groupSettings);
				});

				settings->addRow(row);

				/*
				settings->addEntry(pgettext("game_options", feat.submenu.c_str()), true, [window, configName, feat, items, system, emulator, core]
				{
					GuiSettings* groupSettings = new GuiSettings(window, pgettext("game_options", feat.submenu.c_str()));

					for (auto feat : items)
						addFeatureItem(window, groupSettings, feat, configName, system, emulator, core);

					window->pushGui(groupSettings);
				});*/
			}
		}
	}
}

// Defined with the cloud-setup wizard further down; the cloud menus use
// these for the CLOUD FOLDER row and for rows that must stay visible
// before a remote is configured.
static void cloudSetupOpenSyncPathEditor(Window* window, const std::string& current, const std::function<void()>& onDone);

void GuiMenu::openCloudFolderEditor(Window* window, const std::string& current)
{
	cloudSetupOpenSyncPathEditor(window, current, nullptr);
}
static std::map<std::string, std::string> cloudSetupInfo();

static void cloudAddGatedEntry(GuiSettings* s, Window* window, bool configured, const std::string& label, const std::string& description, const std::function<void()>& action);
static std::string cloudShellQuote(const std::string& value);

// The picker's selection, handed to the content script as it closes. The
// names are folder names in the player's cloud (--scan's first field) --
// the owner's own, or another of their devices', and still data: inside
// double quotes $(, backticks and " stayed live for the shell
// executeScriptLegacy runs, so a folder named a$(reboot)b ran (audit #307
// PL-030). One single-quoted argument, as the script has always taken
// them (it splits on the spaces itself); what it accepts as a name is the
// script's to refuse.
static std::string cloudSetSystemsCommand(const std::vector<std::string>& names)
{
	std::string picked;
	for (auto& name : names)
		picked += (picked.empty() ? "" : " ") + name;
	return "/usr/bin/cloud_content_restore --set-systems " + cloudShellQuote(picked);
}

// Where --set-systems keeps the selection (cloud_content_restore's
// SELECTION), which --selected reads when the run starts. A macro so a test
// can point it elsewhere (tests/cloud-content-selection.py).
#ifndef CLOUD_CONTENT_SELECTION
#define CLOUD_CONTENT_SELECTION "/storage/.cache/cloud_sync/content-systems"
#endif

// The selection file's names, one a line. An ifstream, not readAllText:
// that reads a file under three bytes as empty (es-code-traps.md), and a
// one-letter name makes a two-byte file.
// The names in the selection file, and whether the file was read whole:
// a path that is not a regular file (a directory opens, then getline
// fails) or a read that ends in an error is not an empty selection (the
// audit of the fixes, gpt G3-E-03).
static bool cloudSelectionRead(const std::string& path, std::vector<std::string>& names)
{
	names.clear();
	if (!Utils::FileSystem::isRegularFile(path))
		return false;
	std::ifstream in(path);
	if (!in.is_open())
		return false;
	std::string line;
	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			names.push_back(line);
	}
	return in.eof() && !in.bad();
}

// The picker's selection, saved and then read back before anything moves on
// it (audit of the fix round PL-019). --set-systems exits 0 over a rename
// that failed -- a full card, a folder gone read-only -- and the file then
// still holds the last run's picks, which --selected carries: systems the
// player did not tick, in either direction. So the answer is the file, not
// the exit status alone. True when the file names exactly what was ticked;
// nothing ticked is a file with nothing in it, never no file (the script
// writes one either way).
static bool cloudSaveSelection(const std::vector<std::string>& names)
{
	const int rc = ApiSystem::executeScriptLegacy(cloudSetSystemsCommand(names), nullptr).second;
	if (rc != 0)
	{
		LOG(LogError) << "cloud content: --set-systems exited " << rc << "; the selection was not saved";
		return false;
	}
	std::vector<std::string> saved;
	if (!cloudSelectionRead(CLOUD_CONTENT_SELECTION, saved) || std::set<std::string>(saved.begin(), saved.end()) != std::set<std::string>(names.begin(), names.end()))
	{
		LOG(LogError) << "cloud content: --set-systems exited 0, but " << CLOUD_CONTENT_SELECTION << " does not name what was ticked";
		return false;
	}
	return true;
}

// What the picker says when the selection could not be saved: the why, and
// that nothing started. The page stays, and its verb is the way to try again.
static std::string cloudSelectionNotSaved(bool backup)
{
	return backup ? _("COULDN'T SAVE WHAT YOU TICKED, SO NOTHING WAS BACKED UP.")
	              : _("COULDN'T SAVE WHAT YOU TICKED, SO NOTHING WAS RESTORED.");
}

// Horizontal padding matching what ComponentList applies to selectable
// rows, so informational text lines up with the actionable rows.
#define CLOUD_SETUP_ROW_PADDING Vector4f(10, 0, 10, 0)
static void cloudSetupAddInfoRow(GuiSettings* s, Window* window, const std::string& text, bool accent);

// Which systems this device syncs.
//
// One cloud library, many handhelds, and they are not interchangeable. An
// H700 cannot run GameCube, so downloading it spends card space on games that
// will never launch -- and capability is not the only axis: a 3:2 panel makes
// a device a Game Boy Advance machine by preference. So the set is chosen per
// device, from what the cloud actually holds rather than from what happens to
// be here already, which on a fresh handheld is nothing.
//
// The scan reads sizes because without them "should I take PSX?" cannot be
// answered. It is one recursive listing rather than a size call per system,
// but it is still a network round trip, so it runs behind GuiLoading.
// The content scripts' mode for a pair of ticks (D-CLOUD-050): ROMs and
// BIOS alone is the default; game content rides along with --with-media, or
// moves on its own with --media-only. The game list is game content
// (D-CLOUD-049), so a ROMs-only transfer leaves it where it is.
static std::string cloudContentMode(bool content, bool media)
{
	if (content && media)
		return " --with-media";
	if (media)
		return " --media-only";
	return "";
}

// One page per direction (D-CLOUD-048). CONTENT TO BACK UP lists what this
// device holds; CONTENT TO RESTORE lists what the cloud holds. Both sides are
// listed by the transfer's own rule and compared by file name and size,
// because totals cannot say whether one side has what the other has -- a
// restored-then-scraped device read "different size" on every system for
// that reason. Each row's line leads with what this run would move, since
// that is the question a player on this page is asking (maintainer,
// 2026-09-08: "what they'll want to know is the delta, or what's being sent
// up, not just what's in their cloud"; 2026-09-09: "instead of saying
// 'already in your cloud,' what we're trying to address is 'not yet in your
// cloud'" -- D-UI-027).
// A centred, unselectable line of standard text: what a page says about
// itself before its rows begin.
static void cloudAddCentredLine(GuiSettings* s, Window* window, const std::string& text)
{
	auto theme = ThemeData::getMenuTheme();
	ComponentListRow row;
	row.selectable = false;
	auto tc = std::make_shared<TextComponent>(window, text, theme->Text.font, theme->Text.color, ALIGN_CENTER);
	tc->setPadding(CLOUD_SETUP_ROW_PADDING);
	row.addElement(tc, true);
	s->addRow(row);
}

// What --scan said and how it ended. The exit code is the difference between
// an empty cloud and a cloud that could not be read (below).
struct CloudScanResult
{
	std::vector<std::string> scan;
	int rc = -1;
	std::vector<std::string> selected;
};

static void cloudContentSystemPicker(Window* window, const std::function<void()>& onDone, const std::string& proceedLabel, bool backup, bool content, bool media, const std::string& perSystem, const std::string& wholeDevice)
{
	window->pushGui(new GuiLoading<CloudScanResult>(
		window, backup ? _("COMPARING THIS DEVICE'S CONTENT WITH YOUR CLOUD") : _("COMPARING YOUR CLOUD'S CONTENT WITH THIS DEVICE"),
		[content, media](auto gui)
		{
			// The pair form, for the exit code: the list form threw it away,
			// and a scan that exited 69 with nothing on stdout -- no network
			// -- was handed to the page as an empty cloud (#105).
			CloudScanResult r;
			r.rc = ApiSystem::executeScriptLegacy("/usr/bin/cloud_content_restore --scan" + cloudContentMode(content, media),
				[&r](const std::string& line) { r.scan.push_back(line); }).second;
			r.selected = ApiSystem::executeScriptLegacy("/usr/bin/cloud_content_restore --systems");
			return r;
		},
		[window, onDone, proceedLabel, backup, content, perSystem, wholeDevice](CloudScanResult result)
		{
			// A scan that could not read the cloud says so, and says what to
			// do; only a scan that read it and found nothing shows the
			// nothing-found text below (D-CLOUD-077: a scan that fails must
			// say so, not show an empty cloud).
			if (result.rc == CloudExit::NoNetwork)
			{
				window->pushGui(new GuiMsgBox(window, _("COULDN'T REACH YOUR CLOUD.\n\nTRY AGAIN WHEN YOU'RE ONLINE.")));
				return;
			}
			if (result.rc != 0)
			{
				window->pushGui(new GuiMsgBox(window, _("COULDN'T READ YOUR CLOUD'S CONTENT.\n\nTRY AGAIN.")));
				return;
			}

			std::set<std::string> chosen;
			for (auto& line : result.selected)
			{
				auto name = Utils::String::trim(line);
				if (!name.empty())
					chosen.insert(name);
			}
			// name|cloud_bytes|supported|device_bytes|files_in_cloud_not_here|files_here_not_in_cloud|bytes_in_cloud_not_here|bytes_here_not_in_cloud
			// Fields 7 and 8 -- the bytes a copy in each direction would send --
			// arrived with the scripts of 2026-09-08. `sized` records whether
			// this scan carried them, so a page running ahead of its scripts
			// falls back to the count-only verdict rather than reading two
			// missing fields as "nothing to move".
			struct Found { std::string name; unsigned long cloudBytes; bool supported; unsigned long localBytes; int cloudNotHere; int hereNotCloud; unsigned long cloudNotHereBytes; unsigned long hereNotCloudBytes; bool sized; };
			std::vector<Found> found;
			Found bios{ "bios", 0, true, 0, 0, 0, 0, 0, false };
			bool biosListed = false;
			for (auto& line : result.scan)
			{
				auto p = Utils::String::split(Utils::String::trim(line), '|', true);
				if (p.size() < 3)
					continue;
				Found f{ p[0], (unsigned long) atol(p[1].c_str()), p[2] == "1", 0, 0, 0, 0, 0, false };
				if (p.size() >= 6)
				{
					f.localBytes   = (unsigned long) atol(p[3].c_str());
					f.cloudNotHere = atoi(p[4].c_str());
					f.hereNotCloud = atoi(p[5].c_str());
				}
				if (p.size() >= 8)
				{
					f.cloudNotHereBytes = (unsigned long) atol(p[6].c_str());
					f.hereNotCloudBytes = (unsigned long) atol(p[7].c_str());
					f.sized = true;
				}
				if (f.name == "bios")
				{
					// Not a system; comes with the tier (D-CLOUD-043). Kept
					// aside for the case below.
					bios = f;
					biosListed = true;
					continue;
				}
				// Each direction lists only what it can act on: backup sends what
				// is here, restore brings what is there. A file counts when it
				// would move, whatever its size: a system whose only file is
				// empty read as nothing to move and was left out (#308 8a gpt
				// F-ES-10).
				if (backup ? (f.localBytes == 0 && f.hereNotCloud == 0) : (f.cloudBytes == 0 && f.cloudNotHere == 0))
					continue;
				found.push_back(f);
			}
			// One line under the label (D-UI-023), and it answers the one
			// question this page asks: is there anything not yet on the
			// far side, and how much? "253.9 MB NOT YET IN YOUR CLOUD . 14
			// FILES" when there is, NOTHING NEW TO BACK UP when there is
			// not; the restore page reads NOT YET ON THIS DEVICE / NOTHING
			// NEW TO RESTORE. No total anywhere on the row (D-UI-027): the
			// zero case used to read ALREADY IN YOUR CLOUD (253.9 MB), and
			// a size beside a system reads as an amount about to move --
			// "does that mean I'm backing up 253.9 MB?" (maintainer,
			// 2026-09-09). The count stays because a delta of many tiny
			// files is a different job from one big file. Sizes are the
			// transfer page's (GuiCloudTransfer::sizeLabel): a whole KB
			// below a megabyte, rounded up so a one-byte difference never
			// reads "0 KB", and no decimals that could only ever be zero
			// (#85). A difference of only empty files is carried by the
			// count instead. The BIOS row, when it is the only thing to
			// move (below), reads the same way.
			auto noteFor = [backup](const Found& f)
			{
				const unsigned long total = backup ? f.localBytes : f.cloudBytes;
				const unsigned long other = backup ? f.cloudBytes : f.localBytes;
				// The bytes a copy in this direction would send. A six-field
				// scan, from scripts older than this page, has no such figure:
				// its row is carried by the count alone rather than by a total
				// that says nothing about what moves.
				unsigned long delta = !f.sized ? 0 : backup ? f.hereNotCloudBytes : f.cloudNotHereBytes;
				const int count = backup ? f.hereNotCloud : f.cloudNotHere;
				// Nothing on the far side means all of it moves. That is true
				// by construction, needs no byte fields, and is the only true
				// reading of a scan line that was never compared (the pre-tier
				// layout, which the script lists for size alone).
				if (other == 0)
					delta = total;
				std::string note;
				if (delta > 0 || count > 0)
				{
					// The size leads when there is one. A file the far side
					// lacks can be empty -- pico-8 ships a 0-byte Splore.png --
					// and a copy sends it all the same, so when the bytes are
					// 0 the count carries the line ("1 FILE NOT YET IN YOUR
					// CLOUD") rather than the row reading NOTHING NEW over a
					// file that would move, or "0.00 KB" beside it. Beside THIS
					// DEVICE CANNOT RUN IT the count is noise, and size, count
					// and suffix together run a 640px row to a third line
					// (D-UI-023), so the count is dropped there.
					const std::string where = backup ? _("NOT YET IN YOUR CLOUD") : _("NOT YET ON THIS DEVICE");
					const std::string files = std::to_string(count) + " " + (count == 1 ? _("FILE") : _("FILES"));
					if (delta == 0)
						note = files + " " + where;
					else if (count > 0 && f.supported)
						note = GuiCloudTransfer::sizeLabel(delta) + " " + where + " · " + files;
					else
						note = GuiCloudTransfer::sizeLabel(delta) + " " + where;
				}
				else
					note = backup ? _("NOTHING NEW TO BACK UP") : _("NOTHING NEW TO RESTORE");
				if (!f.supported)
					note += " · " + _("THIS DEVICE CANNOT RUN IT");
				return note;
			};
			// BIOS files and no system to pick: the tier's other half still
			// moves (#308 8a gpt F-ES-10). It used to end in NO SYSTEM ...
			// HOLDS WHAT YOU TICKED with the BIOS files sitting there. The
			// selection names bios alone: cloud_content_restore --selected
			// refuses an empty selection before it adds bios, and both
			// scripts take bios in the selection as the tier. Not for game
			// content alone, which BIOS is not.
			//
			// And it moves on the player's press, as every run from this page
			// does (audit of the fix round PL-019): the branch wrote the
			// selection, threw the answer away and started the run straight
			// from the scan. So it is the same page with no system on it --
			// NONE under the systems heading, the BIOS files under their own
			// with what would move -- and its verb saves the selection, reads
			// it back and only then goes on.
			const bool biosToMove = biosListed && content
				&& (backup ? (bios.localBytes > 0 || bios.hereNotCloud > 0) : (bios.cloudBytes > 0 || bios.cloudNotHere > 0));
			if (found.empty() && biosToMove)
			{
				LOG(LogInfo) << "cloud content: no system to pick; the page offers the BIOS files alone, to " << (backup ? "go up" : "come down") << " on the verb's press";
				auto s = new GuiSettings(window, backup ? _("CONTENT TO BACK UP") : _("CONTENT TO RESTORE"));
				cloudAddCentredLine(s, window, perSystem);
				if (!wholeDevice.empty())
					cloudAddCentredLine(s, window, wholeDevice);
				s->addGroup(backup ? _("SYSTEMS ON THIS DEVICE") : _("SYSTEMS IN YOUR CLOUD"));
				cloudSetupAddInfoRow(s, window, _("NONE"), false);
				s->addGroup(_("BIOS FILES"));
				s->addWithDescription(Utils::String::toUpper(bios.name), noteFor(bios), nullptr);
				auto proceed = std::make_shared<bool>(false);
				s->getMenu().clearButtons();
				s->getMenu().addButton(_("BACK"), _("back"), [s] { s->close(); });
				if (onDone)
					s->getMenu().addButton(proceedLabel.empty() ? _("CONTINUE") : proceedLabel, _("continue"),
						[window, s, proceed, backup]
						{
							if (!cloudSaveSelection({ "bios" }))
							{
								window->pushGui(new GuiMsgBox(window, cloudSelectionNotSaved(backup)));
								return;
							}
							*proceed = true;
							s->close();
						});
				window->pushGui(s);
				s->onFinalize([proceed, onDone] { if (*proceed && onDone) onDone(); });
				return;
			}
			if (found.empty())
			{
				window->pushGui(new GuiMsgBox(window, backup
					? _("NO SYSTEM ON THIS DEVICE HOLDS WHAT YOU TICKED.")
					: _("NO SYSTEM IN YOUR CLOUD HOLDS WHAT YOU TICKED.\n\nPUT FILES INTO THE ROMS FOLDER FROM A COMPUTER, THEN SCAN AGAIN.")));
				return;
			}
			// CONTENT TO ..., not SYSTEMS TO ...: settings cover the whole device
			// and do not belong to a system, and the maintainer read a page
			// called SYSTEMS that listed them as a contradiction (2026-09-06).
			// Two centred lines say what this run carries: the per-system
			// classes the choice below applies to, then the whole-device ones
			// that ride along regardless.
			auto s = new GuiSettings(window, backup ? _("CONTENT TO BACK UP") : _("CONTENT TO RESTORE"));
			cloudAddCentredLine(s, window, perSystem);
			if (!wholeDevice.empty())
				cloudAddCentredLine(s, window, wholeDevice);
			auto switches = std::make_shared<std::vector<std::pair<std::string, std::shared_ptr<SwitchComponent>>>>();
			s->addGroup(backup ? _("SYSTEMS ON THIS DEVICE") : _("SYSTEMS IN YOUR CLOUD"));
			for (auto& f : found)
			{
				auto sw = std::make_shared<SwitchComponent>(window);
				sw->setState(chosen.find(f.name) != chosen.end());
				switches->push_back({ f.name, sw });
				s->addWithDescription(Utils::String::toUpper(f.name), noteFor(f), sw);
			}
			auto picked = [switches]
			{
				std::vector<std::string> names;
				for (auto& entry : *switches)
					if (entry.second->getState())
						names.push_back(entry.first);
				return names;
			};
			auto proceed = std::make_shared<bool>(false);
			// BACK keeps the ticks for next time, as it always has; a save
			// that fails there is logged (cloudSaveSelection) and the next
			// open reads the file as it is. The verb saves them itself, and
			// checks, before the run (PL-019), so they are not written twice.
			s->addSaveFunc([picked, proceed]
			{
				if (!*proceed)
					cloudSaveSelection(picked());
			});
			// SELECT ALL / SELECT NONE is one button that reads as the thing it
			// would do next, in the bar with BACK and the verb -- rows in the
			// list read as choices of their own (maintainer, 2026-09-06).
			// Rebuilding the bar destroys its buttons, so it must never run
			// from inside a button's own callback: the first cut did, and
			// EmulationStation died on the SELECT ALL press (VM, 2026-09-06).
			// The rebuild is posted to the UI thread instead, and the switches
			// stay quiet while the button sets them. A weak reference from
			// the callbacks to the rebuild breaks the cycle a shared one made.
			auto buttons = std::make_shared<std::function<void()>>();
			std::weak_ptr<std::function<void()>> weak = buttons;
			auto quiet = std::make_shared<bool>(false);
			// fromButton: the toggle was pressed, so the rebuilt bar hands the
			// focus back to it; a switch row's change leaves the list focused.
			auto later = [window, weak, s](bool fromButton)
			{
				window->postToUiThread([weak, s, fromButton]
				{
					if (auto b = weak.lock())
					{
						(*b)();
						if (fromButton)
							s->getMenu().setCursorToButton(1);
					}
				});
			};
			*buttons = [window, s, switches, proceed, onDone, proceedLabel, quiet, later, picked, backup]()
			{
				bool allOn = !switches->empty();
				for (auto& e : *switches)
					allOn = allOn && e.second->getState();
				s->getMenu().clearButtons();
				s->getMenu().addButton(_("BACK"), _("back"), [s] { s->close(); });
				s->getMenu().addButton(allOn ? _("SELECT NONE") : _("SELECT ALL"), allOn ? _("select none") : _("select all"),
					[switches, allOn, quiet, later]
					{
						*quiet = true;
						for (auto& e : *switches)
							e.second->setState(!allOn);
						*quiet = false;
						later(true);
					});
				// The selection is saved and read back before the run (PL-019):
				// one that did not stick says so, and the page stays for
				// another press.
				if (onDone)
					s->getMenu().addButton(proceedLabel.empty() ? _("CONTINUE") : proceedLabel,
						_("continue"), [window, s, proceed, picked, backup]
						{
							if (!cloudSaveSelection(picked()))
							{
								window->pushGui(new GuiMsgBox(window, cloudSelectionNotSaved(backup)));
								return;
							}
							*proceed = true;
							s->close();
						});
			};
			for (auto& e : *switches)
				e.second->setOnChangedCallback([quiet, later] { if (!*quiet) later(false); });
			(*buttons)();
			window->pushGui(s);
			// The page owns the rebuild; nothing else holds the shared_ptr, so
			// the weak references above go dead the moment the page does.
			s->onFinalize([buttons, proceed, onDone] { if (*proceed && onDone) onDone(); });
		}));
}

// What happened the last time this ran: when, how it ended, and -- when it
// did not finish -- the scripts' own sentence for why.
//
// Read once, said in two places. The row under the action carries the date
// and the outcome word (cloudLastRunDetail); the confirmation dialog the
// action opens carries the why (cloudLastRunWhy). The row used to carry
// both, and at 1280 px "LAST 09/09/2026 23:52  -  COULDN'T FINISH, YOUR
// CLOUD DIDN'T ANSWER. ITS SIGN-IN MAY HAVE EXPIRED" wrapped to two lines
// of small text under the label -- three lines on a row, which is the shape
// the maintainer's rule forbids (D-UI-023), on a 4-inch panel worse. The
// rule also says where the extra line goes: into the dialog, where it is
// read at the moment of deciding to try again (D-UI-029).
struct CloudLastRun
{
	bool ran = false;
	time_t when = 0;
	int code = 0;
	std::string token;
	std::string why;      // upper case, no trailing period; empty when it completed
	std::string outcome;  // the row's word(s): COMPLETED, COULDN'T FINISH, SKIPPED, ...
	bool finished = false;
};

static CloudLastRun cloudReadLastRun(const std::string& name)
{
	CloudLastRun r;
	const std::string path = "/storage/.cache/cloud_sync/last-" + name;
	// Uncached: the stamps are written by the scripts and by
	// ThreadedCloudSync while this process runs, and Utils::FileSystem::exists
	// remembers a miss (UseFileCache) until a game launch or a restart clears
	// it. A page opened once before a run read NOT DONE ON THIS DEVICE YET
	// after it, stamp on disk or not (guest d, 2026-09-10). The same goes for
	// every file another process writes: rclone.conf below, once the wizard
	// has made it.
	if (!Utils::FileSystem::exists(path, false))
		return r;
	// The line itself -- its fields, its two shapes of third field, and
	// which of the four outcomes it names -- is parsed in CloudText, where
	// a test can reach it without a device or a locale (es-app/tests/unit).
	// What stays here is the read, the fallback why for a stamp that gave
	// none, and the outcome said in the player's language -- the why too:
	// the stamp keeps the script's English sentence for its readers, and
	// CloudText::localizedWhy turns it into the interface's language, as the
	// card does (#308 F-CS-31; one it does not list comes back as it was).
	const CloudText::LastRun p = CloudText::parseLastRun(Utils::FileSystem::readAllText(path));
	if (!p.ran)
		return r;

	r.ran = true;
	r.when = p.when;
	r.code = p.code;
	r.token = p.token;
	r.finished = p.finished;

	switch (p.outcome)
	{
	case CloudText::Outcome::Completed:
		r.outcome = _("COMPLETED");
		break;
	case CloudText::Outcome::Gaps:
		// A run whose parts disagreed. The stamp keeps its own token so a log
		// can tell it from a total failure; the row says what every other
		// failure says (D-UI-030).
		r.outcome = _("COULDN'T FINISH");
		r.why = p.why.empty() ? ThreadedCloudSync::whyForToken(p.token) : CloudText::localizedWhy(p.why);
		break;
	case CloudText::Outcome::SkippedLockHeld:
		r.outcome = _("SKIPPED, ANOTHER SYNC WAS RUNNING");
		break;
	case CloudText::Outcome::SkippedNoNetwork:
		r.outcome = _("SKIPPED, NO NETWORK");
		break;
	case CloudText::Outcome::SkippedGameStarted:
		r.outcome = _("SKIPPED, A GAME WAS STARTED");
		break;
	case CloudText::Outcome::SkippedCancelled:
		r.outcome = _("SKIPPED, YOU CANCELLED IT");
		break;
	case CloudText::Outcome::Failed:
	{
		// The token's phrase for one of ours, the code's for anything else.
		std::string why = CloudText::localizedWhy(p.why);
		if (why.empty() && p.knownToken)
			why = ThreadedCloudSync::whyForToken(p.token);
		if (why.empty())
			why = ThreadedCloudSync::whyForCode(p.code);
		r.outcome = _("COULDN'T FINISH");
		r.why = why;
		break;
	}
	}
	return r;
}

// The line under a row: the date and how it ended, one line (D-UI-023).
//
// This used to be its own menu row -- LAST CONTENT UPLOAD sitting in a list
// next to the actions, which is where you put a fact you have nowhere better
// for. It belongs on the row it describes: the answer to "should I back this
// up?" is "you last did, and it worked", read in the moment of deciding.
// Was the run this stamp records one the player started, or one of the two
// automatic syncs? EmulationStation stamps last-sync-startup/-exit as each
// automatic run ends, within a second or two of the script writing its own
// last-backup/last-restore, so a matching time says which it was. Inferred
// rather than recorded because the scripts do not know who called them --
// and it only ever changes the words, never the outcome.
//
// It exists because the page contradicted itself: BACK UP SAVES TO THE CLOUD
// reported the sync that runs when you exit a game, under a label naming
// something the player never pressed, while SYNC SAVES WITH THE CLOUD said
// NOT DONE ON THIS DEVICE YET (maintainer, 2026-09-10, fork #112).
static std::string cloudRunOrigin(time_t when)
{
	if (when <= 0)
		return "";
	// In preference order, and each stamp read only if it is still needed:
	// the exit stamp answers most rows, and the window that makes two
	// stamps the same run is CloudText's (es-app/tests/unit checks its
	// edges).
	const CloudLastRun exitRun = cloudReadLastRun("sync-exit");
	if (CloudText::runOrigin(when, exitRun.ran ? exitRun.when : 0, 0) == CloudText::RunOrigin::AfterLastGame)
		return _("AFTER YOUR LAST GAME");

	const CloudLastRun startupRun = cloudReadLastRun("sync-startup");
	if (CloudText::runOrigin(when, 0, startupRun.ran ? startupRun.when : 0) == CloudText::RunOrigin::AtStartup)
		return _("AT STARTUP");

	return "";
}

// "LAST <date>" in the player's own date format and clock, not ours.
//
// timeToString already resolves through localtime(), so the timezone comes
// from /etc/localtime and was never wrong; the hardcoded "%Y-%m-%d %H:%M"
// was. Somebody who set SHOW CLOCK IN 12-HOUR FORMAT gets 24-hour times
// here and nowhere else in the app, which reads as a bug in the row rather
// than a setting that did not reach it.
//
// The date half comes from the system locale (getSystemDateFormat caches,
// which is fine -- a locale does not change while the menu is open). The
// time half is decided live, because ClockMode12 is a switch the player can
// flip and come straight back to this page. One helper, so the row under a
// transfer whose outcome page has not yet been dismissed (fork #187,
// D-UI-070) dates its outcome the way every other LAST row does.
static std::string cloudLastLabel(time_t when)
{
	const std::string fmt = Utils::Time::getSystemDateFormat()
		+ (Settings::ClockMode12() ? " %I:%M %p" : " %H:%M");
	return _("LAST") + std::string(" ") + Utils::Time::timeToString(when, fmt);
}

// The run a row reports. SYNC SAVES WITH THE CLOUD moves saves both ways,
// and so do the two automatic syncs; a row that counted only the manual one
// said NOT DONE ON THIS DEVICE YET on a device that had been syncing all
// along, so it reports the newest sync by any route (#112). The row's line
// and its confirmation's LAST TIME both read this one, so they speak of the
// same run: the confirmation read the manual stamp alone, and could explain
// a failure the row no longer showed (#308 8a gpt F-ES-16).
static CloudLastRun cloudLatestRun(const std::string& name)
{
	CloudLastRun r = cloudReadLastRun(name);
	if (name == "sync-manual")
		for (auto* other : { "sync-exit", "sync-startup" })
		{
			const CloudLastRun a = cloudReadLastRun(other);
			if (a.ran && (!r.ran || a.when > r.when))
				r = a;
		}
	return r;
}

static std::string cloudLastRunDetail(const std::string& name)
{
	const CloudLastRun r = cloudLatestRun(name);
	if (!r.ran)
		return _("NOT DONE ON THIS DEVICE YET");
	const time_t when = r.when;

	// An automatic run says which one it was instead of its date: the date is
	// what the player would have had to reason from to work that out, and the
	// row has two lines to say it in (D-UI-023).
	const std::string origin = cloudRunOrigin(when);
	if (!origin.empty())
		return origin + "  -  " + r.outcome;

	return cloudLastLabel(when) + "  -  " + r.outcome;
}

// The second paragraph of the confirmation dialog: why the last run could
// not finish, so the decision to try again is made knowing what stopped the
// last one. Empty when it finished, was skipped, or gave no reason -- the
// dialog then reads as it always has.
static std::string cloudLastRunWhy(const std::string& name)
{
	const CloudLastRun r = cloudLatestRun(name);
	if (!r.ran || r.why.empty())
		return "";
	return std::string("\n\n") + _("LAST TIME IT COULDN'T FINISH:") + " " + r.why + ".";
}

// One data class, with what it carries and how it last went.
static void cloudAddClassRow(GuiSettings* s, Window* window, bool configured,
	const std::string& label, const std::string& stamp,
	const std::function<void()>& action)
{
	// Two lines, never three: the label says what and where, and the one line
	// under it is how it last went -- the part somebody scans for after walking
	// off. What the row carries (game saves, save states, and screenshots) is
	// said in its confirmation dialog, where it is additive rather than a third
	// line of small text. Maintainer's rule, 2026-09-06 (D-UI-023).
	cloudAddGatedEntry(s, window, configured, label, cloudLastRunDetail(stamp), action);
}

// Step two: having chosen a direction, tick what moves and confirm.
//
// Checkboxes rather than one row per class plus a separate "all of it": with
// three classes the combinations people actually want are not one-or-all, and
// an EVERYTHING row is a fourth thing to read that exists only because the
// first three could not be combined. The button bar carries the verb, so the
// page states a selection and the action happens once.
static void cloudOpenTransfer(Window* window, bool backup)
{
	const bool configured = Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false);
	auto s = new GuiSettings(window, backup ? _("BACK UP TO THE CLOUD") : _("RESTORE FROM THE CLOUD"));
	s->addGroup(backup ? _("WHAT WOULD YOU LIKE TO BACK UP?") : _("WHAT WOULD YOU LIKE TO RESTORE?"));

	// The ticks are remembered per direction.
	//
	// Somebody who syncs ROMs to a handheld syncs ROMs to it every time, and
	// re-ticking the same boxes on every visit is the page asking a question it
	// already knows the answer to. Separate keys for backup and restore because
	// they are separate intentions: taking a library down to a device says
	// nothing about wanting to push it back up.
	const std::string key = std::string("cloudsync.pick.") + (backup ? "backup." : "restore.");
	auto remembered = [&key](const std::string& what, bool dflt)
	{
		const std::string v = SystemConf::getInstance()->get(key + what);
		return v.empty() ? dflt : v == "1";
	};

	// "SAVES", not "GAME SAVES": the class contains game saves, and a heading
	// that repeats one of its own members reads as a mistake. Not "SAVE DATA"
	// either: "data" adds nothing a player uses (D-UI-022).
	auto saves = std::make_shared<SwitchComponent>(window);
	saves->setState(remembered("saves", true));
	s->addWithDescription(_("SAVES"),
		_("GAME SAVES, SAVE STATES, AND SCREENSHOTS"), saves);

	auto content = std::make_shared<SwitchComponent>(window);
	const bool hasContent = Utils::FileSystem::exists(
		backup ? "/usr/bin/cloud_content_backup" : "/usr/bin/cloud_content_restore");
	content->setState(hasContent && remembered("content", false));
	if (hasContent)
	{
		s->addWithDescription(_("ROMS AND BIOS"),
			_("THE SYSTEMS YOU CHOSE FOR THIS DEVICE"), content);
	}
	// Game content is a class of its own (D-CLOUD-050) -- what the scraper
	// made, the game list included (D-CLOUD-049) -- so a scraped device can
	// back up its ROMs alone and read as matching, or send the artwork on
	// its own. Which systems it comes from is CONTINUE's question.
	auto media = std::make_shared<SwitchComponent>(window);
	media->setState(hasContent && remembered("media", false));
	if (hasContent)
		s->addWithDescription(_("GAME CONTENT"),
			_("SCRAPED ARTWORK, VIDEOS, MANUALS, AND GAME LISTS"), media);

	auto settings = std::make_shared<SwitchComponent>(window);
	settings->setState(remembered("settings", false));
	s->addWithDescription(_("SETTINGS"),
		_("CONFIGURATION, CONTROLS, AND THEMES"), settings);

	// Written on the way out, by whichever exit -- BACK included, because a
	// tick somebody set and then thought better of running is still their
	// answer to "what moves". GuiSettings::save() also returns early when a
	// page registers no save function, so this is what flushes SystemConf.
	s->addSaveFunc([key, saves, content, media, settings]
	{
		auto conf = SystemConf::getInstance();
		conf->set(key + "saves",    saves->getState()    ? "1" : "0");
		conf->set(key + "content",  content->getState()  ? "1" : "0");
		conf->set(key + "media",    media->getState()    ? "1" : "0");
		conf->set(key + "settings", settings->getState() ? "1" : "0");
	});

	// The two lines the content page opens with. The first names the
	// per-system classes -- what the choice of systems on that page applies
	// to -- separated by middle dots, not joined by AND (one class is called
	// ROMS AND BIOS). The second names what rides along for the whole device:
	// settings always; saves too, today, because the saves sync is one pass
	// over the whole tree until the reconciler owns it (#22).
	auto perSystemLine = [backup, content, media, hasContent]()
	{
		std::vector<std::string> parts;
		if (hasContent && content->getState())  parts.push_back(_("ROMS AND BIOS"));
		if (hasContent && media->getState())    parts.push_back(_("GAME CONTENT"));
		std::string list;
		for (size_t i = 0; i < parts.size(); i++)
			list += (i > 0 ? "  \u00B7  " : "") + parts[i];
		return (backup ? _("BACKING UP: ") : _("RESTORING: ")) + list;
	};
	auto wholeDeviceLine = [saves, settings]()
	{
		const bool sv = saves->getState(), st = settings->getState();
		if (sv && st) return std::string(_("PLUS SAVES AND SETTINGS FOR THE WHOLE DEVICE"));
		if (sv)       return std::string(_("PLUS SAVES FOR THE WHOLE DEVICE"));
		if (st)       return std::string(_("PLUS SETTINGS FOR THE WHOLE DEVICE"));
		return std::string();
	};

	// The run itself, shared by the button and by the system chooser that can
	// precede it. A shared_ptr because the two lambdas have to reach the same
	// function and one of them is built before the other exists.
	auto run = std::make_shared<std::function<void()>>();
	*run = [window, s, backup, configured, saves, content, media, settings, hasContent]
	{
		if (!configured)
		{
			window->pushGui(new GuiMsgBox(window, _("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.\n\nSET IT UP NOW?"), _("YES"),
				[window] { GuiMenu::openCloudAddRemote(window); }, _("NO"), nullptr));
			return;
		}

		const bool wantSaves = saves->getState();
		const bool wantContent = hasContent && content->getState();
		const bool wantMedia = hasContent && media->getState();
		const bool wantSettings = settings->getState();
		if (!wantSaves && !wantContent && !wantMedia && !wantSettings)
		{
			window->pushGui(new GuiMsgBox(window, _("NOTHING IS TICKED.\n\nCHOOSE AT LEAST ONE THING TO MOVE.")));
			return;
		}

		// Each tier runs whatever the one before it did, and the whole run
		// fails if any part did.
		//
		// "A ; B ; C" reports C's exit status, so a saves restore that failed
		// followed by a settings restore that worked reported success -- the
		// exact failure this subsystem is prone to, announced by the page
		// built to stop it. Independent tiers must not be chained with && (a
		// failed saves restore would silently skip the ROMs), so the status is
		// accumulated instead.
		//
		// And each part reports itself as it ends -- ">>> tier <label>|<rc>",
		// the label being what the page calls the item (SAVES, ROMS AND BIOS,
		// SETTINGS) -- so the page can say which parts finished and which did
		// not: a run where one did and one did not is a failure with that part's why,
		// never the last part's code over the first part's files (D-UI-028).
		std::string cmd;
		auto add = [&cmd](const std::string& label, const std::string& part)
		{
			if (cmd.empty())
				cmd = "rc=0";
			cmd += " ; _t=0 ; { " + part + " ; } || _t=$? ; echo \">>> tier " + label + "|$_t\" ; [ \"$_t\" = 0 ] || rc=$_t";
		};

		// A settings restore rewrites the configuration and restarts the
		// device, so it cannot be one link in a chain -- anything after it
		// would never run. The journey marker is what carries the rest
		// across the restart -- and backuptool sets it (--then-cloud), after
		// its extract has been verified, where this used to touch it before
		// the restore ran: a download that never happened, or a restore that
		// failed, still produced YOUR SETTINGS WERE RESTORED at the next
		// boot (D-CLOUD-078).
		//
		// One tier with two parts, the settings tier of a backup read
		// backwards: the archive comes down, then backuptool puts it in
		// place -- and only if it arrived, so a download that failed is not
		// followed by an extract of whatever was already on the card. ES
		// announces the item and what the second part is busy with, because
		// backuptool prints nothing this page can use (the same reason the
		// backup tier announces its own).
		//
		// --no-restart, and the page restarts the device instead: backuptool
		// left to itself sleeps five seconds and reboots, which takes the
		// screen away at the exact moment it has the outcome on it. An older
		// backuptool ignores the option and restarts as it always did (#114).
		//
		// What else was ticked rides across the restart in a record of its own
		// (JourneyTiers, audit #307 PL-029): the start builds the continuation
		// from it, where it used to run everything whatever was ticked -- the
		// dialog promising the ticks, the continuation restoring ROMs and BIOS
		// over Wi-Fi to a player who had asked for saves. It is written on YES,
		// not before the question, so a NO leaves nothing behind. With nothing
		// else ticked there is nothing to continue: no marker is asked for, the
		// dialog does not promise one, and a record an earlier attempt left is
		// removed.
		if (!backup && wantSettings)
		{
			const bool rest = wantSaves || wantContent || wantMedia;
			add("SETTINGS", std::string("echo '>>> unit SETTINGS||' ; /usr/bin/cloud_restore --yes --system-only"
				" && { echo '>>> doing unpack' ; /usr/bin/backuptool restore")
				+ (rest ? " --then-cloud" : "") + " --no-restart ; }");
			cmd += " ; exit $rc";
			const std::string question = rest
				? _("RESTORE SETTINGS FIRST, THEN RESTART?\n\nYOUR EXISTING CONFIGURATION IS REPLACED. ANYTHING ELSE YOU TICKED IS RESTORED AFTER THE RESTART. WI-FI AND ACCOUNT PASSWORDS MUST BE RE-ENTERED.")
				: _("RESTORE SETTINGS, THEN RESTART?\n\nYOUR EXISTING CONFIGURATION IS REPLACED. WI-FI AND ACCOUNT PASSWORDS MUST BE RE-ENTERED.");
			window->pushGui(new GuiMsgBox(window, question, _("YES"),
				[window, s, cmd, rest, wantSaves, wantContent, wantMedia]
				{
					const std::string record = JourneyTiers::PATH;
					if (rest)
					{
						Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(record));
						// This restore's ticks, or no record at all: an earlier
						// attempt's record at the same path was read as this one's
						// when the write failed (audit of the fixes, E2 gpt G-E2-01).
						// No record leaves the start with a marker and nothing to
						// read: it offers everything and its prompt names
						// everything. A record that can be neither replaced nor
						// removed stops the restore here, before anything changes.
						const JourneyTiers::Replaced replaced = JourneyTiers::replaceRecord(
							JourneyTiers::record(wantSaves, wantContent, wantMedia),
							[&record](const std::string& text) { return Utils::AtomicFile::writeText(record, text); },
							[&record]() { return Utils::FileSystem::removeFile(record); },
							[&record]() { return Utils::FileSystem::exists(record, false); });
						if (replaced == JourneyTiers::Replaced::NoRecord)
							LOG(LogWarning) << "restore: the journey record could not be written to " << record << "; the start will offer everything";
						else if (replaced == JourneyTiers::Replaced::OldRecordStands)
						{
							LOG(LogError) << "restore: an earlier journey record at " << record << " could be neither replaced nor removed; the restore was not started";
							s->close();
							window->pushGui(new GuiMsgBox(window, _("COULDN'T SAVE WHAT YOU TICKED, SO NOTHING WAS RESTORED."), _("OK")));
							return;
						}
					}
					else if (Utils::FileSystem::exists(record, false))
						Utils::FileSystem::removeFile(record);
					s->close();
					auto page = new GuiCloudTransfer(window, cmd, _("RESTORING SETTINGS FROM THE CLOUD"), 1);
					page->setCompletedAction([]
					{
						// The configuration on disk is no longer the one
						// this process holds. Re-read it before anything
						// can write the old values back over the restore:
						// ViewController::saveState() saves es_settings.cfg
						// on the way out whenever the player has moved to
						// another system since this boot, and that write
						// carries every other setting with it.
						Settings::getInstance()->loadFile();
						SystemConf::getInstance()->loadSystemConf();
						ViewController::configurationReplaced();
						Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT);
					}, _("RESTART"), _("PRESS ANY BUTTON TO RESTART"),
					   _("THIS DEVICE RESTARTS SO YOUR RESTORED SETTINGS TAKE EFFECT."));
					window->pushGui(page);
				}, _("NO"), nullptr));
			return;
		}

		// Order matters on a backup: the settings archive is written last so
		// the copy that goes up is the current one.
		// --saves-only: without it the saves scripts run their settings-archive
		// phase too, and a run with SETTINGS unticked still moved the archive
		// under a SETTINGS BACKUP label the page then kept showing through the
		// ROMs (maintainer, 2026-09-06). The settings tier below is the only
		// thing that moves settings.
		if (wantSaves)
			add("SAVES", backup ? "/usr/bin/cloud_backup --yes --saves-only" : "/usr/bin/cloud_restore --yes --saves-only");
		if (wantContent || wantMedia)
			add(wantContent && wantMedia ? "ROMS, BIOS, AND GAME CONTENT" : wantContent ? "ROMS AND BIOS" : "GAME CONTENT",
			    (backup ? std::string("/usr/bin/cloud_content_backup --selected")
			            : std::string("/usr/bin/cloud_content_restore --selected"))
			    + cloudContentMode(wantContent, wantMedia));
		// The settings item announces itself before backuptool runs, and says
		// what it is doing while the archive is written: backuptool prints
		// nothing the page can use, and without these two lines the item sat
		// on PREPARING... over a spinner while every other item showed its
		// files (maintainer, 2026-09-09, D-UI-026). cloud_backup announces
		// ">>> unit SETTINGS||" again when its turn comes; the same label, so
		// the page does not count it twice.
		if (backup && wantSettings)
			add("SETTINGS", "echo '>>> unit SETTINGS||' ; echo '>>> doing archive' ; /usr/bin/backuptool backup >/dev/null 2>&1 && /usr/bin/cloud_backup --yes --system-only");

		// How many items the page should expect (ITEM i OF n, D-UI-026): one
		// for saves, one per system the picker left ticked, one for settings.
		// The selection is the file cloud_content_restore --set-systems wrote
		// as the picker closed -- read here rather than asked for through the
		// script, whose startup runs rclone listremotes: a process on the UI
		// thread for a number the content script corrects anyway when it
		// announces its own count (it adds bios to what was ticked). The
		// settings item comes after the content phase, so that correction is
		// told to keep counting it.
		int items = (wantSaves ? 1 : 0) + (backup && wantSettings ? 1 : 0);
		int itemsAfterContent = 0;
		if (wantContent || wantMedia)
		{
			{
				std::vector<std::string> picked;
				cloudSelectionRead(CLOUD_CONTENT_SELECTION, picked);
				items += (int) picked.size();
			}
			itemsAfterContent = backup && wantSettings ? 1 : 0;
		}

		// A screen, not a card. This is the flow that moves gigabytes, and the
		// card closes itself the moment the job ends -- so a restore somebody
		// walked away from left no trace but a log file. The quick save actions
		// in GAME SETTINGS keep the card, because those finish while you watch.
		if (!cmd.empty())
			cmd += " ; exit $rc";

		s->close();
		window->pushGui(new GuiCloudTransfer(window, cmd,
			backup ? _("BACKING UP TO THE CLOUD") : _("RESTORING FROM THE CLOUD"), items, itemsAfterContent));
	};

	// Which systems is a question only the per-system classes raise -- ROMS AND
	// BIOS and GAME CONTENT -- so it is a second step rather than a row sitting
	// there whether or not it applies, and the button says CONTINUE when there
	// is a step after it, which is the only honest label for a control that
	// does not yet perform the action.
	auto rebuildButtons = [s, window, backup, content, media, hasContent, run, perSystemLine, wholeDeviceLine]()
	{
		const bool staged = hasContent && (content->getState() || media->getState());
		s->getMenu().clearButtons();
		s->getMenu().addButton(_("BACK"), _("back"), [s] { s->close(); });
		s->getMenu().addButton(
			staged ? _("CONTINUE") : (backup ? _("BACK UP") : _("RESTORE")),
			staged ? _("choose systems") : (backup ? _("back up") : _("restore")),
			[window, staged, run, backup, content, media, perSystemLine, wholeDeviceLine]
			{
				if (staged)
					cloudContentSystemPicker(window, [run] { (*run)(); },
						backup ? _("BACK UP") : _("RESTORE"), backup,
						content->getState(), media->getState(), perSystemLine(), wholeDeviceLine());
				else
					(*run)();
			});
	};
	rebuildButtons();
	if (hasContent)
	{
		content->setOnChangedCallback([rebuildButtons] { rebuildButtons(); });
		media->setOnChangedCallback([rebuildButtons] { rebuildButtons(); });
		// Each switch's callback holds rebuildButtons, which holds both
		// switches (and run, and the line builders, which hold them too): a
		// cycle that kept every closed form alive, with its switches, its
		// run and a pointer to the page it was built for (#308 8a gpt
		// F-ES-11). The page lets go of the callbacks as it closes, and the
		// rest goes with the page's rows. Owned by the page, as the picker's
		// rebuild is (es-code-traps.md, a button bar rebuilt from a button).
		s->onFinalize([content, media]
		{
			content->setOnChangedCallback(nullptr);
			media->setOnChangedCallback(nullptr);
		});
	}

	window->pushGui(s);
}

// Step three: make this device match the cloud.
//
// The only cloud operation that deletes, so it previews first and the
// confirmation names what goes. The preview is a network round trip -- an
// rclone dry run per selected system -- so it runs behind GuiLoading rather
// than freezing the menu.
// What --match said and how it ended.
struct CloudMatchPreview
{
	std::vector<std::string> lines;
	int rc = -1;
};

static void cloudOpenMatch(Window* window)
{
	window->pushGui(new GuiLoading<CloudMatchPreview>(
		window, _("COMPARING YOUR ROMS AND BIOS FILES WITH THE CLOUD"),
		[](auto gui)
		{
			CloudMatchPreview r;
			r.rc = ApiSystem::executeScriptLegacy("/usr/bin/cloud_content_restore --match",
				[&r](const std::string& line) { r.lines.push_back(line); }).second;
			return r;
		},
		[window](CloudMatchPreview result)
		{
			// The backend declines -- no network, an unreachable remote, an
			// empty content root, exclusions not loaded, no systems selected
			// for this device -- by exiting non-zero and printing why. Show
			// its words rather than inventing a summary of a refusal we did
			// not make: "No systems are selected for this device yet." used
			// to fall through to THIS DEVICE ALREADY MATCHES YOUR CLOUD,
			// which was false (#105).
			if (result.rc == CloudExit::NoNetwork)
			{
				window->pushGui(new GuiMsgBox(window, _("COULDN'T REACH YOUR CLOUD.\n\nTRY AGAIN WHEN YOU'RE ONLINE."), _("OK")));
				return;
			}
			bool refusal = false;
			std::string said;
			for (auto& raw : result.lines)
			{
				const std::string line = Utils::String::trim(raw);
				if (line.empty() || line.find('|') != std::string::npos)
					continue;   // a plan line, read below
				if (Utils::String::startsWith(line, "Refusing"))
					refusal = true;
				said += (said.empty() ? "" : "\n") + Utils::String::toUpper(line);
			}
			if (result.rc != 0 || refusal)
			{
				if (said.empty())
					said = _("COULDN'T READ YOUR CLOUD'S CONTENT.");
				window->pushGui(new GuiMsgBox(window, said + "\n\n" + _("NOTHING WAS CHANGED."), _("OK")));
				return;
			}

			struct Change { std::string name; long files; long bytes; };
			std::vector<Change> changes;
			long totalFiles = 0, totalBytes = 0;
			for (auto& line : result.lines)
			{
				auto p = Utils::String::split(Utils::String::trim(line), '|', true);
				if (p.size() < 4)
					continue;
				long files = atol(p[2].c_str());
				if (files <= 0)
					continue;
				long bytes = atol(p[3].c_str());
				changes.push_back({ p[0], files, bytes });
				totalFiles += files;
				totalBytes += bytes;
			}

			if (changes.empty())
			{
				window->pushGui(new GuiMsgBox(window,
					_("THIS DEVICE ALREADY MATCHES YOUR CLOUD.\n\nTHERE IS NOTHING TO REMOVE."), _("OK")));
				return;
			}

			// Sizes are the transfer page's (GuiCloudTransfer::sizeLabel), as
			// on the picker rows and the done page this dialog leads to, so
			// the number a player agrees to here is the number they see
			// removed there. kiloBytesToString of a KB-rounded size read
			// "200.00 KB" -- two decimals that could only ever be zero (#85).
			std::string detail;
			for (auto& c : changes)
			{
				detail += Utils::String::toUpper(c.name) + "  -  "
					+ std::to_string(c.files) + " " + (c.files == 1 ? _("FILE") : _("FILES"));
				if (c.bytes > 0)
					detail += "  (" + GuiCloudTransfer::sizeLabel((unsigned long) c.bytes) + ")";
				detail += "\n";
			}

			// Says what goes, what arrives, and what is never touched. The
			// third line matters most: this is the one action that can delete
			// a library, and somebody about to press YES needs to know their
			// saves are not in scope.
			std::string text = _("REMOVE") + std::string(" ") + std::to_string(totalFiles)
				+ " " + (totalFiles == 1 ? _("FILE FROM THIS DEVICE?") : _("FILES FROM THIS DEVICE?"));
			if (totalBytes > 0)
				text += "  (" + GuiCloudTransfer::sizeLabel((unsigned long) totalBytes) + ")";
			text += "\n\n" + detail
				+ "\n" + _("ANYTHING YOUR CLOUD HAS THAT THIS DEVICE DOES NOT IS DOWNLOADED AT THE SAME TIME.")
				+ "\n" + _("GAME SAVES, SAVE STATES, AND SCREENSHOTS ARE NEVER TOUCHED.");

			window->pushGui(new GuiMsgBox(window, text, _("YES"), [window]
			{
				window->pushGui(new GuiCloudTransfer(window,
					"/usr/bin/cloud_content_restore --match --apply",
					_("MATCHING THIS DEVICE TO THE CLOUD")));
			}, _("NO"), nullptr));
		}));
}

// What cloud_migrate_layout --check would move, shown for confirmation. The
// listing is the same remote round trip the row's arrival waited on, so it
// runs behind GuiLoading rather than in the row's callback; a minute is the
// box, and past it the dialog shows whatever the script had said by then.
static void cloudPreviewTidyFolders(Window* window)
{
	window->pushGui(new GuiLoading<CloudMatchPreview>(window, _("CHECKING..."),
		[](IGuiLoadingHandler*)
		{
			CloudMatchPreview r;
			r.rc = ApiSystem::executeScriptLegacy("timeout 60 /usr/bin/cloud_migrate_layout --check",
				[&r](const std::string& line) { r.lines.push_back(line); }).second;
			return r;
		},
		[window](CloudMatchPreview result)
		{
			std::string detail;
			bool refusing = false;
			for (auto& line : result.lines)
			{
				if (Utils::String::startsWith(Utils::String::trim(line), "REFUSING"))
					refusing = true;
				detail += line + "\n";
			}

			// MOVE is offered only over a plan the script stands behind: a
			// refusal (the destination already exists), an answer that is
			// not a plan, or a listing cut off by the minute's timeout is
			// shown as it is, with nothing to press but OK (#105). The row
			// stays, so the check can be run again.
			if (result.rc != 0 || refusing)
			{
				if (result.rc == 124)
					detail += (detail.empty() ? "" : "\n") + _("YOUR CLOUD DID NOT ANSWER. TRY AGAIN WHEN YOU'RE ONLINE.") + "\n";
				window->pushGui(new GuiMsgBox(window, detail + "\n" + _("NOTHING WAS MOVED."), _("OK")));
				return;
			}

			window->pushGui(new GuiMsgBox(window,
				detail + "\n" + _("MOVE THEM?"),
				_("MOVE"), [window]
				{
					ThreadedCloudSync::start(window,
						"/usr/bin/cloud_migrate_layout --apply",
						_("TIDY CLOUD FOLDERS"), _("TIDYING CLOUD FOLDERS"),
						ThreadedCloudSync::Origin::None);
				},
				_("LEAVE THEM"), nullptr));
		}));
}

// The TIDY UP YOUR CLOUD FOLDERS row, added to the CLOUD page once
// cloud_migrate_layout --check has said there is something to move; openCloud
// says why it is asked on a worker. Thirty seconds is the box -- the check
// is a handful of rclone listings -- and past it the answer is "not now":
// the row is withheld, never guessed at, since a press on it runs the same
// check against the same network. The page's token says whether there is
// still a page to add the row to when the answer comes back.
static void cloudOfferTidyFolders(Window* window, GuiSettings* s)
{
	std::weak_ptr<void> alive = s->lifeToken();
	std::thread([window, s, alive]
	{
		const int rc = ApiSystem::executeScriptLegacy("timeout 30 /usr/bin/cloud_migrate_layout --check",
			[](const std::string&) {}).second;
		if (rc != 0)
			return;
		AppWindow::post(window, [window, s, alive]
		{
			if (alive.expired())
				return;
			s->addWithDescription(_("TIDY UP YOUR CLOUD FOLDERS"),
				_("MOVE SAVES AND SETTINGS BACKUPS INTO /ROCKNIX. NOTHING IS DELETED."),
				nullptr, [window] { cloudPreviewTidyFolders(window); }, "", false, true);
		});
	}).detach();
}

// The three rows of BACKUP AND RESTORE follow the transfer that is current
// (CloudTransferJob::current). Built for a page that could be left with
// its run going on (fork #187; the scan row under SCAN GAMES was the model,
// audit #186 PL-07); since D-UI-078 the transfer page is sat in, so while
// a run goes this hub is under it and is not ticked, and what these rows
// meet is the run's outcome, kept current until the outcome page has been
// dismissed (D-UI-070): LAST <date> - COMPLETED, then the row's own line
// again. The running words (BACKING UP... - ITEM 2 OF 4 . NES) and the
// press that opens the run's page remain for the seam -- a run that is
// current with no page over it -- and are not a design anybody can reach
// by pressing B (#258 PL-006). The other two rows dim meanwhile -- the scripts' flock refuses
// a second run, so the only thing a press on them can do is show the one in
// flight, which it does. Nothing here is written to disk: the scripts stamp
// each part of a run as they always did (last-backup, last-content-restore,
// ...), and the rows that read those stamps go on reading them. The line
// returns to what the row does because that is the line the maintainer
// chose for these rows (2026-09-06: a stamp on MATCH read as a third kind of
// row beside two submenus); it changes only while there is a run to report.

// A hub row is dimmed while another row's run is in flight, by the entry
// itself: ComponentList::render sets every element's colour every frame
// from the theme, so a colour set once is gone by the first frame.
// MultiLineMenuEntry::setDimmed does that (ComponentListFlags::dimmed, the
// same 0x50), as the offline scan row in GuiRetroAchievementsSettings.cpp
// uses it; the page's own subclass for it went (#308 8-es claude F-ES-25).
// It applied the dim at the next frame, where setDimmed applies it at once;
// nothing else differed.

// The three rows, held weakly as every row on a page is, each with the line
// it returns to.
struct CloudHubRows
{
	struct Row
	{
		std::weak_ptr<MultiLineMenuEntry> entry;
		std::string idle;
	};
	Row backup, restore, match;
};

// The line under the row that launched the run: in flight, the page's own
// words for it (GuiCloudTransfer::rowWords) with the item where the panel
// has room and the verb's word as the form that fits any panel (D-UI-035);
// ended, the outcome dated the way every LAST row is. The description is
// drawn in the menu's small font, in a row that spans the menu less its
// insets; 0.86 of the menu width is what a 640x480 frame showed the line
// to have (the scan row's measure).
static std::string cloudTransferRowLine(const std::shared_ptr<CloudTransferJob>& job)
{
	std::vector<std::string> candidates;
	if (!job->finished())
	{
		const GuiCloudTransfer::RowWords w = GuiCloudTransfer::rowWords(job);
		if (!w.item.empty())
			candidates.push_back(w.counted + "  ·  " + w.item);
		if (w.counted != w.word)
			candidates.push_back(w.counted);
		candidates.push_back(w.word);
	}
	else
	{
		const std::string outcome = GuiCloudTransfer::outcomeWord(job);
		candidates.push_back(cloudLastLabel(job->finishedAt()) + "  -  " + outcome);
		candidates.push_back(outcome);
	}
	auto theme = ThemeData::getMenuTheme();
	const float menuWidth = Renderer::ScreenSettings::fullScreenMenus()
		? (float) Renderer::getScreenWidth()
		: (float) Math::min((int) Renderer::getScreenHeight(), (int) (Renderer::getScreenWidth() * 0.90f));
	std::shared_ptr<Font> font = theme->TextSmall.font;
	return CloudText::chooseThatFits(candidates, menuWidth * 0.86f,
		[font](const std::string& t) { return font ? font->sizeText(t).x() : 0.0f; });
}

static void cloudHubRefreshRow(CloudHubRows::Row& row, const std::shared_ptr<CloudTransferJob>& job, bool launchedHere)
{
	auto entry = row.entry.lock();
	if (!entry)
		return;
	std::string text = row.idle;
	bool dimmed = false;
	if (job != nullptr)
	{
		if (launchedHere)
			text = cloudTransferRowLine(job);
		else
			dimmed = true;
	}
	entry->setDimmed(dimmed);
	// Only when the words change: the refresher asks once a second, and a
	// line set to itself would still lay the row out again.
	if (text != entry->getDescription())
		entry->setDescription(text);
}

static void cloudHubRefresh(const std::shared_ptr<CloudHubRows>& rows)
{
	const std::shared_ptr<CloudTransferJob> job = CloudTransferJob::current();
	const CloudText::TransferKind kind = job != nullptr
		? CloudText::transferKind(job->command()) : CloudText::TransferKind::Other;
	cloudHubRefreshRow(rows->backup,  job, kind == CloudText::TransferKind::Backup);
	cloudHubRefreshRow(rows->restore, job, kind == CloudText::TransferKind::Restore);
	cloudHubRefreshRow(rows->match,   job, kind == CloudText::TransferKind::Match);
}

// The hub carries one component that is never drawn and never focused,
// asks once a second while the hub is the top page, and refreshes the rows
// -- which change their words only when they differ (cloudHubRefreshRow).
// Owned by the page (EXTRACHILDREN: GuiComponent's destructor deletes it),
// so it dies with the page; ticked by the page's update, which Window
// gives to the top page alone, so the transfer page over it pauses it and
// the rows catch up the second that page is dismissed -- which, with the
// page sat in (D-UI-078), is when the run has ended and its outcome is
// what there is to show. Built as the scan row's refresher was, for a run
// nobody was watching (#187).
class CloudHubRefresher : public GuiComponent
{
public:
	CloudHubRefresher(Window* window, const std::shared_ptr<CloudHubRows>& rows)
		: GuiComponent(window), mRows(rows), mElapsedMs(0)
	{
		setVisible(false);
		setExtraType(ExtraType::EXTRACHILDREN);
	}

	void update(int deltaTime) override
	{
		GuiComponent::update(deltaTime);
		mElapsedMs += deltaTime;
		if (mElapsedMs < 1000)
			return;
		mElapsedMs = 0;
		cloudHubRefresh(mRows);
	}

private:
	std::shared_ptr<CloudHubRows> mRows;
	int mElapsedMs;
};

// A row of BACKUP AND RESTORE: cloudAddGatedEntry's row -- the label over
// one wrapped line, the action on A, the setup offer before a cloud is
// connected -- as an entry the page keeps a weak hold of once the cloud is
// connected, so its line can follow a run. A press while a run is current
// opens the run's page: there is nothing else it can do (the flock), and
// the page shows the run or its outcome.
static void cloudAddTransferRow(GuiSettings* s, Window* window, bool configured, CloudHubRows::Row& slot,
	const std::string& label, const std::string& description, const std::function<void()>& action)
{
	slot.idle = description;
	// One press for both branches: a row greyed at build time and set up
	// since runs this too (cloudAddGatedEntry re-checks at the press).
	const std::function<void()> press = [window, action]
	{
		if (const std::shared_ptr<CloudTransferJob> job = CloudTransferJob::current())
		{
			window->pushGui(new GuiCloudTransfer(window, job));
			return;
		}
		action();
	};
	if (!configured)
	{
		cloudAddGatedEntry(s, window, false, label, description, press);
		return;
	}
	auto entry = std::make_shared<MultiLineMenuEntry>(window, Utils::String::toUpper(label), description, true);
	slot.entry = entry;
	ComponentListRow row;
	row.addElement(entry, true);
	row.makeAcceptInputHandler(press);
	s->addRow(row);
}

// The cloud hub. One page, reachable from one place.
//
// Cloud used to live in two menus: saves and content under GAME SETTINGS,
// connection and system settings under NETWORK SETTINGS. Two of the three data
// classes were in a different menu from the third, so "back up my handheld"
// meant knowing which half of the split you wanted before you could start.
// Direction first, then what moves -- the split the system actually keeps is
// the one the player is shown.
void GuiMenu::openCloud(Window* window, bool onFolderRow)
{
	const bool configured = Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false);
	auto s = new GuiSettings(window, _("CLOUD"));

	// The three rows follow the current transfer's outcome (D-UI-070; the
	// page is sat in since D-UI-078): see cloudAddTransferRow and the
	// refresher above.
	auto rows = std::make_shared<CloudHubRows>();
	s->addGroup(_("BACKUP AND RESTORE"));
	cloudAddTransferRow(s, window, configured, rows->backup, _("BACK UP TO THE CLOUD"),
		_("SEND THIS DEVICE'S DATA TO YOUR CLOUD."),
		[window] { cloudOpenTransfer(window, true); });
	cloudAddTransferRow(s, window, configured, rows->restore, _("RESTORE FROM THE CLOUD"),
		_("BRING DATA BACK ONTO THIS DEVICE."),
		[window] { cloudOpenTransfer(window, false); });

	// The third action, and the only one that deletes. Below the two safe ones
	// deliberately: a destructive row above RESTORE would be the one a thumb
	// reaches first.
	if (Utils::FileSystem::exists("/usr/bin/cloud_content_restore"))
	{
		// Two lines (D-UI-023), and the same two as its siblings: a label and a
		// line saying what it does. This is the one row in the group that removes
		// something, so that is the line -- the preview then names every ROM.
		// It briefly carried its last-run stamp instead, which read as a third
		// kind of row beside two submenus (maintainer, 2026-09-06).
		cloudAddTransferRow(s, window, configured, rows->match, _("MATCH THIS DEVICE TO THE CLOUD"),
			_("REMOVE ROMS YOUR CLOUD NO LONGER HAS."), [window] { cloudOpenMatch(window); });
	}
	// Once now, so the first frame is right, then once a second while the
	// page is up.
	cloudHubRefresh(rows);
	s->addChild(new CloudHubRefresher(window, rows));

	s->addGroup(_("SAVE MANAGEMENT"));

	// The line under each toggle is how its sync last went, read from the
	// whole-run stamp ThreadedCloudSync leaves per cause (fork #94). The
	// card that shows the run is gone seconds after it ends, and a startup
	// sync runs while the player is looking at the boot splash or not at the
	// device at all -- so this row is where "did it run?" gets answered, and
	// it answers whether the toggle is on now or not: a stamp is a fact about
	// a run that happened. It replaced REQUIRES NETWORK ACCESS, which the
	// outcome says better when it matters (SKIPPED, NO NETWORK). Two lines,
	// never three (D-UI-023).
	auto cloud_startup = std::make_shared<SwitchComponent>(window);
	cloud_startup->setState(SystemConf::getInstance()->get("cloudsaves.startup") == "1");
	s->addWithDescription(_("SYNC SAVES DURING STARTUP"), cloudLastRunDetail("sync-startup"), cloud_startup);
	cloud_startup->setOnChangedCallback([cloud_startup] {
		SystemConf::getInstance()->set("cloudsaves.startup", cloud_startup->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});
	auto cloud_gameexit = std::make_shared<SwitchComponent>(window);
	cloud_gameexit->setState(SystemConf::getInstance()->get("cloudsaves.gameexit") == "1");
	s->addWithDescription(_("SYNC SAVES WHEN EXITING A GAME"), cloudLastRunDetail("sync-exit"), cloud_gameexit);
	cloud_gameexit->setOnChangedCallback([cloud_gameexit] {
		SystemConf::getInstance()->set("cloudsaves.gameexit", cloud_gameexit->getState() ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
	});

	s->addGroup(_("CLOUD STORAGE SETUP"));
	if (configured)
	{
		// Who this device is connected to, and whether it answers.
		//
		// The page offered to connect a provider and never said one already
		// was: "there's nowhere where it shows what cloud backend you
		// currently have, which seems like a miss" (maintainer, 2026-09-10,
		// fork #110). The name is the player's own label for the remote when
		// they gave it one, otherwise the provider's name.
		//
		// One cloud_setup --info for the page: it runs rclone listremotes, a
		// route lookup and systemctl, synchronously on the interface thread,
		// and the page asked it three times -- once for a name nothing read
		// (#308 8-es claude F-ES-03).
		std::map<std::string, std::string> info = cloudSetupInfo();
		const std::string provider = CloudText::providerLabel(info["REMOTE_TYPE"]);
		if (!provider.empty())
			s->addWithLabel(_("CONNECTED TO"), std::make_shared<TextComponent>(window, provider,
				ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color));

		// The thing to reach for when a sync has just failed: does the cloud
		// answer right now? Behind GuiLoading, because it is a network round
		// trip -- bounded by the script (a single listing, 10 s to connect,
		// 30 s to answer), so the page cannot hang on it.
		s->addWithDescription(_("CHECK CONNECTION"), _("SEE WHETHER YOUR CLOUD ANSWERS RIGHT NOW."), nullptr,
			[window, provider]
			{
				window->pushGui(new GuiLoading<int>(window, _("CHECKING YOUR CLOUD..."),
					[](auto gui)
					{
						return ApiSystem::executeScriptLegacy("/usr/bin/cloud_setup --check", [](const std::string) {}).second;
					},
					[window, provider](int rc)
					{
						const std::string who = provider.empty() ? _("YOUR CLOUD") : provider;
						std::string text;
						if (rc == 0)
							text = who + " " + _("ANSWERED. YOU'RE CONNECTED.");
						else if (rc == 1)
							text = _("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.");
						else
							text = _("COULDN'T REACH") + " " + who + ".\n\n"
								+ _("CHECK WI-FI, THEN TRY AGAIN. IF IT KEEPS FAILING, SIGN IN AGAIN UNDER CONNECT OR REPAIR CLOUD STORAGE.");
						window->pushGui(new GuiMsgBox(window, text, _("OK")));
					}));
			}, "", false, true);


		// Came here with the rest of NETWORK SETTINGS' cloud group, and was
		// missed when that group was deleted -- which left the folder editable
		// only by walking the setup wizard to its final page again, for
		// somebody whose remote is already configured and working.
		//
		// A verb, like everything else in this group. "CLOUD FOLDER" reads as a
		// heading rather than something you can act on; not "choose" or
		// "select", which promise a list to pick from, when this opens a
		// keyboard and you type a path.
		//
		// A changed folder rebuilds this page, so the line under the row
		// names the new one: with no onDone the page kept the folder it was
		// built with until it was reopened (#308 8-es claude F-ES-27). The
		// editor calls it only when the script took the folder; the new page
		// goes up before the old one closes, so nothing flashes between.
		// And it opens on this row (audit of the fixes, G-E2-O1; D-UI-042):
		// the rebuilt page opened on its first row with this one scrolled
		// off the foot, so the player could not see the confirmation, and
		// the next A opened BACK UP TO THE CLOUD instead of the editor.
		const std::string syncpath = info["SAVES_REMOTE"];
		// One line under the row (D-UI-023): the sentence around the path took
		// 580 of the 620 px a 640x480 description has, so any real path
		// wrapped to a third line (#308 8-es claude F-ES-14; Roboto-Bold at
		// 15 and 20 px, the budget noted at openRestoreRelink's DEVICE
		// PASSWORD row).
		s->addWithDescription(_("CHANGE CLOUD FOLDER"),
			_("YOUR SAVES ARE IN:") + " " + syncpath,
			nullptr, [window, s, syncpath] { cloudSetupOpenSyncPathEditor(window, syncpath, [window, s] { GuiMenu::openCloud(window, true); s->close(); }); },
			"", onFolderRow, true);
	}
	// Moved here from NETWORK SETTINGS. Offered, never automatic: the first
	// layout put everything under /GAMES with backups nested inside saves, and
	// where a player's saves live is theirs to decide. The row only appears
	// when there is something to move -- the script answers 3 when the device
	// is already current.
	// runSystemCommand returns 0 whatever the command exits, so the old
	// "== 0" here was always true and the row appeared on every device,
	// including one set up minutes earlier -- whose press then produced
	// "Already on the current layout. MOVE THEM?" (maintainer, 2026-09-06).
	// executeScriptLegacy's pair form carries the real exit code: 0 means
	// something to move, 3 means already current.
	//
	// Asked off the interface thread (fork #103). --check answers 3 from
	// the config alone when the device is already current, but on one that
	// is not it lists the remote to say what would move -- a network round
	// trip this page used to make in its constructor, with rclone's default
	// timeouts (a minute to connect, five for I/O) as the only bound. The
	// page opens without the row and the row arrives with the answer, which
	// puts it at the end of the group rather than ahead of CONNECT OR
	// REPAIR; on a device that is already tidy it never arrives at all.
	if (configured && Utils::FileSystem::exists("/usr/bin/cloud_migrate_layout"))
		cloudOfferTidyFolders(window, s);

	s->addWithDescription(_("CONNECT OR REPAIR CLOUD STORAGE"),
		configured ? _("CHANGE THE FOLDER, ADD A PROVIDER, OR RENEW A SIGN-IN.")
		           : _("SET UP A PROVIDER WITH RCLONE, FROM THE HANDHELD. NO COMPUTER NEEDED."),
		nullptr, [window] { GuiMenu::openCloudAddRemote(window); }, "", false, true);

	// Only while there is a restore to finish.
	// Read uncached: exists() remembers its answer for the session, and this
	// marker is written by a restore that ran after boot -- a cached "absent"
	// from startup hid the row until the next restart (guest b, 2026-09-13).
	if (Utils::FileSystem::exists("/storage/.config/.restore-finish-pending", false))
	{
		// One line under the label on both panels (D-UI-023; the budget is
		// noted at openRestoreRelink's DEVICE PASSWORD row).
		// FINISH from here finishes the restore, as it does from NETWORK
		// SETTINGS: the row exists only while the marker does, and passing
		// false -- from when the row was always there -- left the marker, so
		// the page came back at the next start after FINISH (#308 8a gpt
		// F-ES-06).
		s->addWithDescription(_("FINISH RESTORE PROCESS"),
			_("RE-ENTER THE PASSWORDS BACKUPS LEAVE OUT (WI-FI, ACCOUNTS, DEVICE)."), nullptr,
			[window] { GuiMenu::openRestoreRelink(window, true); }, "", false, true);
	}

	window->pushGui(s);
}

void GuiMenu::openGamesSettings()
{
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("GAME SETTINGS").c_str());

	s->addGroup(_("TOOLS"));

	// Game List Update
	s->addEntry(_("UPDATE GAMELISTS"), false, [this, window] { updateGameLists(window); });

	s->addEntry(_("MOONLIGHT GAME STREAMING"), false, [this, window] { GuiMoonlight::show(mWindow); });

	if (SystemConf::getInstance()->getBool("global.retroachievements") && !Settings::getInstance()->getBool("RetroachievementsMenuitem") && SystemConf::getInstance()->get("global.retroachievements.username") != "")
	{
		s->addEntry(_("RETROACHIEVEMENTS").c_str(), true, [this] 
		{ 
			if (!OfflineAchievements::proxyOffline() && !checkNetwork())
				return;

			GuiRetroAchievements::show(mWindow); 
		});
	}
	

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS) || (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY)))
		s->addGroup(_("ACCOUNTS"));

	// Retroachievements
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RETROACHIVEMENTS))
		s->addEntry(_("RETROACHIEVEMENTS SETTINGS"), true, [this] { openRetroachievementsSettings(); });

	// Netplay
	if (SystemData::isNetplayActivated() && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::NETPLAY))
		s->addEntry(_("NETPLAY SETTINGS"), true, [this] { openNetplaySettings(); }, "iconNetplay");

	// Missing Bios
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BIOSINFORMATION))
	{
		s->addGroup(_("BIOS SETTINGS"));
		s->addEntry(_("BIOS CHECK"), true, [this, s] { openMissingBiosSettings(); });

		auto checkBiosesAtLaunch = std::make_shared<SwitchComponent>(mWindow);
		checkBiosesAtLaunch->setState(Settings::getInstance()->getBool("CheckBiosesAtLaunch"));
		s->addWithLabel(_("CHECK BIOS FILES BEFORE RUNNING A GAME"), checkBiosesAtLaunch);
		s->addSaveFunc([checkBiosesAtLaunch] { Settings::getInstance()->setBool("CheckBiosesAtLaunch", checkBiosesAtLaunch->getState()); });
	}

	// Custom config for systems
	s->addGroup(_("SAVE STATES"));

	// AUTO SAVE/LOAD
	auto autosave_enabled = std::make_shared<SwitchComponent>(mWindow);
	autosave_enabled->setState(SystemConf::getInstance()->get("global.autosave") == "1");
	s->addWithDescription(_("AUTO SAVE/LOAD"), _("Load latest save state on game launch and save state when exiting game."), autosave_enabled);
	s->addSaveFunc([autosave_enabled] { SystemConf::getInstance()->set("global.autosave", autosave_enabled->getState() ? "1" : ""); });

	// INCREMENTAL SAVESTATES
	auto incrementalSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INCREMENTAL SAVE STATES"));
	// A device upgraded from before the row had two spellings can hold "0",
	// which matches no entry, so the row showed INCREMENT PER SAVE while the
	// launcher ran with auto-index off. "0" reads as DO NOT INCREMENT, which
	// is what it does (fork #209, D-UI-083); the row's next save writes "2".
	// The launcher that reads it is ROCKNIX's setsettings.sh, set_savestates:
	// `0|2|false|none` turn savestate_auto_index off, anything else on -- so
	// the old INCREMENT SLOT ("0") and DO NOT INCREMENT ("2") were one
	// behaviour under two names (#308 8-es claude F-ES-18).
	std::string incrementalValue = SystemConf::getInstance()->get("global.incrementalsavestates");
	if (incrementalValue == "0")
		incrementalValue = "2";
	incrementalSaveStates->addRange({
		{ _("INCREMENT PER SAVE"), _("Never overwrite old save states, always make new ones."), "" }, // Don't use 1 -> 1 is YES, auto too
		{ _("DO NOT INCREMENT"), _("Use current slot on a new game."), "2" } },
		incrementalValue);

	s->addWithLabel(_("INCREMENTAL SAVE STATES"), incrementalSaveStates);
	s->addSaveFunc([incrementalSaveStates] { SystemConf::getInstance()->set("global.incrementalsavestates", incrementalSaveStates->getSelected()); });

	// SHOW SAVE STATES
	auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVE STATE MANAGER"));
	showSaveStates->addRange({ { _("NO"), "auto" },{ _("ALWAYS") , "1" },{ _("IF NOT EMPTY") , "2" } }, SystemConf::getInstance()->get("global.savestates"));
	s->addWithDescription(_("SHOW SAVE STATE MANAGER"), _("Display save state manager before launching a game."), showSaveStates);
	s->addSaveFunc([showSaveStates] { SystemConf::getInstance()->set("global.savestates", showSaveStates->getSelected()); });

	s->addGroup(_("DEFAULT GLOBAL SETTINGS"));

	// Screen ratio choice
	if (!hasGlobalFeature("ratio"))
	{
		auto ratio_choice = createRatioOptionList(mWindow, "global");
		s->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		s->addSaveFunc([ratio_choice] { SystemConf::getInstance()->set("global.ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::RESOLUTION) && !hasGlobalFeature("videomode"))
	{
		auto videoModeOptionList = createVideoResolutionModeOptionList(mWindow, "global");
		s->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoModeOptionList);
		s->addSaveFunc([this, videoModeOptionList] { SystemConf::getInstance()->set("global.videomode", videoModeOptionList->getSelected()); });
	}

	// smoothing	
	if (!hasGlobalFeature("smooth"))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.smooth"));
		s->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		s->addSaveFunc([smoothing_enabled] { SystemConf::getInstance()->set("global.smooth", smoothing_enabled->getSelected()); });
	}

	// rewind
	if (!hasGlobalFeature("rewind"))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.rewind"));
		s->addWithDescription(_("REWIND"), _("Store past states to rewind to in realtime, if the core supports it."), rewind_enabled);
		s->addSaveFunc([rewind_enabled] { SystemConf::getInstance()->set("global.rewind", rewind_enabled->getSelected()); });
	}
	
	// Integer scale
	if (!hasGlobalFeature("integerscale"))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF") , "0" } }, SystemConf::getInstance()->get("global.integerscale"));
		s->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		s->addSaveFunc([integerscale_enabled] { SystemConf::getInstance()->set("global.integerscale", integerscale_enabled->getSelected()); });
	}

	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) && !hasGlobalFeature("shaderset"))
	{
#if !defined(ROCKNIX)
		auto installedShaders = ApiSystem::getInstance()->getShaderList("", "", "");
		if (installedShaders.size() > 0)
		{
#endif
			std::string currentShader = SystemConf::getInstance()->get("global.shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);			
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

#if !defined(ROCKNIX)
			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);
			
#else
			std::string shader;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getshaders)")); getline(ss, shader, ','); )
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);
#endif
			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

			s->addWithLabel(_("SHADER SET"), shaders_choices);
			s->addSaveFunc([shaders_choices] { SystemConf::getInstance()->set("global.shaderset", shaders_choices->getSelected()); });
#if !defined(ROCKNIX)
		}
#endif
	}

	// Video Filters
#if !defined(ROCKNIX)
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) && !hasGlobalFeature("videofilters"))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList("", "", "");
		if (installedVideofilters.size() > 0)
		{
#else
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) && !hasGlobalFeature("shaderset"))
	{
#endif
			std::string currentVideofilter = SystemConf::getInstance()->get("global.videofilters");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

#if !defined(ROCKNIX)
			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

#else
			std::string videofilter;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getfilters)")); getline(ss, videofilter, ','); )
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);
#endif
			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			s->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			s->addSaveFunc([videofilters_choices] { SystemConf::getInstance()->set("global.videofilters", videofilters_choices->getSelected()); });
#if !defined(ROCKNIX)
		}
#endif
	}

	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && !hasGlobalFeature("bezel"))
	{		
		auto sets = GuiMenu::getDecorationsSets(ViewController::get()->getState().getSystem());
		if (sets.size() > 0)
		{
#if defined(BATOCERA)
			s->addEntry(_("DECORATIONS"), true, [this, sets]
			{
				GuiSettings *decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets);

				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel_stretch") != "0" && SystemConf::getInstance()->get("global.bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get("global.bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get("global.bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled] {
						if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set("global.bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TATTOO OVER BEZEL"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo") != "0"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "system"
						&& SystemConf::getInstance()->get("global.bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get("global.bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get("global.bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM IMAGE"), "custom", SystemConf::getInstance()->get("global.bezel.tattoo") == "custom");
				decorations_window->addWithDescription(_("SHOW TATTOO OVER BEZEL"), _("Show an image overlaid on top of the bezel."), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo] {
						if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TATTOO CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NW"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "NE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SE"
						&& SystemConf::getInstance()->get("global.bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get("global.bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("TATTOO CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner] {
						if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set("global.bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
						}
						});
				decorations_window->addInputTextConfigRow(_("CUSTOM .PNG IMAGE PATH"), "global.bezel.tattoo_file", false);

				auto bezel_resize_tattoo = std::make_shared<SwitchComponent>(mWindow);
				bezel_resize_tattoo->setState(SystemConf::getInstance()->getBool("global.bezel.resize_tattoo"));
				decorations_window->addWithDescription(_("RESIZE TATTOO"), _("Shrink/expand tattoo to fit within the bezel's border."), bezel_resize_tattoo);
				decorations_window->addSaveFunc([bezel_resize_tattoo]
				{
					if (SystemConf::getInstance()->getBool("global.bezel.resize_tattoo") != bezel_resize_tattoo->getState()) {
						SystemConf::getInstance()->setBool("global.bezel.resize_tattoo", bezel_resize_tattoo->getState());
					}
				});

				mWindow->pushGui(decorations_window);
			});			
#else
			addDecorationSetOptionListComponent(mWindow, s, sets);
#endif
		}
	}
	
	// latency reduction
	if (!hasGlobalFeature("runahead"))
		s->addEntry(_("LATENCY REDUCTION"), true, [this] { openLatencyReductionConfiguration(mWindow, "global"); });

	//AI-enabled translations
	if (!hasGlobalFeature("ai_service_enabled"))
	{
		s->addEntry(_("AI GAME TRANSLATION"), true, [this]
		{
			GuiSettings *ai_service = new GuiSettings(mWindow, _("AI GAME TRANSLATION").c_str());

			// AI service enabled?
			auto ai_service_enabled = std::make_shared<SwitchComponent>(mWindow);
			ai_service_enabled->setState(
				SystemConf::getInstance()->get("global.ai_service_enabled") == "1");
			ai_service->addWithLabel(_("ENABLE AI TRANSLATION SERVICE"), ai_service_enabled);

			// Target language - order is: popular languages in the Batocera community first
			// then alphabetical order of the 2-char lang code (because the strings are localized)
			auto lang_choices = std::make_shared<OptionListComponent<std::string> >(mWindow,
				_("TARGET LANGUAGE"), false);
			std::string currentLang = SystemConf::getInstance()->get("global.ai_target_lang");
			if (currentLang.empty())
				currentLang = std::string("En");
			lang_choices->add("ENGLISH", "En", currentLang == "En");
			lang_choices->add("FRANÇAIS", "Fr", currentLang == "Fr");
			lang_choices->add("PORTUGUÊS", "Pt", currentLang == "Pt");
			lang_choices->add("DEUTSCH", "De", currentLang == "De");
			lang_choices->add("ΕΛΛΗΝΙΚΆ", "El", currentLang == "El");
			lang_choices->add("ESPAÑOL", "Es", currentLang == "Es");
			lang_choices->add("ČEŠTINA", "Cs", currentLang == "Cs");
			lang_choices->add("DANSK", "Da", currentLang == "Da");
			lang_choices->add("HRVATSKI", "Hr", currentLang == "Hr");
			lang_choices->add("MAGYAR", "Hu", currentLang == "Hu");
			lang_choices->add("ITALIANO", "It", currentLang == "It");
			lang_choices->add("日本語", "Ja", currentLang == "Ja");
			lang_choices->add("한국어", "Ko", currentLang == "Ko");
			lang_choices->add("NEDERLANDS", "Nl", currentLang == "Nl");
			lang_choices->add("NORSK", "Nn", currentLang == "Nn");
			lang_choices->add("POLSKI", "Pl", currentLang == "Pl");
			lang_choices->add("ROMÂNĂ", "Ro", currentLang == "Ro");
			lang_choices->add("РУССКИЙ", "Ru", currentLang == "Ru");
			lang_choices->add("SVENSKA", "Sv", currentLang == "Sv");
			lang_choices->add("TÜRKÇE", "Tr", currentLang == "Tr");
			lang_choices->add("简体中文", "Zh", currentLang == "Zh");
			ai_service->addWithLabel(_("TARGET LANGUAGE"), lang_choices);

			// Service  URL
			ai_service->addInputTextConfigRow(_("AI TRANSLATION SERVICE URL"), "global.ai_service_url", false);

			// Pause game for translation?
			auto ai_service_pause = std::make_shared<SwitchComponent>(mWindow);
			ai_service_pause->setState(
				SystemConf::getInstance()->get("global.ai_service_pause") == "1");
			ai_service->addWithLabel(_("PAUSE ON TRANSLATED SCREEN"), ai_service_pause);

			ai_service->addSaveFunc([ai_service_enabled, lang_choices, ai_service_pause] {
				if (ai_service_enabled->changed())
					SystemConf::getInstance()->set("global.ai_service_enabled",
						ai_service_enabled->getState() ? "1" : "0");
				if (lang_choices->changed())
					SystemConf::getInstance()->set("global.ai_target_lang",
						lang_choices->getSelected());
				if (ai_service_pause->changed())
					SystemConf::getInstance()->set("global.ai_service_pause",
						ai_service_pause->getState() ? "1" : "0");
				SystemConf::getInstance()->saveSystemConf();
			});

			mWindow->pushGui(ai_service);
		});
	}
	
	// Load global custom features
	addFeatures(CustomFeatures::GlobalFeatures, window, s, "global", "", "", "", _("DEFAULT GLOBAL SETTINGS"));
	
	if (!hasGlobalFeature("disableautocontrollers") && SystemData::sSystemVector.any([](auto sys) { return !sys->getCompatibleCoreNames(EmulatorFeatures::autocontrollers).empty(); }))
	{
		auto autoControllers = std::make_shared<SwitchComponent>(mWindow);
		autoControllers->setState(SystemConf::getInstance()->get("global.disableautocontrollers") != "1");
		s->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		s->addSaveFunc([autoControllers] { SystemConf::getInstance()->set("global.disableautocontrollers", autoControllers->getState() ? "" : "1"); });
	}

	// The save actions live here; the rest of cloud lives one row down.
	//
	// These were briefly removed as duplicates of the transfer flow behind
	// ALL CLOUD SETTINGS AND SERVICES, which left this menu with a CLOUD
	// group containing one CLOUD row -- a header naming a single entry that
	// named it again -- and buried the operations people actually repeat
	// under two more presses. Moving saves is the frequent job; backing up
	// ROMs and system settings is occasional, and belongs on the page for
	// occasional things.
	if (Utils::FileSystem::exists("/usr/bin/cloud_backup") && Utils::FileSystem::exists("/usr/bin/cloud_restore"))
	{
		const bool cloudConfigured = Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false);
		s->addGroup(_("CLOUD SETTINGS"));

		// The line under it is how it last went (last-sync-manual, D-UI-023),
		// like the two rows below; "both ways, nothing is deleted" is the
		// dialog's to say. Each half reports itself to the card as it ends
		// (">>> tier <label>|<rc>"), so a restore that finished under a
		// backup that did not is reported with that part's why rather than as the
		// whole run failed (D-UI-028); the backup still waits on the restore.
		cloudAddGatedEntry(s, window, cloudConfigured, _("SYNC SAVES WITH THE CLOUD"),
			cloudLastRunDetail("sync-manual"), [window] {
			window->pushGui(new GuiMsgBox(window, _("SYNC GAME SAVES BOTH WAYS?\n\nTHE NEWEST COPY OF EACH SAVE IS KEPT ON BOTH SIDES. NOTHING IS DELETED.") + cloudLastRunWhy("sync-manual"), _("YES"),
				[window] {
				ThreadedCloudSync::start(window,
					"/usr/bin/cloud_restore --yes --method=copy --update --saves-only; _r=$?; echo \">>> tier RESTORING SAVES|$_r\"; [ \"$_r\" = 0 ] || exit \"$_r\";"
					" /usr/bin/cloud_backup --yes --method=copy --update --saves-only; _b=$?; echo \">>> tier BACKING UP SAVES|$_b\"; exit \"$_b\"",
					_("SYNC SAVES"), _("SYNCING SAVES"), ThreadedCloudSync::Origin::Manual);
				}, _("NO"), nullptr));
		});
		cloudAddClassRow(s, window, cloudConfigured, _("BACK UP SAVES TO THE CLOUD"), "backup", [window] {
			window->pushGui(new GuiMsgBox(window, _("BACK UP GAME SAVES, SAVE STATES, AND SCREENSHOTS TO THE CLOUD?") + cloudLastRunWhy("backup"), _("YES"),
				[window] { ThreadedCloudSync::start(window, "/usr/bin/cloud_backup --yes --saves-only", _("BACK UP SAVES"), _("BACKING UP SAVES"), ThreadedCloudSync::Origin::Manual); },
				_("NO"), nullptr));
		});
		cloudAddClassRow(s, window, cloudConfigured, _("RESTORE SAVES FROM THE CLOUD"), "restore", [window] {
			window->pushGui(new GuiMsgBox(window, _("RESTORE GAME SAVES, SAVE STATES, AND SCREENSHOTS FROM THE CLOUD?") + cloudLastRunWhy("restore"), _("YES"),
				[window] { ThreadedCloudSync::start(window, "/usr/bin/cloud_restore --yes --saves-only", _("RESTORE SAVES"), _("RESTORING SAVES"), ThreadedCloudSync::Origin::Manual); },
				_("NO"), nullptr));
		});

		s->addWithDescription(_("MANAGE CLOUD STORAGE"),
			_("BACKUP AND RESTORE, SAVE MANAGEMENT, CLOUD STORAGE SETUP."), nullptr,
			[window] { GuiMenu::openCloud(window); }, "", false, true);
	}


	s->addGroup(_("SYSTEM SETTINGS"));

	// Custom config for systems
	s->addEntry(_("PER SYSTEM ADVANCED CONFIGURATION"), true, [this, s, window]
		{
			s->save();
			GuiSettings* configuration = new GuiSettings(window, _("PER SYSTEM ADVANCED CONFIGURATION").c_str());

			// For each activated system
			std::vector<SystemData*> systems = SystemData::sSystemVector;
			for (auto system : systems)
			{
				if (system->isCollection() || !system->isGameSystem())
					continue;

				if (system->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
					continue;

				if (!system->hasFeatures() && !system->hasEmulatorSelection())
					continue;

				configuration->addEntry(system->getFullName(), true, [this, system, window] {
					popSystemConfigurationGui(window, system);
					});
			}

			window->pushGui(configuration);
		});

	mWindow->pushGui(s);
}

void GuiMenu::openMissingBiosSettings()
{
	GuiBios::show(mWindow);
}

// Native cloud-remote setup wizard, driven by the cloud_setup script:
// 1 SSH setup (password + service), 2 connect, 3 create/repair the
// remote, then a completion page. Every step verifies real state - the
// preconditions via `--info`, the connection via `--connected` (an SSH
// session must actually be open) and the remote via `--check` - so
// mandatory steps cannot be skipped, only exited. EXIT SETUP and
// CONTINUE are bottom buttons (exit left, continue right; CONTINUE only
// exists once the step's check passes).
//
// Page typography, consistent across steps: section headers via
// addGroup; body text in the small theme font on non-selectable rows
// padded to align with selectable ones; commands and values in the
// accent color with commands additionally 'quoted'; actions in the
// standard menu font. Remote names lose the trailing colon rclone
// prints before they are shown.

enum class CloudSetupMode { FirstRemote, AddRemote, RepairRemote };

static void cloudSetupShowSshStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev);
static void cloudSetupShowConnectStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev);
static void cloudSetupShowConfigureStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev);
static void cloudSetupShowDoneStep(Window* window, const std::string& remote, GuiSettings* prev);

static std::string cloudSetupTitle(CloudSetupMode mode)
{
	switch (mode)
	{
	case CloudSetupMode::AddRemote:
		return _("ADD A CONNECTION");
	case CloudSetupMode::RepairRemote:
		return _("REPAIR A CONNECTION");
	default:
		return _("SET UP YOUR CLOUD STORAGE");
	}
}

// rclone prints remote names with a trailing colon ("dropbox:"); the
// colon is rclone path syntax, not part of the name - strip it for
// anything the player reads.
static std::string cloudSetupDisplayName(const std::string& remote)
{
	if (!remote.empty() && remote.back() == ':')
		return remote.substr(0, remote.size() - 1);
	return remote;
}

static std::map<std::string, std::string> cloudSetupInfo()
{
	std::map<std::string, std::string> info;
	// executeScriptLegacy keeps line boundaries; GetShOutput joins all
	// output lines together, which breaks the key=value parse.
	// Local -- `ip route get`, systemctl, rclone listremotes -- and read
	// while pages are being built, so bounded: the route lookup is a
	// netlink call a wedged Wi-Fi driver can hold (fork #103), and rclone's
	// start-up is the rest of the cost.
	for (auto& line : ApiSystem::executeScriptLegacy("timeout 10 /usr/bin/cloud_setup --info"))
	{
		auto pos = line.find('=');
		if (pos != std::string::npos)
			info[line.substr(0, pos)] = Utils::String::trim(line.substr(pos + 1));
	}
	return info;
}


// A body-text row: never selectable, small theme font, padded to align
// with the selectable rows. `accent` renders the accent color used for
// values and commands.
static void cloudSetupAddInfoRow(GuiSettings* s, Window* window, const std::string& text, bool accent = false)
{
	auto theme = ThemeData::getMenuTheme();
	ComponentListRow row;
	row.selectable = false;
	// Standard text size, not the small one: these rows carry a step's own
	// content - a status, an instruction, the command to type - so they sit
	// at the same weight as the action rows beside them. Only genuinely
	// subordinate text (a row's description) drops to the small size, which
	// MultiLineMenuEntry already does for us.
	auto tc = std::make_shared<TextComponent>(window, text, theme->Text.font, accent ? theme->Text.selectedColor : theme->Text.color);
	tc->setPadding(CLOUD_SETUP_ROW_PADDING);
	row.addElement(tc, true);
	s->addRow(row);
}


// A cloud action row. Before a remote exists the row stays visible but
// dimmed and offers the setup flow instead of its action - hiding it
// would tell the player nothing about what configuring a remote buys
// them. Shared by every row on the cloud page so they behave identically.
static void cloudAddGatedEntry(GuiSettings* s, Window* window, bool configured, const std::string& label, const std::string& description, const std::function<void()>& action)
{
	if (configured)
	{
		s->addWithDescription(label, description, nullptr, action, "", false, true);
		return;
	}

	ComponentListRow row;
	auto entry = std::make_shared<MultiLineMenuEntry>(window, Utils::String::toUpper(label), description, true);
	// Dimmed by the entry on every frame, not by a colour set once here:
	// ComponentList recolours every element from the theme each frame, and
	// a row dimmed at construction was back to full colour by the first one
	// -- these rows never once rendered dim (fork #182).
	entry->setDimmed(true);
	row.addElement(entry, true);
	// Configured is decided when the page is built, and setup is opened from
	// this very row: CLOUD SETUP COMPLETE's FINISH closes back to the page it
	// came from, built before rclone.conf existed, whose rows went on
	// offering the setup just finished until the page was reopened (audit
	// #307 PL-062). So the press asks again -- a stat, read uncached -- and a
	// row that finds the cloud set up does what it says and stops drawing
	// dim. Every page with these rows gets it, which reopening one page from
	// FINISH would not. Weak: the row holds the entry, not the other way.
	std::weak_ptr<MultiLineMenuEntry> weakEntry = entry;
	row.makeAcceptInputHandler([window, weakEntry, action]
	{
		if (Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false))
		{
			if (auto shown = weakEntry.lock())
				shown->setDimmed(false);
			action();
			return;
		}
		window->pushGui(new GuiMsgBox(window, _("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.\n\nSET IT UP NOW?"), _("YES"),
			[window] { GuiMenu::openCloudAddRemote(window); },
			_("NO"), nullptr));
	});
	s->addRow(row);
}

// A block of prose that ES wraps for us. Hand-splitting a sentence across
// fixed rows breaks on a narrower screen and in every translation, so
// anything longer than a line goes through MultiLineMenuEntry - the same
// component every description row uses - with wrapping switched on.
static void cloudSetupAddProse(GuiSettings* s, Window* window, const std::string& title, const std::string& body)
{
	ComponentListRow row;
	row.selectable = false;
	// ComponentList insets selectable rows but not unselectable ones, so a
	// prose row is flush left unless it pads itself -- misaligned against the
	// group headers and value rows around it.
	auto entry = std::make_shared<MultiLineMenuEntry>(window, title, body, true);
	entry->setPadding(CLOUD_SETUP_ROW_PADDING);
	row.addElement(entry, true);
	s->addRow(row);
}

// Several lines of body text stacked as one component, so they can sit in a
// column beside something tall. ComponentList lays a row's elements out left
// to right, never top to bottom, so a stack has to be a single element -- and
// MultiLineMenuEntry is not it: its second line is deliberately smaller and
// dimmer, which is wrong for an address and a code that both have to be read
// off the screen and typed.
static std::shared_ptr<ComponentGrid> cloudSetupTextStack(Window* window,
	const std::vector<std::pair<std::string, bool>>& lines)
{
	auto theme = ThemeData::getMenuTheme();
	auto grid = std::make_shared<ComponentGrid>(window, Vector2i(1, (int)lines.size()));

	float height = 0;
	for (int i = 0; i < (int)lines.size(); i++)
	{
		auto tc = std::make_shared<TextComponent>(window, lines[i].first, theme->Text.font,
			lines[i].second ? theme->Text.selectedColor : theme->Text.color);
		tc->setPadding(CLOUD_SETUP_ROW_PADDING);
		tc->setVerticalAlignment(ALIGN_TOP);
		grid->setEntry(tc, Vector2i(0, i), false, true);

		// Absolute row heights, so the stack keeps its shape when
		// ComponentList later resizes it to the width left over beside the
		// image. A percentage would re-divide whatever height it was given.
		const float lineHeight = tc->getSize().y();
		grid->setRowHeight(i, lineHeight, false);
		height += lineHeight;
	}

	grid->setSize(0, height);
	return grid;
}

// An image with text beside it rather than under it.
//
// The sign-in page cannot scroll: every row on it is unselectable, so
// ComponentList never moves its camera and anything past the fold is
// unreachable rather than merely off-screen (which is why a QR stacked above
// the address cut the address in half). Vertical space is therefore the
// binding constraint, and a square code sitting alone on a wide row wastes
// most of it. A row centres its elements vertically and gives fixed-width
// ones their own column, so the code and its text share one row's height.
// An image that keeps its own colour inside a menu row.
//
// ComponentList::render calls setColor(Text.color) on every element of every
// row, every frame, and ImageComponent::setColor is setColorShift. So a QR
// added to a row is tinted with the menu's muted text colour sixty times a
// second, and setting the shift once at construction achieves nothing -- the
// first cut of this did exactly that and the code stayed grey. The text
// beside it kept its brightness only because it sits in a ComponentGrid,
// whose setColor does not reach the labels inside.
//
// A QR is not text and must not be themed like text: a phone camera needs
// black on white, and "dimmer than the address next to it" is the visible
// form of that bug.
class UntintedImageComponent : public ImageComponent
{
public:
	UntintedImageComponent(Window* window) : ImageComponent(window)
	{
		ImageComponent::setColorShift(0xFFFFFFFF);
	}
	void setColor(unsigned int /*color*/) override
	{
		ImageComponent::setColorShift(0xFFFFFFFF);
	}
};

static void cloudSetupAddQrRow(GuiSettings* s, Window* window, const std::string& imagePath,
	const std::vector<std::pair<std::string, bool>>& lines)
{
	ComponentListRow row;
	row.selectable = false;

	// A share of the screen, not of the menu: the menu is capped at the
	// screen height, so this stays the same physical size on a wide panel
	// and a square one, and always leaves the majority of the width to text.
	const float qrEdge = Renderer::getScreenHeight() * 0.28f;

	auto qr = std::make_shared<UntintedImageComponent>(window);
	qr->setOpacity(255);
	// Padding before setMaxSize -- ImageComponent folds it into the size it
	// reports, which is what the row measures the column by.
	qr->setPadding(CLOUD_SETUP_ROW_PADDING);
	qr->setImage(imagePath, false, MaxSizeInfo(qrEdge, qrEdge));
	qr->setMaxSize(qrEdge, qrEdge);
	row.addElement(qr, false);

	row.addElement(cloudSetupTextStack(window, lines), true);
	s->addRow(row);
}

// A blank row. Group headers butt straight against whatever precedes them,
// which reads as cramped where a group follows body text rather than another
// group. Small font, so it is one short line rather than a full gap.
static void cloudSetupAddSpacer(GuiSettings* s, Window* window)
{
	auto theme = ThemeData::getMenuTheme();
	ComponentListRow row;
	row.selectable = false;
	row.addElement(std::make_shared<TextComponent>(window, "", theme->TextSmall.font, theme->Text.color), true);
	s->addRow(row);
}

// A label/value row: the same read-only fact shape the rest of the app
// uses (see NETWORK SETTINGS' IP ADDRESS and the system information
// page) - a label with the value as its right-hand component. Passing a
// func makes the row actionable; without one it is a plain fact.
static void cloudSetupAddFact(GuiSettings* s, Window* window, const std::string& label, const std::string& value, const std::function<void()>& func = nullptr)
{
	auto theme = ThemeData::getMenuTheme();
	s->addWithLabel(label, std::make_shared<TextComponent>(window, value, theme->Text.font, theme->Text.color), false, func);
}

// The sign-in pages that own a waiting `cloud_oauth serve` (#308 8a gpt
// F-ES-17). A page handed on to the next one by cloudSetupPresent leaves
// the session to it; a page that closes any other way -- EXIT, back -- is
// the player leaving the sign-in, and the session is cancelled then rather
// than left listening until its own 900-second timeout. The interface
// thread only, like the pages.
static std::set<GuiSettings*> sOAuthSessionPages;

static void cloudOAuthOwnSession(GuiSettings* s)
{
	sOAuthSessionPages.insert(s);
	s->onFinalize([s]
	{
		if (sOAuthSessionPages.erase(s) == 0)
			return;   // handed on: the next page owns the session
		LOG(LogInfo) << "cloud_oauth: the sign-in was left; cancelling its session";
		Utils::Platform::runSystemCommand("/usr/bin/cloud_oauth cancel", "", nullptr);
	});
}

// Show a freshly built wizard page: give it the standard large-menu
// height (the cap MenuComponent::updateSize applies) so the window does
// not jump between steps, push it, then close the page it replaces -
// closing first would flash the menu underneath.
static void cloudSetupPresent(Window* window, GuiSettings* s, GuiSettings* prev)
{
	if (prev != nullptr)
		sOAuthSessionPages.erase(prev);   // handed on, not left
	if (!Renderer::ScreenSettings::fullScreenMenus())
	{
		float width = Renderer::getScreenWidth() * 0.90f;
		if (width > Renderer::getScreenHeight())
			width = (float)Renderer::getScreenHeight();
		s->getMenu().setSize(width, Renderer::getScreenHeight() * 0.75f);
	}
	window->pushGui(s);
	if (prev != nullptr)
		prev->close();
}

// Bottom button row for a wizard page: EXIT SETUP on the left and,
// once the step's check has passed, CONTINUE on the right - replacing
// the default BACK button so each page offers exactly those two ways
// out.
static void cloudSetupSetButtons(GuiSettings* s, const std::function<void()>& onContinue)
{
	s->getMenu().clearButtons();
	// One word each. A button bar is read at a glance and the page title
	// already says what is being exited, so "EXIT SETUP" spent width on a
	// word that carried nothing -- and the wider the buttons, the more two
	// of them read as a single control.
	s->getMenu().addButton(_("EXIT"), _("exit setup"), [s] { s->close(); });
	if (onContinue != nullptr)
		s->getMenu().addButton(_("CONTINUE"), _("continue"), onContinue);
}

// View or change the SSH password; onDone runs after the page closes.
static void cloudSetupOpenPasswordPage(Window* window, const std::string& current, const std::function<void()>& onDone)
{
	auto pw = new GuiSettings(window, _("SSH PASSWORD"));
	cloudSetupAddFact(pw, window, _("CURRENT PASSWORD"), current.empty() ? _("<NOT SET>") : current);
	pw->addInputTextConfigRow(_("CHANGE PASSWORD"), "root.password", false);
	pw->addSaveFunc([current]
	{
		// Apply only a real change; running setrootpass with an
		// empty value would clear the device password.
		const std::string changed = SystemConf::getInstance()->get("root.password");
		if (!changed.empty() && changed != current)
		{
			SystemConf::getInstance()->saveSystemConf();
			LOG(LogInfo) << "cloud_setup wizard: applying new device password";
			Utils::Platform::runSystemCommand("setrootpass " + Utils::String::shellQuote(changed), "", nullptr); // fork #198
		}
	});
	pw->onFinalize(onDone);
	window->pushGui(pw);
}

// Edit the cloud folder (SYNCPATH) the sync tools read from and write
// to, without touching config files by hand. onDone runs after a
// successful change.
static void cloudSetupOpenSyncPathEditor(Window* window, const std::string& current, const std::function<void()>& onDone)
{
	auto save = [window, onDone](const std::string& value)
	{
		const std::string trimmed = Utils::String::trim(value);
		if (trimmed.empty() || trimmed == "/")
			return;
		LOG(LogInfo) << "cloud_setup wizard: setting sync path to " << trimmed;
		// The script asks the provider whether it will take the folder and
		// refuses one it will not, saying why. That answer has to reach the
		// screen: with the exit code discarded, a refused folder left the
		// setting as it was while the page carried on as if it had changed
		// (the A3 fixture, 2026-09-06).
		// Line by line: GetShOutput glues lines together, which turned the
		// script's paragraphs into "thefolder" and "nameshave" (VM frame,
		// 2026-09-07). rclone's own log line -- the one starting with a date
		// -- is for the log, not the screen; the script's sentences are.
		//
		// Behind GuiLoading: asking the provider is an rclone listing with
		// no timeout of its own, and it ran in this callback on the
		// interface thread (fork #103). Thirty seconds is the box; when it
		// closes the exit code is timeout's 124, which reads as refused,
		// and the setting is left as it was.
		window->pushGui(new GuiLoading<std::pair<std::string, std::string>>(window, _("CHECKING..."),
			[trimmed](IGuiLoadingHandler*)
			{
				std::string why, rc;
				for (auto& line : Utils::Platform::GetShOutputLines(
					"timeout 30 /usr/bin/cloud_setup --set-syncpath " + Utils::String::shellQuote(trimmed) + " 2>&1; echo \"RC=$?\"")) // fork #198
				{
					const std::string l = Utils::String::trim(line);
					if (Utils::String::startsWith(l, "RC="))
						rc = l.substr(3);
					else if (l.size() > 10 && isdigit((unsigned char) l[0]) && l[4] == '/' && l[7] == '/')
						LOG(LogWarning) << "cloud_setup wizard: " << l;
					else if (!l.empty())
						why += (why.empty() ? "" : "\n") + l;
				}
				return std::make_pair(rc, why);
			},
			[window, onDone](std::pair<std::string, std::string> result)
			{
				const std::string& rc = result.first;
				const std::string& why = result.second;
				if (rc != "0")
				{
					LOG(LogWarning) << "cloud_setup wizard: the folder was refused: " << why;
					window->pushGui(new GuiMsgBox(window,
						_("THE CLOUD FOLDER WAS NOT CHANGED") + (why.empty() ? "" : "\n\n" + why), _("OK"), nullptr));
					return;
				}
				if (onDone != nullptr)
					onDone();
			}));
	};
	if (Settings::getInstance()->getBool("UseOSK"))
		window->pushGui(new GuiTextEditPopupKeyboard(window, _("CLOUD FOLDER"), current, save, false));
	else
		window->pushGui(new GuiTextEditPopup(window, _("CLOUD FOLDER"), current, save, false));
}

// Run `cloud_setup --check [remote]` and hand (exit code, remote name) to
// onResult on the UI thread. 0 = working, 1 = missing, 2 = unreachable.
// The only slow call in the wizard - a remote check can take up to the
// rclone timeout - so the only one behind the GuiLoading spinner.
static void cloudSetupRunCheck(Window* window, const std::string& remote, const std::function<void(int, const std::string&)>& onResult)
{
	std::string cmd = "/usr/bin/cloud_setup --check";
	if (!remote.empty())
		cmd += " " + remote;
	window->pushGui(new GuiLoading<std::pair<int, std::string>>(window, _("CHECKING..."),
		[cmd](auto gui)
		{
			std::string out = Utils::String::trim(Utils::Platform::GetShOutput(cmd + "; echo RC=$?"));
			int rc = 1;
			auto pos = out.rfind("RC=");
			if (pos != std::string::npos)
			{
				rc = atoi(out.substr(pos + 3).c_str());
				out = Utils::String::trim(out.substr(0, pos));
			}
			std::string name = out.find(' ') != std::string::npos ? out.substr(out.find(' ') + 1) : "";
			LOG(LogInfo) << "cloud_setup wizard: --check rc=" << rc << " remote=" << name;
			return std::pair<int, std::string>(rc, name);
		},
		[onResult](std::pair<int, std::string> result)
		{
			onResult(result.first, result.second);
		}));
}

// Step 1 of 3: SSH must be reachable - a device password and a running
// SSH service, checked together on one page. Each actionable row is
// marked with its state; CONTINUE appears once both checks pass.
static void cloudSetupShowSshStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev)
{
	auto info = cloudSetupInfo();
	if (info["IP"].empty())
	{
		window->pushGui(new GuiMsgBox(window, _("NO NETWORK CONNECTION DETECTED.\n\nCONNECT TO A NETWORK FIRST, THEN TRY AGAIN.")));
		return;
	}

	// cloud_setup --info says whether a password is set, never what it is:
	// a credential that crosses a script boundary is one careless echo from
	// a log (#116, D-INFRA-010). The value, where this page must show or
	// pre-fill it, is read in-process from the device's own settings.
	const bool pwOk = info["PASSWORD_SET"] == "1";
	const bool sshOk = info["SSH_UP"] == "active";
	LOG(LogInfo) << "cloud_setup wizard: step 1 ssh setup, password=" << pwOk << " sshd=" << info["SSH_UP"];

	auto s = new GuiSettings(window, cloudSetupTitle(mode));
	s->setSubTitle(_("STEP 1 OF 3 - SET UP SSH"));

	const std::string current = SystemConf::getInstance()->get("root.password");
	s->addEntry((pwOk ? _U("\uF058  ") : _U("\uF071  ")) + _("SET SSH PASSWORD"), true, [window, s, mode, remote, preexisting, current]
	{
		cloudSetupOpenPasswordPage(window, current, [window, s, mode, remote, preexisting]
		{
			cloudSetupShowSshStep(window, mode, remote, preexisting, s);
		});
	});

	if (sshOk)
		cloudSetupAddInfoRow(s, window, _U("\uF058  ") + _("SSH SERVICE IS ENABLED"));
	else
		s->addEntry(_U("\uF071  ") + _("ENABLE SSH SERVICE"), true, [window, s, mode, remote, preexisting]
		{
			LOG(LogInfo) << "cloud_setup wizard: enabling sshd";
			Utils::Platform::runSystemCommand("mkdir -p /storage/.cache/services/", "", nullptr);
			Utils::Platform::runSystemCommand("touch /storage/.cache/services/sshd.conf", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl enable sshd", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl start sshd", "", nullptr);
			SystemConf::getInstance()->set("ssh.enabled", "1");
			SystemConf::getInstance()->saveSystemConf();
			cloudSetupShowSshStep(window, mode, remote, preexisting, s);
		});

	std::function<void()> onContinue = nullptr;
	if (pwOk && sshOk)
		onContinue = [window, s, mode, remote, preexisting]
		{
			cloudSetupShowConnectStep(window, mode, remote, preexisting, s);
		};
	cloudSetupSetButtons(s, onContinue);

	cloudSetupPresent(window, s, prev);
}

// Step 2 of 3: connect from the computer; CONTINUE passes only when
// --connected sees an established inbound SSH session.
static void cloudSetupShowConnectStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev)
{
	auto info = cloudSetupInfo();
	LOG(LogInfo) << "cloud_setup wizard: step 2 connect, override=" << info["OVERRIDE"];

	auto s = new GuiSettings(window, cloudSetupTitle(mode));
	s->setSubTitle(_("STEP 2 OF 3 - CONNECT FROM YOUR COMPUTER"));

	s->addGroup(_("HOW TO CONNECT"));
	cloudSetupAddProse(s, window, _("ON YOUR COMPUTER"),
		_("OPEN A TERMINAL ON A COMPUTER ON THE SAME NETWORK, RUN THE COMMAND BELOW, AND ENTER THE PASSWORD WHEN ASKED."));
	cloudSetupAddInfoRow(s, window, info["SSH_CMD"], true);

	cloudSetupAddSpacer(s, window);
	s->addGroup(_("CONNECTION DETAILS"));
	// Read in-process, not from the script (#116): this row exists so the
	// player can type the password into their computer's ssh prompt.
	const std::string current = SystemConf::getInstance()->get("root.password");
	cloudSetupAddFact(s, window, _("CURRENT PASSWORD"), current, [window, s, mode, remote, preexisting, current]
	{
		cloudSetupOpenPasswordPage(window, current, [window, s, mode, remote, preexisting]
		{
			cloudSetupShowConnectStep(window, mode, remote, preexisting, s);
		});
	});
	// With a connection override the direct address is not the one to
	// use, so showing it would only mislead.
	if (info["OVERRIDE"] != "1")
		cloudSetupAddFact(s, window, _("DEVICE IP"), info["IP"]);

	cloudSetupSetButtons(s, [window, s, mode, remote, preexisting]
	{
		bool connected = Utils::String::trim(Utils::Platform::GetShOutput("/usr/bin/cloud_setup --connected")) == "CONNECTED";
		LOG(LogInfo) << "cloud_setup wizard: --connected=" << connected;
		if (connected)
		{
			cloudSetupShowConfigureStep(window, mode, remote, preexisting, s);
			return;
		}
		window->pushGui(new GuiMsgBox(window,
			_("THERE IS NO ACTIVE SSH CONNECTION TO THE DEVICE.\n\nTRY CONNECTING FROM YOUR COMPUTER AGAIN, LEAVE THE TERMINAL OPEN, AND THEN SELECT 'CONTINUE'."),
			_("OK"), nullptr,
			_("EXIT"), [s] { s->close(); }));
	});

	cloudSetupPresent(window, s, prev);
}

// Gate for the configure step: advance to the done step only when the
// checked remote verifies; otherwise explain and stay (or exit).
static void cloudSetupGateCheck(Window* window, GuiSettings* s, const std::string& remote)
{
	cloudSetupRunCheck(window, remote, [window, s](int rc, const std::string& name)
	{
		if (rc == 0)
		{
			cloudSetupShowDoneStep(window, name, s);
		}
		else if (rc == 2)
			window->pushGui(new GuiMsgBox(window,
				_("THIS CONNECTION ISN'T ANSWERING:") + " " + cloudSetupDisplayName(name) + "\n\n" + _("ITS SIGN-IN MAY BE INCOMPLETE OR EXPIRED. IN THE TERMINAL, RUN:") + "\n'rclone config reconnect " + name + "'",
				_("OK"), nullptr,
				_("EXIT"), [s] { s->close(); }));
		else
			window->pushGui(new GuiMsgBox(window,
				_("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.\n\nFINISH 'RCLONE CONFIG' IN THE TERMINAL, THEN SELECT 'CONTINUE' AGAIN."),
				_("OK"), nullptr,
				_("EXIT"), [s] { s->close(); }));
	});
}

// Step 3 of 3: what to do inside the SSH session, as short numbered
// steps branched by scenario; CONTINUE passes only when the remote
// verifies via --check. The auto-config sign-in must go through the
// step 2 tunnel - `rclone authorize` on the computer cannot bind its
// port while that tunnel is open, so the instructions forbid it, and a
// stale sign-in webserver on the device gets a clear-it action.
static void cloudSetupShowConfigureStep(Window* window, CloudSetupMode mode, const std::string& remote, const std::string& preexisting, GuiSettings* prev)
{
	auto info = cloudSetupInfo();
	LOG(LogInfo) << "cloud_setup wizard: step 3 configure, mode=" << (int)mode << " remote=" << remote << " auth_port=" << info["AUTH_PORT"];

	auto s = new GuiSettings(window, cloudSetupTitle(mode));

	if (mode == CloudSetupMode::RepairRemote)
	{
		s->setSubTitle(_("STEP 3 OF 3 - REPAIR THE CONNECTION"));
		s->addGroup(_("IN THE TERMINAL"));
		cloudSetupAddInfoRow(s, window, _("1. RUN 'rclone config reconnect ") + remote + "'");
		cloudSetupAddInfoRow(s, window, _("2. THAT RENEWS AN EXPIRED SIGN-IN."));
		cloudSetupAddInfoRow(s, window, _("3. FOR OTHER OPTIONS: RUN 'rclone config', PRESS 'E' TO EDIT."));
		cloudSetupAddInfoRow(s, window, _("4. PRESS 'Q' TO QUIT WHEN YOU ARE DONE."));
	}
	else
	{
		s->setSubTitle(_("STEP 3 OF 3 - CREATE THE CONNECTION"));
		s->addGroup(_("IN THE TERMINAL"));
		// Step 5 is the one people get wrong: rclone's own prompt says to
		// answer N on a machine with no browser, which describes this
		// handheld exactly. Answering N is fatal here.
		// Steps 6-7 describe what rclone actually does. There is no browser
		// on the device, so it always takes its "Failed to open browser
		// automatically ... please go to the following link" branch and
		// prints the URL to the terminal. Saying "the link in your browser"
		// implied one opens by itself and left people with nothing to click.
		cloudSetupAddInfoRow(s, window, _("1. RUN 'rclone config'"));
		cloudSetupAddInfoRow(s, window, _("2. PRESS 'N' FOR A NEW REMOTE (RCLONE'S WORD FOR A CONNECTION)"));
		cloudSetupAddInfoRow(s, window, _("3. NAME IT, PICK YOUR PROVIDER, TAKE DEFAULTS"));
		cloudSetupAddInfoRow(s, window, _("4. AT 'USE AUTO CONFIG?' PRESS 'Y'"));
		cloudSetupAddInfoRow(s, window, _("5. 'FAILED TO OPEN BROWSER' IS NORMAL HERE"));
		cloudSetupAddInfoRow(s, window, _("6. COPY THE LINK IT PRINTS INTO YOUR BROWSER"));
		cloudSetupAddInfoRow(s, window, _("7. SIGN IN, THEN PRESS 'Q' WHEN THE REMOTE IS LISTED"));
		// Kept short on purpose: this page has no selectable rows, so
		// ComponentList cannot scroll it (the camera follows the cursor,
		// and the cursor skips unselectable rows). Anything past the fold
		// is unreachable, so the text has to fit. Steps 5 and 6 already
		// say to answer 'Y' and sign in; the only new fact is the
		// 'rclone authorize' trap.
		s->addGroup(_("IMPORTANT"));
		cloudSetupAddProse(s, window, _("DO NOT USE 'rclone authorize'"),
			_("IT CANNOT BIND ITS PORT WHILE STEP 2'S CONNECTION IS OPEN."));
		if (mode == CloudSetupMode::AddRemote)
			cloudSetupAddInfoRow(s, window, _("NOTE: CLOUD SYNC USES THE FIRST REMOTE IN ALPHABETICAL ORDER."));
	}

	if (info["AUTH_PORT"] == "busy")
		s->addEntry(_U("\uF071  ") + _("A STALE SIGN-IN IS BLOCKING THE PORT - PRESS TO CLEAR IT"), true, [window, s, mode, remote, preexisting]
		{
			LOG(LogInfo) << "cloud_setup wizard: freeing stale auth port";
			Utils::Platform::runSystemCommand("/usr/bin/cloud_setup --free-auth-port", "", nullptr);
			cloudSetupShowConfigureStep(window, mode, remote, preexisting, s);
		});

	cloudSetupSetButtons(s, [window, s, mode, remote, preexisting]
	{
		if (mode == CloudSetupMode::AddRemote)
		{
			// The new remote is whichever name was not configured when the
			// wizard started - gate on that one, not the existing first.
			std::string newRemote;
			for (auto& name : Utils::String::split(cloudSetupInfo()["REMOTES"], ' '))
			{
				auto n = Utils::String::trim(name);
				if (!n.empty() && (" " + preexisting + " ").find(" " + n + " ") == std::string::npos)
					newRemote = n;
			}
			LOG(LogInfo) << "cloud_setup wizard: add-mode new remote=" << newRemote;
			if (newRemote.empty())
			{
				window->pushGui(new GuiMsgBox(window,
					_("NO NEW CONNECTION WAS ADDED YET.\n\nFINISH 'RCLONE CONFIG' IN THE TERMINAL, THEN SELECT 'CONTINUE' AGAIN."),
					_("OK"), nullptr,
					_("EXIT"), [s] { s->close(); }));
				return;
			}
			cloudSetupGateCheck(window, s, newRemote);
			return;
		}
		cloudSetupGateCheck(window, s, mode == CloudSetupMode::RepairRemote ? remote : "");
	});

	cloudSetupPresent(window, s, prev);
}

// Completion page: confirmation, the cloud-folder setting, and a
// clearly-optional immediate backup. `seeded` is what --seed-folders
// printed; cloudSetupShowDoneStep ran it.
static void cloudSetupBuildDoneStep(Window* window, const std::string& remote, GuiSettings* prev, const std::vector<std::string>& seeded)
{
	auto info = cloudSetupInfo();
	LOG(LogInfo) << "cloud_setup wizard: complete, remote=" << remote << " saves_remote=" << info["SAVES_REMOTE"];

	auto s = new GuiSettings(window, _("CLOUD SETUP COMPLETE"));
	s->setSubTitle(_("YOUR CLOUD STORAGE IS READY"));

	cloudSetupAddInfoRow(s, window, _U("\uF058  ") + _("YOUR CLOUD IS ANSWERING:") + " " + cloudSetupDisplayName(remote));
	cloudSetupAddInfoRow(s, window, _("CLOUD SETTINGS ARE NOW AVAILABLE IN GAME SETTINGS."));
	cloudSetupAddInfoRow(s, window, _("YOU CAN CLOSE THE TERMINAL ON YOUR COMPUTER."));

	// A new remote is an empty folder, and nothing on it says where anything
	// goes. Somebody setting up a first handheld wants to seed their library
	// from a computer before restoring it here -- and until the first upload
	// there is nothing to seed into, so they would have to guess our folder
	// names and the per-system convention underneath them.
	//
	// The wizard creates them and says so. Not offered as a choice: asking
	// whether to make four folders makes the player form an opinion about our
	// internals. Not silent either, because this writes to somebody's cloud
	// account, and that is exactly the kind of thing to say out loud.
	//
	// Reported from what the seeding found afterwards rather than from the fact
	// that it ran: on a bucket-based remote an mkdir can succeed and leave
	// nothing behind.
	s->addGroup(_("YOUR CLOUD FOLDERS"));
	// Said before the list, because the list is the result. Writing into
	// somebody's cloud account without saying so is the half of this that
	// should never be silent -- the choice is ours to make, the fact is
	// theirs to know.
	//
	// And reported honestly: a folder the seeding could not confirm is
	// listed as MISSING, and when none was confirmed at all -- the remote
	// went away between the check and this page, or the ninety seconds ran
	// out -- the group says so instead of pointing at an empty list. The
	// first backup creates whatever is missing; nothing is lost (#105).
	bool anyOk = false;
	for (auto& line : seeded)
		if (Utils::String::trim(line).rfind("OK ", 0) == 0)
			anyOk = true;
	if (!anyOk)
		cloudSetupAddInfoRow(s, window, _("YOUR CLOUD FOLDERS COULDN'T BE SET UP YET. THEY ARE CREATED AT YOUR FIRST BACKUP."));
	else
	{
		cloudSetupAddInfoRow(s, window, _("SET UP FOR YOU IN YOUR CLOUD ACCOUNT, IF NOT ALREADY THERE:"));
		for (auto& line : seeded)
		{
			auto text = Utils::String::trim(line);
			if (text.rfind("OK ", 0) == 0)
				cloudSetupAddInfoRow(s, window, _U("\uF07B  ") + text.substr(3));
			else if (text.rfind("MISSING ", 0) == 0)
				cloudSetupAddInfoRow(s, window, _U("\uF071  ") + _("MISSING") + " " + text.substr(8));
		}
		cloudSetupAddInfoRow(s, window, _("PUT ROMS AND BIOS FILES IN THESE FROM A COMPUTER, THEN RESTORE THEM HERE."));
	}

	s->addGroup(_("OPTIONAL NEXT STEPS"));
	const std::string syncpath = info["SAVES_REMOTE"];
	cloudSetupAddFact(s, window, _("CLOUD FOLDER"), syncpath, [window, s, remote, syncpath]
	{
		cloudSetupOpenSyncPathEditor(window, syncpath, [window, s, remote]
		{
			cloudSetupShowDoneStep(window, remote, s);
		});
	});
	s->addEntry(_("BACK UP SETTINGS AND SAVES NOW"), true, [window, s]
	{
		window->pushGui(new GuiMsgBox(window, _("BACK UP SETTINGS AND SAVES TO THE CLOUD?\n\nGAME SAVES, SAVE STATES, AND SCREENSHOTS ARE INCLUDED. ROMS AND BIOS FILES ARE NOT."), _("YES"),
			[window, s]
			{
				s->close();
				ThreadedCloudSync::start(window, "/usr/bin/backuptool backup >/dev/null 2>&1 && /usr/bin/cloud_backup --yes && /usr/bin/cloud_backup --yes --system-only", _("BACK UP SETTINGS AND SAVES"), "", ThreadedCloudSync::Origin::None);
			}, _("NO"), nullptr));
	});

	s->getMenu().clearButtons();
	s->getMenu().addButton(_("FINISH"), _("finish"), [s] { s->close(); });

	cloudSetupPresent(window, s, prev);
}

// Seed the folders first, behind a spinner, then build the page from what
// the seeding said. --seed-folders is up to two dozen rclone calls against
// the remote -- a mkdir, a listing and a README per folder -- and this page
// used to run them while it was being built, on the interface thread, with
// rclone's default timeouts as the only bound: on a link that had just
// dropped, a screen that stopped drawing at the moment the wizard said it
// was done (fork #103). Ninety seconds is the box; a run that outlives it
// reports the folders it did manage to create. prev stays under the
// spinner untouched, so the page built afterwards can still replace it.
static void cloudSetupShowDoneStep(Window* window, const std::string& remote, GuiSettings* prev)
{
	window->pushGui(new GuiLoading<std::vector<std::string>>(window, _("SETTING UP YOUR CLOUD FOLDERS"),
		[](IGuiLoadingHandler*)
		{
			return ApiSystem::executeScriptLegacy("timeout 90 /usr/bin/cloud_setup --seed-folders");
		},
		[window, remote, prev](std::vector<std::string> seeded)
		{
			cloudSetupBuildDoneStep(window, remote, prev, seeded);
		}));
}

// Wizard entry point: network is a hard precondition; then branch on the
// scenario - no remotes yet goes straight into the first-remote wizard,
// otherwise offer check / add / repair-or-modify for the existing ones.
// ---------------------------------------------------------------------------
// Add a cloud remote without rclone's interactive configuration.
//
// `rclone config` asks its questions in a terminal, which on a handheld means
// an SSH session from another computer -- a flow that fails in ways the player
// cannot act on, and that needs a machine able to reach the device over the
// LAN. `cloud_remote` asks rclone what a backend needs, we ask the player here,
// and it writes the remote directly. See fork issue #51.
//
// Only backends that need no browser are offered. The rest keep the SSH
// wizard, listed separately so nobody discovers three screens in that they
// need an account somewhere else first.
// ---------------------------------------------------------------------------

struct CloudBackend
{
	std::string tier;         // keys | oauth | fallback | plumbing
	std::string name;         // rclone's backend id, e.g. "sftp"
	std::string label;        // human name, e.g. "SSH/SFTP Connection"
	std::string subprovider;  // vendor within the backend, where it has one
};

struct CloudBackendField
{
	std::string name;
	bool required = false;
	bool password = false;
	bool sensitive = false;
	std::string type;
	std::string deflt;
	std::string help;
	int choices = 0;
};

// Backends worth putting in front of someone who has not decided yet. rclone
// carries 69, most of which are plumbing (alias, chunker, combine, hasher,
// memory) that is meaningless as a "cloud provider" choice.
//
// Each carries a short name of our own. rclone's descriptions are written for
// its documentation, so "Amazon S3 Compliant Storage Providers including AWS,
// Alibaba, ArvanCloud..." is accurate and useless on a handheld -- it is
// truncated mid-word in a menu and tells the player nothing the first three
// characters did not.
// The table and the label it gives a provider live in CloudText, which is
// where the pure cloud text is tested from (es-app/tests/unit): translated
// at the point of use, since _() at static-init time would run before the
// locale is loaded.

static std::vector<std::string> cloudRemoteLines(const std::string& args)
{
	// GetShOutputLines, not GetShOutput: the latter drops the last character
	// of every chunk it reads, so multi-line output arrives as one
	// concatenated string. Parsing that yields a single nonsense record
	// rather than an error, which is how this first shipped -- the provider
	// list rendered empty with nothing logged.
	std::vector<std::string> lines;
	for (auto& line : Utils::Platform::GetShOutputLines("/usr/bin/cloud_remote " + args))
		if (!Utils::String::trim(line).empty())
			lines.push_back(line);
	return lines;
}

// Split a TSV record, keeping empty fields. removeEmptyEntries would drop
// them, and most rows have an empty column somewhere -- a backend option with
// no default, or no help text -- which shifts every later field left and
// silently discards the row for being too short.
static std::string cloudTsvField(const std::vector<std::string>& cols, size_t i)
{
	return i < cols.size() ? cols[i] : std::string();
}

static std::vector<CloudBackend> cloudRemoteBackends()
{
	std::vector<CloudBackend> list;
	for (auto& line : cloudRemoteLines("providers"))
	{
		auto cols = Utils::String::splitAny(line, "\t", false);
		if (cols.size() < 3)
			continue;
		list.push_back({ cols[0], cols[1], cols[2], std::string() });
	}
	return list;
}

// Shell-quote a value on its way to cloud_remote. Passwords and keys routinely
// contain characters the shell would otherwise act on, and a mangled secret
// fails in a way that looks like the provider rejecting the credential.
static std::string cloudShellQuote(const std::string& value)
{
	std::string quoted = "'";
	for (char c : value)
	{
		if (c == '\'')
			quoted += "'\\''";
		else
			quoted += c;
	}
	return quoted + "'";
}

// Open an editor that reads the value from `values` rather than the copy
// addInputTextRow captured when the row was built. Without this the second
// edit of a field starts from the original value again.
//
// The obvious alternative -- rebuild the page after each edit -- is unsafe
// here: GuiSettings::close() is `delete this`, and the callback doing the
// rebuild is owned by the page being deleted.
static std::function<void(Window*, std::string, std::string,
	const std::function<void(std::string)>&)>
cloudRemoteEditor(std::shared_ptr<std::map<std::string, std::string>> values,
	const std::string& key)
{
	return [values, key](Window* w, std::string title, std::string /*stale*/,
		const std::function<void(std::string)>& onsave)
	{
		std::string current = values->count(key) ? (*values)[key] : "";
		if (Settings::getInstance()->getBool("UseOSK"))
			w->pushGui(new GuiTextEditPopupKeyboard(w, title, current, onsave, false));
		else
			w->pushGui(new GuiTextEditPopup(w, title, current, onsave, false));
	};
}

// One provider's questions.
static void cloudRemoteShowForm(Window* window, const CloudBackend& backend,
	std::shared_ptr<std::map<std::string, std::string>> values, GuiSettings* prev)
{
	auto s = new GuiSettings(window, _("CONNECT CLOUD STORAGE"));
	s->setSubTitle(CloudText::providerSubtitle(backend.name, backend.label));

	std::vector<CloudBackendField> fields;
	// The backend id is quoted: rclone has ids with spaces in them
	// ("google cloud storage", "google photos"), which would otherwise split
	// into extra arguments.
	std::string fieldsArgs = "fields " + cloudShellQuote(backend.name);
	if (!backend.subprovider.empty())
		fieldsArgs += " " + cloudShellQuote(backend.subprovider);
	for (auto& line : cloudRemoteLines(fieldsArgs))
	{
		auto cols = Utils::String::splitAny(line, "\t", false);
		if (cols.size() < 5 || cols[0].empty())
			continue;
		CloudBackendField f;
		f.name      = cloudTsvField(cols, 0);
		f.required  = cloudTsvField(cols, 1) == "1";
		f.password  = cloudTsvField(cols, 2) == "1";
		f.sensitive = cloudTsvField(cols, 3) == "1";
		f.type      = cloudTsvField(cols, 4);
		f.deflt     = cloudTsvField(cols, 5);
		f.help      = cloudTsvField(cols, 6);
		f.choices   = atoi(cloudTsvField(cols, 7).c_str());
		fields.push_back(f);
	}

	if (values->find("__name__") == values->end())
		(*values)["__name__"] = backend.name;

	s->addGroup(_("NAME"));
	s->addInputTextRow(_("CONNECTION NAME"), (*values)["__name__"], false,
		cloudRemoteEditor(values, "__name__"),
		[values](const std::string& newVal) { (*values)["__name__"] = newVal; });

	// Required first: on a small panel the fold is close, and a required field
	// below it reads as absent rather than as further down.
	for (int pass = 0; pass < 2; pass++)
	{
		bool wantRequired = (pass == 0);
		bool headed = false;
		for (auto& f : fields)
		{
			if (f.required != wantRequired)
				continue;
			if (!headed)
			{
				s->addGroup(wantRequired ? _("REQUIRED") : _("OPTIONAL"));
				headed = true;
			}
			// Mask anything the backend calls a password or marks sensitive.
			// rclone's two flags do not agree on what a credential is -- it
			// marks sftp's host sensitive, and s3's secret_access_key
			// sensitive but not a password -- so masking either flag is the
			// only rule that never leaves a key on screen. Opening the row
			// still shows the value, which is the reveal.
			bool mask = f.password || f.sensitive;
			if (!values->count(f.name) && !f.deflt.empty())
				(*values)[f.name] = f.deflt;
			std::string current = values->count(f.name) ? (*values)[f.name] : "";
			// The rclone name is the key the value is stored under; the row
			// shows CloudText::fieldLabel's words for it (#123).
			std::string fieldName = f.name;

			// Match the widget to the option. Everything was a text box before,
			// which meant typing "true" for a switch, and guessing that the
			// answer for a generic WebDAV server is the literal string "other".
			if (f.type == "bool")
			{
				auto sw = std::make_shared<SwitchComponent>(window);
				sw->setState(current == "true");
				s->addWithLabel(CloudText::fieldLabel(fieldName), sw);
				sw->setOnChangedCallback([values, fieldName, sw]
				{
					(*values)[fieldName] = sw->getState() ? "true" : "false";
				});
				continue;
			}

			// Only offer a list when it is short enough to scroll on a handheld.
			// s3's endpoint carries 351 suggestions and its region 150; those are
			// effectively free-form and stay as text.
			if (f.choices > 0 && f.choices <= 12)
			{
				std::string choicesArgs = "choices " + cloudShellQuote(backend.name)
					+ " " + cloudShellQuote(fieldName);
				if (!backend.subprovider.empty())
					choicesArgs += " " + cloudShellQuote(backend.subprovider);

				auto list = std::make_shared<OptionListComponent<std::string>>(
					window, CloudText::fieldLabel(fieldName), false);
				bool any = false;
				for (auto& line : cloudRemoteLines(choicesArgs))
				{
					auto cols = Utils::String::splitAny(line, "\t", false);
					if (cols.empty() || cols[0].empty())
						continue;
					std::string value = cols[0];
					std::string label = cloudTsvField(cols, 1).empty() ? value : cols[1];
					list->add(label, value, current == value);
					any = true;
				}
				if (any)
				{
					// Seed the map with whatever the list starts on, and keep
					// it current as the choice changes.
					//
					// NOT addSaveFunc: those run from GuiSettings::save(),
					// which happens when the page closes. The CONNECT row builds
					// its command while the page is still open, so a value
					// left to save() never reaches it -- the chosen vendor was
					// dropped, silently and with a success dialog, because
					// WebDAV tolerates an empty one. On Sharepoint or
					// Nextcloud that is the wrong dialect, not a no-op.
					// getSelected() asserts that exactly one entry is
					// selected and abort()s otherwise, and nothing is
					// selected when the option has no default and so no
					// entry matched. getSelectedIndex() returns -1 instead.
					// Leaving it unset is right: we should not invent a
					// vendor the player did not choose.
					if (list->getSelectedIndex() >= 0)
						(*values)[fieldName] = list->getSelected();
					list->setSelectedChangedCallback(
						[values, fieldName](const std::string& picked)
						{
							(*values)[fieldName] = picked;
						});
					s->addWithLabel(CloudText::fieldLabel(fieldName), list);
					continue;
				}
			}

			s->addInputTextRow(CloudText::fieldLabel(fieldName), current, mask,
				cloudRemoteEditor(values, fieldName),
				[values, fieldName](const std::string& newVal)
				{
					(*values)[fieldName] = newVal;
				});
		}
	}

	s->addGroup(_("FINISH"));
	s->addEntry(_("CONNECT"), true, [window, backend, values, s]
	{
		std::string name = Utils::String::trim((*values)["__name__"]);
		if (name.empty())
		{
			window->pushGui(new GuiMsgBox(window,
				_("GIVE THE CONNECTION A NAME FIRST."), _("OK"), nullptr));
			return;
		}

		std::string cmd = "create " + cloudShellQuote(name) + " "
			+ cloudShellQuote(backend.name);
		// The sub-provider is a real config value, not just a filter.
		if (!backend.subprovider.empty())
			cmd += " " + cloudShellQuote("provider=" + backend.subprovider);
		for (auto& kv : *values)
		{
			if (kv.first == "__name__" || Utils::String::trim(kv.second).empty())
				continue;
			cmd += " " + cloudShellQuote(kv.first + "=" + kv.second);
		}

		window->pushGui(new GuiMsgBox(window,
			_("CONNECT TO THIS PROVIDER NOW?\n\nTHE CONNECTION IS ONLY SAVED IF IT ANSWERS."),
			_("YES"), [window, cmd, s]
			{
				// cloud_remote verifies the remote -- an rclone listing it
				// bounds at 10 s to connect and 20 s to answer -- and removes
				// it again if it cannot be reached, so a failure here leaves
				// nothing behind. Behind GuiLoading, because that round trip
				// ran in this callback on the interface thread (fork #103);
				// the outer box is for an rclone that outlives its own flags.
				window->pushGui(new GuiLoading<std::string>(window, _("CONNECTING..."),
					[cmd](IGuiLoadingHandler*)
					{
						std::string out = Utils::Platform::GetShOutput(
							"timeout 60 /usr/bin/cloud_remote " + cmd + " 2>&1");
						// Masked: the command carries the form's pass=, key= and
						// token= values, and a script that echoes its arguments
						// in a usage or an error would put them in es_log.txt
						// (#308 8-es claude F-ES-21; fork #177's rule, every
						// logged command line).
						LOG(LogInfo) << "cloud_remote create: " << Utils::String::maskSecrets(out);
						return out;
					},
					[window, s](std::string out)
					{
						if (out.find("OK=") != std::string::npos)
						{
							window->pushGui(new GuiMsgBox(window,
								_("YOUR CLOUD IS ANSWERING.\n\nYOU CAN NOW SYNC, BACK UP, AND RESTORE YOUR SAVES FROM GAME SETTINGS."),
								_("OK"), [s] { s->close(); }));
							return;
						}
						// Show what actually went wrong. The player can fix a
						// typo in a key far more easily than they can act on
						// "it failed".
						window->pushGui(new GuiMsgBox(window,
							_("THE CONNECTION COULDN'T BE SAVED.") + std::string("\n\n") +
								Utils::String::trim(out),
							_("OK"), nullptr));
					}));
			},
			_("NO"), nullptr));
	});

	// prev->close(), never delete: the window owns it and is mid-input on it.
	cloudSetupPresent(window, s, prev);
}

// Several backends ask which vendor you are using before their other
// options mean anything: koofr carries three separate options all named
// `password` (Koofr, Digi Storage, other) and s3 has 51 vendors with
// options like ibm_api_key that are noise unless you picked IBMCOS.
// Asking first keeps the form to the questions that apply.
static void cloudRemoteChooseBackend(Window* window, const CloudBackend& backend)
{
	std::vector<std::pair<std::string, std::string>> subs;
	for (auto& line : cloudRemoteLines("subproviders " + cloudShellQuote(backend.name)))
	{
		auto cols = Utils::String::splitAny(line, "\t", false);
		if (cols.size() < 1 || cols[0].empty())
			continue;
		subs.push_back({ cols[0], cloudTsvField(cols, 1).empty() ? cols[0] : cols[1] });
	}

	if (subs.empty())
	{
		auto values = std::make_shared<std::map<std::string, std::string>>();
		cloudRemoteShowForm(window, backend, values, nullptr);
		return;
	}

	auto s = new GuiSettings(window, _("CONNECT CLOUD STORAGE"));
	s->setSubTitle(_("WHICH SERVICE?"));
	for (auto& sub : subs)
	{
		CloudBackend chosen = backend;
		chosen.subprovider = sub.first;
		s->addEntry(Utils::String::toUpper(sub.second), true, [window, chosen]
		{
			auto values = std::make_shared<std::map<std::string, std::string>>();
			cloudRemoteShowForm(window, chosen, values, nullptr);
		});
	}
	cloudSetupPresent(window, s, nullptr);
}

// Sign in to a provider using a phone, with nothing else involved.
//
// cloud_oauth starts rclone's own authorize flow, reads the provider's real
// sign-in URL out of it, and serves that on the LAN behind a PIN. All this
// page does is start it, show where to go, and ask rclone afterwards whether
// it worked -- the same shape as the SSH wizard's CONTINUE gate, which exists
// because a page that assumes success is how a failed setup gets reported as
// a working one.
struct CloudOAuthReady
{
	bool started = false;
	bool onDevice = false;
	std::string url;
	std::string exitHint = "SELECT + START";
};

static void cloudOAuthShowSignIn(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev, bool usePhone,
	const CloudOAuthReady& ready);
static void cloudOAuthPresentChoice(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev,
	const CloudOAuthReady& ready);
static void cloudOAuthShowConnected(Window* window, const CloudBackend& backend,
	GuiSettings* prev);
static void cloudOAuthChooseInput(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev);

static void cloudOAuthStart(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev)
{
	// Clear the previous attempt's state first, and wait for it: a finished
	// session leaves its address and PIN behind, and the poll below would
	// read those and put a dead code on the screen. Doing it here rather
	// than inside the detached launch means it has already happened before
	// the first poll, so there is no window where the old one is readable.
	Utils::Platform::runSystemCommand("/usr/bin/cloud_oauth cancel", "", nullptr);

	// Detached, because it stays up until someone signs in or it times out.
	// Its own output carries the URL and PIN, which `cloud_oauth info` reads
	// back rather than this parsing them out of a launch.
	// --label carries the name we already show in our own menu, so the page
	// on the phone says "Connect Dropbox" rather than "Connect dropbox".
	// rclone's backend names are lowercase identifiers, and a page headed
	// with one reads like a typo on the screen where somebody is deciding
	// whether to trust us with their storage.
	std::string cmd = "setsid sh -c " + cloudShellQuote(
		"/usr/bin/cloud_oauth serve " + cloudShellQuote(backend.name)
		+ " --name " + cloudShellQuote(remoteName)
		+ " --label " + cloudShellQuote(backend.label)
		+ " --timeout 900 >/dev/null 2>&1") + " </dev/null >/dev/null 2>&1 &";
	Utils::Platform::runSystemCommand(cmd, "", nullptr);

	cloudOAuthChooseInput(window, backend, remoteName, prev);
}

// Wait for the listener to say it is *waiting*, not merely for an address to
// appear: a URL on its own proves nothing about whose session it belongs to,
// which is how a previous attempt's PIN once reached the screen.
static bool cloudOAuthAwaitSession(std::string& url, bool& onDevice,
	std::string& exitHint)
{
	url.clear();
	onDevice = false;
	// A sane default for an image whose cloud_oauth predates EXIT_HINT.
	// Select exists on every pad we ship for; Mode does not.
	exitHint = "SELECT + START";
	bool understood = true;

	// Sixty rather than twenty-four: this runs on a worker thread now, so
	// patience costs a spinner rather than a frozen screen, and a second
	// attempt after a cancel was timing out at the old limit.
	for (int attempt = 0; attempt < 60 && understood; attempt++)
	{
		std::string status;
		url.clear();

		auto reported = Utils::Platform::GetShOutputLines("/usr/bin/cloud_oauth info");
		// An older script answers `info` with its usage on stderr and nothing
		// on stdout. Fall through to `url` at once rather than spending the
		// whole poll waiting for a command that will never answer.
		understood = !reported.empty();

		for (auto& line : reported)
		{
			const std::string trimmed = Utils::String::trim(line);
			if (Utils::String::startsWith(trimmed, "URL="))
				url = trimmed.substr(4);
			else if (Utils::String::startsWith(trimmed, "STATUS="))
				status = trimmed.substr(7);
			else if (Utils::String::startsWith(trimmed, "ON_DEVICE="))
				onDevice = trimmed.substr(10) == "yes";
			// Which two buttons leave differs by handheld, so the device is
			// asked rather than the string being written here.
			else if (Utils::String::startsWith(trimmed, "EXIT_HINT="))
			{
				const std::string reported = Utils::String::trim(trimmed.substr(10));
				if (!reported.empty())
					exitHint = reported;
			}
		}

		if (status == "waiting" && !url.empty())
			return true;

		url.clear();
		// A sleep, not a shell running sleep (#308 8-es claude F-ES-16): a
		// fork and exec every poll for nothing, and runSystemCommand is not
		// a promise to wait for its child.
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	// A script that answered `info` and never said waiting has had its say:
	// failed, or not yet, for the whole poll. The address `url` still holds
	// then is not this session's (#308 8a gpt F-ES-18) -- the case the wait
	// above exists to rule out. Fail closed.
	if (understood)
		return false;

	// An image whose cloud_oauth predates `info` still answers `url`.
	auto lines = Utils::Platform::GetShOutputLines("/usr/bin/cloud_oauth url");
	if (!lines.empty())
		url = Utils::String::trim(lines.front());
	return !url.empty();
}

// How do you want to type?
//
// The sign-in page used to open onto the QR screen whichever way you meant to
// work, so somebody signing in on the handheld met a page about their phone
// and stopped there. The phone was never the place the sign-in happens -- it
// is one of two keyboards -- and a screen that leads with a code to scan says
// the opposite.
//
// Asked once, up front, because it is the only thing that differs between the
// two routes and everything after it follows.
static void cloudOAuthChooseInput(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev)
{
	// Off the UI thread. This waits on a provider round trip and used to run
	// inline, spawning a subprocess per poll, so ES painted nothing for the
	// fifteen-odd seconds it took -- indistinguishable from a freeze, and
	// reported as one. It is also why the wait could not afford to be
	// patient; now it can.
	window->pushGui(new GuiLoading<CloudOAuthReady>(window, _("STARTING SIGN-IN"),
		[](IGuiLoadingHandler*)
		{
			CloudOAuthReady ready;
			ready.started = cloudOAuthAwaitSession(ready.url, ready.onDevice,
			                                       ready.exitHint);
			return ready;
		},
		[window, backend, remoteName, prev](CloudOAuthReady ready)
		{
			cloudOAuthPresentChoice(window, backend, remoteName, prev, ready);
		}));
}

static void cloudOAuthPresentChoice(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev,
	const CloudOAuthReady& ready)
{
	// An image that cannot host the page has only one route; asking would be
	// a question with a single answer.
	if (!ready.started || !ready.onDevice)
	{
		cloudOAuthShowSignIn(window, backend, remoteName, prev, true, ready);
		return;
	}

	auto s = new GuiSettings(window, _("CONNECT CLOUD STORAGE"));
	s->setSubTitle(CloudText::providerSubtitle(backend.name, backend.label));

	// Showing this choice closes prev. Each action must replace this page,
	// not the provider page that has already been deleted.
	s->addGroup(_("HOW DO YOU WANT TO TYPE?"));
	s->addWithDescription(_("WITH THE ON-SCREEN KEYBOARD"),
		_("WORKED WITH THE D-PAD, ON THIS SCREEN."),
		nullptr, [window, backend, remoteName, s, ready]
		{
			cloudOAuthShowSignIn(window, backend, remoteName, s, false, ready);
		}, "", false, true);

	s->addWithDescription(_("WITH MY PHONE"),
		// One line (D-UI-023): the sentence this replaced measured 929 of
		// 780 px at 1280x800 and 701 of 620 at 640x480 (#308 F-ES-14).
		_("YOUR PHONE BECOMES THE KEYBOARD. SCAN THE CODE, THEN CONTINUE."),
		nullptr, [window, backend, remoteName, s, ready]
		{
			cloudOAuthShowSignIn(window, backend, remoteName, s, true, ready);
		}, "", false, true);

	cloudSetupSetButtons(s, nullptr);
	cloudOAuthOwnSession(s);
	cloudSetupPresent(window, s, prev);
}

static void cloudOAuthShowSignIn(Window* window, const CloudBackend& backend,
	const std::string& remoteName, GuiSettings* prev, bool usePhone,
	const CloudOAuthReady& ready)
{
	auto s = new GuiSettings(window, _("CONNECT CLOUD STORAGE"));
	s->setSubTitle(CloudText::providerSubtitle(backend.name, backend.label));

	// The address carries its own PIN in its path. ADDRESS and PIN are still
	// reported by `info` for anything driving this from a shell, but the page
	// has nothing to do with them separately -- two numbers on one screen is
	// what made "code" ambiguous the first time.
	const std::string url = ready.url;
	const bool onDevice = ready.onDevice;
	const std::string exitHint = ready.exitHint;

	if (!ready.started)
	{
		cloudSetupAddProse(s, window, _("SIGN-IN DID NOT START"),
			_("THE DEVICE COULD NOT REACH THE PROVIDER. CHECK THE NETWORK AND TRY AGAIN."));
		// One button, and it goes back. This page used to carry CONTINUE as
		// well, which offered to continue with a sign-in that had not begun.
		s->getMenu().clearButtons();
		s->getMenu().addButton(_("GO BACK"), _("go back"), [s] { s->close(); });
		cloudOAuthOwnSession(s);
		cloudSetupPresent(window, s, prev);
		return;
	}

	// Typing on the handheld: nothing to read off the screen, nothing to
	// scan. Open the provider's page and get out of the way. The keyboard
	// raises itself on the first field, so there is no instruction to give
	// that the page does not give better.
	const bool openImmediately = onDevice && !usePhone;
	if (openImmediately)
	{
		cloudSetupAddProse(s, window, _("SIGNING IN"),
			_("THE PROVIDER'S PAGE IS OPENING. A KEYBOARD APPEARS WHEN YOU PICK A BOX; MOVE WITH THE D-PAD AND CHOOSE WITH A."));
		cloudSetupAddSpacer(s, window);
		cloudSetupAddInfoRow(s, window,
			Utils::String::format(_("TO COME BACK HERE, PRESS %s").c_str(), exitHint.c_str()));
		cloudSetupAddInfoRow(s, window, _("THEN CHOOSE 'CONTINUE' TO FINISH"));
	}
	else
	{
		// The phone is a keyboard, not the place the sign-in happens. Saying
		// "sign in on your phone" sent people looking for a provider page
		// there; the page is on the handheld either way.
		const std::string qrPath = "/tmp/cloud-oauth-qr.png";
		Utils::Platform::runSystemCommand(
			"rm -f " + qrPath + "; /usr/bin/qrencode -o " + qrPath
			+ " -s 8 -m 4 --background=FFFFFF --foreground=000000 -l M "
			+ cloudShellQuote(url), "", nullptr);

		std::vector<std::pair<std::string, bool>> lines;
		if (onDevice)
		{
			s->addGroup(_("USE YOUR PHONE AS A KEYBOARD"));
			cloudSetupAddSpacer(s, window);
			lines.push_back(std::make_pair(_("SCAN THIS, OR OPEN"), false));
			lines.push_back(std::make_pair(url, true));
			lines.push_back(std::make_pair(_("WHAT YOU TYPE THERE APPEARS HERE"), false));
		}
		else
		{
			// The older route, for an image that cannot host the page: the
			// player signs in on their phone and brings an address back.
			lines.push_back(std::make_pair(_("OPEN THIS IN A BROWSER"), false));
			lines.push_back(std::make_pair(url, true));
			lines.push_back(std::make_pair(_("THE LAST 4 DIGITS ARE A PIN"), false));
			lines.push_back(std::make_pair(_("IT KEEPS OTHERS OFF THIS PAGE"), false));
		}

		if (Utils::FileSystem::exists(qrPath))
			cloudSetupAddQrRow(s, window, qrPath, lines);
		else
			for (auto& line : lines)
				cloudSetupAddInfoRow(s, window, line.first, line.second);

		if (onDevice)
			cloudSetupAddProse(s, window, _("THEN"),
				_("CONNECT YOUR PHONE FIRST, THEN CHOOSE 'CONTINUE', AND THE PROVIDER'S PAGE OPENS ON THIS SCREEN."));
		else
			cloudSetupAddProse(s, window, _("THEN"),
				_("THAT PAGE WALKS YOU THROUGH SIGNING IN. COME BACK HERE AND SELECT 'CONTINUE' WHEN IT SAYS YOU ARE CONNECTED."));
	}

	// Ask cloud_oauth, which asks rclone. Nothing here decides on its own
	// that a sign-in worked.
	//
	// openOnContinue is false when the page is already open: pressing
	// CONTINUE then means "I am done", not "open it".
	const bool openOnContinue = onDevice && usePhone;
	cloudSetupSetButtons(s, [window, backend, remoteName, s, openOnContinue]
	{
		if (openOnContinue)
		{
			auto info = Utils::Platform::GetShOutputLines("/usr/bin/cloud_oauth status");
			std::string now = info.empty() ? "" : Utils::String::trim(info.front());
			if (now == "waiting")
			{
				// Blocks until the player comes back out of the page, so
				// there is no dialog to dismiss afterwards and no second
				// CONTINUE to press -- fall through and say what happened.
				ApiSystem::getInstance()->launchCloudSignIn(window, true);
			}
		}

		auto lines = Utils::Platform::GetShOutputLines("/usr/bin/cloud_oauth status");
		std::string status = lines.empty() ? "" : Utils::String::trim(lines.front());
		LOG(LogInfo) << "cloud_oauth status=" << status;

		if (status == "signed-in")
		{
			cloudOAuthShowConnected(window, backend, s);
			return;
		}
		if (status == "failed")
		{
			window->pushGui(new GuiMsgBox(window,
				_("THE SIGN-IN DID NOT COMPLETE.\n\nCODES ARE SINGLE-USE AND EXPIRE QUICKLY - START AGAIN TO GET A NEW ONE."),
				_("OK"), nullptr,
				_("EXIT"), [s] { s->close(); }));
			return;
		}
		window->pushGui(new GuiMsgBox(window,
			_("STILL WAITING FOR THE SIGN-IN.\n\nFINISH IT, THEN CHOOSE 'CONTINUE' AGAIN."),
			_("OK"), nullptr,
			_("CANCEL SIGN-IN"), [window, s]
			{
				Utils::Platform::runSystemCommand("/usr/bin/cloud_oauth cancel", "", nullptr);
				s->close();
			}));
	});

	cloudOAuthOwnSession(s);
	cloudSetupPresent(window, s, prev);

	// After the page is on the stack, never while it is being built: this
	// blocks for as long as somebody is signing in, and ES is suspended for
	// the duration. The page has to already be there for ES to come back to.
	if (openImmediately)
		ApiSystem::getInstance()->launchCloudSignIn(window, false);
}

// What it means to have connected something, said once, at the only moment
// anybody is going to read it.
//
// The old ending was a one-line dialog -- "CONNECTED. CLOUD TOOLS ARE NOW
// AVAILABLE" -- which named neither the provider just connected nor anything
// the player could go and do. Somebody who has just finished a sign-in on a
// handheld deserves to be told what they now have.
static void cloudOAuthShowConnected(Window* window, const CloudBackend& backend,
	GuiSettings* prev)
{
	auto s = new GuiSettings(window, _("CONNECTED"));
	s->setSubTitle(CloudText::providerSubtitle(backend.name, backend.label));

	cloudSetupAddProse(s, window,
		Utils::String::format(_("%s IS CONNECTED").c_str(),
		                      Utils::String::toUpper(backend.label).c_str()),
		_("YOUR GAME SAVES, SAVE STATES, AND SCREENSHOTS CAN NOW BE KEPT IN THE CLOUD AND PICKED UP ON ANOTHER DEVICE."));

	// A named menu rather than a vague "it is all available now" -- and one
	// name, because there is one page. This said two, which was accurate when
	// cloud was split across two menus and became a way to send somebody to
	// the wrong half of a page that no longer exists.
	//
	// The rows are numbered and named exactly as that page names them, under
	// an "in" line, so the list reads as what you can do once you are there
	// rather than as three unrelated facts (maintainer, 2026-09-06).
	s->addGroup(_("WHAT YOU CAN DO NOW"));
	cloudSetupAddInfoRow(s, window, _("IN GAME SETTINGS:"), true);
	cloudSetupAddInfoRow(s, window, "1.  " + _("SYNC SAVES WITH THE CLOUD"));
	cloudSetupAddInfoRow(s, window, "2.  " + _("BACK UP SAVES TO THE CLOUD, OR RESTORE THEM"));
	cloudSetupAddInfoRow(s, window, "3.  " + _("MANAGE CLOUD STORAGE"));

	// Which is true of this device: what syncs on its own is two switches
	// under MANAGE CLOUD STORAGE (SYNC SAVES DURING STARTUP, WHEN EXITING A
	// GAME), not systems, and a device whose settings were restored may have
	// them on already -- its next exit syncs to the cloud just connected. The
	// page said NOTHING SYNCS YET whatever the switches said, told the player
	// to turn on systems, and said ROMs and BIOS are never included, which
	// the ROMS AND BIOS tier has not been true of since D-UI-022 (#308 8a gpt
	// F-ES-19).
	const bool syncsOnItsOwn = SystemConf::getInstance()->get("cloudsaves.startup") == "1"
		|| SystemConf::getInstance()->get("cloudsaves.gameexit") == "1";
	if (syncsOnItsOwn)
		cloudSetupAddProse(s, window, _("YOUR SAVES ALREADY SYNC"),
			_("THIS DEVICE WAS SET TO SYNC THEM, SO FROM NOW ON THEY GO TO THIS CLOUD."));
	else
		cloudSetupAddProse(s, window, _("NOTHING SYNCS YET"),
			_("CONNECTING ONLY GIVES THIS DEVICE SOMEWHERE TO PUT THINGS. TO SYNC SAVES ON THEIR OWN, TURN IT ON UNDER MANAGE CLOUD STORAGE."));

	// One button, and it leaves. There is nothing left to continue to -- the
	// sign-in is done, and the page it would return to is the provider list,
	// which is the last place somebody who has just connected wants to be.
	s->getMenu().clearButtons();
	s->getMenu().addButton(_("DONE"), _("done"), [s] { s->close(); });

	cloudSetupPresent(window, s, prev);
}

// A filtered list of backends. `tier` empty means every tier.
static void cloudRemoteShowList(Window* window, const std::string& title,
	const std::string& tier, const std::string& textFilter)
{
	auto s = new GuiSettings(window, title);

	auto backends = cloudRemoteBackends();
	int shown = 0;
	for (auto& b : backends)
	{
		if (b.tier == "plumbing")
			continue;
		// "needs-browser" spans both tiers that cannot be finished on the
		// handheld today.
		if (tier == "needs-browser")
		{
			if (b.tier != "oauth")
				continue;
		}
		else if (!tier.empty() && b.tier != tier)
			continue;
		if (!textFilter.empty()
			&& !Utils::String::containsIgnoreCase(b.label, textFilter)
			&& !Utils::String::containsIgnoreCase(b.name, textFilter))
			continue;

		CloudBackend backend = b;
		shown++;
		// "oauth" means a backend we could drive natively once the
		// device-flow work lands (#51 P2). Until then it needs a browser
		// exactly like "fallback" does, and the form would ask a player for
		// a client_id and a token they have no way to produce. Group it with
		// what is honest today rather than with what is planned.
		if (b.tier == "oauth")
		{
			// Not "sign in with your phone": the provider's page opens on
			// this screen, and the phone is only one of the two keyboards
			// you can type on. Which one is asked after this, so the row
			// says what kind of sign-in it is and nothing about where.
			s->addWithDescription(Utils::String::toUpper(b.label),
				_("OPENS THE PROVIDER'S SIGN-IN PAGE."), nullptr,
				[window, backend, s] { cloudOAuthStart(window, backend, backend.name, s); },
				"", false, true);
		}
		else
		{
			s->addEntry(Utils::String::toUpper(b.label), true, [window, backend]
			{
				cloudRemoteChooseBackend(window, backend);
			});
		}
	}

	if (shown == 0)
		s->addEntry(_("NOTHING MATCHED THAT SEARCH"), false, [] {});

	cloudSetupPresent(window, s, nullptr);
}

void GuiMenu::openCloudAddRemote(Window* window)
{
	auto s = new GuiSettings(window, _("CONNECT CLOUD STORAGE"));

	auto backends = cloudRemoteBackends();
	if (backends.empty())
	{
		// cloud_remote asks rclone for this list, so an empty result means
		// rclone is missing or broken -- not that there are no providers.
		s->addEntry(_("PROVIDER LIST UNAVAILABLE"), false, [] {});
		cloudSetupPresent(window, s, nullptr);
		return;
	}

	s->addGroup(_("RECOMMENDED"));
	for (auto& want : CloudText::recommendedProviders())
	{
		for (auto& b : backends)
		{
			if (b.name != want.first)
				continue;
			// Show our short name here; the form still titles itself with
			// rclone's description, which is useful once you have chosen.
			// The tier decides what happens next, so the player never has to
			// know there are two kinds of sign-in before they pick.
			CloudBackend backend = b;
			// The list is handed over as the page this replaces, so the flow
			// closes back to network settings rather than depositing somebody
			// who has just connected onto the list of things to connect.
			s->addEntry(_(want.second.c_str()), true, [window, backend, s]
			{
				if (backend.tier == "oauth")
					cloudOAuthStart(window, backend, backend.name, s);
				else
					cloudRemoteChooseBackend(window, backend);
			});
			break;
		}
	}

	s->addGroup(_("MORE"));
	s->addEntry(_("SEARCH PROVIDERS"), true, [window]
	{
		auto onSearch = [window](const std::string& term)
		{
			if (Utils::String::trim(term).empty())
				return;
			cloudRemoteShowList(window, _("SEARCH RESULTS"), "", Utils::String::trim(term));
		};
		if (Settings::getInstance()->getBool("UseOSK"))
			window->pushGui(new GuiTextEditPopupKeyboard(window, _("SEARCH PROVIDERS"), "", onSearch, false));
		else
			window->pushGui(new GuiTextEditPopup(window, _("SEARCH PROVIDERS"), "", onSearch, false));
	});

	// One complete list, not one per tier. Two entries that each showed part
	// of the catalogue made the recommendations above look like a third
	// category rather than a shortlist -- and left no single place to answer
	// "is my provider supported at all?".
	s->addWithDescription(_("COMPLETE LIST"),
		_("EVERY PROVIDER RCLONE SUPPORTS."),
		nullptr, [window] { cloudRemoteShowList(window, _("COMPLETE LIST"), "", ""); },
		"", false, true);

	// The old SSH wizard, one level down. It is still the only way to reach
	// rclone's own config for the things a form cannot express -- crypt and
	// union remotes, or reconnecting one whose sign-in has lapsed -- but it
	// is no longer a second front door offering the same job as the first.
	s->addWithDescription(_("USE A COMPUTER INSTEAD"),
		_("RUN RCLONE'S SETUP OVER SSH. FOR CONNECTIONS THIS PAGE CAN'T MAKE."),
		nullptr, [window] { GuiMenu::openCloudSetup(window); }, "", false, true);

	cloudSetupPresent(window, s, nullptr);
}

void GuiMenu::openCloudSetup(Window* window)
{
	auto info = cloudSetupInfo();
	if (info["IP"].empty())
	{
		window->pushGui(new GuiMsgBox(window, _("NO NETWORK CONNECTION DETECTED.\n\nCONNECT TO A NETWORK FIRST, THEN TRY AGAIN.")));
		return;
	}

	// Remote names from `rclone listremotes` keep their trailing colon.
	std::vector<std::string> remotes;
	for (auto& name : Utils::String::split(info["REMOTES"], ' '))
		if (!Utils::String::trim(name).empty())
			remotes.push_back(Utils::String::trim(name));

	LOG(LogInfo) << "cloud_setup wizard: entry, remotes=" << remotes.size();

	if (remotes.empty())
	{
		cloudSetupShowSshStep(window, CloudSetupMode::FirstRemote, "", "", nullptr);
		return;
	}

	auto s = new GuiSettings(window, _("CLOUD STORAGE SETUP"));

	s->addGroup(_("YOUR CONNECTIONS"));
	for (auto remote : remotes)
	{
		s->addEntry(cloudSetupDisplayName(remote) + "  -  " + _("CHECK IT WORKS"), true, [window, remote]
		{
			cloudSetupRunCheck(window, remote, [window](int rc, const std::string& name)
			{
				if (rc == 0)
					window->pushGui(new GuiMsgBox(window, _("YOUR CLOUD IS ANSWERING:") + " " + cloudSetupDisplayName(name)));
				else if (rc == 2)
					window->pushGui(new GuiMsgBox(window, _("THIS CONNECTION ISN'T ANSWERING:") + " " + cloudSetupDisplayName(name) + "\n\n" + _("USE 'REPAIR A CONNECTION' TO RENEW ITS SIGN-IN.")));
				else
					window->pushGui(new GuiMsgBox(window, _("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.")));
			});
		});
	}

	s->addGroup(_("WHAT DO YOU WANT TO DO?"));
	const std::string syncpath = info["SAVES_REMOTE"];
	cloudSetupAddFact(s, window, _("CLOUD FOLDER"), syncpath, [window, s, syncpath]
	{
		cloudSetupOpenSyncPathEditor(window, syncpath, [window, s]
		{
			s->close();
			GuiMenu::openCloudSetup(window);
		});
	});
	std::string preexisting = Utils::String::trim(info["REMOTES"]);
	s->addEntry(_("ADD ANOTHER CONNECTION"), true, [window, s, preexisting]
	{
		cloudSetupShowSshStep(window, CloudSetupMode::AddRemote, "", preexisting, s);
	});
	s->addEntry(_("REPAIR A CONNECTION"), true, [window, s, remotes]
	{
		if (remotes.size() == 1)
		{
			cloudSetupShowSshStep(window, CloudSetupMode::RepairRemote, remotes.front(), "", s);
			return;
		}
		auto picker = new GuiSettings(window, _("WHICH CONNECTION?"));
		for (auto remote : remotes)
		{
			picker->addEntry(cloudSetupDisplayName(remote), true, [window, s, picker, remote]
			{
				// The chooser (s) is not the page owning this handler,
				// so closing it here is safe; the picker itself is
				// replaced by the wizard page.
				s->close();
				cloudSetupShowSshStep(window, CloudSetupMode::RepairRemote, remote, "", picker);
			});
		}
		window->pushGui(picker);
	});

	window->pushGui(s);
}

// Post-restore credential re-entry. Backups deliberately carry no
// secrets, so a restored device needs its credentials back; this page
// shows each one with its verified state and an inline way to fix it.
//
// It is a single page rather than a gated wizard on purpose: every item
// here is independent and optional (a player with no RetroAchievements
// account should not be walked through a step for it), so there is no
// mandatory sequence to enforce - only state to show and actions to
// offer.
//
// Wi-Fi comes first because it is the one item other things depend on:
// the sanitized system.cfg drops `wifi.key`, so a settings restore
// disconnects Wi-Fi, and the cloud-journey continuation that follows
// this page needs the network back.
static void networkApplyWifi(Window* window, const std::string& title,
	const std::function<bool()>& apply, const std::function<void(bool)>& onDone);

void GuiMenu::openRestoreRelink(Window* window, bool consumeMarker)
{
	const std::string restoreMarker = "/storage/.config/.restore-finish-pending";

	auto theme = ThemeData::getMenuTheme();
	auto s = new GuiSettings(window, _("FINISH RESTORE PROCESS"));
	s->setSubTitle(_("RE-ENTER THE PASSWORDS BACKUPS DO NOT INCLUDE"));

	// A state row: check-circle when the credential is present, an empty
	// circle when it is not. Deliberately NOT a warning triangle -- these
	// rows are a checklist of things a backup cannot carry, so "not set
	// yet" is the expected state on arrival, and an alert glyph reads as
	// "something is broken" to someone who has just restored.
	// Each row carries a description: the label alone does not say why the
	// item is here or what happens if it is skipped.
	auto addCredentialRow = [s, window](const std::string& label, const std::string& description,
	                                    bool ok, const std::function<void()>& action)
	{
		s->addWithDescription((ok ? _U("\uF058  ") : _U("\uF10C  ")) + label,
			description, nullptr, action, "", false, true);
	};

	auto reopen = [window, consumeMarker]
	{
		// Rebuild so every row re-reads live state after an edit.
		GuiMenu::openRestoreRelink(window, consumeMarker);
	};

	s->addGroup(_("NETWORK"));
	const bool online = !ApiSystem::getInstance()->getIpAddress().empty()
		&& ApiSystem::getInstance()->getIpAddress() != "NOT CONNECTED";
	addCredentialRow(_("WI-FI PASSWORD"),
		_("BACKUPS NEVER INCLUDE YOUR WI-FI KEY. RE-ENTER IT TO GET BACK ONLINE."),
		online, [window, s, reopen]
	{
		auto wifi = new GuiSettings(window, _("WI-FI PASSWORD"));
		wifi->addInputTextConfigRow(_("WI-FI PASSWORD"), "wifi.key", true);
		// The association behind the spinner every other Wi-Fi apply in this
		// file uses (networkApplyWifi): it ran from a save function on the
		// interface thread, and wifictl connect can take the better part of
		// two minutes -- a frozen screen after a restore, with no word (#308
		// 8-es claude F-ES-02, 8a gpt F-ES-07). The page comes back when it
		// ends, and a failure is said over it.
		// The key row writes SystemConf in memory only, and the page has no
		// save function to write the file (GuiSettings::save returns early
		// without one): written here, before the association reads it.
		wifi->onFinalize([window, s, reopen]
		{
			SystemConf::getInstance()->saveSystemConf();
			s->close();
			const std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
			const std::string key = SystemConf::getInstance()->get("wifi.key");
			const std::string country = SystemConf::getInstance()->get("wifi.country");
			if (!SystemConf::getInstance()->getBool("wifi.enabled") || ssid.empty() || key.empty())
			{
				reopen();
				return;
			}
			LOG(LogInfo) << "restore relink: reconnecting wifi to " << ssid;
			networkApplyWifi(window, _("CONNECTING TO WI-FI"),
				[ssid, key, country] { return ApiSystem::getInstance()->enableWifi(ssid, key, country); },
				[window, reopen, ssid](bool ok)
				{
					reopen();
					// The picker's words for the same failure (GuiWifi::connect):
					// WI-FI CONFIGURATION ERROR was upstream's, in a register
					// a player does not speak (D-UI-031; audit of the fixes, E2
					// claude G-E2-08).
					if (!ok)
						window->pushGui(new GuiMsgBox(window,
							Utils::String::format(_("COULDN'T CONNECT TO %s.").c_str(), ssid.c_str()) + "\n\n" + _("CHECK THE KEY AND TRY AGAIN.")));
				});
		});
		window->pushGui(wifi);
	});

	// Account rows appear only when the restored configuration actually
	// references that service - the username survives a backup, the
	// password does not, so a username is the signal that an account
	// exists and needs its password back.
	const std::string raUser = SystemConf::getInstance()->get("global.retroachievements.username");
	const std::string ssUser = Settings::getInstance()->getString("ScreenScraperUser");
	if (!raUser.empty() || !ssUser.empty())
	{
		s->addGroup(_("ACCOUNTS"));
		if (!raUser.empty())
			addCredentialRow(_("RETROACHIEVEMENTS") + " (" + raUser + ")",
				_("YOUR USERNAME WAS RESTORED; THE PASSWORD WAS NOT."),
				!SystemConf::getInstance()->get("global.retroachievements.password").empty(),
				[window, s, reopen]
			{
				auto ra = new GuiSettings(window, _("RETROACHIEVEMENTS"));
				ra->addInputTextConfigRow(_("PASSWORD"), "global.retroachievements.password", true);
#ifndef CHEEVOS_DEV_LOGIN
				// The web API key is held back from backups too (#68).
				ra->addInputTextConfigRow(_("WEB API KEY"), "global.retroachievements.key", true);
#endif
				// Written here: the rows set memory only, and a page with no
				// save function writes no file (GuiSettings::save).
				ra->onFinalize([s, reopen] { SystemConf::getInstance()->saveSystemConf(); s->close(); reopen(); });
				window->pushGui(ra);
			});

		if (!ssUser.empty())
			addCredentialRow(_("SCREENSCRAPER") + " (" + ssUser + ")",
				_("YOUR USERNAME WAS RESTORED; THE PASSWORD WAS NOT."),
				!Settings::getInstance()->getString("ScreenScraperPass").empty(),
				[window, s, reopen]
			{
				auto ss = new GuiSettings(window, _("SCREENSCRAPER"));
				ss->addInputTextConfigRow(_("PASSWORD"), "ScreenScraperPass", true, true);
#if defined(SCREENSCRAPER_RUNTIME_DEV_LOGIN) && !defined(SCREENSCRAPER_DEV_LOGIN)
				// The developer password is held back from backups too (#64).
				ss->addInputTextConfigRow(_("DEVELOPER PASSWORD"), "ScreenScraperDevPass", true, true);
#endif
				ss->onFinalize([s, reopen] { Settings::getInstance()->saveFile(); s->close(); reopen(); });
				window->pushGui(ss);
			});
	}

	// Netplay's password is stripped too, but it is only meaningful to a
	// player who has netplay switched on.
	if (SystemConf::getInstance()->getBool("global.netplay"))
	{
		s->addGroup(_("NETPLAY"));
		addCredentialRow(_("NETPLAY PASSWORD"),
			_("NEEDED TO REJOIN PASSWORD-PROTECTED NETPLAY ROOMS."),
			!SystemConf::getInstance()->get("global.netplay.password").empty(),
			[window, s, reopen]
		{
			auto np = new GuiSettings(window, _("NETPLAY PASSWORD"));
			np->addInputTextConfigRow(_("NETPLAY PASSWORD"), "global.netplay.password", true);
			np->onFinalize([s, reopen] { SystemConf::getInstance()->saveSystemConf(); s->close(); reopen(); });
			window->pushGui(np);
		});
	}

	s->addGroup(_("THIS DEVICE"));
	// One password covers SSH, Samba, the Syncthing GUI and the file
	// server - they all derive from it via setrootpass.
	const std::string rootPass = SystemConf::getInstance()->get("root.password");
	// A description here is one line on both panels (D-UI-023). The 800 px
	// menu at 1280x800 gives a 20 px description 780 px, the 640 px
	// full-screen menu at 640x480 gives a 15 px one 620 px; the first is
	// the tighter, and the budget that clears both is about 66 characters
	// of the theme's bold font. Measured against the font, not guessed:
	// the sentence this replaced ran to 876 px and 657 px (#151 PL-05).
	addCredentialRow(_("DEVICE PASSWORD (SSH, SAMBA, FILE SERVER)"),
		_("YOUR EXISTING PASSWORD STILL WORKS. SET ONE ONLY TO CHANGE IT."),
		!rootPass.empty(), [window, s, reopen, rootPass]
	{
		auto pw = new GuiSettings(window, _("DEVICE PASSWORD"));
		pw->addInputTextConfigRow(_("DEVICE PASSWORD"), "root.password", false);
		pw->addSaveFunc([rootPass]
		{
			const std::string changed = SystemConf::getInstance()->get("root.password");
			if (!changed.empty() && changed != rootPass)
			{
				SystemConf::getInstance()->saveSystemConf();
				LOG(LogInfo) << "restore relink: applying device password";
				Utils::Platform::runSystemCommand("setrootpass " + Utils::String::shellQuote(changed), "", nullptr); // fork #198
			}
		});
		pw->onFinalize([s, reopen] { s->close(); reopen(); });
		window->pushGui(pw);
	});

	if (Utils::FileSystem::exists("/usr/bin/cloud_setup"))
	{
		// cloud_setup ships in every image, so its presence gates nothing.
		// What matters is whether a remote is actually configured: without
		// this, a player who never set up cloud sync was shown the row and
		// then told their cloud "isn't answering", implying a fault where
		// there was simply nothing to check. cloudAddGatedEntry greys the
		// row and offers to set one up instead.
		const bool cloudConfigured = Utils::FileSystem::exists("/storage/.config/rclone/rclone.conf", false);
		// Checked on demand rather than at page build: it is a network
		// round-trip to the provider and would stall this page.
		cloudAddGatedEntry(s, window, cloudConfigured, _("CHECK CONNECTION"),
			_("CONFIRMS YOUR CLOUD STORAGE STILL SIGNS IN AFTER THE RESTORE."), [window]
		{
			window->pushGui(new GuiLoading<int>(window, _("CHECKING..."),
				[](auto gui)
				{
					std::string out = Utils::String::trim(Utils::Platform::GetShOutput("/usr/bin/cloud_setup --check >/dev/null 2>&1; echo $?"));
					int rc = atoi(out.c_str());
					LOG(LogInfo) << "restore relink: cloud --check rc=" << rc;
					return rc;
				},
				[window](int rc)
				{
					if (rc == 0)
						window->pushGui(new GuiMsgBox(window, _("YOUR CLOUD IS ANSWERING.")));
					else if (rc == 1)
						// A restored device has no cloud tokens on purpose: backups never
						// carry rclone.conf (#52). Say where to connect rather than
						// reporting a cloud that is not there as not answering.
						window->pushGui(new GuiMsgBox(window, _("NO CLOUD STORAGE IS SET UP ON THIS DEVICE YET.\n\nBACKUPS NEVER CARRY YOUR CLOUD SIGN-IN. CONNECT IT AGAIN UNDER GAME SETTINGS > MANAGE CLOUD STORAGE.")));
					else
						window->pushGui(new GuiMsgBox(window, _("YOUR CLOUD ISN'T ANSWERING.\n\nUSE CONNECT OR REPAIR CLOUD STORAGE IN GAME SETTINGS > MANAGE CLOUD STORAGE.")));
				}));
		});
	}

	s->addEntry(_("BLUETOOTH CONTROLLERS MUST BE PAIRED AGAIN"), false, nullptr);

	// LATER keeps the marker, so this page returns on the next boot -- but
	// nothing said so, leaving the player unable to tell defer from discard.
	// One line under the label, as above: the page name is the long part,
	// so the sentence around it is a fragment. Only when there is a marker
	// to keep: the RetroAchievements prompt at startup opens this page with
	// none (main.cpp), and then nothing brings it back -- both rows that
	// would are shown only while the marker is there (#308 8-es claude
	// F-ES-12). Read uncached, like those rows.
	if (Utils::FileSystem::exists(restoreMarker, false))
		s->addWithDescription(_("LATER KEEPS THIS LIST"),
			_("BACK AT STARTUP, OR IN NETWORK SETTINGS > FINISH RESTORE PROCESS."),
			nullptr, nullptr, "", false, true);

	// FINISH consumes the marker; LATER leaves it so the next boot
	// offers this page again. Consuming on completion rather than on
	// display means a crash, a power-off or a walk-away cannot silently
	// lose the flow.
	s->getMenu().clearButtons();
	s->getMenu().addButton(_("LATER"), _("later"), [s] { s->close(); });
	s->getMenu().addButton(_("FINISH"), _("finish"), [s, restoreMarker, consumeMarker]
	{
		if (consumeMarker)
		{
			LOG(LogInfo) << "restore relink: complete, clearing marker";
			std::remove(restoreMarker.c_str());
		}
		s->close();
	});

	window->pushGui(s);
}

void GuiMenu::updateGameLists(Window* window, bool confirm)
{
	if (ThreadedScraper::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("SCRAPER IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedScraper::stop(); }, 
			_("NO"), nullptr));

		return;
	}

	if (ThreadedHasher::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("GAME HASHING IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [] { ThreadedHasher::stop(); },
			_("NO"), nullptr));

		return;
	}
	
	if (!confirm)
	{
		// The same update without the question (the API, the collections page): the
		// same rescan, and the same index of new games after it (fork #299).
		ViewController::reloadAllGames(window, true, true, true);
		return;
	}

	window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMELISTS?"), _("YES"), [window]
		{
			Scripting::fireEvent("update-gamelists");
			ViewController::reloadAllGames(window, true, true, true);
		}, 
		_("NO"), nullptr));
}

void GuiMenu::openSystemEmulatorSettings(SystemData* system)
{
	auto theme = ThemeData::getMenuTheme();

	GuiSettings* s = new GuiSettings(mWindow, system->getFullName().c_str());

	auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
	auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Core"), false);

	std::string currentEmul = system->getEmulator(false);
	std::string defaultEmul = system->getDefaultEmulator();

	emul_choice->add(_("AUTO"), "", false);

	bool found = false;
	for (auto emul : system->getEmulators())
	{
		if (emul.name == currentEmul)
			found = true;

		emul_choice->add(emul.name, emul.name, emul.name == currentEmul);
	}

	if (!found)
		emul_choice->selectFirstItem();

	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Emulator")), theme->Text.font, theme->Text.color), true);
	row.addElement(emul_choice, false);

	s->addRow(row);

	emul_choice->setSelectedChangedCallback([this, system, core_choice](std::string emulatorName)
	{
		std::string currentCore = system->getCore(false);
		std::string defaultCore = system->getDefaultCore(emulatorName);

		core_choice->clear();	
		core_choice->add(_("AUTO"), "", false);

		bool found = false;

		for (auto& emulator : system->getEmulators())
		{
			if (emulatorName != emulator.name)
				continue;
			
			for (auto core : emulator.cores)
			{
				core_choice->add(core.name, core.name, currentCore == core.name);
				if (currentCore == core.name)
					found = true;
			}			
		}
	
		if (!found)
			core_choice->selectFirstItem();
		else
			core_choice->invalidate();
	});

	row.elements.clear();
	row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Core")), theme->Text.font, theme->Text.color), true);
	row.addElement(core_choice, false);
	s->addRow(row);

	// force change event to load core list
	emul_choice->invalidate();


	s->addSaveFunc([system, emul_choice, core_choice]
	{
		Settings::getInstance()->setString(system->getName() + ".emulator", emul_choice->getSelected());
		Settings::getInstance()->setString(system->getName() + ".core", core_choice->getSelected());
	});

	mWindow->pushGui(s);
}

void GuiMenu::openEmulatorSettings()
{
	GuiSettings* configuration = new GuiSettings(mWindow, _("EMULATOR SETTINGS").c_str());

	Window* window = mWindow;

	// For each activated system
	for (auto system : SystemData::sSystemVector)
	{
		if (system->isCollection())
			continue;

		if (system->getEmulators().size() == 0)
			continue;

		if (system->getEmulators().size() == 1 && system->getEmulators().cbegin()->cores.size() <= 1)
			continue;

		configuration->addEntry(system->getFullName(), true, [this, system] { openSystemEmulatorSettings(system); });
	}

	window->pushGui(configuration);
}

struct ThemeConfigOption
{
	std::string defaultSettingName;
	std::string subset;
	std::shared_ptr<OptionListComponent<std::string>> component;
};

void GuiMenu::openThemeConfiguration(Window* mWindow, GuiComponent* s, std::shared_ptr<OptionListComponent<std::string>> theme_set, const std::string systemTheme)
{
	if (theme_set != nullptr && Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU MUST APPLY THE THEME BEFORE EDITING ITS CONFIGURATION"), _("OK")));
		return;
	}

	Window* window = mWindow;

	auto system = ViewController::get()->getState().getSystem();
	auto theme = system->getTheme();

	auto themeconfig = new GuiSettings(mWindow, (systemTheme.empty() ? _("THEME CONFIGURATION") : _("VIEW CUSTOMIZATION")).c_str());

	auto themeSubSets = theme->getSubSets();

	std::string viewName;
	bool showGridFeatures = true;
	if (!systemTheme.empty())
	{
		auto glv = ViewController::get()->getGameListView(system);
		viewName = glv->getName();
		std::string baseType = theme->getCustomViewBaseType(viewName);

		showGridFeatures = (viewName == "grid" || baseType == "grid");
	}

	// gamelist_style
	std::shared_ptr<OptionListComponent<std::string>> gamelist_style = nullptr;

	if (systemTheme.empty() || showGridFeatures && system != NULL && theme->hasView("grid"))
		themeconfig->addGroup(_("GAMELIST STYLE"));

	if (systemTheme.empty())
	{
		gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);

		std::vector<std::pair<std::string, std::string>> styles;
		styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

		bool showViewStyle = true;

		if (system != NULL)
		{
			auto mViews = theme->getViewsOfTheme();

			showViewStyle = mViews.size() > 1;

			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->first == "basic" || it->first == "detailed" || it->first == "grid" || it->first == "video" || it->first == "gamecarousel")
					styles.push_back(std::pair<std::string, std::string>(it->first, _(it->first.c_str())));
				else
					styles.push_back(*it);
			}
		}
		else
		{
			styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
			styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
		}

		auto viewPreference = systemTheme.empty() ? Settings::getInstance()->getString("GamelistViewStyle") : system->getSystemViewMode();
		if (!theme->hasView(viewPreference))
			viewPreference = "automatic";

		for (auto it = styles.cbegin(); it != styles.cend(); it++)
			gamelist_style->add(it->second, it->first, viewPreference == it->first);

		if (!gamelist_style->hasSelection())
			gamelist_style->selectFirstItem();

		if (showViewStyle)
			themeconfig->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
	}

	// Default grid size
	std::shared_ptr<OptionListComponent<std::string>> mGridSize = nullptr;
	if (showGridFeatures && system != NULL && theme->hasView("grid"))
	{
		Vector2f gridOverride =
			systemTheme.empty() ? Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize")) :
			system->getGridSizeOverride();

		auto ovv = std::to_string((int)gridOverride.x()) + "x" + std::to_string((int)gridOverride.y());

		mGridSize = std::make_shared<OptionListComponent<std::string>>(mWindow, _("DEFAULT GRID SIZE"), false);

		bool found = false;
		for (auto it = GuiGamelistOptions::gridSizes.cbegin(); it != GuiGamelistOptions::gridSizes.cend(); it++)
		{
			bool sel = (gridOverride == Vector2f(0, 0) && *it == "automatic") || ovv == *it;
			if (sel)
				found = true;

			mGridSize->add(_(it->c_str()), *it, sel);
		}

		if (!found)
			mGridSize->selectFirstItem();

		themeconfig->addWithLabel(_("DEFAULT GRID SIZE"), mGridSize);
	}



	std::map<std::string, ThemeConfigOption> options;

	auto subsetNames = theme->getSubSetNames(viewName);
	
	// push appliesTo at end of list
	std::stable_sort(subsetNames.begin(), subsetNames.end(), [themeSubSets](const std::string& a, const std::string& b) -> bool
	{ 
		auto sa = ThemeData::getSubSet(themeSubSets, a);
		auto sb = ThemeData::getSubSet(themeSubSets, b);

		bool aHasApplies = sa.size() > 0 && !sa.cbegin()->appliesTo.empty();
		bool bHasApplies = sb.size() > 0 && !sb.cbegin()->appliesTo.empty();

		return aHasApplies < bHasApplies;
	});

	bool hasThemeOptionGroup = false;
	bool hasApplyToGroup = false;
	for (std::string subset : subsetNames) // theme->getSubSetNames(viewName)
	{
		std::string settingName = "subset." + subset;
		std::string perSystemSettingName = systemTheme.empty() ? "" : "subset." + systemTheme + "." + subset;

		if (subset == "colorset") settingName = "ThemeColorSet";
		else if (subset == "iconset") settingName = "ThemeIconSet";
		else if (subset == "menu") settingName = "ThemeMenu";
		else if (subset == "systemview") settingName = "ThemeSystemView";
		else if (subset == "gamelistview") settingName = "ThemeGamelistView";
		else if (subset == "region") settingName = "ThemeRegionName";

		auto themeColorSets = ThemeData::getSubSet(themeSubSets, subset);

		if (themeColorSets.size() > 0)
		{
			auto selectedColorSet = themeColorSets.end();
			auto selectedName = !perSystemSettingName.empty() ? Settings::getInstance()->getString(perSystemSettingName) : Settings::getInstance()->getString(settingName);

			if (!perSystemSettingName.empty() && selectedName.empty())
				selectedName = Settings::getInstance()->getString(settingName);

			for (auto it = themeColorSets.begin(); it != themeColorSets.end() && selectedColorSet == themeColorSets.end(); it++)
				if (it->name == selectedName)
					selectedColorSet = it;

			std::string displayName;
			if (!themeColorSets.empty())
				displayName = themeColorSets.cbegin()->subSetDisplayName;

			std::shared_ptr<OptionListComponent<std::string>> item = std::make_shared<OptionListComponent<std::string> >(mWindow, displayName.empty() ? _(("THEME " + Utils::String::toUpper(subset)).c_str()) : displayName, false);
			item->setTag(!perSystemSettingName.empty() ? perSystemSettingName : settingName);

			std::string defaultName;
			for (auto it = themeColorSets.begin(); it != themeColorSets.end(); it++)
			{
				std::string displayName = it->displayName;

				if (!systemTheme.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(settingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(subset);

					if (it->name == defaultValue)
					{
						defaultName = Utils::String::toUpper(displayName);
						// displayName = displayName + " (" + _("DEFAULT") + ")";
					}
				}

				item->add(displayName, it->name, it == selectedColorSet);
			}

			if (selectedColorSet == themeColorSets.end())
				item->selectFirstItem();

			if (!themeColorSets.empty())
			{				
				if (!displayName.empty())
				{
					bool hasApplyToSubset = themeColorSets.cbegin()->appliesTo.size() > 0;

					std::string prefix;

					if (systemTheme.empty())
					{
						for (auto subsetName : themeColorSets.cbegin()->appliesTo)
						{
							std::string pfx = theme->getViewDisplayName(subsetName);
							if (!pfx.empty())
							{
								if (prefix.empty())
									prefix = pfx;
								else
									prefix = prefix + ", " + pfx;
							}
						}

						prefix = Utils::String::toUpper(prefix);
					}

					if (hasApplyToSubset && !hasApplyToGroup)
					{
						hasApplyToGroup = true;
						themeconfig->addGroup(_("GAMELIST THEME OPTIONS"));
					}
					else if (!hasApplyToSubset && !hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					if (displayName == "-" && item->size() <= 1)
					{
						ComponentListRow row;
						row.selectable = false;

						auto font = ThemeData::getMenuTheme()->TextSmall.font;
						auto text = std::make_shared<TextComponent>(mWindow, "", font, 0); 						
						text->setLineSpacing(1.0f);
						row.addElement(text, true);						

						themeconfig->addRow(row);
					}
					else if (!prefix.empty())
						themeconfig->addWithDescription(displayName, prefix, item);
					else if (!defaultName.empty())
						themeconfig->addWithDescription(displayName, _("DEFAULT VALUE") + " : " + defaultName, item);
					else 
						themeconfig->addWithLabel(displayName + prefix, item);
				}
				else
				{
					if (!hasThemeOptionGroup)
					{
						hasThemeOptionGroup = true;
						themeconfig->addGroup(_("THEME OPTIONS"));
					}

					themeconfig->addWithLabel(_(("THEME " + Utils::String::toUpper(subset)).c_str()), item);
				}
			}

			ThemeConfigOption opt;
			opt.component = item;
			opt.subset = subset;
			opt.defaultSettingName = settingName;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
		else
		{
			ThemeConfigOption opt;
			opt.component = nullptr;
			options[!perSystemSettingName.empty() ? perSystemSettingName : settingName] = opt;
		}
	}
	

	if (!systemTheme.empty())
	{
		themeconfig->addGroup(_("GAMELIST OPTIONS"));

		// Show favorites first in gamelists
		auto fav = Settings::getInstance()->getString(system->getName() + ".FavoritesFirst");
		auto favoritesFirst = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FAVORITES ON TOP"), false);
		std::string defFav = Settings::getInstance()->getBool("FavoritesFirst") ? _("YES") : _("NO");
		favoritesFirst->add(_("AUTO"), "", fav == "" || fav == "auto");
		favoritesFirst->add(_("YES"), "1", fav == "1");
		favoritesFirst->add(_("NO"), "0", fav == "0");
		themeconfig->addWithDescription(_("SHOW FAVORITES ON TOP"), _("DEFAULT VALUE") + " : " + defFav, favoritesFirst);
		themeconfig->addSaveFunc([themeconfig, favoritesFirst, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", favoritesFirst->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show favorites first in gamelists
		auto defHid = Settings::ShowHiddenFiles() ? _("YES") : _("NO");
		auto curhid = Settings::getInstance()->getString(system->getName() + ".ShowHiddenFiles");
		auto hiddenFiles = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW HIDDEN FILES"), false);
		hiddenFiles->add(_("AUTO"), "", curhid == "" || curhid == "auto");
		hiddenFiles->add(_("YES"), "1", curhid == "1");
		hiddenFiles->add(_("NO"), "0", curhid == "0");
		themeconfig->addWithDescription(_("SHOW HIDDEN FILES"), _("DEFAULT VALUE") + " : " + defHid, hiddenFiles);
		themeconfig->addSaveFunc([themeconfig, hiddenFiles, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", hiddenFiles->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Folder View Mode
		auto folderView = Settings::getInstance()->getString("FolderViewMode");
		auto defFol = folderView.empty() ? "" : Utils::String::toUpper(_(folderView.c_str()));
		auto curFol = Settings::getInstance()->getString(system->getName() + ".FolderViewMode");

		auto foldersBehavior = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FOLDERS"), false);
		foldersBehavior->add(_("AUTO"), "", curFol == "" || curFol == "auto"); //  + " (" + defFol + ")"
		foldersBehavior->add(_("always"), "always", curFol == "always");
		foldersBehavior->add(_("never"), "never", curFol == "never");
		foldersBehavior->add(_("having multiple games"), "having multiple games", curFol == "having multiple games");

		themeconfig->addWithDescription(_("SHOW FOLDERS"), _("DEFAULT VALUE") + " : " + defFol, foldersBehavior);
		themeconfig->addSaveFunc([themeconfig, foldersBehavior, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".FolderViewMode", foldersBehavior->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});
		
		// Show parent folder in gamelists
		auto defPf = Settings::getInstance()->getBool("ShowParentFolder") ? _("YES") : _("NO");
		auto curPf = Settings::getInstance()->getString(system->getName() + ".ShowParentFolder");
		auto parentFolder = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW '..' PARENT FOLDER"), false);
		parentFolder->add(_("AUTO"), "", curPf == "" || curPf == "auto");
		parentFolder->add(_("YES"), "1", curPf == "1");
		parentFolder->add(_("NO"), "0", curPf == "0");
		themeconfig->addWithDescription(_("SHOW '..' PARENT FOLDER"), _("DEFAULT VALUE") + " : " + defPf, parentFolder);
		themeconfig->addSaveFunc([themeconfig, parentFolder, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", parentFolder->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		themeconfig->addGroup(_("ICONS"));

		// Tags
		auto defShowTag = Settings::getInstance()->getString("ShowTags");
		if (defShowTag == "1")
			defShowTag = _("AFTER NAME");
		else if (defShowTag == "2")
			defShowTag = _("NO");
		else
			defShowTag = _("BEFORE NAME");

		auto curShowTag = Settings::getInstance()->getString(system->getName() + ".ShowTags");
		auto showTags = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TAGS ICONS"), false);
		showTags->addRange({ { _("AUTO"), "auto" }, { _("BEFORE NAME"), "0" }, { _("AFTER NAME"), "1" }, { _("NO") , "2" } }, curShowTag);

		themeconfig->addWithDescription(_("SHOW TAGS ICONS"), _("DEFAULT VALUE") + " : " + defShowTag, showTags);
		themeconfig->addSaveFunc([themeconfig, showTags, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowTags", showTags->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});


		// Show flags
		auto defSF = Settings::getInstance()->getString("ShowFlags");
		if (defSF == "1")
			defSF = _("BEFORE NAME");
		else if (defSF == "2")
			defSF = _("AFTER NAME");
		else 
			defSF = _("NO");
		
		auto curSF = Settings::getInstance()->getString(system->getName() + ".ShowFlags");
		auto showRegionFlags = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW REGION FLAG"), false);

		showRegionFlags->addRange({ 
			{ _("AUTO"), "auto" },
			{ _("NO"), "0" },
			{ _("BEFORE NAME") , "1" },
			{ _("AFTER NAME"), "2" } }, 
			curSF);

		themeconfig->addWithDescription(_("SHOW REGION FLAG"), _("DEFAULT VALUE") + " : " + defSF, showRegionFlags);
		themeconfig->addSaveFunc([themeconfig, showRegionFlags, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFlags", showRegionFlags->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show SaveStates
		auto defSS = Settings::getInstance()->getBool("ShowSaveStates") ? _("YES") : _("NO");
		auto curSS = Settings::getInstance()->getString(system->getName() + ".ShowSaveStates");
		auto showSaveStates = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SAVE STATE ICON"), false);
		showSaveStates->add(_("AUTO"), "", curSS == "" || curSS == "auto");
		showSaveStates->add(_("YES"), "1", curSS == "1");
		showSaveStates->add(_("NO"), "0", curSS == "0");
		themeconfig->addWithDescription(_("SHOW SAVE STATE ICON"), _("DEFAULT VALUE") + " : " + defSS, showSaveStates);
		themeconfig->addSaveFunc([themeconfig, showSaveStates, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowSaveStates", showSaveStates->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show Manual
		auto defMM = Settings::getInstance()->getBool("ShowManualIcon") ? _("YES") : _("NO");
		auto curMM = Settings::getInstance()->getString(system->getName() + ".ShowManualIcon");
		auto showManual = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW MANUAL ICON"), false);
		showManual->add(_("AUTO"), "", curMM == "" || curMM == "auto");
		showManual->add(_("YES"), "1", curMM == "1");
		showManual->add(_("NO"), "0", curMM == "0");
		themeconfig->addWithDescription(_("SHOW MANUAL ICON"), _("DEFAULT VALUE") + " : " + defMM, showManual);
		themeconfig->addSaveFunc([themeconfig, showManual, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowManualIcon", showManual->getSelected()))
				themeconfig->setVariable("reloadAll", true);
		});

		// Show Cheevos
		auto defCI = Settings::getInstance()->getBool("ShowCheevosIcon") ? _("YES") : _("NO");
		auto curCI = Settings::getInstance()->getString(system->getName() + ".ShowCheevosIcon");
		auto showCheevos = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW RETROACHIEVEMENTS ICON"), false);
		showCheevos->add(_("AUTO"), "", curCI == "" || curCI == "auto");
		showCheevos->add(_("YES"), "1", curCI == "1");
		showCheevos->add(_("NO"), "0", curCI == "0");
		themeconfig->addWithDescription(_("SHOW RETROACHIEVEMENTS ICON"), _("DEFAULT VALUE") + " : " + defCI, showCheevos);
		themeconfig->addSaveFunc([themeconfig, showCheevos, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowCheevosIcon", showCheevos->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});
#if !defined(ROCKNIX)
		// Show gun icons
		auto defGI = Settings::getInstance()->getBool("ShowGunIconOnGames") ? _("YES") : _("NO");
		auto curGI = Settings::getInstance()->getString(system->getName() + ".ShowGunIconOnGames");
		auto showGun = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW GUN ICON"), false);
		showGun->add(_("AUTO"), "", curGI == "" || curGI == "auto");
		showGun->add(_("YES"), "1", curGI == "1");
		showGun->add(_("NO"), "0", curGI == "0");
		themeconfig->addWithDescription(_("SHOW GUN ICON"), _("DEFAULT VALUE") + " : " + defGI, showGun);
		themeconfig->addSaveFunc([themeconfig, showGun, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowGunIconOnGames", showGun->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show wheel icons
		auto defWI = Settings::getInstance()->getBool("ShowWheelIconOnGames") ? _("YES") : _("NO");
		auto curWI = Settings::getInstance()->getString(system->getName() + ".ShowWheelIconOnGames");
		auto showWheel = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW WHEEL ICON"), false);
		showWheel->add(_("AUTO"), "", curWI == "" || curWI == "auto");
		showWheel->add(_("YES"), "1", curWI == "1");
		showWheel->add(_("NO"), "0", curWI == "0");
		themeconfig->addWithDescription(_("SHOW WHEEL ICON"), _("DEFAULT VALUE") + " : " + defWI, showWheel);
		themeconfig->addSaveFunc([themeconfig, showWheel, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowWheelIconOnGames", showWheel->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show trackball icons
		auto defTI = Settings::getInstance()->getBool("ShowTrackballIconOnGames") ? _("YES") : _("NO");
		auto curTI = Settings::getInstance()->getString(system->getName() + ".ShowTrackballIconOnGames");
		auto showTrackball = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW TRACKBALL ICON"), false);
		showTrackball->add(_("AUTO"), "", curTI == "" || curTI == "auto");
		showTrackball->add(_("YES"), "1", curTI == "1");
		showTrackball->add(_("NO"), "0", curTI == "0");
		themeconfig->addWithDescription(_("SHOW TRACKBALL ICON"), _("DEFAULT VALUE") + " : " + defTI, showTrackball);
		themeconfig->addSaveFunc([themeconfig, showTrackball, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowTrackballIconOnGames", showTrackball->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});

		// Show spinner icons
		auto defSI = Settings::getInstance()->getBool("ShowSpinnerIconOnGames") ? _("YES") : _("NO");
		auto curSI = Settings::getInstance()->getString(system->getName() + ".ShowSpinnerIconOnGames");
		auto showSpinner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW SPINNER ICON"), false);
		showSpinner->add(_("AUTO"), "", curSI == "" || curSI == "auto");
		showSpinner->add(_("YES"), "1", curSI == "1");
		showSpinner->add(_("NO"), "0", curSI == "0");
		themeconfig->addWithDescription(_("SHOW SPINNER ICON"), _("DEFAULT VALUE") + " : " + defSI, showSpinner);
		themeconfig->addSaveFunc([themeconfig, showSpinner, system]
			{
				if (Settings::getInstance()->setString(system->getName() + ".ShowSpinnerIconOnGames", showSpinner->getSelected()))
					themeconfig->setVariable("reloadAll", true);
			});
#endif
		// Show filenames
		auto defFn = Settings::getInstance()->getBool("ShowFilenames") ? _("YES") : _("NO");
		auto curFn = Settings::getInstance()->getString(system->getName() + ".ShowFilenames");

		auto showFilenames = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW FILENAMES INSTEAD"), false);
		showFilenames->add(_("AUTO"), "", curFn == "");
		showFilenames->add(_("YES"), "1", curFn == "1");
		showFilenames->add(_("NO"), "0", curFn == "0");
		themeconfig->addWithDescription(_("SHOW FILENAMES INSTEAD"), _("DEFAULT VALUE") + " : " + defFn, showFilenames);
		themeconfig->addSaveFunc([themeconfig, showFilenames, system]
		{
			if (Settings::getInstance()->setString(system->getName() + ".ShowFilenames", showFilenames->getSelected()))
			{
				SystemData::resetSettings();
				FileData::resetSettings();

		//		themeconfig->setVariable("reloadCollections", true);
				themeconfig->setVariable("reloadAll", true);				
			}
		});
		

		// File extensions
		if (!system->isCollection() && system->isGameSystem())
		{
			auto hiddenExts = Utils::String::split(Settings::getInstance()->getString(system->getName() + ".HiddenExt"), ';');

			auto hiddenCtrl = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FILE EXTENSIONS"), true);

			for (auto ext : system->getExtensions())
			{
				std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
				hiddenCtrl->add(ext, extid, std::find(hiddenExts.cbegin(), hiddenExts.cend(), extid) == hiddenExts.cend());
			}

			themeconfig->addWithLabel(_("FILE EXTENSIONS"), hiddenCtrl);
			themeconfig->addSaveFunc([themeconfig, system, hiddenCtrl]
			{
				std::string hiddenSystems;

				std::vector<std::string> sel = hiddenCtrl->getSelectedObjects();

				for (auto ext : system->getExtensions())
				{
					std::string extid = Utils::String::toLower(Utils::String::replace(ext, ".", ""));
					if (std::find(sel.cbegin(), sel.cend(), extid) == sel.cend())
					{
						if (hiddenSystems.empty())
							hiddenSystems = extid;
						else
							hiddenSystems = hiddenSystems + ";" + extid;
					}
				}

				if (Settings::getInstance()->setString(system->getName() + ".HiddenExt", hiddenSystems))
				{
					Settings::getInstance()->saveFile();

					themeconfig->setVariable("reloadAll", true);
					themeconfig->setVariable("forceReloadGames", true);
				}
			});
		}
	}

	if (systemTheme.empty())
	{
		themeconfig->addGroup(_("TOOLS"));

		themeconfig->addEntry(_("RESET CUSTOMIZATIONS"), false, [s, themeconfig, window]
		{
			themeconfig->setVariable("resetTheme", true);
			themeconfig->setVariable("reloadAll", true);
			themeconfig->close();
		});
	}

	//  theme_colorset, theme_iconset, theme_menu, theme_systemview, theme_gamelistview, theme_region,
	themeconfig->addSaveFunc([systemTheme, system, themeconfig, options, gamelist_style, mGridSize, window]
	{
		bool reloadAll = false;

		for (auto option : options)
		{
			ThemeConfigOption& opt = option.second;

			std::string value;

			if (opt.component != nullptr)
			{
				value = opt.component->getSelected();

				if (!systemTheme.empty() && !value.empty())
				{
					std::string defaultValue = Settings::getInstance()->getString(opt.defaultSettingName);
					if (defaultValue.empty())
						defaultValue = system->getTheme()->getDefaultSubSetValue(opt.subset);

					if (value == defaultValue)
						value = "";
				}
				else if (systemTheme.empty() && value == system->getTheme()->getDefaultSubSetValue(opt.subset))
					value = "";
			}

			if (value != Settings::getInstance()->getString(option.first))
				reloadAll |= Settings::getInstance()->setString(option.first, value);
		}

		Vector2f gridSizeOverride(0, 0);

		if (mGridSize != nullptr)
		{
			std::string str = mGridSize->getSelected();
			std::string value = "";

			size_t divider = str.find('x');
			if (divider != std::string::npos)
			{
				std::string first = str.substr(0, divider);
				std::string second = str.substr(divider + 1, std::string::npos);

				gridSizeOverride = Vector2f((float)atof(first.c_str()), (float)atof(second.c_str()));
				value = Utils::String::replace(Utils::String::replace(gridSizeOverride.toString(), ".000000", ""), "0 0", "");
			}

			if (systemTheme.empty())
				reloadAll |= Settings::getInstance()->setString("DefaultGridSize", value);
		}
		else if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("DefaultGridSize", "");

		if (systemTheme.empty())
			reloadAll |= Settings::getInstance()->setString("GamelistViewStyle", gamelist_style == nullptr ? "" : gamelist_style->getSelected());
		else
		{
			std::string viewMode = gamelist_style == nullptr ? system->getSystemViewMode() : gamelist_style->getSelected();
			reloadAll |= system->setSystemViewMode(viewMode, gridSizeOverride);
		}

		if (themeconfig->getVariable("resetTheme"))
		{
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");
			Settings::getInstance()->setString("ThemeRegionName", "");
			Settings::getInstance()->setString("ThemeColorSet", "");
			Settings::getInstance()->setString("ThemeIconSet", "");
			Settings::getInstance()->setString("ThemeMenu", "");
			Settings::getInstance()->setString("ThemeSystemView", "");
			Settings::getInstance()->setString("ThemeGamelistView", "");
			Settings::getInstance()->setString("GamelistViewStyle", "");
			Settings::getInstance()->setString("DefaultGridSize", "");

			for (auto sm : Settings::getInstance()->getStringMap())
				if (Utils::String::startsWith(sm.first, "subset."))
					Settings::getInstance()->setString(sm.first, "");

			for (auto system : SystemData::sSystemVector)
			{
				system->setSystemViewMode("automatic", Vector2f(0, 0));

				Settings::getInstance()->setString(system->getName() + ".FavoritesFirst", "");
				Settings::getInstance()->setString(system->getName() + ".ShowHiddenFiles", "");
				Settings::getInstance()->setString(system->getName() + ".FolderViewMode", "");
				Settings::getInstance()->setString(system->getName() + ".ShowFilenames", "");
				Settings::getInstance()->setString(system->getName() + ".ShowParentFolder", "");
			}

			Settings::getInstance()->saveFile();
			std::string path = Paths::getUserEmulationStationPath() + "/themesettings/" + Settings::getInstance()->getString("ThemeSet") + ".cfg";
			if (Utils::FileSystem::exists(path))
				Utils::FileSystem::removeFile(path);
		}

		if (reloadAll || themeconfig->getVariable("reloadAll"))
		{
			if (themeconfig->getVariable("forceReloadGames"))
			{
				ViewController::reloadAllGames(window, false);
			}
			else if (systemTheme.empty())
			{				
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->reloadAll(window);
				window->closeSplashScreen();
			}
			else
			{
				system->loadTheme();
				system->resetFilters();

				ViewController::get()->reloadSystemListViewTheme(system);
				ViewController::get()->reloadGameListView(system);
			}
		}
	});

	mWindow->pushGui(themeconfig);
}

void GuiMenu::openUISettings() 
{
	auto pthis = this;
	Window* window = mWindow;

	auto s = new GuiSettings(mWindow, _("USER INTERFACE SETTINGS").c_str());

	// theme set
	auto theme = ThemeData::getMenuTheme();
	auto themeSets = ThemeData::getThemeSets();
	auto system = ViewController::get()->getState().getSystem();

	s->addGroup(_("APPEARANCE"));

	if (system != nullptr && !themeSets.empty())
	{		
		auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
		if (selectedSet == themeSets.end())
			selectedSet = themeSets.begin();

		auto theme_set = std::make_shared<OptionListComponent<std::string> >(mWindow, _("THEME SET"), false);

		std::vector<std::string> themeList;
		for (auto it = themeSets.begin(); it != themeSets.end(); it++)
			themeList.push_back(it->first);

		std::sort(themeList.begin(), themeList.end(), [](const std::string& a, const std::string& b) -> bool { return Utils::String::toLower(a).compare(Utils::String::toLower(b)) < 0; });

		for (auto themeName : themeList)
			theme_set->add(themeName, themeName, themeName == selectedSet->first);

		//for (auto it = themeSets.begin(); it != themeSets.end(); it++)
		//	theme_set->add(it->first, it->first, it == selectedSet);

		s->addWithLabel(_("THEME SET"), theme_set);
		s->addSaveFunc([s, theme_set, pthis, window, system]
		{
			std::string oldTheme = Settings::getInstance()->getString("ThemeSet");
			if (oldTheme != theme_set->getSelected())
			{			
				saveSubsetSettings();

				Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

				// theme changed without setting options, forcing options to avoid crash/blank theme
				Settings::getInstance()->setString("ThemeRegionName", "");
				Settings::getInstance()->setString("ThemeColorSet", "");
				Settings::getInstance()->setString("ThemeIconSet", "");
				Settings::getInstance()->setString("ThemeMenu", "");
				Settings::getInstance()->setString("ThemeSystemView", "");
				Settings::getInstance()->setString("ThemeGamelistView", "");
				Settings::getInstance()->setString("GamelistViewStyle", "");
				Settings::getInstance()->setString("DefaultGridSize", "");

				for(auto sm : Settings::getInstance()->getStringMap())
					if (Utils::String::startsWith(sm.first, "subset."))
						Settings::getInstance()->setString(sm.first, "");

				for (auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); sysIt++)
					(*sysIt)->setSystemViewMode("automatic", Vector2f(0,0));

				loadSubsetSettings(theme_set->getSelected());

				s->setVariable("reloadCollections", true);
				s->setVariable("reloadAll", true);
				s->setVariable("reloadGuiMenu", true);

				Scripting::fireEvent("theme-changed", theme_set->getSelected(), oldTheme);				
			}
		});

		bool showThemeConfiguration = system->getTheme()->hasSubsets() || system->getTheme()->hasView("grid");
		if (showThemeConfiguration)
		{
			s->addSubMenu(_("THEME CONFIGURATION"), [this, s, theme_set]() { openThemeConfiguration(mWindow, s, theme_set); });
		}
		else // GameList view style only, acts like Retropie for simple themes
		{
			auto gamelist_style = std::make_shared< OptionListComponent<std::string> >(mWindow, _("GAMELIST VIEW STYLE"), false);
			std::vector<std::pair<std::string, std::string>> styles;
			styles.push_back(std::pair<std::string, std::string>("automatic", _("automatic")));

			auto system = ViewController::get()->getState().getSystem();
			if (system != NULL)
			{
				auto mViews = system->getTheme()->getViewsOfTheme();
				for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
					styles.push_back(*it);
			}
			else
			{
				styles.push_back(std::pair<std::string, std::string>("basic", _("basic")));
				styles.push_back(std::pair<std::string, std::string>("detailed", _("detailed")));
				styles.push_back(std::pair<std::string, std::string>("video", _("video")));
				styles.push_back(std::pair<std::string, std::string>("grid", _("grid")));				
			}

			auto viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
			if (!system->getTheme()->hasView(viewPreference))
				viewPreference = "automatic";

			for (auto it = styles.cbegin(); it != styles.cend(); it++)
				gamelist_style->add(it->second, it->first, viewPreference == it->first);

			s->addWithLabel(_("GAMELIST VIEW STYLE"), gamelist_style);
			s->addSaveFunc([s, gamelist_style, window] {
				if (Settings::getInstance()->setString("GamelistViewStyle", gamelist_style->getSelected()))
				{
					s->setVariable("reloadAll", true);
					s->setVariable("reloadGuiMenu", true);
				}
			});
		}		
	}

#if defined(ROCKNIX)
	s->addGroup(_("CONTROL OPTIONS"));
	auto invertJoy = std::make_shared<SwitchComponent>(mWindow);
	invertJoy->setState(Settings::getInstance()->getBool("InvertButtons"));
	s->addWithDescription(_("SWITCH CONFIRM & CANCEL BUTTONS IN EMULATIONSTATION"), _("Switches the South and East buttons' functionality"), invertJoy);
	s->addSaveFunc([this, s, invertJoy]
	{
		if (Settings::getInstance()->setBool("InvertButtons", invertJoy->getState()))
		{
			InputConfig::AssignActionButtons();
			s->setVariable("reloadAll", true);
		}
	});

	auto invertLongPress = std::make_shared<SwitchComponent>(mWindow);
	invertLongPress->setState(Settings::getInstance()->getBool("GameOptionsAtNorth"));
	s->addWithDescription(_("ACCESS GAME OPTIONS WITH NORTH BUTTON"), _("Switches to short-press North for Save states & long-press South button for Game Options"), invertLongPress);
	s->addSaveFunc([this, s, invertLongPress]
	{
	if (Settings::getInstance()->setBool("GameOptionsAtNorth", invertLongPress->getState()))
		s->setVariable("reloadAll", true);
	});

	if (Utils::Platform::GetEnv("DEVICE_HAS_TOUCHSCREEN") == "true") {
		auto disableTouchscreen = std::make_shared<SwitchComponent>(mWindow);
		disableTouchscreen->setState(Settings::getInstance()->getBool("DisableTouchscreen"));
		s->addWithLabel(_("DISABLE TOUCHSCREEN IN EMULATIONSTATION"), disableTouchscreen);

		s->addSaveFunc([this, s, disableTouchscreen]
		{
			if (Settings::getInstance()->setBool("DisableTouchscreen", disableTouchscreen->getState()))
				s->setVariable("reloadAll", true);
		});
	};
#endif
	s->addGroup(_("DISPLAY OPTIONS"));
	s->addEntry(_("SCREENSAVER SETTINGS"), true, std::bind(&GuiMenu::openScreensaverOptions, this));
	s->addOptionList(_("LIST TRANSITION"), { { _("auto"), "auto" },{ _("fade"), "fade" },{ _("slide"), "slide" },{ _("fade & slide"), "fade & slide" },{ _("instant"), "instant" } }, "TransitionStyle", true);
	s->addOptionList(_("GAME LAUNCH TRANSITION"), { { _("auto"), "auto" },{ _("fade"), "fade" },{ _("fast fade"), "fast fade" },{ _("slide"), "slide" },{ _("fast slide"), "fast slide" },{ _("instant"), "instant" } }, "GameTransitionStyle", true);

	s->addSwitch(_("GAME MEDIAS DURING FAST SCROLLING"), "ScrollLoadMedias", false); 

	s->addSwitch(_("SHOW CLOCK"), "DrawClock", true);
	s->addSwitch(_("ON-SCREEN HELP"), "ShowHelpPrompts", true, [s] { s->setVariable("reloadAll", true); });

	if (Utils::Platform::queryBatteryInformation().hasBattery)
		s->addOptionList(_("SHOW BATTERY STATUS"), { { _("NO"), "" },{ _("ICON"), "icon" },{ _("ICON AND TEXT"), "text" } }, "ShowBattery", true);

	s->addGroup(_("GAMELIST OPTIONS"));
#if defined(ROCKNIX)
	s->addSwitch(_("SHOW VIDEO PREVIEWS"), "ShowVideoPreviews", true, [s] { s->setVariable("reloadAll", true); });
#endif
	s->addSwitch(_("SHOW FAVORITES ON TOP"), "FavoritesFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW HIDDEN FILES"), "ShowHiddenFiles", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW FOLDERS"), { { _("always"), "always" },{ _("never") , "never" },{ _("having multiple games"), "having multiple games" } }, "FolderViewMode", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FOLDERS FIRST"), "ShowFoldersFirst", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW '..' PARENT FOLDER"), "ShowParentFolder", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW FILENAMES INSTEAD"), "ShowFilenames", true, [s]
		{
			SystemData::resetSettings();
			FileData::resetSettings();

			s->setVariable("reloadCollections", true);
			s->setVariable("reloadAll", true);
		});
	s->addSwitch(_("IGNORE LEADING ARTICLES WHEN SORTING"), _("Ignore 'The' and 'A' if at the start."), "IgnoreLeadingArticles", true, [s] { s->setVariable("reloadAll", true); });

	s->addGroup(_("ICONS"));
	s->addOptionList(_("SHOW TAGS ICONS"), { { _("BEFORE NAME"), "auto" },{ _("AFTER NAME") , "1" },{ _("NO"), "2" } }, "ShowTags", true, [s] { s->setVariable("reloadAll", true); });
	s->addOptionList(_("SHOW REGION FLAG"), { { _("NO"), "auto" },{ _("BEFORE NAME") , "1" },{ _("AFTER NAME"), "2" } }, "ShowFlags", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW SAVE STATE ICON"), "ShowSaveStates", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW MANUAL ICON"), "ShowManualIcon", true, [s] { s->setVariable("reloadAll", true); });	
	s->addSwitch(_("SHOW RETROACHIEVEMENTS ICON"), "ShowCheevosIcon", true, [s] { s->setVariable("reloadAll", true); });
#if !defined(ROCKNIX)
	s->addSwitch(_("SHOW GUN ICON"), "ShowGunIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW WHEEL ICON"), "ShowWheelIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW TRACKBALL ICON"), "ShowTrackballIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
	s->addSwitch(_("SHOW SPINNER ICON"), "ShowSpinnerIconOnGames", true, [s] { s->setVariable("reloadAll", true); });
#endif
	s->onFinalize([s, pthis, window]
	{
		if (s->getVariable("reloadCollections"))
			CollectionSystemManager::get()->updateSystemsList();

		if (s->getVariable("reloadAll"))
		{
			ViewController::get()->reloadAll(window);
			window->closeSplashScreen();
		}

		if (s->getVariable("reloadGuiMenu"))
		{
			delete pthis;
			window->pushGui(new GuiMenu(window));
		}
	});

	mWindow->pushGui(s);
}

void GuiMenu::openSoundSettings()
{
	auto s = new GuiSettings(mWindow, _("SOUND SETTINGS").c_str());

	if (VolumeControl::getInstance()->isAvailable())
	{
		s->addGroup(_("VOLUME"));

		// volume
		auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		volume->setValue((float)VolumeControl::getInstance()->getVolume());
		volume->setOnValueChanged([](const float &newVal) { VolumeControl::getInstance()->setVolume((int)Math::round(newVal)); });
		s->addWithLabel(_("SYSTEM VOLUME"), volume);
		s->addSaveFunc([this, volume]
		{
			VolumeControl::getInstance()->setVolume((int)Math::round(volume->getValue()));
#if !WIN32
			SystemConf::getInstance()->set("audio.volume", std::to_string((int)round(volume->getValue())));
#endif
		});


		// Music Volume
		auto musicVolume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		musicVolume->setValue(Settings::getInstance()->getInt("MusicVolume"));
		musicVolume->setOnValueChanged([](const float &newVal) { Settings::getInstance()->setInt("MusicVolume", (int)round(newVal)); });
		s->addWithLabel(_("MUSIC VOLUME"), musicVolume);

		s->addSwitch(_("SHOW OVERLAY WHEN VOLUME CHANGES"), "VolumePopup", true);
	}

	s->addGroup(_("MUSIC"));

	s->addSwitch(_("FRONTEND MUSIC"), "audio.bgmusic", true, []
	{
		if (Settings::getInstance()->getBool("audio.bgmusic"))
			AudioManager::getInstance()->playRandomMusic();
		else
			AudioManager::getInstance()->stopMusic();
	});
	
	s->addSwitch(_("DISPLAY SONG TITLES"), "audio.display_titles", true);
 
	// how long to display the song titles?
	auto titles_time = std::make_shared<SliderComponent>(mWindow, 2.f, 120.f, 2.f, "s");
	titles_time->setValue(Settings::getInstance()->getInt("audio.display_titles_time"));
	s->addWithLabel(_("SONG TITLE DISPLAY DURATION"), titles_time);
	s->addSaveFunc([titles_time] {
		Settings::getInstance()->setInt("audio.display_titles_time", (int)Math::round(titles_time->getValue()));
	});

	s->addSwitch(_("ONLY PLAY SYSTEM-SPECIFIC MUSIC FOLDER"), "audio.persystem", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); } );
	s->addSwitch(_("PLAY SYSTEM-SPECIFIC MUSIC"), "audio.thememusics", true, [] { AudioManager::getInstance()->changePlaylist(ViewController::get()->getState().getSystem()->getTheme(), true); });	
	s->addSwitch(_("LOWER MUSIC WHEN PLAYING VIDEO"), "VideoLowersMusic", true);


    auto favoriteSwitch = std::make_shared<SwitchComponent>(mWindow);
    std::string favoritesFile = FavoriteMusicManager::getFavoriteMusicFilePath();
    bool hasFavorites = false;
    if (Utils::FileSystem::exists(favoritesFile))
    {
        auto favorites = FavoriteMusicManager::loadFavoriteSongs(favoritesFile);
        hasFavorites = !favorites.empty();
    }
    bool shouldUseFavorites = Settings::getInstance()->getBool("audio.useFavoriteMusic") && hasFavorites;
    if (Settings::getInstance()->getBool("audio.useFavoriteMusic") && !hasFavorites)
    {
        Settings::getInstance()->setBool("audio.useFavoriteMusic", false);
        Settings::getInstance()->saveFile();
    }
    favoriteSwitch->setState(shouldUseFavorites);
    s->addWithDescription(_("PLAY ONLY SONGS FROM YOUR FAVORITES PLAYLIST"), "", favoriteSwitch, nullptr);
    s->addSaveFunc([favoriteSwitch, hasFavorites]() 
    {
        bool useFavorite = favoriteSwitch->getState();
        if (useFavorite && !hasFavorites)
        {
            useFavorite = false;
        }
        Settings::getInstance()->setBool("audio.useFavoriteMusic", useFavorite);
        Settings::getInstance()->saveFile();
        AudioManager::getInstance()->playRandomMusic(useFavorite);
    });

    s->addEntry(_("SELECTION OF FAVORITE SONGS"), true, [this] {
        GuiFavoriteMusicSelector::openSelectFavoriteSongs(mWindow, false, true);
    });

	s->addGroup(_("SOUNDS"));

	s->addSwitch(_("ENABLE NAVIGATION SOUNDS"), "EnableSounds", true, []
	{
		if (Settings::getInstance()->getBool("EnableSounds") && PowerSaver::getMode() == PowerSaver::INSTANT)
		{
			Settings::getInstance()->setPowerSaverMode("default");
			PowerSaver::init();
		}
	});

	auto batteryWarning = std::make_shared<SwitchComponent>(mWindow);
	bool batteryWarningEnabled = SystemConf::getInstance()->get("system.battery.warning") == "1";
	batteryWarning->setState(batteryWarningEnabled);
	s->addWithLabel(_("ENABLE AUDIBLE BATTERY WARNING"), batteryWarning);
	s->addSaveFunc([batteryWarning] {
		bool batteryWarningEnabled = batteryWarning->getState();
		SystemConf::getInstance()->set("system.battery.warning", batteryWarningEnabled ? "1" : "0");
	});

	auto batteryWarningThreshold = std::make_shared<SliderComponent>(mWindow, 0.f, 50.f, 1.f, "%");
	if (SystemConf::getInstance()->get("system.battery.warning_threshold").length() == 0) {
		SystemConf::getInstance()->set("system.battery.warning_threshold", "25");
	}
	float batteryWarningThresholdValue = (float)atoi(SystemConf::getInstance()->get("system.battery.warning_threshold").c_str());
	batteryWarningThreshold->setValue(batteryWarningThresholdValue);
	s->addWithLabel(_("BATTERY WARNING THRESHOLD"), batteryWarningThreshold);
	s->addSaveFunc([batteryWarningThreshold] {
		SystemConf::getInstance()->set("system.battery.warning_threshold", std::to_string((int)round(batteryWarningThreshold->getValue())));
	});

	s->addSwitch(_("ENABLE VIDEO PREVIEW AUDIO"), "VideoAudio", true);
	
	mWindow->pushGui(s);
}

// Provides a complete list of ISO 3166-1 alpha-2 country codes.
std::vector<std::pair<std::string, std::string>> getCountryCodes()
{
    return {
        { "AF", "Afghanistan" }, { "AX", "Åland Islands" }, { "AL", "Albania" }, { "DZ", "Algeria" }, { "AS", "American Samoa" },
        { "AD", "Andorra" }, { "AO", "Angola" }, { "AI", "Anguilla" }, { "AQ", "Antarctica" }, { "AG", "Antigua and Barbuda" },
        { "AR", "Argentina" }, { "AM", "Armenia" }, { "AW", "Aruba" }, { "AU", "Australia" }, { "AT", "Austria" },
        { "AZ", "Azerbaijan" }, { "BS", "Bahamas" }, { "BH", "Bahrain" }, { "BD", "Bangladesh" }, { "BB", "Barbados" },
        { "BY", "Belarus" }, { "BE", "Belgium" }, { "BZ", "Belize" }, { "BJ", "Benin" }, { "BM", "Bermuda" },
        { "BT", "Bhutan" }, { "BO", "Bolivia" }, { "BQ", "Bonaire, Sint Eustatius and Saba" }, { "BA", "Bosnia and Herzegovina" }, { "BW", "Botswana" },
        { "BV", "Bouvet Island" }, { "BR", "Brazil" }, { "IO", "British Indian Ocean Territory" }, { "BN", "Brunei Darussalam" }, { "BG", "Bulgaria" },
        { "BF", "Burkina Faso" }, { "BI", "Burundi" }, { "CV", "Cabo Verde" }, { "KH", "Cambodia" }, { "CM", "Cameroon" },
        { "CA", "Canada" }, { "KY", "Cayman Islands" }, { "CF", "Central African Republic" }, { "TD", "Chad" }, { "CL", "Chile" },
        { "CN", "China" }, { "CX", "Christmas Island" }, { "CC", "Cocos (Keeling) Islands" }, { "CO", "Colombia" }, { "KM", "Comoros" },
        { "CG", "Congo" }, { "CD", "Congo, Democratic Republic of the" }, { "CK", "Cook Islands" }, { "CR", "Costa Rica" }, { "CI", "Côte d'Ivoire" },
        { "HR", "Croatia" }, { "CU", "Cuba" }, { "CW", "Curaçao" }, { "CY", "Cyprus" }, { "CZ", "Czechia" },
        { "DK", "Denmark" }, { "DJ", "Djibouti" }, { "DM", "Dominica" }, { "DO", "Dominican Republic" }, { "EC", "Ecuador" },
        { "EG", "Egypt" }, { "SV", "El Salvador" }, { "GQ", "Equatorial Guinea" }, { "ER", "Eritrea" }, { "EE", "Estonia" },
        { "SZ", "Eswatini" }, { "ET", "Ethiopia" }, { "FK", "Falkland Islands (Malvinas)" }, { "FO", "Faroe Islands" }, { "FJ", "Fiji" },
        { "FI", "Finland" }, { "FR", "France" }, { "GF", "French Guiana" }, { "PF", "French Polynesia" }, { "TF", "French Southern Territories" },
        { "GA", "Gabon" }, { "GM", "Gambia" }, { "GE", "Georgia" }, { "DE", "Germany" }, { "GH", "Ghana" },
        { "GI", "Gibraltar" }, { "GR", "Greece" }, { "GL", "Greenland" }, { "GD", "Grenada" }, { "GP", "Guadeloupe" },
        { "GU", "Guam" }, { "GT", "Guatemala" }, { "GG", "Guernsey" }, { "GN", "Guinea" }, { "GW", "Guinea-Bissau" },
        { "GY", "Guyana" }, { "HT", "Haiti" }, { "HM", "Heard Island and McDonald Islands" }, { "VA", "Holy See" }, { "HN", "Honduras" },
        { "HK", "Hong Kong" }, { "HU", "Hungary" }, { "IS", "Iceland" }, { "IN", "India" }, { "ID", "Indonesia" },
        { "IR", "Iran" }, { "IQ", "Iraq" }, { "IE", "Ireland" }, { "IM", "Isle of Man" }, { "IL", "Israel" },
        { "IT", "Italy" }, { "JM", "Jamaica" }, { "JP", "Japan" }, { "JE", "Jersey" }, { "JO", "Jordan" },
        { "KZ", "Kazakhstan" }, { "KE", "Kenya" }, { "KI", "Kiribati" }, { "KP", "Korea, Democratic People's Republic of" }, { "KR", "Korea, Republic of" },
        { "KW", "Kuwait" }, { "KG", "Kyrgyzstan" }, { "LA", "Lao People's Democratic Republic" }, { "LV", "Latvia" }, { "LB", "Lebanon" },
        { "LS", "Lesotho" }, { "LR", "Liberia" }, { "LY", "Libya" }, { "LI", "Liechtenstein" }, { "LT", "Lithuania" },
        { "LU", "Luxembourg" }, { "MO", "Macao" }, { "MG", "Madagascar" }, { "MW", "Malawi" }, { "MY", "Malaysia" },
        { "MV", "Maldives" }, { "ML", "Mali" }, { "MT", "Malta" }, { "MH", "Marshall Islands" }, { "MQ", "Martinique" },
        { "MR", "Mauritania" }, { "MU", "Mauritius" }, { "YT", "Mayotte" }, { "MX", "Mexico" }, { "FM", "Micronesia" },
        { "MD", "Moldova" }, { "MC", "Monaco" }, { "MN", "Mongolia" }, { "ME", "Montenegro" }, { "MS", "Montserrat" },
        { "MA", "Morocco" }, { "MZ", "Mozambique" }, { "MM", "Myanmar" }, { "NA", "Namibia" }, { "NR", "Nauru" },
        { "NP", "Nepal" }, { "NL", "Netherlands" }, { "NC", "New Caledonia" }, { "NZ", "New Zealand" }, { "NI", "Nicaragua" },
        { "NE", "Niger" }, { "NG", "Nigeria" }, { "NU", "Niue" }, { "NF", "Norfolk Island" }, { "MK", "North Macedonia" },
        { "MP", "Northern Mariana Islands" }, { "NO", "Norway" }, { "OM", "Oman" }, { "PK", "Pakistan" }, { "PW", "Palau" },
        { "PS", "Palestine, State of" }, { "PA", "Panama" }, { "PG", "Papua New Guinea" }, { "PY", "Paraguay" }, { "PE", "Peru" },
        { "PH", "Philippines" }, { "PN", "Pitcairn" }, { "PL", "Poland" }, { "PT", "Portugal" }, { "PR", "Puerto Rico" },
        { "QA", "Qatar" }, { "RE", "Réunion" }, { "RO", "Romania" }, { "RU", "Russian Federation" }, { "RW", "Rwanda" },
        { "BL", "Saint Barthélemy" }, { "SH", "Saint Helena, Ascension and Tristan da Cunha" }, { "KN", "Saint Kitts and Nevis" }, { "LC", "Saint Lucia" }, { "MF", "Saint Martin (French part)" },
        { "PM", "Saint Pierre and Miquelon" }, { "VC", "Saint Vincent and the Grenadines" }, { "WS", "Samoa" }, { "SM", "San Marino" }, { "ST", "Sao Tome and Principe" },
        { "SA", "Saudi Arabia" }, { "SN", "Senegal" }, { "RS", "Serbia" }, { "SC", "Seychelles" }, { "SL", "Sierra Leone" },
        { "SG", "Singapore" }, { "SX", "Sint Maarten (Dutch part)" }, { "SK", "Slovakia" }, { "SI", "Slovenia" }, { "SB", "Solomon Islands" },
        { "SO", "Somalia" }, { "ZA", "South Africa" }, { "GS", "South Georgia and the South Sandwich Islands" }, { "SS", "South Sudan" }, { "ES", "Spain" },
        { "LK", "Sri Lanka" }, { "SD", "Sudan" }, { "SR", "Suriname" }, { "SJ", "Svalbard and Jan Mayen" }, { "SE", "Sweden" },
        { "CH", "Switzerland" }, { "SY", "Syrian Arab Republic" }, { "TW", "Taiwan" }, { "TJ", "Tajikistan" }, { "TZ", "Tanzania" },
        { "TH", "Thailand" }, { "TL", "Timor-Leste" }, { "TG", "Togo" }, { "TK", "Tokelau" }, { "TO", "Tonga" },
        { "TT", "Trinidad and Tobago" }, { "TN", "Tunisia" }, { "TR", "Turkey" }, { "TM", "Turkmenistan" }, { "TC", "Turks and Caicos Islands" },
        { "TV", "Tuvalu" }, { "UG", "Uganda" }, { "UA", "Ukraine" }, { "AE", "United Arab Emirates" }, { "GB", "United Kingdom" },
        { "US", "United States of America" }, { "UM", "United States Minor Outlying Islands" }, { "UY", "Uruguay" }, { "UZ", "Uzbekistan" }, { "VU", "Vanuatu" },
        { "VE", "Venezuela" }, { "VN", "Viet Nam" }, { "VG", "Virgin Islands (British)" }, { "VI", "Virgin Islands (U.S.)" }, { "WF", "Wallis and Futuna" },
        { "EH", "Western Sahara" }, { "YE", "Yemen" }, { "ZM", "Zambia" }, { "ZW", "Zimbabwe" }
    };
}

// NETWORK SETTINGS' two facts, filled in from worker threads.
//
// The page used to compute them in its constructor: getifaddrs, then up to
// three pings of two seconds each. Six seconds of a frozen menu on a device
// that is routed but offline -- and with a Wi-Fi driver wedged (fork #102)
// no bound at all, because getifaddrs is a netlink dump that waits on the
// lock the driver is holding, and ping sits in sendto. So the page opens
// with CHECKING... where the answer will go and each row fills in when its
// worker has one. Two workers rather than one, so a kernel that never
// answers the address query cannot keep the status row from giving up on
// schedule: ping is time-boxed in ApiSystem::ping (five seconds for the
// whole sequence); the address query has nothing to put a box around, and
// if it never returns the row stays blank, which is the truth.
//
// Everything the workers touch on the page is held weakly. Closing the page
// destroys its rows and its list, and a result that arrives afterwards
// finds nothing to write to. The strings are looked up here, on the
// interface thread, so the workers carry text and nothing else.
static void networkSettingsFillIn(Window* window, GuiSettings* s,
	const std::shared_ptr<TextComponent>& ipRow, const std::shared_ptr<std::vector<std::string>>& ipList,
	const std::shared_ptr<TextComponent>& statusRow)
{
	std::weak_ptr<ComponentList> list = s->getMenu().getList();
	std::weak_ptr<TextComponent> ip = ipRow;
	std::weak_ptr<std::vector<std::string>> addresses = ipList;
	std::weak_ptr<TextComponent> status = statusRow;
	const std::string connected = _("CONNECTED");
	const std::string notConnected = _("NOT CONNECTED");

	// A value that changes width after its row was laid out has to be laid
	// out again, or an address arriving into an empty slot runs off the
	// right edge of the row: the list positions a row's elements once, when
	// the row is added, and again only when its size changes.
	auto fill = [list](const std::weak_ptr<TextComponent>& row, const std::string& text)
	{
		auto component = row.lock();
		auto rows = list.lock();
		if (component == nullptr || rows == nullptr)
			return;
		component->setText(text);
		rows->onSizeChanged();
	};

	std::thread([window, ip, addresses, notConnected, fill]
	{
		// The row's value is the first address on a link of the device's own
		// (NOT CONNECTED when there is none, as INTERNET STATUS says beside
		// it); the list behind A carries every address with its interface, a
		// tunnel's included and named, since a tailnet address is what the
		// player reaches the device by from elsewhere (fork #279).
		const std::vector<Utils::Platform::InterfaceAddress> found = Utils::Platform::queryInterfaceAddresses();
		std::string first;
		std::vector<std::string> lines;
		for (const auto& a : found)
		{
			lines.push_back(a.address + "  " + a.interface);
			if (first.empty() && a.physical)
				first = a.address;
		}
		AppWindow::post(window, [ip, addresses, first, lines, notConnected, fill]
		{
			if (auto held = addresses.lock())
				*held = lines;

			if (first.empty())
				fill(ip, notConnected);
			else
				fill(ip, lines.size() == 1 ? first : first + " (+)");
		});
	}).detach();

	std::thread([window, status, connected, notConnected, fill]
	{
		const bool online = ApiSystem::getInstance()->ping();
		AppWindow::post(window, [status, online, connected, notConnected, fill]
		{
			fill(status, online ? connected : notConnected);
		});
	}).detach();
}

// The WI-FI NETWORK row's value (WI-FI SSID until D-UI-071): the network the device is on now, from
// NetworkManager, asked off the interface thread as the IP address and the
// internet status are (fork #191). It used to be wifi.ssid, the network
// configured last, because that is what the row edited -- and once
// autoconnect had joined a saved network the two differed, so the RG SP
// showed a setting as though it were the connection. Maintainer,
// 2026-09-15: "Why won't the right-hand value always be the network you're
// connected to? This matches the paradigm on other operating systems,
// phones, etc., where it shows the currently connected network, and then
// you can choose to connect to another network." So the value is the
// connection: the network, NOT CONNECTED when there is none, COULDN'T CHECK
// when NetworkManager did not answer (a silence is not "not connected"),
// CHECKING... until the answer is in. A opens the picker (GuiWifi), whose
// press connects or joins and has this page rebuilt to read the result
// back. The setting still exists and still follows the player's choice; it
// is just not what this row shows.
static void networkSettingsFillInSsid(Window* window, GuiSettings* s, const std::shared_ptr<TextComponent>& valueRow)
{
	std::weak_ptr<ComponentList> list = s->getMenu().getList();
	std::weak_ptr<TextComponent> value = valueRow;
	const std::string notConnected = _("NOT CONNECTED");
	const std::string couldNotCheck = _("COULDN'T CHECK");

	std::thread([window, list, value, notConnected, couldNotCheck]
	{
		std::string joined;
		const bool answered = ApiSystem::getInstance()->getCurrentWifiSsid(joined);
		const std::string text = !answered ? couldNotCheck : (joined.empty() ? notConnected : joined);
		AppWindow::post(window, [list, value, text]
		{
			// A value that changes width after its row was laid out has to
			// be laid out again (networkSettingsFillIn). Both are held
			// weakly: the page may be gone before the answer is.
			auto component = value.lock();
			auto rows = list.lock();
			if (component == nullptr || rows == nullptr)
				return;
			component->setText(text);
			rows->onSizeChanged();
		});
	}).detach();
}

// Connect or disconnect Wi-Fi off the interface thread. wifictl connect can
// take the better part of two minutes on a healthy stack (ApiSystem::
// enableWifi says why), and every caller of this is a menu callback: the
// screen froze for the whole association, which on a handheld reads as a
// crash. GuiLoading keeps a spinner drawing and hands the result back on
// the interface thread; nothing reaches the page beneath it meanwhile, so a
// caller that captured the page still has it when onDone runs.
static void networkApplyWifi(Window* window, const std::string& title,
	const std::function<bool()>& apply, const std::function<void(bool)>& onDone)
{
	window->pushGui(new GuiLoading<bool>(window, title,
		[apply](IGuiLoadingHandler*) { return apply(); },
		[onDone](bool ok)
		{
			if (onDone != nullptr)
				onDone(ok);
		}));
}

void GuiMenu::openNetworkSettings(bool selectWifiEnable, bool selectAdhocEnable)
{
	bool baseWifiEnabled = SystemConf::getInstance()->getBool("wifi.enabled");
	bool baseAdhocEnabled = SystemConf::getInstance()->getBool("wifi.adhoc.enabled");

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("NETWORK SETTINGS").c_str());
	s->addGroup(_("INFORMATION"));

	// Both arrive later; see networkSettingsFillIn. The address row keeps
	// upstream's shape once it is in: NOT CONNECTED, the one address, or the
	// first with (+) after it and A listing them all -- filled from the same
	// worker, since getifaddrs is what #103 moved off the interface thread.
	// The value is a link's address; the list names every interface, a
	// tunnel's too, and opens whenever there is one to show (fork #279).
	auto ip = std::make_shared<TextComponent>(mWindow, "", font, color);
	auto ipList = std::make_shared<std::vector<std::string>>();
	s->addWithLabel(_("IP ADDRESS"), ip, false, [window, ipList]
	{
		if (ipList->empty())
			return;

		std::string text = _("AVAILABLE IP ADDRESSES:\n\n");
		for (const auto& address : *ipList)
			text += address + "\n";
		text.pop_back();

		window->pushGui(new GuiMsgBox(window, text, _("OK"), nullptr));
	});

	// What this device is called on the network. system.hostname keeps whatever
	// the player typed; the network only takes letters, digits and hyphens, so
	// network-base-setup applies the cleaned form at boot and this says what
	// that form is -- shown only when it differs from the name they gave, since
	// otherwise it repeats the HOSTNAME row below (#106).
	const std::string deviceName = SystemConf::getInstance()->get("system.hostname");
	const std::string networkName = CloudText::cleanHostname(deviceName);
	if (!networkName.empty() && networkName != deviceName)
		s->addWithLabel(_("NETWORK NAME"), std::make_shared<TextComponent>(mWindow, networkName, font, color));

	auto status = std::make_shared<TextComponent>(mWindow, _("CHECKING..."), font, color);
	s->addWithLabel(_("INTERNET STATUS"), status);

	networkSettingsFillIn(window, s, ip, ipList, status);

	// Network Indicator
	auto networkIndicator = std::make_shared<SwitchComponent>(mWindow);
	networkIndicator->setState(Settings::getInstance()->getBool("ShowNetworkIndicator"));
	s->addWithLabel(_("SHOW NETWORK INDICATOR"), networkIndicator);
	s->addSaveFunc([networkIndicator] { Settings::getInstance()->setBool("ShowNetworkIndicator", networkIndicator->getState()); });

	s->addGroup(_("SETTINGS"));

#if !WIN32
	// Hostname
	s->addInputTextConfigRow(_("HOSTNAME"), "system.hostname", false);
#endif

	// Wifi enable
	auto enable_wifi = std::make_shared<SwitchComponent>(mWindow);
	enable_wifi->setState(baseWifiEnabled);
	s->addWithLabel(_("ENABLE WI-FI"), enable_wifi, selectWifiEnable);

#ifdef RK3399
        // Add option to disable RG552 wifi gpio
        auto internal_wifi = std::make_shared<SwitchComponent>(mWindow);
        bool internalmoduleEnabled = SystemConf::getInstance()->get("internal.wifi") == "1";
        internal_wifi->setState(internalmoduleEnabled);
        s->addWithLabel(_("ENABLE WI-FI GPIO"), internal_wifi);
        internal_wifi->setOnChangedCallback([internal_wifi] {
                if (internal_wifi->getState() == false) {
                        Utils::Platform::runSystemCommand("/usr/bin/internalwifi disable", "", nullptr);
                } else {
                        Utils::Platform::runSystemCommand("/usr/bin/internalwifi enable", "", nullptr);
                }
                bool internalwifi = internal_wifi->getState();
                SystemConf::getInstance()->set("internal.wifi", internalwifi ? "1" : "0");
        });
#endif

	// window, title, settingstring,
	const std::string baseSSID = SystemConf::getInstance()->get("wifi.ssid");
#if !WIN32
	const std::string baseCountry = SystemConf::getInstance()->get("wifi.country");
#endif

	// Adhoc mode options
	auto enable_adhoc = std::make_shared<SwitchComponent>(mWindow);
	enable_adhoc->setState(baseAdhocEnabled);

	if (baseWifiEnabled)
	{
		if (!baseAdhocEnabled)
		{
			// The value is the network the device is on now, filled in
			// from NetworkManager (networkSettingsFillInSsid); the arrow
			// says A leads somewhere -- the picker, whose press connects or
			// joins and rebuilds this page (as the ENABLE WI-FI switch
			// does) so every row reads the connection back. The value's
			// width follows its text, as the IP ADDRESS row's does.
			ComponentListRow ssidRow;
			// WI-FI NETWORK, since the value is the network the device is on
			// (D-UI-071; the row was WI-FI SSID while its value was the setting).
			auto ssidLabel = std::make_shared<TextComponent>(mWindow, _("WI-FI NETWORK"), font, color);
			if (EsLocale::isRTL())
				ssidLabel->setHorizontalAlignment(Alignment::ALIGN_RIGHT);
			ssidRow.addElement(ssidLabel, true);
			auto ssidValue = std::make_shared<TextComponent>(mWindow, _("CHECKING..."), font, color, ALIGN_RIGHT);
			if (EsLocale::isRTL())
				ssidValue->setHorizontalAlignment(Alignment::ALIGN_LEFT);
			ssidRow.addElement(ssidValue, false);
			auto ssidSpacer = std::make_shared<GuiComponent>(mWindow);
			ssidSpacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
			ssidRow.addElement(ssidSpacer, false);
			auto ssidArrow = std::make_shared<ImageComponent>(mWindow);
			ssidArrow->setImage(theme->Icons.arrow);
			ssidArrow->setResize(Vector2f(0, font->getLetterHeight()));
			if (EsLocale::isRTL())
				ssidArrow->setFlipX(true);
			ssidRow.addElement(ssidArrow, false);
			ssidRow.makeAcceptInputHandler([this, s, window]
			{
				window->pushGui(new GuiWifi(window, _("WI-FI NETWORKS"), [this, s]
				{
					delete s;
					openNetworkSettings();
				}));
			});
			s->addRow(ssidRow);
			networkSettingsFillInSsid(window, s, ssidValue);
			// No WI-FI KEY row (#318, D-UI-118, as ROCKNIX's own saved-Wi-Fi
			// work has it): a key field with no network beside it asked
			// "which network's key?", and the key it held was the last one
			// typed, for whatever network that was. A key is typed where the
			// network is named -- the picker's WI-FI KEY page, per network,
			// and the restore wizard's WI-FI PASSWORD page.

#if !WIN32
		        // Batocera-specific WI-FI COUNTRY option
		        auto country_codes = getCountryCodes();
		        auto country = std::make_shared<OptionListComponent<std::string>>(mWindow, _("WI-FI COUNTRY"), false);

		        country->add(_("N/A"), "", baseCountry.empty());

		        for (auto it = country_codes.cbegin(); it != country_codes.cend(); ++it)
		            country->add(it->second, it->first, baseCountry == it->first);

		        if (country->getSelectedObjects().size() == 0)
		            country->selectFirstItem();

		        s->addWithLabel(_("WI-FI COUNTRY"), country);
		        s->addSaveFunc([country] { SystemConf::getInstance()->set("wifi.country", country->getSelected()); });
#endif

			// The networks NetworkManager remembers, and the way to forget
			// one. A page, not a line here: there is more than one thing to
			// do behind it (es-player-text: a row that leads somewhere is a
			// label).
			s->addEntry(_("MANAGE SAVED NETWORKS"), true, [window] { GuiMenu::openManageNetworks(window); });
		}

		if (ApiSystem::getInstance()->isWifiAPModeSupported())
			s->addWithLabel(_("LOCAL PLAY MODE"), enable_adhoc, selectAdhocEnable);
	}

	auto optionsAdhocID = std::make_shared<OptionListComponent<std::string> >(mWindow, _("LOCAL PLAY ID"), false);
	std::string selectedAdhocID = SystemConf::getInstance()->get("wifi.adhoc.id");

	if (selectedAdhocID.empty())
		selectedAdhocID = "1";

	optionsAdhocID->add(_("1 (HOST)"),"1", selectedAdhocID == "1");
	optionsAdhocID->add(_("2 (CLIENT 1)"),"2", selectedAdhocID == "2");
	optionsAdhocID->add(_("3 (CLIENT 2)"),"3", selectedAdhocID == "3");
	optionsAdhocID->add(_("4 (CLIENT 3)"),"4", selectedAdhocID == "4");

	if (baseAdhocEnabled)
	{
		s->addWithLabel(_("LOCAL PLAY ID"), optionsAdhocID);
		s->addSaveFunc([optionsAdhocID] { SystemConf::getInstance()->set("wifi.adhoc.id", optionsAdhocID->getSelected()); });
	}


	s->addSaveFunc([baseWifiEnabled, baseSSID,
#if !WIN32
	baseCountry,
#endif
	enable_wifi, window]
	{
		bool wifienabled = enable_wifi->getState();

		SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

		if (wifienabled)
		{
			std::string newSSID = SystemConf::getInstance()->get("wifi.ssid");
			std::string newKey = SystemConf::getInstance()->get("wifi.key");
#if !WIN32
			std::string newCountry = SystemConf::getInstance()->get("wifi.country");

			if (baseSSID != newSSID || baseCountry != newCountry || !baseWifiEnabled)
			{
				auto apply = [newSSID, newKey, newCountry] { return ApiSystem::getInstance()->enableWifi(newSSID, newKey, newCountry); };
#else
			if (baseSSID != newSSID || !baseWifiEnabled)
			{
				auto apply = [newSSID, newKey] { return ApiSystem::getInstance()->enableWifi(newSSID, newKey); };
#endif
				// This runs as the page closes, so the spinner and then the
				// verdict appear over whatever the page returns to.
				networkApplyWifi(window, _("CONNECTING TO WI-FI"), apply, [window, newSSID](bool ok)
				{
					// The picker's words for a failure (G-E2-08), not upstream's
					// WI-FI CONFIGURATION ERROR.
					window->pushGui(new GuiMsgBox(window, ok ? _("WI-FI ENABLED")
						: Utils::String::format(_("COULDN'T CONNECT TO %s.").c_str(), newSSID.c_str()) + "\n\n" + _("CHECK THE KEY AND TRY AGAIN.")));
				});
			}
		}
		else if (baseWifiEnabled)
			networkApplyWifi(window, _("TURNING WI-FI OFF"), [] { return ApiSystem::getInstance()->disableWifi(); }, nullptr);
	});

	enable_wifi->setOnChangedCallback([this, s, window, baseWifiEnabled, enable_wifi, baseAdhocEnabled, enable_adhoc]()
	{
		bool wifienabled = enable_wifi->getState();
		bool adhocenabled = enable_adhoc->getState();
		if (baseWifiEnabled != wifienabled || baseAdhocEnabled != adhocenabled)
		{
			SystemConf::getInstance()->setBool("wifi.enabled", wifienabled);

			const std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
			const std::string key = SystemConf::getInstance()->get("wifi.key");
#if !WIN32
			const std::string country = SystemConf::getInstance()->get("wifi.country");
			auto apply = [wifienabled, ssid, key, country]
			{
				return wifienabled ? ApiSystem::getInstance()->enableWifi(ssid, key, country)
				                   : ApiSystem::getInstance()->disableWifi();
			};
#else
			auto apply = [wifienabled, ssid, key]
			{
				return wifienabled ? ApiSystem::getInstance()->enableWifi(ssid, key)
				                   : ApiSystem::getInstance()->disableWifi();
			};
#endif
			// Rebuilt once the change has been applied, so every row re-reads
			// the state it produced. The spinner shields the page meanwhile,
			// so s is still here when the result arrives.
			networkApplyWifi(window, wifienabled ? _("CONNECTING TO WI-FI") : _("TURNING WI-FI OFF"), apply,
				[this, s](bool)
				{
					delete s;
					openNetworkSettings(true);
				});
		}
	});

	enable_adhoc->setOnChangedCallback([this, s, window, baseAdhocEnabled, baseWifiEnabled, enable_wifi, enable_adhoc, optionsAdhocID, selectedAdhocID]
	{
		bool wifienabled = enable_wifi->getState();
		bool adhocenabled = enable_adhoc->getState();

		SystemConf::getInstance()->set("wifi.adhoc.id", optionsAdhocID->getSelected());

		SystemConf::getInstance()->set("global.netplay.host", "192.168.80.1");
		SystemConf::getInstance()->set("global.netplay.port", "55435");
		SystemConf::getInstance()->set("global.netplay.relay", "none");

		SystemConf::getInstance()->setBool("wifi.adhoc.enabled", adhocenabled);
		SystemConf::getInstance()->saveSystemConf();

		// Rebuilt once the mode has been applied; see the ENABLE WI-FI
		// callback for why s survives the wait.
		auto reopen = [this, s] { delete s; openNetworkSettings(false, true); };
		if (!wifienabled)
		{
			reopen();
			return;
		}

		const std::string ssid = SystemConf::getInstance()->get("wifi.ssid");
		const std::string key = SystemConf::getInstance()->get("wifi.key");
#if !WIN32
		const std::string country = SystemConf::getInstance()->get("wifi.country");
		auto apply = [ssid, key, country]
		{
			ApiSystem::getInstance()->disableWifi();
			return ApiSystem::getInstance()->enableWifi(ssid, key, country);
		};
#else
		auto apply = [ssid, key]
		{
			ApiSystem::getInstance()->disableWifi();
			return ApiSystem::getInstance()->enableWifi(ssid, key);
		};
#endif
		networkApplyWifi(window, _("CONNECTING TO WI-FI"), apply, [reopen](bool) { reopen(); });
	});

	// NETWORK SERVICES
	s->addGroup(_("NETWORK SERVICES"));

       auto sshd_enabled = std::make_shared<SwitchComponent>(mWindow);
	bool sshbaseEnabled = SystemConf::getInstance()->get("ssh.enabled") == "1";
	sshd_enabled->setState(sshbaseEnabled);
	s->addWithLabel(_("ENABLE SSH"), sshd_enabled);
	sshd_enabled->setOnChangedCallback([sshd_enabled] {
		if (sshd_enabled->getState() == false) {
			Utils::Platform::runSystemCommand("systemctl stop sshd", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl disable sshd", "", nullptr);
			Utils::Platform::runSystemCommand("rm /storage/.cache/services/sshd.conf", "", nullptr);
		} else {
			Utils::Platform::runSystemCommand("mkdir -p /storage/.cache/services/", "", nullptr);
			Utils::Platform::runSystemCommand("touch /storage/.cache/services/sshd.conf", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl enable sshd", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl start sshd", "", nullptr);
						}
			bool sshenabled = sshd_enabled->getState();
			SystemConf::getInstance()->set("ssh.enabled", sshenabled ? "1" : "0");
		});

       auto samba_enabled = std::make_shared<SwitchComponent>(mWindow);
	bool smbbaseEnabled = SystemConf::getInstance()->get("samba.enabled") == "1";
	samba_enabled->setState(smbbaseEnabled);
	s->addWithLabel(_("ENABLE SAMBA"), samba_enabled);
	samba_enabled->setOnChangedCallback([samba_enabled] {
		if (samba_enabled->getState() == false) {
			Utils::Platform::runSystemCommand("systemctl stop nmbd", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl stop smbd", "", nullptr);
			Utils::Platform::runSystemCommand("rm /storage/.cache/services/smb.conf", "", nullptr);
		} else {
			Utils::Platform::runSystemCommand("mkdir -p /storage/.cache/services/", "", nullptr);
			Utils::Platform::runSystemCommand("touch /storage/.cache/services/smb.conf", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl start nmbd", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl start smbd", "", nullptr);
						}
			bool sambaenabled = samba_enabled->getState();
			SystemConf::getInstance()->set("samba.enabled", sambaenabled ? "1" : "0");
		});

	auto simple_http_enabled = std::make_shared<SwitchComponent>(mWindow);
	bool simplehttpEnabled = SystemConf::getInstance()->get("simplehttp.enabled") == "1";
	simple_http_enabled->setState(simplehttpEnabled);
	s->addWithLabel(_("ENABLE SIMPLE HTTP SERVER"), simple_http_enabled);
	simple_http_enabled->setOnChangedCallback([simple_http_enabled] {
		if(simple_http_enabled->getState() == false) {
			Utils::Platform::runSystemCommand("systemctl disable --now simple-http-server", "", nullptr);
		} else {
			Utils::Platform::runSystemCommand("systemctl enable --now simple-http-server", "", nullptr);
		}
		bool simplehttpenabled = simple_http_enabled->getState();
		SystemConf::getInstance()->set("simplehttp.enabled", simplehttpenabled ? "1" : "0");
	});

	const std::string usbGadgetScript = "/usr/bin/usbgadget";
	auto optionsUSBGadget = std::make_shared<OptionListComponent<std::string> >(mWindow, _("USB GADGET FUNCTION"), false);
	std::string selectedUSBGadget = std::string(Utils::Platform::GetShOutput(R"(/usr/bin/usbgadget)"));
	if (selectedUSBGadget.empty())
		selectedUSBGadget = "disabled";

	std::string a;
	for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/usbgadget --options)")); getline(ss, a, ' '); ) {
		optionsUSBGadget->add(a, a, a == selectedUSBGadget);
	}
	s->addWithLabel(_("USB GADGET FUNCTION"), optionsUSBGadget);

	s->addSaveFunc([this, window, usbGadgetScript, optionsUSBGadget, selectedUSBGadget] {
		if (optionsUSBGadget->changed()) {
			Utils::Platform::runSystemCommand(usbGadgetScript + " " + optionsUSBGadget->getSelected(), "", nullptr);
			if (optionsUSBGadget->getSelected() == "network") {
				std::string usbip = std::string(Utils::Platform::GetShOutput(R"(/usr/bin/usbgadget address)"));
				mWindow->pushGui(new GuiMsgBox(mWindow, _("USB Networking enabled, the device IP is ") + usbip, _("OK"), nullptr));
			}
		}
	});

	// Setting the remote up, and whole-device snapshots. Everything cloud
	// lives in GAME SETTINGS > CLOUD SETTINGS.
	// Reachable without a cloud remote. The same page is offered inside
	// BACKUP/RESTORE SYSTEM DATA, but that entry is gated on a configured
	// remote, so a player who restored and pressed LATER had no way back to
	// it except another reboot.
	// Read uncached: exists() remembers its answer for the session, and this
	// marker is written by a restore that ran after boot -- a cached "absent"
	// from startup hid the row until the next restart (guest b, 2026-09-13).
	if (Utils::FileSystem::exists("/storage/.config/.restore-finish-pending", false))
	{
		Window* restoreWindow = mWindow;
		s->addGroup(_("RESTORE"));
		// The same words as the row in openCloud: one door, one label, one
		// line under it on both panels (D-UI-023; the budget is noted at
		// openRestoreRelink's DEVICE PASSWORD row). The longer sentence this
		// replaced wrapped to a third line at 640x480 (#155).
		s->addWithDescription(_("FINISH RESTORE PROCESS"),
			_("RE-ENTER THE PASSWORDS BACKUPS LEAVE OUT (WI-FI, ACCOUNTS, DEVICE)."),
			nullptr, [restoreWindow] { GuiMenu::openRestoreRelink(restoreWindow, true); }, "", false, true);
	}

	// No cloud entry here. It lived in this menu because cloud storage is a
	// network service, and that is true of the plumbing and false of the job:
	// what the page moves is game saves, ROMs, and this device's settings.
	// A pointer row was tried and is still two doors onto one room -- somebody
	// who set a device up from here and comes back through GAME SETTINGS has
	// no way to know they are looking at the same page. GAME SETTINGS >
	// CLOUD SETTINGS is the one way in.

	// SYNCTHING SERVICES
	s->addGroup(_("SYNCTHING SERVICES"));

       auto enable_syncthing = std::make_shared<SwitchComponent>(mWindow);
	bool syncthingEnabled = SystemConf::getInstance()->get("syncthing.enabled") == "1";
	enable_syncthing->setState(syncthingEnabled);
	s->addWithLabel(_("ENABLE SYNCTHING"), enable_syncthing);
	enable_syncthing->setOnChangedCallback([enable_syncthing] {
		if (enable_syncthing->getState() == false) {
			Utils::Platform::runSystemCommand("systemctl stop syncthing", "", nullptr);
		} else {
			Utils::Platform::runSystemCommand("systemctl start syncthing", "", nullptr);
		}
		bool syncthingenabled = enable_syncthing->getState();
		SystemConf::getInstance()->set("syncthing.enabled", syncthingenabled ? "1" : "0");
	});


	s->addGroup(_("VPN SERVICES"));

	const std::string wireguardConfigFile = "/storage/.config/wireguard/wg0.conf";
	if (Utils::FileSystem::exists(wireguardConfigFile)) {
		auto wireguard = std::make_shared<SwitchComponent>(mWindow);
		bool wgUp = SystemConf::getInstance()->get("wireguard.up") == "1";
		wireguard->setState(wgUp);
		s->addWithLabel(_("WIREGUARD VPN"), wireguard);
		wireguard->setOnChangedCallback([wireguard, wireguardConfigFile] {
			if (wireguard->getState() == false) {
				Utils::Platform::runSystemCommand("wg-quick down " + wireguardConfigFile, "", nullptr);
				Utils::Platform::runSystemCommand("systemctl stop connman-vpn", "", nullptr);
			} else {
				Utils::Platform::runSystemCommand("systemctl start connman-vpn", "", nullptr);
				Utils::Platform::runSystemCommand("wg-quick up " + wireguardConfigFile, "", nullptr);
			}
			SystemConf::getInstance()->set("wireguard.up", wireguard->getState() ? "1" : "0");
		});
	}

	auto tailscale = std::make_shared<SwitchComponent>(mWindow);
	bool tsUp = SystemConf::getInstance()->get("tailscale.up") == "1";
	tailscale->setState(tsUp);
	s->addWithLabel(_("TAILSCALE VPN"), tailscale);
	tailscale->setOnChangedCallback([this, tailscale] {
		bool tsEnabled = tailscale->getState();
		// The setting records what the player chose, not what a seven-second
		// probe saw. It is what 004-tailscaled reads at boot to start the
		// daemon, so writing the probe's result there meant that a switch
		// turned on while the node still needed to sign in (tailscale status
		// says "Logged out.") persisted "0": the daemon kept running and the
		// player signed in through the URL, but the next restart read "0"
		// and stopped it (fork #174). The sign-in popup stays informational.
		SystemConf::getInstance()->set("tailscale.up", tsEnabled ? "1" : "0");
		SystemConf::getInstance()->saveSystemConf();
		if (tsEnabled) {
			Utils::Platform::runSystemCommand("systemctl start tailscaled", "", nullptr);
			Utils::Platform::runSystemCommand("tailscale up --timeout=7s", "", nullptr);
			IsTailscaleUp(mWindow);
		} else {
			Utils::Platform::runSystemCommand("tailscale down", "", nullptr);
			Utils::Platform::runSystemCommand("systemctl stop tailscaled", "", nullptr);
		}
	});


	auto zerotier = std::make_shared<SwitchComponent>(mWindow);
	bool ztUp = SystemConf::getInstance()->get("zerotier.up") == "1";
	zerotier->setState(ztUp);
	s->addWithLabel(_("ZeroTier One"), zerotier);
	zerotier->setOnChangedCallback([zerotier] {
		bool ztEnabled = zerotier->getState();
		if(ztEnabled) {
			Utils::Platform::runSystemCommand("systemctl start zerotier-one", "", nullptr);
			ztEnabled = IsZeroTierUp();
		} else {
			Utils::Platform::runSystemCommand("systemctl stop zerotier-one", "", nullptr);
		}
		SystemConf::getInstance()->set("zerotier.up", ztEnabled ? "1" : "0");
	});

	mWindow->pushGui(s);
}

// MANAGE SAVED NETWORKS (fork #191): the networks NetworkManager saved --
// one profile per network the device has joined, autoconnected to whichever
// is in range -- with the one in use marked, and A to forget one. Without
// this page a network joined once was joined again on every boot it was in
// range for, with nothing on the device to say so or to stop it. "Saved" is
// the maintainer's word for them (2026-09-15) and wifictl's; the page, its
// group and its dialogs use no other.

// The help bar says what A does here: FORGET, and only while there is a
// network to forget -- a prompt for a key that does nothing is worse than
// none (es-code-traps).
class GuiManageNetworks : public GuiSettings
{
public:
	GuiManageNetworks(Window* window, bool hasNetworks)
		: GuiSettings(window, _("MANAGE SAVED NETWORKS")), mHasNetworks(hasNetworks) { }

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		std::vector<HelpPrompt> prompts;
		for (const auto& prompt : GuiSettings::getHelpPrompts())
			if (prompt.first != BUTTON_OK)
				prompts.push_back(prompt);
		if (mHasNetworks)
			prompts.push_back(HelpPrompt(BUTTON_OK, _("FORGET")));
		return prompts;
	}

private:
	bool mHasNetworks;
};

// A press on a network: the question names it and says what changes -- the
// device stops joining it on its own, and, for the one in use, drops it now.
// YES first, NO last so B answers NO (es-ui-style-guide). The delete is one
// nmcli call, bounded in ApiSystem, behind the spinner; onForgotten then
// rebuilds the caller's page from NetworkManager so every row reads the
// list as it is now (the manage page reopens itself, the Wi-Fi picker
// refreshes, #318), and the toast over it says what happened, disconnection
// included.
void GuiMenu::forgetWifiNetworkWithConfirmation(Window* window, const std::string& name, bool inUse, const std::function<void()>& onForgotten)
{
	std::string text = Utils::String::format(_("FORGET %s?").c_str(), name.c_str()) + "\n\n";
	if (inUse)
		text += _("YOU'RE CONNECTED TO IT NOW, SO YOU'LL BE DISCONNECTED.") + " ";
	text += _("THIS DEVICE WON'T JOIN IT AGAIN ON ITS OWN.");

	window->pushGui(new GuiMsgBox(window, text, _("YES"), [window, name, onForgotten]
	{
		LOG(LogInfo) << "forget network: forgetting " << name;
		window->pushGui(new GuiLoading<std::pair<bool, bool>>(window, _("PLEASE WAIT"),
			[name](IGuiLoadingHandler*)
			{
				bool disconnected = false;
				const bool forgotten = ApiSystem::getInstance()->forgetWifiNetwork(name, disconnected);
				return std::make_pair(forgotten, disconnected);
			},
			[window, name, onForgotten](std::pair<bool, bool> result)
			{
				if (!result.first)
				{
					LOG(LogWarning) << "forget network: could not forget " << name;
					window->pushGui(new GuiMsgBox(window, _("COULDN'T FORGET") + " " + name + ". " + _("TRY AGAIN."), _("OK")));
					return;
				}
				LOG(LogInfo) << "forget network: forgot " << name << (result.second ? ", disconnected" : "");
				if (onForgotten)
					onForgotten();
				window->displayNotificationMessage(_U("\uF058  ") + name + " : "
					+ (result.second ? _("FORGOTTEN, AND YOU'RE DISCONNECTED") : _("FORGOTTEN")));
			}));
	}, _("NO"), nullptr));
}

static void manageNetworksAddRow(Window* window, GuiSettings* page, const WifiText::SavedNetwork& network)
{
	auto theme = ThemeData::getMenuTheme();
	ComponentListRow row;

	// The name as NetworkManager has it. A network's name is case-sensitive
	// and is what the player recognises it by, so the row is built by hand:
	// addWithLabel and addWithDescription upper-case their label.
	auto name = std::make_shared<TextComponent>(window, network.name, theme->Text.font, theme->Text.color);
	if (EsLocale::isRTL())
		name->setHorizontalAlignment(Alignment::ALIGN_RIGHT);
	row.addElement(name, true);

	// The mark, on the right, at the row's own weight: a fact beside an
	// action rather than a line under it, so every row stays one line high.
	// CONNECTED, the word the Wi-Fi picker beside this page and its toast
	// use for the same fact; it read IN USE (#308 2-wifi claude F-WF-11,
	// least-surprise.md: same thing, same words).
	if (network.inUse)
	{
		const std::string connected = _("CONNECTED");
		auto mark = std::make_shared<TextComponent>(window, connected, theme->Text.font, theme->Text.color, Alignment::ALIGN_RIGHT);
		mark->setSize(theme->Text.font->sizeText(connected + "  ").x(), 0);
		row.addElement(mark, false);
	}

	const std::string networkName = network.name;
	const bool inUse = network.inUse;
	row.makeAcceptInputHandler([window, page, networkName, inUse]
	{
		GuiMenu::forgetWifiNetworkWithConfirmation(window, networkName, inUse, [window, page]
		{
			delete page;
			GuiMenu::openManageNetworks(window);
		});
	});
	page->addRow(row);
}

static void manageNetworksShow(Window* window, const std::vector<WifiText::SavedNetwork>& networks)
{
	auto page = new GuiManageNetworks(window, !networks.empty());
	page->addGroup(_("SAVED NETWORKS"));
	if (networks.empty())
		page->addEntry(_("NO SAVED NETWORKS"), false);
	for (const auto& network : networks)
		manageNetworksAddRow(window, page, network);
	window->pushGui(page);
}

void GuiMenu::openManageNetworks(Window* window)
{
	// The list is one nmcli call, but over D-Bus to a NetworkManager that
	// can stop answering (fork #102), so it is fetched behind the spinner
	// with ApiSystem's bound rather than in a page's constructor. A list
	// that could not be read is a dialog, not an empty page: NO SAVED
	// NETWORKS is a fact about the device, and that would be a guess.
	window->pushGui(new GuiLoading<std::pair<bool, std::vector<WifiText::SavedNetwork>>>(window, _("PLEASE WAIT"),
		[](IGuiLoadingHandler*)
		{
			std::vector<WifiText::SavedNetwork> networks;
			const bool readable = ApiSystem::getInstance()->getSavedWifiNetworks(networks);
			return std::make_pair(readable, networks);
		},
		[window](std::pair<bool, std::vector<WifiText::SavedNetwork>> result)
		{
			if (!result.first)
			{
				LOG(LogWarning) << "manage networks: the saved networks could not be read";
				window->pushGui(new GuiMsgBox(window, _("COULDN'T READ THE SAVED NETWORKS. TRY AGAIN."), _("OK")));
				return;
			}
			manageNetworksShow(window, result.second);
		}));
}

bool GuiMenu::IsTailscaleUp(Window* window) {
  bool loggedOut = false;
  std::string tempUrl;
  ApiSystem::executeScriptLegacy("tailscale status", [&](const std::string& line) {
    const std::string prompt = "Log in at: ";
    if (line.find(prompt) == 0) {
      tempUrl = line.substr(prompt.length());
    }
    if (line.find("Logged out.") != std::string::npos) {
      loggedOut = true;
    }
  });
  if (loggedOut && window && !tempUrl.empty()) {
    std::string msg = _("TAILSCALE REAUTHENTICATE:\n") + tempUrl;
    window->pushGui(new GuiMsgBox(window, msg));
  }
  return !loggedOut;
}

bool GuiMenu::IsZeroTierUp(std::string* networkId) {
	bool running = false;
	ApiSystem::executeScriptLegacy("zerotier-cli -D/storage/.config/zerotier/ info", [networkId, &running](std::string line) {
		if (line.find("Error connecting to the ZeroTier") != std::string::npos ) running = false;
		else running = true;
	});
	return running;
}

void GuiMenu::openQuitMenu()
{
  GuiMenu::openQuitMenu_static(mWindow);
}

void GuiMenu::openQuitMenu_static(Window *window, bool quickAccessMenu, bool animate)
{
#ifdef WIN32
	if (!quickAccessMenu && Settings::getInstance()->getBool("ShowOnlyExit") && Settings::getInstance()->getBool("ShowExit"))
	{
		Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
		return;
	}
#endif

	auto s = new GuiSettings(window, (quickAccessMenu ? _("QUICK ACCESS") : _("QUIT")).c_str());
	s->setCloseButton("select");

	
	if (quickAccessMenu)
	{
    		s->addGroup(_("QUICK ACCESS"));
            if (AudioManager::getInstance()->isSongPlaying())
            {
                std::string songName = AudioManager::getInstance()->getSongName();
                std::string currentSongPath = AudioManager::getInstance()->getCurrentSongPath();

                if (!songName.empty())
                {
                    s->addWithDescription(_("SKIP TO THE NEXT SONG"),
                                          _("NOW PLAYING") + ": " + songName,
                                          nullptr,
                                          [s, window]()
                                          {
                                              Window* w = window;
                                              AudioManager::getInstance()->playRandomMusic(false);
                                              delete s;
                                              GuiMenu::openQuitMenu_static(w, true, false);
                                          },
                                          "iconSound");

                    std::string favoritesFile = FavoriteMusicManager::getFavoriteMusicFilePath();
                    auto favorites = FavoriteMusicManager::loadFavoriteSongs(favoritesFile);

                    bool inFavorites = false;
                    for (const auto& fav : favorites)
                    {
                        if (fav.first == currentSongPath)
                        {
                            inFavorites = true;
                            break;
                        }
                    }

                    std::string fileNameWithoutExt = Utils::FileSystem::getFileName(currentSongPath);
                    size_t lastDot = fileNameWithoutExt.find_last_of('.');
                    if (lastDot != std::string::npos) {
                        fileNameWithoutExt = fileNameWithoutExt.substr(0, lastDot);
                    }

                    if (inFavorites)
                    {
                        s->addWithDescription(_("REMOVE CURRENT SONG FROM THE FAVORITES PLAYLIST"), "",
                                          nullptr,
                                          [s, window, currentSongPath, fileNameWithoutExt]()
                                          {
                                              Window* w = window;
                                              if (FavoriteMusicManager::getInstance().removeSongFromFavorites(currentSongPath, fileNameWithoutExt, window))
                                              {
                                                  AudioManager::getInstance()->playRandomMusic(true);
                                                  delete s;
                                                  GuiMenu::openQuitMenu_static(w, true, false);
                                              }
                                          },
                                          "iconSound");
                    }
                    else
                    {
                        s->addWithDescription(_("SAVE CURRENT SONG TO THE FAVORITES PLAYLIST"), "",
                                          nullptr,
                                          [s, window, currentSongPath, fileNameWithoutExt]()
                                          {
                                              Window* w = window;
                                              if (FavoriteMusicManager::getInstance().saveSongToFavorites(currentSongPath, fileNameWithoutExt, window))
                                              {
                                                  Settings::getInstance()->saveFile();
                                                  AudioManager::getInstance()->playRandomMusic(true);
                                                  delete s;
                                                  GuiMenu::openQuitMenu_static(w, true, false);
                                              }
                                          },
                                          "iconSound");
                    }
                }
            }
					
		s->addEntry(_("LAUNCH SCREENSAVER"), false, [s, window]
			{
				Window* w = window;
				window->postToUiThread([w]()
					{
						w->startScreenSaver();
						w->renderScreenSaver();
					});
				delete s;

			}, "iconScraper", true);

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(Paths::getUserManualPath()))
		{
			s->addEntry(_("VIEW USER MANUAL"), false, [s, window]
				{
					GuiImageViewer::showPdf(window, Paths::getUserManualPath());
					delete s;
				}, "iconManual");
		}

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::WRITEPLANEMODE))
		{
			if (ApiSystem::getInstance()->isPlaneMode())
			{
				s->addEntry(_("DISABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(false);
						delete s;
					}, "iconPlanemode");
			}
			else 
			{
				s->addEntry(_("ENABLE PLANE MODE"), false, [window, s]
					{
						ApiSystem::getInstance()->setPlaneMode(true);
						delete s;
					}, "iconPlanemode");
			}
		}
	}
	
	if (quickAccessMenu)
		s->addGroup(_("QUIT"));

	s->addEntry(_("RESTART EMULATIONSTATION"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART EMULATIONSTATION?"),
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT); },
			_("NO"), nullptr));
	}, "iconRestart");

	s->addEntry(_("RESTART SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::REBOOT); },
			_("NO"), nullptr));
	}, "iconRestart");

	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SUSPEND))
	{
		s->addEntry(_("SUSPEND SYSTEM"), false, [window, s] {
			window->pushGui(new GuiMsgBox(window, _("REALLY SUSPEND ?"),
				_("YES"), [s] { s->close(); ApiSystem::getInstance()->suspend(); },
				_("NO"), nullptr));
		}, "iconFastShutdown");
	}

	s->addEntry(_("SHUTDOWN SYSTEM"), false, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconShutdown");

	s->addWithDescription(_("FAST SHUTDOWN SYSTEM"),_("Shutdown without saving metadata."), nullptr, [window] {
		window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATA?"), 
			_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::FAST_SHUTDOWN); },
			_("NO"), nullptr));
	}, "iconFastShutdown");

#ifdef WIN32
	if (Settings::getInstance()->getBool("ShowExit"))
	{
		s->addEntry(_("QUIT EMULATIONSTATION"), false, [window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), 
				_("YES"), [] { Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT); },
				_("NO"), nullptr));
		}, "iconQuit");
	}
#endif

	if (quickAccessMenu && animate)
		s->getMenu().animateTo(Vector2f((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2));
	else if (quickAccessMenu)
		s->getMenu().setPosition((Renderer::getScreenWidth() - s->getMenu().getSize().x()) / 2, (Renderer::getScreenHeight() - s->getMenu().getSize().y()) / 2);

	window->pushGui(s);
}

void GuiMenu::createDecorationItemTemplate(Window* window, std::vector<DecorationSetInfo> sets, std::string data, ComponentListRow& row)
{
	Vector2f maxSize(Renderer::getScreenWidth() * 0.14, Renderer::getScreenHeight() * 0.14);

	int IMGPADDING = Renderer::getScreenHeight()*0.01f;

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	// spacer between icon and text
	auto spacer = std::make_shared<GuiComponent>(window);
	spacer->setSize(IMGPADDING, maxSize.y());
	row.addElement(spacer, false);

	std::string label = data;
	if (data.empty())
		label = _("AUTO");
	else if (data == "none")
		label = _("NONE");
	else
		label = Utils::String::toUpper(Utils::String::replace(data, "_", " "));
		
	row.addElement(std::make_shared<TextComponent>(window, label, font, color, ALIGN_LEFT), true);

	std::string imageUrl;

	for (auto set : sets)
		if (set.name == data)
			imageUrl = set.imageUrl;

	// image
	if (!imageUrl.empty())
	{
		auto icon = std::make_shared<ImageComponent>(window);
		icon->setImage(imageUrl, false, maxSize);
		icon->setMaxSize(maxSize);
		icon->setColorShift(theme->Text.color);
		icon->setPadding(IMGPADDING);
		row.addElement(icon, false);
	}
}

void GuiMenu::popSystemConfigurationGui(Window* mWindow, SystemData* systemData) 
{  
	popSpecificConfigurationGui(mWindow, 
		systemData->getFullName(), 
		systemData->getName(), 
		systemData, 
		nullptr);
}

void GuiMenu::popGameConfigurationGui(Window* mWindow, FileData* fileData)
{
	popSpecificConfigurationGui(mWindow,
		fileData->getName(),
		fileData->getConfigurationName(),
		fileData->getSourceFileData()->getSystem(),
		fileData);
}

void GuiMenu::popSpecificConfigurationGui(Window* mWindow, std::string title, std::string configName, SystemData *systemData, FileData* fileData, bool selectCoreLine)
{
	// The system configuration
	GuiSettings* systemConfiguration = new GuiSettings(mWindow, title.c_str());

	if (fileData != nullptr)
		systemConfiguration->setSubTitle(systemData->getFullName());

	std::string currentEmulator = fileData != nullptr ? fileData->getEmulator(false) : systemData->getEmulator(false);
	std::string currentCore = fileData != nullptr ? fileData->getCore(false) : systemData->getCore(false);

	if (systemData->hasEmulatorSelection())
	{
		auto emulChoice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
		emulChoice->add(_("AUTO"), "", false);
		for (auto& emul : systemData->getEmulators())
		{
			if (emul.cores.size() == 0)
				emulChoice->add(emul.name, emul.name, emul.name == currentEmulator);
			else
			{
				for (auto& core : emul.cores)
				{
					bool selected = (emul.name == currentEmulator && core.name == currentCore);

					if (emul.name == core.name)
						emulChoice->add(emul.name, emul.name + "/" + core.name, selected);
					else
						emulChoice->add(emul.name + ": " + Utils::String::replace(core.name, "_", " "), emul.name + "/" + core.name, selected);
				}
			}
		}

		if (!emulChoice->hasSelection())
			emulChoice->selectFirstItem();

		emulChoice->setSelectedChangedCallback([mWindow, title, systemConfiguration, systemData, fileData, configName, emulChoice](std::string s)
		{
			std::string newEmul;
			std::string newCore;

			auto values = Utils::String::split(emulChoice->getSelected(), '/');
			if (values.size() > 0)
				newEmul = values[0];

			if (values.size() > 1)
				newCore = values[1];

			if (fileData != nullptr)
			{
				fileData->setEmulator(newEmul);
				fileData->setCore(newCore);
			}
			else
			{
				SystemConf::getInstance()->set(configName + ".emulator", newEmul);
				SystemConf::getInstance()->set(configName + ".core", newCore);
			}

			popSpecificConfigurationGui(mWindow, title, configName, systemData, fileData);
			delete systemConfiguration;

		});

		systemConfiguration->addWithLabel(_("Emulator"), emulChoice);
	}

	// System settings
	systemConfiguration->addEntry(_("SYSTEM OPTIONS"), true, [mWindow, configName] { openSystemOptionsConfiguration(mWindow, configName); });

	auto customFeatures = systemData->getCustomFeatures(currentEmulator, currentCore);

	// Screen ratio choice
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::ratio))
	{
		auto ratio_choice = createRatioOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("GAME ASPECT RATIO"), _("Force the game to render in this aspect ratio."), ratio_choice);
		systemConfiguration->addSaveFunc([configName, ratio_choice] { SystemConf::getInstance()->set(configName + ".ratio", ratio_choice->getSelected()); });
	}

	// video resolution mode
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videomode))
	{
		auto videoResolutionMode_choice = createVideoResolutionModeOptionList(mWindow, configName);
		systemConfiguration->addWithDescription(_("VIDEO MODE"), _("Sets the display's resolution. Does not affect the rendering resolution."), videoResolutionMode_choice);
		systemConfiguration->addSaveFunc([configName, videoResolutionMode_choice] { SystemConf::getInstance()->set(configName + ".videomode", videoResolutionMode_choice->getSelected()); });
	}

	// smoothing
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::smooth))
	{
		auto smoothing_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SMOOTH GAMES (BILINEAR FILTERING)"));
		smoothing_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".smooth"));
		systemConfiguration->addWithLabel(_("SMOOTH GAMES (BILINEAR FILTERING)"), smoothing_enabled);
		systemConfiguration->addSaveFunc([configName, smoothing_enabled] { SystemConf::getInstance()->set(configName + ".smooth", smoothing_enabled->getSelected()); });
	}

	// Rewind
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::rewind))
	{
		auto rewind_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("REWIND"));
		rewind_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".rewind"));
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);
		systemConfiguration->addSaveFunc([configName, rewind_enabled] { SystemConf::getInstance()->set(configName + ".rewind", rewind_enabled->getSelected()); });
	}

	// AUTO SAVE/LOAD
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autosave) && !customFeatures.hasFeature("autosave"))
	{
		auto autosave_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTO SAVE/LOAD ON GAME LAUNCH"));
		autosave_enabled->addRange({ { _("AUTO"), "auto" }, { _("ON") , "1" }, { _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".autosave"));
		systemConfiguration->addWithDescription(_("AUTO SAVE/LOAD ON GAME LAUNCH"), _("Load latest save state on game launch and save state when exiting game."), autosave_enabled);
		systemConfiguration->addSaveFunc([configName, autosave_enabled] { SystemConf::getInstance()->set(configName + ".autosave", autosave_enabled->getSelected()); });
	}
	
	// Shaders preset
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
#if !defined(ROCKNIX)
		auto installedShaders = ApiSystem::getInstance()->getShaderList(systemData->getName(), currentEmulator, currentCore);
		if (installedShaders.size() > 0)
		{
#endif
			std::string currentShader = SystemConf::getInstance()->get(configName + ".shaderset");

			auto shaders_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("SHADER SET"), false);
			shaders_choices->add(_("AUTO"), "auto", currentShader.empty() || currentShader == "auto");
			shaders_choices->add(_("NONE"), "none", currentShader == "none");

#if !defined(ROCKNIX)
			for (auto shader : installedShaders)
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);

#else
			std::string shader;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getshaders)")); getline(ss, shader, ','); )
				shaders_choices->add(_(Utils::String::toUpper(shader).c_str()), shader, currentShader == shader);
#endif
			if (!shaders_choices->hasSelection())
				shaders_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("SHADER SET"), shaders_choices);
			systemConfiguration->addSaveFunc([configName, shaders_choices] { SystemConf::getInstance()->set(configName + ".shaderset", shaders_choices->getSelected()); });
#if !defined(ROCKNIX)
		}
#endif
	}

	// Video Filters preset
#if !defined(ROCKNIX)
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::VIDEOFILTERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::videofilters))
	{
		auto installedVideofilters = ApiSystem::getInstance()->getVideoFilterList(systemData->getName(), currentEmulator, currentCore);
		if (installedVideofilters.size() > 0)
		{
#else
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::SHADERS) &&
		systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::shaders))
	{
#endif
			std::string currentVideofilter = SystemConf::getInstance()->get(configName + ".videofilter");

			auto videofilters_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("VIDEO FILTER"), false);
			videofilters_choices->add(_("AUTO"), "auto", currentVideofilter.empty() || currentVideofilter == "auto");
			videofilters_choices->add(_("NONE"), "none", currentVideofilter == "none");

#if !defined(ROCKNIX)
			for (auto videofilter : installedVideofilters)
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);

#else
			std::string videofilter;
			for(std::stringstream ss(Utils::Platform::GetShOutput(R"(/usr/bin/getfilters)")); getline(ss, videofilter, ','); )
				videofilters_choices->add(_(Utils::String::toUpper(videofilter).c_str()), videofilter, currentVideofilter == videofilter);
#endif
			if (!videofilters_choices->hasSelection())
				videofilters_choices->selectFirstItem();

			systemConfiguration->addWithLabel(_("VIDEO FILTER"), videofilters_choices);
			systemConfiguration->addSaveFunc([configName, videofilters_choices] { SystemConf::getInstance()->set(configName + ".videofilter", videofilters_choices->getSelected()); });
#if !defined(ROCKNIX)
		}
#endif
	}

	// Integer scale
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::pixel_perfect))
	{
		auto integerscale_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTEGER SCALING (PIXEL PERFECT)"));
		integerscale_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".integerscale"));
		systemConfiguration->addWithLabel(_("INTEGER SCALING (PIXEL PERFECT)"), integerscale_enabled);
		systemConfiguration->addSaveFunc([integerscale_enabled, configName] { SystemConf::getInstance()->set(configName + ".integerscale", integerscale_enabled->getSelected()); });
	}

	// decorations
	if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::DECORATIONS) && systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::decoration))
	{
		auto sets = GuiMenu::getDecorationsSets(systemData);
		if (sets.size() > 0)
		{
#if defined(BATOCERA)
			systemConfiguration->addEntry(_("DECORATIONS"), true, [mWindow, configName, systemData, sets]
			{
				GuiSettings* decorations_window = new GuiSettings(mWindow, _("DECORATIONS").c_str());

				addDecorationSetOptionListComponent(mWindow, decorations_window, sets, configName);
				
				// stretch bezels
				auto bezel_stretch_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("STRETCH BEZELS (4K & ULTRAWIDE)"));
				bezel_stretch_enabled->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel_stretch") != "0" && SystemConf::getInstance()->get(configName + ".bezel_stretch") != "1");
				bezel_stretch_enabled->add(_("ON"), "1", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "1");
				bezel_stretch_enabled->add(_("OFF"), "0", SystemConf::getInstance()->get(configName + ".bezel_stretch") == "0");
				decorations_window->addWithLabel(_("STRETCH BEZELS (4K & ULTRAWIDE)"), bezel_stretch_enabled);
				decorations_window->addSaveFunc([bezel_stretch_enabled, configName] {
					if (bezel_stretch_enabled->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel_stretch", bezel_stretch_enabled->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				// tattoo and controller overlays
				auto bezel_tattoo = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SHOW CONTROLLER OVERLAYS"));
				bezel_tattoo->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "0"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "system"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo") != "custom");
				bezel_tattoo->add(_("NO"), "0", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "0");
				bezel_tattoo->add(_("SYSTEM CONTROLLERS"), "system", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "system");
				bezel_tattoo->add(_("CUSTOM .PNG IMAGE"), "custom", SystemConf::getInstance()->get(configName + ".bezel.tattoo") == "custom");
				decorations_window->addWithLabel(_("SHOW CONTROLLER OVERLAYS"), bezel_tattoo);
				decorations_window->addSaveFunc([bezel_tattoo, configName] {
					if (bezel_tattoo->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo", bezel_tattoo->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				auto bezel_tattoo_corner = std::make_shared<OptionListComponent<std::string>>(mWindow, _("OVERLAY CORNER"));
				bezel_tattoo_corner->add(_("AUTO"), "auto", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NW"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "NE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SE"
					&& SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") != "SW");
				bezel_tattoo_corner->add(_("NORTH WEST"), "NW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NW");
				bezel_tattoo_corner->add(_("NORTH EAST"), "NE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "NE");
				bezel_tattoo_corner->add(_("SOUTH EAST"), "SE", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SE");
				bezel_tattoo_corner->add(_("SOUTH WEST"), "SW", SystemConf::getInstance()->get(configName + ".bezel.tattoo_corner") == "SW");
				decorations_window->addWithLabel(_("OVERLAY CORNER"), bezel_tattoo_corner);
				decorations_window->addSaveFunc([bezel_tattoo_corner, configName] {
					if (bezel_tattoo_corner->changed()) {
						SystemConf::getInstance()->set(configName + ".bezel.tattoo_corner", bezel_tattoo_corner->getSelected());
						SystemConf::getInstance()->saveSystemConf();
					}
				});

				std::string tatpath = configName + ".bezel.tattoo_file";
				const char *bezelpath = const_cast<char*>(tatpath.data());
				decorations_window->addInputTextConfigRow(_("CUSTOM .PNG IMAGE PATH"), bezelpath, false);

				mWindow->pushGui(decorations_window);
			});
#else
			addDecorationSetOptionListComponent(mWindow, systemConfiguration, sets, configName);
#endif		
		}
	}	

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::latency_reduction))	
		systemConfiguration->addEntry(_("LATENCY REDUCTION"), true, [mWindow, configName] { openLatencyReductionConfiguration(mWindow, configName); });

	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::colorization))
	{
		// gameboy colorize
		auto colorizations_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("COLORIZATION"), false);
		std::string currentColorization = SystemConf::getInstance()->get(configName + "-renderer.colorization");
		if (currentColorization.empty())
			currentColorization = std::string("auto");
		
		colorizations_choices->add(_("AUTO"), "auto", currentColorization == "auto");
		colorizations_choices->add(_("NONE"), "none", currentColorization == "none");

		const char* all_gambate_gc_colors_modes[] = { "GB - DMG",
								 "GB - Light",
								 "GB - Pocket",
								 "GBC - Blue",
								 "GBC - Brown",
								 "GBC - Dark Blue",
								 "GBC - Dark Brown",
								 "GBC - Dark Green",
								 "GBC - Grayscale",
								 "GBC - Green",
								 "GBC - Inverted",
								 "GBC - Orange",
								 "GBC - Pastel Mix",
								 "GBC - Red",
								 "GBC - Yellow",
								 "SGB - 1A",
								 "SGB - 1B",
								 "SGB - 1C",
								 "SGB - 1D",
								 "SGB - 1E",
								 "SGB - 1F",
								 "SGB - 1G",
								 "SGB - 1H",
								 "SGB - 2A",
								 "SGB - 2B",
								 "SGB - 2C",
								 "SGB - 2D",
								 "SGB - 2E",
								 "SGB - 2F",
								 "SGB - 2G",
								 "SGB - 2H",
								 "SGB - 3A",
								 "SGB - 3B",
								 "SGB - 3C",
								 "SGB - 3D",
								 "SGB - 3E",
								 "SGB - 3F",
								 "SGB - 3G",
								 "SGB - 3H",
								 "SGB - 4A",
								 "SGB - 4B",
								 "SGB - 4C",
								 "SGB - 4D",
								 "SGB - 4E",
								 "SGB - 4F",
								 "SGB - 4G",
								 "SGB - 4H",
								 "Special 1",
								 "Special 2",
								 "Special 3",
								 "TWB01 - 756 Production",
								 "TWB02 - AKB48 Pink",
								 "TWB03 - Angry Volcano",
								 "TWB04 - Anime Expo",
								 "TWB05 - Aqours Blue",
								 "TWB06 - Aquatic Iro",
								 "TWB07 - Bandai Namco",
								 "TWB08 - Blossom Pink",
								 "TWB09 - Bubbles Blue",
								 "TWB10 - Builder Yellow",
								 "TWB11 - Buttercup Green",
								 "TWB12 - Camouflage",
								 "TWB13 - Cardcaptor Pink",
								 "TWB14 - Christmas",
								 "TWB15 - Crunchyroll Orange",
								 "TWB16 - Digivice",
								 "TWB17 - Do The Dew",
								 "TWB18 - Eevee Brown",
								 "TWB19 - Fruity Orange",
								 "TWB20 - Game.com",
								 "TWB21 - Game Grump Orange",
								 "TWB22 - GameKing",
								 "TWB23 - Game Master",
								 "TWB24 - Ghostly Aoi",
								 "TWB25 - Golden Wild",
								 "TWB26 - Green Banana",
								 "TWB27 - Greenscale",
								 "TWB28 - Halloween",
								 "TWB29 - Hero Yellow",
								 "TWB30 - Hokage Orange",
								 "TWB31 - Labo Fawn",
								 "TWB32 - Legendary Super Saiyan",
								 "TWB33 - Lemon Lime Green",
								 "TWB34 - Lime Midori",
								 "TWB35 - Mania Plus Green",
								 "TWB36 - Microvision",
								 "TWB37 - Million Live Gold",
								 "TWB38 - Miraitowa Blue",
								 "TWB39 - NASCAR",
								 "TWB40 - Neo Geo Pocket",
								 "TWB41 - Neon Blue",
								 "TWB42 - Neon Green",
								 "TWB43 - Neon Pink",
								 "TWB44 - Neon Red",
								 "TWB45 - Neon Yellow",
								 "TWB46 - Nick Orange",
								 "TWB47 - Nijigasaki Orange",
								 "TWB48 - Odyssey Gold",
								 "TWB49 - Patrick Star Pink",
								 "TWB50 - Pikachu Yellow",
								 "TWB51 - Pocket Tales",
								 "TWB52 - Pokemon mini",
								 "TWB53 - Pretty Guardian Gold",
								 "TWB54 - S.E.E.S. Blue",
								 "TWB55 - Saint Snow Red",
								 "TWB56 - Scooby-Doo Mystery",
								 "TWB57 - Shiny Sky Blue",
								 "TWB58 - Sidem Green",
								 "TWB59 - Slime Blue",
								 "TWB60 - Spongebob Yellow",
								 "TWB61 - Stone Orange",
								 "TWB62 - Straw Hat Red",
								 "TWB63 - Superball Ivory",
								 "TWB64 - Super Saiyan Blue",
								 "TWB65 - Super Saiyan Rose",
								 "TWB66 - Supervision",
								 "TWB67 - Survey Corps Brown",
								 "TWB68 - Tea Midori",
								 "TWB69 - TI-83",
								 "TWB70 - Tokyo Midtown",
								 "TWB71 - Travel Wood",
								 "TWB72 - Virtual Boy",
								 "TWB73 - VMU",
								 "TWB74 - Wisteria Murasaki",
								 "TWB75 - WonderSwan",
								 "TWB76 - Yellow Banana" };

		int n_all_gambate_gc_colors_modes = 126;
		for (int i = 0; i < n_all_gambate_gc_colors_modes; i++)
			colorizations_choices->add(all_gambate_gc_colors_modes[i], all_gambate_gc_colors_modes[i], currentColorization == std::string(all_gambate_gc_colors_modes[i]));
		
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "gb" || systemData->getName() == "gbc" || systemData->getName() == "gb2players" || systemData->getName() == "gbc2players")))  // only for gb, gbc and gb2players
		{
			systemConfiguration->addWithLabel(_("COLORIZATION"), colorizations_choices);
			systemConfiguration->addSaveFunc([colorizations_choices, configName] { SystemConf::getInstance()->set(configName + "-renderer.colorization", colorizations_choices->getSelected()); });
		}		
	}

	// ps2 full boot
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::fullboot))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "ps2")) // only for ps2			
		{
			auto fullboot_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("FULL BOOT"));
			fullboot_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".fullboot"));
			systemConfiguration->addWithLabel(_("FULL BOOT"), fullboot_enabled);
			systemConfiguration->addSaveFunc([fullboot_enabled, configName] { SystemConf::getInstance()->set(configName + ".fullboot", fullboot_enabled->getSelected()); });
		}
	}

	// wii emulated wiimotes
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::emulated_wiimotes))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "wii"))  // only for wii
		{
			auto emulatedwiimotes_enabled = std::make_shared<OptionListComponent<std::string>>(mWindow, _("EMULATED WIIMOTES"));
			emulatedwiimotes_enabled->addRange({ { _("AUTO"), "auto" },{ _("ON") , "1" },{ _("OFF"), "0" } }, SystemConf::getInstance()->get(configName + ".emulatedwiimotes"));
			systemConfiguration->addWithLabel(_("EMULATED WIIMOTES"), emulatedwiimotes_enabled);
			systemConfiguration->addSaveFunc([emulatedwiimotes_enabled, configName] { SystemConf::getInstance()->set(configName + ".emulatedwiimotes", emulatedwiimotes_enabled->getSelected()); });
		}
	}

	// citra change screen layout
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::screen_layout))
	{
		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && systemData->getName() == "3ds"))  // only for 3ds
		{
			auto changescreen_layout = std::make_shared<OptionListComponent<std::string>>(mWindow, _("CHANGE SCREEN LAYOUT"));
			changescreen_layout->addRange({ { _("AUTO"), "auto" },{ _("LARGE SCREEN") , "2" },{ _("SIDE BY SIDE"), "3" } }, SystemConf::getInstance()->get(configName + ".layout_option"));
			systemConfiguration->addWithLabel(_("CHANGE SCREEN LAYOUT"), changescreen_layout);
			systemConfiguration->addSaveFunc([changescreen_layout, configName] { SystemConf::getInstance()->set(configName + ".layout_option", changescreen_layout->getSelected()); });
		}
	}

	// psp internal resolution
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::internal_resolution))
	{
		std::string curResol = SystemConf::getInstance()->get(configName + ".internalresolution");

		auto internalresolution = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INTERNAL RESOLUTION"));
		internalresolution->add(_("AUTO"), "auto", curResol.empty() || curResol == "auto");
		internalresolution->add("1:1", "0", curResol == "0");
		internalresolution->add("x1", "1", curResol == "1");
		internalresolution->add("x2", "2", curResol == "2");
		internalresolution->add("x3", "3", curResol == "3");
		internalresolution->add("x4", "4", curResol == "4");
		internalresolution->add("x5", "5", curResol == "5");
		internalresolution->add("x8", "8", curResol == "8");
		internalresolution->add("x10", "10", curResol == "10");

		if (!internalresolution->hasSelection())
			internalresolution->selectFirstItem();

		if (CustomFeatures::FeaturesLoaded || (!CustomFeatures::FeaturesLoaded && (systemData->getName() == "psp" || systemData->getName() == "wii" || systemData->getName() == "gamecube"))) // only for psp, wii, gamecube
		{
			systemConfiguration->addWithLabel(_("INTERNAL RESOLUTION"), internalresolution);
			systemConfiguration->addSaveFunc([internalresolution, configName] { SystemConf::getInstance()->set(configName + ".internalresolution", internalresolution->getSelected()); });
		}
	}

	// Load per-game / per-emulator / per-system custom features
	addFeatures(customFeatures, mWindow, systemConfiguration, configName, systemData->getName(), currentEmulator.empty() ? systemData->getEmulator(true) : currentEmulator, currentCore.empty() ? systemData->getCore(true) : currentCore, _("SETTINGS"), true);

	// automatic controller configuration
	if (systemData->isFeatureSupported(currentEmulator, currentCore, EmulatorFeatures::autocontrollers))
	{
		auto autoControllers = std::make_shared<OptionListComponent<std::string>>(mWindow, _("AUTOCONFIGURE CONTROLLERS"));
		autoControllers->addRange({ { _("AUTO"), "" },{ _("ON"), "0" },{ _("OFF"), "1" } }, SystemConf::getInstance()->get(configName + ".disableautocontrollers"));
		systemConfiguration->addWithLabel(_("AUTOCONFIGURE CONTROLLERS"), autoControllers);
		systemConfiguration->addSaveFunc([configName, autoControllers] { SystemConf::getInstance()->set(configName + ".disableautocontrollers", autoControllers->getSelected()); });
	}

	if (fileData == nullptr && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::EVMAPY) && systemData->isCurrentFeatureSupported(EmulatorFeatures::Features::padTokeyboard))
	{
		if (systemData->hasKeyboardMapping())
			systemConfiguration->addEntry(_("EDIT PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
		else
			systemConfiguration->addEntry(_("CREATE PADTOKEY PROFILE"), true, [mWindow, systemData] { editKeyboardMappings(mWindow, systemData, true); });
	}
	
#ifndef WIN32
	// Set as boot game 
	if (fileData != nullptr)
	{
		std::string gamePath = fileData->getFullPath();

		auto bootgame = std::make_shared<SwitchComponent>(mWindow);
		bootgame->setState(SystemConf::getInstance()->get("global.bootgame.path") == gamePath);
		systemConfiguration->addWithLabel(_("LAUNCH THIS GAME AT STARTUP"), bootgame);
		systemConfiguration->addSaveFunc([bootgame, fileData, gamePath]
		{ 
			if (bootgame->changed()) 
			{
				SystemConf::getInstance()->set("global.bootgame.path", bootgame->getState() ? gamePath : "");
				SystemConf::getInstance()->set("global.bootgame.cmd", bootgame->getState() ? fileData->getlaunchCommand(false) : "");
			}
		});
	}
#endif

	mWindow->pushGui(systemConfiguration);
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createRatioOptionList(Window *window, std::string configname)
{
	auto ratio_choice = std::make_shared<OptionListComponent<std::string> >(window, _("GAME ASPECT RATIO"), false);
	std::string currentRatio = SystemConf::getInstance()->get(configname + ".ratio");
	if (currentRatio.empty())
		currentRatio = std::string("auto");

	std::map<std::string, std::string> *ratioMap = LibretroRatio::getInstance()->getRatio();
	for (auto ratio = ratioMap->begin(); ratio != ratioMap->end(); ratio++)
		ratio_choice->add(_(ratio->first.c_str()), ratio->second, currentRatio == ratio->second);	

	if (!ratio_choice->hasSelection())
		ratio_choice->selectFirstItem();
	
	return ratio_choice;
}

std::shared_ptr<OptionListComponent<std::string>> GuiMenu::createVideoResolutionModeOptionList(Window *window, std::string configname, std::string configoptname, const std::string output) 
{
	auto videoResolutionMode_choice = std::make_shared<OptionListComponent<std::string> >(window, _("VIDEO MODE"), false);

	std::string currentVideoMode = SystemConf::getInstance()->get(configname + "." + configoptname);
	if (currentVideoMode.empty())
		currentVideoMode = std::string("auto");
	
	std::vector<std::string> videoResolutionModeMap = ApiSystem::getInstance()->getVideoModes(output);
	videoResolutionMode_choice->add(_("AUTO"), "auto", currentVideoMode == "auto");
	for (auto videoMode = videoResolutionModeMap.begin(); videoMode != videoResolutionModeMap.end(); videoMode++)
	{
		std::vector<std::string> tokens = Utils::String::split(*videoMode, ':');

		// concatenat the ending words
		std::string vname;
		for (unsigned int i = 1; i < tokens.size(); i++) 
		{
			if (i > 1) 
				vname += ":";

			vname += tokens.at(i);
		}

		videoResolutionMode_choice->add(_(vname.c_str()), tokens.at(0), currentVideoMode == tokens.at(0));
	}

	if (!videoResolutionMode_choice->hasSelection())
		videoResolutionMode_choice->selectFirstItem();

	return videoResolutionMode_choice;
}

std::vector<DecorationSetInfo> GuiMenu::getDecorationsSets(SystemData* system)
{
	std::vector<DecorationSetInfo> sets;
	if (system == nullptr)
		return sets;

	static const size_t pathCount = 3;

	std::vector<std::string> paths = 
	{
#if WIN32
		Paths::getUserEmulationStationPath() + "/decorations", // for win32 testings
#endif
		Paths::getUserDecorationsPath(),
		Paths::getDecorationsPath()
	};
	
	Utils::FileSystem::stringList dirContent;
	std::string folder;

	for (auto path : paths)
	{
		if (!Utils::FileSystem::isDirectory(path))
			continue;

		dirContent = Utils::FileSystem::getDirContent(path);
		for (Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if (Utils::FileSystem::isDirectory(*it))
			{
				folder = *it;

				DecorationSetInfo info;
				info.name = folder.substr(path.size() + 1);
				info.path = folder;

				if (system != nullptr && Utils::String::startsWith(info.name, "default"))
				{
					std::string systemImg = path + "/"+ info.name +"/systems/" + system->getName() + ".png";
					if (Utils::FileSystem::exists(systemImg))
						info.imageUrl = systemImg;
				}

				if (info.imageUrl.empty())
				{
					std::string img = folder + "/default.png";
					if (Utils::FileSystem::exists(img))
						info.imageUrl = img;
				}

				sets.push_back(info);
			}
		}
	}

	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name < b.name; } } compareByName;
	struct { bool operator()(DecorationSetInfo& a, DecorationSetInfo& b) const { return a.name == b.name; } } nameEquals;

	// sort and remove duplicates
	std::sort(sets.begin(), sets.end(), compareByName);
	sets.erase(std::unique(sets.begin(), sets.end(), nameEquals), sets.end());

	return sets;
}


void GuiMenu::openFormatDriveSettings()
{
	Window *window = mWindow;

	auto s = new GuiSettings(mWindow, _("FORMAT DEVICE").c_str());

	// Drive
	auto optionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("DEVICE TO FORMAT"), false);

	std::vector<std::string> disks = ApiSystem::getInstance()->getFormatDiskList();
	if (disks.size() == 0)
		optionsStorage->add(_("NONE"), "", false);
	else 
	{
		for (auto disk : disks)
		{
			auto idx = disk.find(" ");
			if (idx != std::string::npos)
				optionsStorage->add(disk.substr(idx + 1), disk.substr(0, idx), false);
		}
	}

	optionsStorage->selectFirstItem();
	s->addWithLabel(_("DEVICE TO FORMAT"), optionsStorage);

	// File system
	auto fileSystem = std::make_shared<OptionListComponent<std::string> >(window, _("FILE SYSTEM"), false);

	std::vector<std::string> fileSystems = ApiSystem::getInstance()->getFormatFileSystems();
	if (fileSystems.size() == 0)
		fileSystem->add(_("NONE"), "", false);
	else
	{
		for (auto fs : fileSystems)
			fileSystem->add(fs, fs, false);
	}

	fileSystem->selectFirstItem();
	s->addWithLabel(_("FILE SYSTEM"), fileSystem);

	s->addEntry(_("FORMAT NOW"), false, [s, optionsStorage, fileSystem, window]
		{
			std::string disk = optionsStorage->getSelected();
			std::string fs = fileSystem->getSelected();

			if (disk.empty() || fs.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("SELECTED OPTIONS ARE INVALID")));
				return;
			}

			window->pushGui(new GuiMsgBox(window, _("ARE YOU SURE YOU WANT TO FORMAT THIS DRIVE?"), _("YES"), [s, window, disk, fs]
			{
				ThreadedFormatter::start(window, disk, fs);
				s->close();
			}, _("NO"), nullptr));
			
		});

	mWindow->pushGui(s);
}



void GuiMenu::saveSubsetSettings()
{
	auto currentSystem = ViewController::get()->getState().getSystem();
	if (currentSystem == nullptr || currentSystem->getTheme() == nullptr)
		return;

	std::string fileData;

	auto subsets = currentSystem->getTheme()->getSubSetNames();
	for (auto subset : subsets)
	{
		std::string name = subset;
		std::string value;

		if (name == "colorset")
			value = Settings::getInstance()->getString("ThemeColorSet");
		else if (name == "iconset")
			value = Settings::getInstance()->getString("ThemeIconSet");
		else if (name == "menu")
			value = Settings::getInstance()->getString("ThemeMenu");
		else if (name == "systemview")
			value = Settings::getInstance()->getString("ThemeSystemView");
		else if (name == "gamelistview")
			value = Settings::getInstance()->getString("ThemeGamelistView");
		else if (name == "region")
			value = Settings::getInstance()->getString("ThemeRegionName");
		else
		{
			value = Settings::getInstance()->getString("subset." + name);
			name = "subset." + name;
		}

		if (!value.empty())
			fileData += name + "=" + value + "\r";

		for (auto system : SystemData::sSystemVector)
		{
			value = Settings::getInstance()->getString("subset." + system->getThemeFolder() + "." + subset);
			if (!value.empty())
				fileData += "subset." + system->getThemeFolder() + "." + subset + "=" + value + "\r";
		}
	}

	if (!Settings::getInstance()->getString("GamelistViewStyle").empty() && Settings::getInstance()->getString("GamelistViewStyle") != "automatic")
		fileData += "GamelistViewStyle=" + Settings::getInstance()->getString("GamelistViewStyle") + "\r";

	if (!Settings::getInstance()->getString("DefaultGridSize").empty())
		fileData += "DefaultGridSize=" + Settings::getInstance()->getString("DefaultGridSize") + "\r";

	for (auto system : SystemData::sSystemVector)
	{
		auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
		if (!defaultView.empty())
			fileData += system->getName() + ".defaultView=" + defaultView + "\r";

		auto gridSizeOverride = Settings::getInstance()->getString(system->getName() + ".gridSize");
		if (!gridSizeOverride.empty())
			fileData += system->getName() + ".gridSize=" + gridSizeOverride + "\r";
	}

	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string themeSet = Settings::getInstance()->getString("ThemeSet");
	std::string fileName = path + "/" + themeSet + ".cfg";

	if (fileData.empty())
	{
		if (Utils::FileSystem::exists(fileName))
			Utils::FileSystem::removeFile(fileName);
	}
	else
		Utils::FileSystem::writeAllText(fileName, fileData);

}

void GuiMenu::loadSubsetSettings(const std::string themeName)
{
	std::string path = Paths::getUserEmulationStationPath() + "/themesettings";
	if (!Utils::FileSystem::exists(path))
		Utils::FileSystem::createDirectory(path);

	std::string fileName = path + "/" + themeName + ".cfg";
	if (!Utils::FileSystem::exists(fileName))
		return;

	std::string line;
	std::ifstream systemConf(fileName);
	if (systemConf && systemConf.is_open())
	{
		while (std::getline(systemConf, line, '\r'))
		{
			int idx = line.find("=");
			if (idx == std::string::npos || line.find("#") == 0 || line.find(";") == 0)
				continue;

			std::string name = line.substr(0, idx);
			std::string value = line.substr(idx + 1);
			if (!name.empty() && !value.empty())
			{
				if (name == "colorset")
					Settings::getInstance()->setString("ThemeColorSet", value);
				else if (name == "iconset")
					Settings::getInstance()->setString("ThemeIconSet", value);
				else if (name == "menu")
					Settings::getInstance()->setString("ThemeMenu", value);
				else if (name == "systemview")
					Settings::getInstance()->setString("ThemeSystemView", value);
				else if (name == "gamelistview")
					Settings::getInstance()->setString("ThemeGamelistView", value);
				else if (name == "region")
					Settings::getInstance()->setString("ThemeRegionName", value);
				else if (name == "GamelistViewStyle")
					Settings::getInstance()->setString("GamelistViewStyle", value);
				else if (name == "DefaultGridSize")
					Settings::getInstance()->setString("DefaultGridSize", value);
				else if (name.find(".defaultView") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (name.find(".gridSize") != std::string::npos)
					Settings::getInstance()->setString(name, value);
				else if (Utils::String::startsWith(name, "subset."))
					Settings::getInstance()->setString(name, value);
			}
		}
		systemConf.close();

		for (auto system : SystemData::sSystemVector)
		{
			auto defaultView = Settings::getInstance()->getString(system->getName() + ".defaultView");
			auto gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString(system->getName() + ".gridSize"));
			system->setSystemViewMode(defaultView, gridSizeOverride, false);
		}
	}
	else
		LOG(LogError) << "Unable to open " << fileName;
}

void GuiMenu::editKeyboardMappings(Window *window, IKeyboardMapContainer* mapping, bool editable)
{
	window->pushGui(new GuiKeyMappingEditor(window, mapping, editable));
}

bool GuiMenu::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult) pResult->push_back(this); // Always return this as it's a fake fullscreen, so we always have click events
	GuiComponent::hitTest(x, y, parentTransform, pResult);
	return true;
}

bool GuiMenu::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mMenu.isMouseOver())
	{
		delete this;
		return true;
	}

	return (button == 1);
}

#ifdef BATOCERA
void GuiMenu::openUnmountDriveSettings()
{
	Window *window = mWindow;
	auto s = new GuiSettings(mWindow, _("SAFELY EJECT A DISK").c_str());
	auto optionsStorage = std::make_shared<OptionListComponent<std::string>>(window, _("MERGED DRIVE"), false);

	// Ask the manager for a list of merged drives that can be ejected
	std::vector<std::string> merged_drives = ApiSystem::getInstance()->getEjectableDrives();
    
	bool found = false;
	for(const auto& line : merged_drives)
	{
		size_t delimiter = line.find(":");
		if (delimiter != std::string::npos)
		{
			std::string name = line.substr(0, delimiter);
			std::string path = line.substr(delimiter + 1);
			optionsStorage->add(name, path, false);
			found = true;
		}
	}
	
	if (!found) {
		optionsStorage->add(_("NO MERGED DRIVES FOUND"), "", true);
	} else {
		optionsStorage->selectFirstItem();
	}

	s->addWithLabel(_("MERGED DRIVE"), optionsStorage);

	s->addEntry(_("EJECT"), false, [s, optionsStorage, window]
	{
		std::string path = optionsStorage->getSelected();
		if (path.empty()) {
			window->pushGui(new GuiMsgBox(window, _("NO DRIVE SELECTED")));
			return;
		}

		window->pushGui(new GuiMsgBox(window, "ARE YOU SURE YOU WANT TO EJECT THIS DRIVE?\n\nThis will unmount the drive and remove it from the boot configuration.",
			"YES, EJECT", [s, window, path]
			{
				auto* ac = window->createAsyncNotificationComponent();
				ac->updateText(_("Ejecting..."));

				window->postToUiThread([window, ac, path, s]() {
					bool success = ApiSystem::getInstance()->ejectDrive(path);
					
					window->postToUiThread([window, ac, success, s]() {
						ac->close();

						if (success) {
							window->pushGui(new GuiMsgBox(window, "DEVICE EJECTED SAFELY.\nGAME LISTS WILL REFRESH WHEN YOU CLICK OK.", "OK", [window, s] {
								s->close(); 
                                if (ViewController::get()) {
                                    if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning()) {
                                        return;
                                    }
                                    Scripting::fireEvent("update-gamelists");
                                    ViewController::reloadAllGames(window, true, true);
                                }
							}));
						} else {
							window->pushGui(new GuiMsgBox(window, "FAILED TO EJECT DEVICE.", "OK"));
						}
					});
				});
			}, "NO", nullptr));
	});

	mWindow->pushGui(s);
}
#endif
