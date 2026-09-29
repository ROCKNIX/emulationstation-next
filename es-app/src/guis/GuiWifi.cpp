#include "guis/GuiWifi.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "components/TextComponent.h"
#include "ApiSystem.h"
#include "GuiLoading.h"
#include "LocaleES.h"
#include "Log.h"
#include "Settings.h"
#include "SystemConf.h"
#include "ThemeData.h"
#include "Window.h"
#include "utils/StringUtil.h"

GuiWifi::GuiWifi(Window* window, const std::string& title, const std::function<void()>& onJoined)
	: GuiComponent(window), mMenu(window, title.c_str()), mTitle(title), mOnJoined(onJoined), mSavedKnown(true), mWaitingLoad(false)
{
	addChild(&mMenu);

	// Fetched off the interface thread. wifictl list is itself a rescan --
	// it waits for the adapter, asks NetworkManager to scan, sleeps, then
	// lists -- so this constructor used to run up to twenty seconds of
	// nmcli with nothing drawn, and with NetworkManager unresponsive (fork
	// #102) nothing bounded it at all. Posted rather than pushed here,
	// because the page has to be on the stack before anything can go over
	// it; an empty first answer still turns into the full scan it always did.
	mWindow->postToUiThread([this]() { onRefresh(false); });

	mMenu.addButton(_("REFRESH"), "refresh", [&] { onRefresh(); });
	mMenu.addButton(_("INPUT MANUALLY"), "manual input", [&] { onManualInput(); });
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiWifi::load(const std::vector<WifiText::PickerRow>& rows)
{
	mMenu.clear();

	// A list whose SAVED marks could not be read says so, once, under the
	// title (#308 F-WF-03): the rows are then neither saved nor unsaved, and
	// a press on one asks before it acts.
	mMenu.setSubTitle(mSavedKnown ? "" : _("COULDN'T CHECK WHICH NETWORKS ARE SAVED."));

	if (rows.empty())
		mMenu.addEntry(_("NO WI-FI NETWORKS FOUND"), false, [this] { onRefresh(); });
	else
		for (const auto& row : rows)
			addRow(row);

	mMenu.updateSize();

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);

	mWaitingLoad = false;
}

// The name as NetworkManager has it -- case and spaces kept, since a
// network's name is what the player recognises it by (addEntry would
// upper-case it) -- and, on the right at the row's own weight, CONNECTED for
// the network the device is on or SAVED for one it holds a profile for: the
// shape and the word of MANAGE SAVED NETWORKS' mark, a fact beside the
// action.
void GuiWifi::addRow(const WifiText::PickerRow& network)
{
	auto theme = ThemeData::getMenuTheme();
	ComponentListRow row;

	auto name = std::make_shared<TextComponent>(mWindow, network.name, theme->Text.font, theme->Text.color);
	if (EsLocale::isRTL())
		name->setHorizontalAlignment(Alignment::ALIGN_RIGHT);
	row.addElement(name, true);

	if (network.connected || network.saved)
	{
		const std::string mark = network.connected ? _("CONNECTED") : _("SAVED");
		auto label = std::make_shared<TextComponent>(mWindow, mark, theme->Text.font, theme->Text.color, Alignment::ALIGN_RIGHT);
		label->setSize(theme->Text.font->sizeText(mark + "  ").x(), 0);
		row.addElement(label, false);
	}

	const WifiText::PickerRow chosen = network;
	row.makeAcceptInputHandler([this, chosen] { onSelect(chosen); });
	mMenu.addRow(row);
}

// A press: the network the device is on joins again with the key
// NetworkManager holds -- it answers at once, or comes back if it had
// dropped (#308 F-WF-05: this used to close on the list's snapshot); a
// saved one is offered CONNECT / FORGET / CANCEL (#318, D-UI-118); any
// other is asked for its key; one whose profile could not be asked about
// is asked about again first (F-WF-03/06). WifiText::pressAction decides.
void GuiWifi::onSelect(const WifiText::PickerRow& row)
{
	if (mWaitingLoad)
		return;

	act(WifiText::pressAction(row), row.name, WifiText::joinName(row));
}

// A join goes by the profile's name, which need not be the SSID the row
// shows (#308 claude F-WF-12, gpt F-WF-03): the connected row joined by its
// SSID, which is no profile's name for a renamed one, and the player read
// COULDN'T CONNECT, with forget-and-rejoin advice, about the network they
// were on.
void GuiWifi::act(WifiText::PressAction action, const std::string& name, const std::string& joinAs)
{
	switch (action)
	{
	case WifiText::PressAction::Join:
		join(name, joinAs);
		break;
	case WifiText::PressAction::ChooseSaved:
	{
		// A saved network the device is not on: CONNECT with the key
		// NetworkManager holds, or FORGET it -- the second choice is the one
		// ROCKNIX's own saved-Wi-Fi work offers, behind the manage page's
		// confirmation and reader (GuiMenu::forgetWifiNetworkWithConfirmation),
		// which rebuilds this list once the profile is gone. CONNECT first;
		// CANCEL last, so B lands on it (es-ui-style-guide: back never lands
		// on the destructive choice). The profile is forgotten by the name
		// NetworkManager has for it (joinAs), which is the name the manage
		// page shows.
		const std::string text = Utils::String::format(_("%s IS SAVED.").c_str(), name.c_str())
			+ "\n\n" + _("CONNECT WITH ITS SAVED KEY, OR FORGET IT?");
		mWindow->pushGui(new GuiMsgBox(mWindow, text,
			_("CONNECT"), [this, name, joinAs] { join(name, joinAs); },
			_("FORGET"), [this, joinAs] { GuiMenu::forgetWifiNetworkWithConfirmation(mWindow, joinAs, false, [this] { onRefresh(false); }); },
			_("CANCEL"), nullptr));
		break;
	}
	case WifiText::PressAction::AskKey:
		askKeyAndConnect(name);
		break;
	case WifiText::PressAction::CheckAgain:
		// Never taken for a network with no profile: the key path rebuilds a
		// profile of that name from what is typed.
		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("COULDN'T CHECK WHICH NETWORKS ARE SAVED.") + std::string("\n\n") + _("CHECK AGAIN NOW?"),
			_("YES"), [this] { onRefresh(false); },
			_("NO"), nullptr));
		break;
	}
}

// INPUT MANUALLY: a hidden network's name, taken as a row would be -- a
// name NetworkManager holds a profile for joins, the one the device is on
// needs nothing, any other is asked for its key. Not trimmed: a name may
// begin or end with a space, and it is matched as NetworkManager has it.
void GuiWifi::onManualInput()
{
	auto onName = [this](const std::string& name)
	{
		if (name.empty())
			return;
		act(WifiText::manualAction(name, mCurrent, mSaved, mSavedKnown), name,
			WifiText::manualJoinName(name, mCurrent, mCurrentProfile));
	};

	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, mTitle, "", onName, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, mTitle, "", onName, false));
}

// A saved network: its profile, with the key NetworkManager holds, behind
// the spinner -- wifictl join waits up to 90 s for the association, as
// connect does, and the screen would otherwise freeze for it. The script
// also moves the settings wifi.ssid and wifi.key onto the joined network,
// so the paths that connect from the settings (the restore wizard, the
// ENABLE WI-FI switch) still name the network the player is on; SystemConf
// is re-read so the interface sees the same file.
void GuiWifi::join(const std::string& name, const std::string& profile)
{
	Window* window = mWindow;
	mWaitingLoad = true;
	LOG(LogInfo) << "wifi picker: joining the saved network " << name << " (profile " << profile << ")";
	window->pushGui(new GuiLoading<WifiText::JoinAnswer>(window, _("CONNECTING TO WI-FI"),
		[profile](IGuiLoadingHandler*) { return ApiSystem::getInstance()->joinWifiNetwork(profile); },
		[this, window, name, profile](WifiText::JoinAnswer answer)
		{
			mWaitingLoad = false;
			const int code = answer.code;
			if (!answer.joined())
			{
				LOG(LogWarning) << "wifi picker: could not join the saved network " << name << " (profile " << profile << ", wifictl join exited " << code << ")";
				// The advice by what happened (WifiText::joinFailure, #308
				// F-WF-03/06): NetworkManager not answering is not a key that
				// changed, and forgetting the network on that advice would
				// lose a profile that was fine.
				const std::string advice = WifiText::joinFailure(code) == WifiText::JoinFailure::ServiceNotAnswering
					? _("THE WI-FI SERVICE DIDN'T ANSWER. TRY AGAIN IN A MOMENT.")
					: _("IF ITS KEY HAS CHANGED, FORGET IT UNDER MANAGE SAVED NETWORKS AND JOIN IT AGAIN WITH THE NEW KEY.");
				window->pushGui(new GuiMsgBox(window,
					Utils::String::format(_("COULDN'T CONNECT TO %s.").c_str(), name.c_str()) + "\n\n" + advice, _("OK")));
				return;
			}
			// Re-read for what the join wrote (wifi.ssid, wifi.key), keeping
			// any change this interface has not saved yet -- one the settings
			// lock refused -- unless the join wrote that key (G-E1-03).
			SystemConf::getInstance()->loadSystemConf(true);
			joined(name);
		}));
}

// Any other network: its key first -- empty for an open network -- then
// the connection wifictl connect makes, which leaves a profile behind for
// next time. On success the settings take the network and its key, as they
// did when the row edited them: the network configured last, for the paths
// that connect from the settings.
void GuiWifi::askKeyAndConnect(const std::string& name)
{
	auto onKey = [this, name](const std::string& key) { connect(name, key); };

	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("WI-FI KEY"), "", onKey, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, _("WI-FI KEY"), "", onKey, false));
}

void GuiWifi::connect(const std::string& name, const std::string& key)
{
	Window* window = mWindow;
	mWaitingLoad = true;
	LOG(LogInfo) << "wifi picker: connecting to " << name;
#if !WIN32
	const std::string country = SystemConf::getInstance()->get("wifi.country");
	auto apply = [name, key, country](IGuiLoadingHandler*) { return ApiSystem::getInstance()->enableWifi(name, key, country); };
#else
	auto apply = [name, key](IGuiLoadingHandler*) { return ApiSystem::getInstance()->enableWifi(name, key); };
#endif
	window->pushGui(new GuiLoading<bool>(window, _("CONNECTING TO WI-FI"), apply,
		[this, window, name, key](bool ok)
		{
			mWaitingLoad = false;
			if (!ok)
			{
				LOG(LogWarning) << "wifi picker: could not connect to " << name;
				window->pushGui(new GuiMsgBox(window,
					Utils::String::format(_("COULDN'T CONNECT TO %s.").c_str(), name.c_str()) + "\n\n" + _("CHECK THE KEY AND TRY AGAIN."), _("OK")));
				return;
			}
			SystemConf::getInstance()->set("wifi.ssid", name);
			SystemConf::getInstance()->set("wifi.key", key);
			SystemConf::getInstance()->saveSystemConf();
			joined(name);
		}));
}

// The picker's job is done: the toast says so over whatever the page
// returns to, and the page that opened the picker rebuilds to read the
// connection back. The callback is copied out first -- it belongs to that
// page, and this component is gone before it runs.
void GuiWifi::joined(const std::string& name)
{
	Window* window = mWindow;
	auto onJoined = mOnJoined;
	// <glyph> <subject> : <outcome>, one space after the glyph, as every
	// toast (#308 F-WF-08); CONNECTED is the row's word for the same fact.
	window->displayNotificationMessage(_U("\uF058 ") + WifiText::joinedNotice(name, _("CONNECTED")));
	delete this;
	if (onJoined != nullptr)
		onJoined();
}

bool GuiWifi::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		if (!mWaitingLoad)
			delete this;

		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiWifi::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}

// rescan: ask NetworkManager for a fresh scan first (REFRESH, and an empty
// first list); false lists what it already knows, which is what the page
// opens with. The saved profiles and the network joined now come with the
// same wait, behind the spinner, each time-boxed in ApiSystem.
void GuiWifi::onRefresh(bool rescan)
{
	Window* window = mWindow;
	mWaitingLoad = true;

	window->pushGui(new GuiLoading<Answer>(window, _("SEARCHING WI-FI NETWORKS"),
		[rescan](IGuiLoadingHandler*)
		{
			// Each getter says whether it could ask at all (true for an
			// answer, "none" included), and that is kept: "could not ask"
			// used to be read as "none" (#308 F-WF-03/06).
			Answer answer;
			answer.inRange = ApiSystem::getInstance()->getWifiNetworks(rescan);
			answer.savedKnown = ApiSystem::getInstance()->getSavedWifiNetworks(answer.saved);
			answer.currentKnown = ApiSystem::getInstance()->getCurrentWifiSsid(answer.current);
			return answer;
		},
		[this, rescan](Answer answer)
		{
			mWaitingLoad = false;
			if (answer.inRange.empty() && !rescan)
			{
				onRefresh(true);
				return;
			}
			const std::vector<WifiText::PickerRow> rows = WifiText::pickerRows(answer.inRange, answer.saved, answer.current,
				answer.savedKnown, answer.currentKnown);
			mSaved = answer.saved;
			mSavedKnown = answer.savedKnown;
			// The network the device is on, as the rows have it: from the
			// saved list's active profile when current could not say.
			mCurrent.clear();
			mCurrentProfile.clear();
			for (const auto& row : rows)
				if (row.connected)
				{
					mCurrent = row.name;
					mCurrentProfile = row.profile;
				}
			load(rows);
		}));
}
