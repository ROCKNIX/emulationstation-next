#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include <string>
#include <algorithm>
#include "Log.h"
#include "Settings.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "LocaleES.h"
#include "GuiWifi.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "GuiLoading.h"

GuiWifi::GuiWifi(Window* window, const std::string title)
	: GuiComponent(window), mMenu(window, title.c_str())
{
	mTitle = title;
	mWaitingLoad = false;

	auto theme = ThemeData::getMenuTheme();

	addChild(&mMenu);

	mWindow->postToUiThread([this]() { onRefresh(); });

	mMenu.addButton(_("REFRESH"), "refresh", [&] { onRefresh(); });
	mMenu.addButton(_("INPUT MANUALLY"), "manual input", [&] { onManualInput(); });
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiWifi::load(std::vector<std::string> availableNetworks)
{
	mMenu.clear();

	std::vector<std::string> savedNetworks = ApiSystem::getInstance()->getSavedWifiNetworks();
	std::string currentSSID = SystemConf::getInstance()->get("wifi.ssid");
	bool isSystemConnected = ApiSystem::getInstance()->ping();

	// Add unique networks from savedNetworks just in case they are not currently in range
	for (const auto& saved : savedNetworks) {
		if (std::find(availableNetworks.begin(), availableNetworks.end(), saved) == availableNetworks.end()) {
			availableNetworks.push_back(saved);
		}
	}

	if (availableNetworks.empty()) {
		mMenu.addEntry(_("NO WI-FI NETWORKS FOUND"), false, std::bind(&GuiWifi::onRefresh, this));
	} else {
		for (const auto& ssid : availableNetworks) {
			bool isSaved = (std::find(savedNetworks.begin(), savedNetworks.end(), ssid) != savedNetworks.end());
			bool isConnected = (ssid == currentSSID && isSystemConnected);

			std::string displayName = ssid;
			if (isConnected) {
				displayName += " (" + _("CONNECTED") + ")";
			}
			else if (isSaved) {
				displayName += " (" + _("SAVED") + ")";
			}

			mMenu.addEntry(displayName, true, [this, ssid, isSaved] {
				Window* window = mWindow;
				if (isSaved) {
					window->pushGui(new GuiMsgBox(window, _("NETWORK OPTIONS FOR: ") + ssid,
						_("CONNECT"), [this, ssid, isSaved] { connectNetwork(ssid, isSaved); },
						_("FORGET"), [this, window, ssid] {
							ApiSystem::getInstance()->forgetWifiNetwork(ssid);
							window->pushGui(new GuiMsgBox(window, _("NETWORK FORGOTTEN"), _("OK"), [this] {
								this->load(ApiSystem::getInstance()->getWifiNetworks());
							}));
						},
						_("CANCEL"), nullptr
					));
				} else {
					window->pushGui(new GuiMsgBox(window, _("NETWORK OPTIONS FOR: ") + ssid,
						_("CONNECT"), [this, ssid, isSaved] { connectNetwork(ssid, isSaved); },
						_("CANCEL"), nullptr,
						"", nullptr
					));
				}
			});
		}
	}

	mMenu.updateSize();

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);

	mWaitingLoad = false;
}

void GuiWifi::connectNetwork(const std::string& ssid, bool isSaved)
{
	Window* window = mWindow;
	if (isSaved) {
		window->pushGui(new GuiLoading<bool>(window, _("CONNECTING..."),
			[ssid](auto gui) {
				return ApiSystem::getInstance()->connectSavedWifiNetwork(ssid);
			},
			[this, window, ssid](bool success) {
				if (success) {
					SystemConf::getInstance()->set("wifi.ssid", ssid);
					SystemConf::getInstance()->set("wifi.key", "");
					SystemConf::getInstance()->saveSystemConf();
					window->pushGui(new GuiMsgBox(window, _("CONNECTED SUCCESSFULLY"), _("OK"), [this] {
						this->load(ApiSystem::getInstance()->getWifiNetworks());
					}));
				} else {
					window->pushGui(new GuiMsgBox(window, _("FAILED TO CONNECT"), _("OK")));
				}
			}
		));
	} else {
		// Prompt for password if the network is not saved
		auto keyboardCallback = [this, window, ssid](const std::string& password) {
			window->pushGui(new GuiLoading<bool>(window, _("CONNECTING..."),
				[ssid, password](auto gui) {
					SystemConf::getInstance()->set("wifi.ssid", ssid);
					SystemConf::getInstance()->set("wifi.key", password);
					SystemConf::getInstance()->saveSystemConf();

					#if !WIN32
						std::string country = SystemConf::getInstance()->get("wifi.country");
						return ApiSystem::getInstance()->enableWifi(ssid, password, country);
					#else
						return ApiSystem::getInstance()->enableWifi(ssid, password);
					#endif
				},
				[this, window, ssid](bool success) {
					if (success) {
						window->pushGui(new GuiMsgBox(window, _("CONNECTED SUCCESSFULLY"), _("OK"), [this] {
							this->load(ApiSystem::getInstance()->getWifiNetworks());
						}));
					} else {
						window->pushGui(new GuiMsgBox(window, _("FAILED TO CONNECT"), _("OK")));
					}
				}
			));
		};

		if (Settings::getInstance()->getBool("UseOSK"))
			window->pushGui(new GuiTextEditPopupKeyboard(window, _("WI-FI KEY FOR ") + ssid, "", keyboardCallback, false));
		else
			window->pushGui(new GuiTextEditPopup(window, _("WI-FI KEY FOR ") + ssid, "", keyboardCallback, false));
	}
}

void GuiWifi::onManualInput()
{
	auto keyboardCallback = [this](const std::string& ssid) {
		connectNetwork(ssid, false);
	};

	if (Settings::getInstance()->getBool("UseOSK"))
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("ENTER WI-FI SSID"), "", keyboardCallback, false));
	else
		mWindow->pushGui(new GuiTextEditPopup(mWindow, _("ENTER WI-FI SSID"), "", keyboardCallback, false));
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

void GuiWifi::onRefresh()
{
	Window* window = mWindow;

	mWindow->pushGui(new GuiLoading<std::vector<std::string>>(mWindow, _("SEARCHING WI-FI NETWORKS"),
		[this, window](auto gui)
		{
			mWaitingLoad = true;
			return ApiSystem::getInstance()->getWifiNetworks(true);
		},
		[this, window](std::vector<std::string> ssids)
		{
			mWaitingLoad = false;
			load(ssids);
		}));
}
