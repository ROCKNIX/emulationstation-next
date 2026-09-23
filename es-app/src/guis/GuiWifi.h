#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"

#include <thread>

class GuiWifi : public GuiComponent
{
public:
	GuiWifi(Window* window, const std::string title);
	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void	load(std::vector<std::string> ssids);

	void	onManualInput();
	void	onRefresh();

	void connectNetwork(const std::string& ssid, bool isSaved);

	MenuComponent mMenu;

	std::string mTitle;

	bool		mWaitingLoad;
};
