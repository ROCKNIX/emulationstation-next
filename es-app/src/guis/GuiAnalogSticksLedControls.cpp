#include "GuiAnalogSticksLedControls.h"

#include <cstdlib>
#include "components/SliderComponent.h"
#include "SystemConf.h"
#include "utils/Platform.h"
#include "utils/StringUtil.h"

GuiAnalogSticksLedControls::GuiAnalogSticksLedControls(Window* window) : GuiSettings(window, _("ANALOG STICKS LEDS").c_str())
{
	float brightness_value = 0.f;
	float left_red_value = 0.f, left_green_value = 0.f, left_blue_value = 0.f;
	float right_red_value = 0.f, right_green_value = 0.f, right_blue_value = 0.f;

	std::string line = SystemConf::getInstance()->get("analogsticks.led");
	if (!line.empty()) {
		std::vector<std::string> tokens = Utils::String::split(line, ' ');
		if (tokens.size() == 7) {
			brightness_value = (float) std::atoi(tokens[0].c_str()) / 255;
			right_red_value = (float) std::atoi(tokens[1].c_str()) / 255;
			right_green_value = (float) std::atoi(tokens[2].c_str()) / 255;
			right_blue_value = (float) std::atoi(tokens[3].c_str()) / 255;
			left_red_value = (float) std::atoi(tokens[4].c_str()) / 255;
			left_green_value = (float) std::atoi(tokens[5].c_str()) / 255;
			left_blue_value = (float) std::atoi(tokens[6].c_str()) / 255;
		}
	}

	auto brightness = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	brightness->setValue(brightness_value);
	addWithLabel(_("BRIGHTNESS"), brightness);

	addGroup(_("LEFT"));
	auto left_red = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	left_red->setValue(left_red_value);
	addWithLabel(_("RED"), left_red);

	auto left_green = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	left_green->setValue(left_green_value);
	addWithLabel(_("GREEN"), left_green);

	auto left_blue = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	left_blue->setValue(left_blue_value);
	addWithLabel(_("BLUE"), left_blue);

	addGroup(_("RIGHT"));
	auto right_red = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	right_red->setValue(right_red_value);
	addWithLabel(_("RED"), right_red);

	auto right_green = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	right_green->setValue(right_green_value);
	addWithLabel(_("GREEN"), right_green);

	auto right_blue = std::make_shared<SliderComponent>(mWindow, 0.f, 1.f, 0.1f);
	right_blue->setValue(right_blue_value);
	addWithLabel(_("BLUE"), right_blue);

	const std::string analogSticksLedScript = "/usr/bin/analog_sticks_ledcontrol";
	mMenu.addButton(_("APPLY"), "apply", [this, analogSticksLedScript, brightness,
		left_red, left_green, left_blue, right_red, right_green, right_blue]
	{
		char buffer[128];

		sprintf(buffer, "%u %u %u %u %u %u %u",
			static_cast<uint8_t>(brightness->getValue() * 255),
			static_cast<uint8_t>(right_red->getValue() * 255),
			static_cast<uint8_t>(right_green->getValue() * 255),
			static_cast<uint8_t>(right_blue->getValue() * 255),
			static_cast<uint8_t>(left_red->getValue() * 255),
			static_cast<uint8_t>(left_green->getValue() * 255),
			static_cast<uint8_t>(left_blue->getValue() * 255)
		);
		
		Utils::Platform::runSystemCommand(analogSticksLedScript + " " + buffer, "", nullptr);
		SystemConf::getInstance()->set("analogsticks.led", buffer);
	});
}
