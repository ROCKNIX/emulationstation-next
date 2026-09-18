#include "GuiRetroAchievementsSettings.h"
#include "ThreadedHasher.h"
#include "GuiHashStart.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "RetroAchievements.h"
#include "utils/Platform.h"

#include "guis/GuiMsgBox.h"
#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"

GuiRetroAchievementsSettings::GuiRetroAchievementsSettings(Window* window) : GuiSettings(window, _("RETROACHIEVEMENTS SETTINGS").c_str())
{
	addGroup(_("SETTINGS"));

	bool retroachievementsEnabled = SystemConf::getInstance()->getBool("global.retroachievements");
	std::string username = SystemConf::getInstance()->get("global.retroachievements.username");
	std::string password = SystemConf::getInstance()->get("global.retroachievements.password");

	// retroachievements_enable
	auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_enabled->setState(retroachievementsEnabled);
	addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);
	
	// retroachievements, username, password
	addInputTextConfigRow(_("USERNAME"), "global.retroachievements.username", false);
	addInputTextConfigRow(_("PASSWORD"), "global.retroachievements.password", true);
#ifndef CHEEVOS_DEV_LOGIN
	// A fork build compiles no developer pair in, so the menu pages authenticate
	// with the player's own web API key, from the account's settings page on
	// retroachievements.org (#68). Held back from backups like the password.
	addInputTextConfigRow(_("WEB API KEY"), "global.retroachievements.key", true);
#endif

	addGroup(_("OPTIONS"));

	addSwitch(_("HARDCORE MODE"), _("Disable loading states, rewind and cheats for more points."), "global.retroachievements.hardcore", false, nullptr);
	addSwitch(_("LEADERBOARDS"), _("Compete in high-score and best time leaderboards (requires hardcore)."), "global.retroachievements.leaderboards", false, nullptr);
	addSwitch(_("VERBOSE MODE"), _("Show achievement progression on game launch and other notifications."), "global.retroachievements.verbose", false, nullptr);
	addSwitch(_("RICH PRESENCE"), "global.retroachievements.richpresence", false);
	addSwitch(_("ENCORE MODE"), _("Unlocked achievements can be earned again."), "global.retroachievements.encore", false, nullptr);
	addSwitch(_("AUTOMATIC SCREENSHOT"), _("Automatically take a screenshot when an achievement is earned."), "global.retroachievements.screenshot", false, nullptr);
	addSwitch(_("CHALLENGE INDICATORS"), _("Shows icons in the bottom right corner when eligible achievements can be earned."), "global.retroachievements.challenge_indicators", false, nullptr);
	// RetroArch's progress tracker is a separate widget from the challenge
	// indicators -- it counts measured progress (34/99 rings) while an
	// indicator marks a live "do X without Y" achievement -- and it had no
	// switch here, so it could not be turned off (maintainer, 2026-09-07).
	// On by default, as RetroArch ships it: absent reads as on, so an
	// upgraded device shows the state it is actually in. Written by hand
	// rather than through addSwitch, whose bool means "store in Settings",
	// not "default" -- the first version passed true there and the value
	// went to es_settings.cfg, where the launch script never looks.
	auto progressTracker = std::make_shared<SwitchComponent>(mWindow);
	progressTracker->setState(SystemConf::getInstance()->getBool("global.retroachievements.progress_tracker", true));
	addWithDescription(_("PROGRESS TRACKER"), _("Shows how far you are toward an achievement while you play."), progressTracker);
	addSaveFunc([progressTracker] { SystemConf::getInstance()->setBool("global.retroachievements.progress_tracker", progressTracker->getState()); });
	addSwitch(_("UNOFFICIAL ACHIEVEMENTS"), _("Enable unlocking of unofficial achievements."), "global.retroachievements.unofficial", false, nullptr);

	// Unlock sound
	auto installedRSounds = ApiSystem::getInstance()->getRetroachievementsSoundsList();
	if (installedRSounds.size() > 0)
	{
		std::string currentSound = SystemConf::getInstance()->get("global.retroachievements.sound");

		auto rsounds_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("RETROACHIEVEMENTS UNLOCK SOUND"), false);
		rsounds_choices->add(_("none"), "none", currentSound.empty() || currentSound == "none");

		for (auto snd : installedRSounds)
			rsounds_choices->add(_(Utils::String::toUpper(snd).c_str()), snd, currentSound == snd);

		if (!rsounds_choices->hasSelection())
			rsounds_choices->selectFirstItem();

		addWithLabel(_("UNLOCK SOUND"), rsounds_choices);
		addSaveFunc([rsounds_choices] { SystemConf::getInstance()->set("global.retroachievements.sound", rsounds_choices->getSelected()); });
	}

#if defined(ROCKNIX)
        if (Utils::Platform::GetEnv("DEVICE_ANALOG_STICKS_LED_CONTROL") == "true") {
                // Enable LED Notifications
                auto cheevos_led_enabled = std::make_shared<SwitchComponent>(mWindow);
                bool cheevosledenabled = SystemConf::getInstance()->get("global.retroachievements.leds") == "1";
                cheevos_led_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.leds"));
                //s->addWithLabel(_("LED NOTIFICATIONS"), cheevos_led_enabled);
                addWithLabel(_("LED NOTIFICATIONS"), cheevos_led_enabled);
                cheevos_led_enabled->setOnChangedCallback([cheevos_led_enabled] {
                        bool cheevosledenabled = cheevos_led_enabled->getState();
                                SystemConf::getInstance()->set("global.retroachievements.leds", cheevosledenabled ? "1" : "0");
                });
        }
#endif
	// retroachievements_hardcore_mode
	addSwitch(_("SHOW RETROACHIEVEMENTS ENTRY IN MAIN MENU"), _("View your RetroAchievements stats right from the main menu!"), "RetroachievementsMenuitem", true, nullptr);

	addGroup(_("GAME INDEXES"));
	addSwitch(_("INDEX NEW GAMES AT STARTUP"), "CheevosCheckIndexesAtStart", true);
	addEntry(_("INDEX GAMES"), true, [this]
	{
		if (ThreadedHasher::checkCloseIfRunning(mWindow))
			mWindow->pushGui(new GuiHashStart(mWindow, ThreadedHasher::HASH_CHEEVOS_MD5));
	});

	addSaveFunc([retroachievementsEnabled, retroachievements_enabled, username, password, window]
	{
		bool newState = retroachievements_enabled->getState();
		std::string newUsername = SystemConf::getInstance()->get("global.retroachievements.username");
		std::string newPassword = SystemConf::getInstance()->get("global.retroachievements.password");
		std::string token = SystemConf::getInstance()->get("global.retroachievements.token");

		if (newState && (!retroachievementsEnabled || username != newUsername || password != newPassword || token.empty()))
		{
			std::string tokenOrError;
			if (RetroAchievements::testAccount(newUsername, newPassword, tokenOrError))
			{
				SystemConf::getInstance()->set("global.retroachievements.token", tokenOrError);
			}
			else
			{
				SystemConf::getInstance()->set("global.retroachievements.token", "");

				window->pushGui(new GuiMsgBox(window, _("UNABLE TO ACTIVATE RETROACHIEVEMENTS:") + "\n" + tokenOrError, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				retroachievements_enabled->setState(false);
				newState = false;
			}
		}
		else if (!newState)
			SystemConf::getInstance()->set("global.retroachievements.token", "");

		if (SystemConf::getInstance()->setBool("global.retroachievements", newState))
			if (!ThreadedHasher::isRunning() && newState)
				ThreadedHasher::start(window, ThreadedHasher::HASH_CHEEVOS_MD5, false, true);
	});
}
