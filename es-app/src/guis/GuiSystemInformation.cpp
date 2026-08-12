#include "GuiSystemInformation.h"
#include "SystemConf.h"
#include "components/SwitchComponent.h"
#include "ThemeData.h"
#include "ApiSystem.h"
#include "views/UIModeController.h"

// rocknix-info provides these labels dynamically. Keep them visible to
// xgettext and use a dedicated context to avoid collisions such as DEVICE
// (hardware device here, disk drive elsewhere).
#define fake_gettext_system_information       pgettext("system_information", "SYSTEM INFORMATION")
#define fake_gettext_device                  pgettext("system_information", "DEVICE")
#define fake_gettext_operating_system        pgettext("system_information", "OPERATING SYSTEM")
#define fake_gettext_version                 pgettext("system_information", "VERSION")
#define fake_gettext_build_id                pgettext("system_information", "BUILD ID")
#define fake_gettext_kernel                  pgettext("system_information", "KERNEL")
#define fake_gettext_soc_serial_number       pgettext("system_information", "SOC SERIAL NUMBER")
#define fake_gettext_product_serial          pgettext("system_information", "PRODUCT SERIAL")
#define fake_gettext_network_information     pgettext("system_information", "NETWORK INFORMATION")
#define fake_gettext_host_name               pgettext("system_information", "HOST NAME")
#define fake_gettext_ip_address              pgettext("system_information", "IP ADDRESS")
#define fake_gettext_disk_space              pgettext("system_information", "DISK SPACE")
#define fake_gettext_battery_information     pgettext("system_information", "BATTERY INFORMATION")
#define fake_gettext_battery_remaining       pgettext("system_information", "BATTERY REMAINING")
#define fake_gettext_battery_health          pgettext("system_information", "BATTERY HEALTH")
#define fake_gettext_battery_state           pgettext("system_information", "BATTERY STATE")
#define fake_gettext_cpu_information         pgettext("system_information", "CPU INFORMATION")
#define fake_gettext_cpu                     pgettext("system_information", "CPU")
#define fake_gettext_cpu_temperature         pgettext("system_information", "CPU TEMPERATURE")
#define fake_gettext_cpu_fan                 pgettext("system_information", "CPU FAN")
#define fake_gettext_cpu_current_frequency   pgettext("system_information", "CPU CURRENT FREQUENCY")
#define fake_gettext_cpu_maximum_frequency   pgettext("system_information", "CPU MAXIMUM FREQUENCY")
#define fake_gettext_cpu_boost               pgettext("system_information", "CPU BOOST")
#define fake_gettext_current_frequency       pgettext("system_information", "CURRENT FREQUENCY")
#define fake_gettext_maximum_frequency       pgettext("system_information", "MAXIMUM FREQUENCY")
#define fake_gettext_threads                 pgettext("system_information", "THREADS")
#define fake_gettext_gpu_information         pgettext("system_information", "GPU INFORMATION")
#define fake_gettext_gpu_temperature         pgettext("system_information", "GPU TEMPERATURE")
#define fake_gettext_gpu_current_frequency   pgettext("system_information", "GPU CURRENT FREQUENCY")
#define fake_gettext_gpu_maximum_frequency   pgettext("system_information", "GPU MAXIMUM FREQUENCY")
#define fake_gettext_ram_information         pgettext("system_information", "RAM INFORMATION")
#define fake_gettext_ram_available           pgettext("system_information", "RAM AVAILABLE")
#define fake_gettext_ram_capacity            pgettext("system_information", "RAM CAPACITY")

#define fake_gettext_info_good               pgettext("system_information_value", "Good")
#define fake_gettext_info_charging           pgettext("system_information_value", "Charging")
#define fake_gettext_info_discharging        pgettext("system_information_value", "Discharging")
#define fake_gettext_info_full               pgettext("system_information_value", "Full")
#define fake_gettext_info_not_charging       pgettext("system_information_value", "Not charging")
#define fake_gettext_info_enabled            pgettext("system_information_value", "Enabled")
#define fake_gettext_info_disabled           pgettext("system_information_value", "Disabled")
#define fake_gettext_info_not_available      pgettext("system_information_value", "Not Available")
#define fake_gettext_info_offline            pgettext("system_information_value", "Offline")
#define fake_gettext_info_off                pgettext("system_information_value", "OFF")
#define fake_gettext_info_unknown            pgettext("system_information_value", "Unknown")
#define fake_gettext_info_overheat           pgettext("system_information_value", "Overheat")
#define fake_gettext_info_dead               pgettext("system_information_value", "Dead")
#define fake_gettext_info_over_voltage       pgettext("system_information_value", "Over voltage")
#define fake_gettext_info_unspecified        pgettext("system_information_value", "Unspecified failure")
#define fake_gettext_info_cold               pgettext("system_information_value", "Cold")
#define fake_gettext_info_watchdog_expire    pgettext("system_information_value", "Watchdog timer expire")
#define fake_gettext_info_safety_expire      pgettext("system_information_value", "Safety timer expire")
#define fake_gettext_info_over_current       pgettext("system_information_value", "Over current")
#define fake_gettext_info_cores              pgettext("system_information_value", "Cores")
#define fake_gettext_info_community          pgettext("system_information_value", "community")

static std::string localizeInformationLabel(const std::string& label)
{
	if (Utils::String::startsWith(label, "THREADS "))
		return std::string(pgettext("system_information", "THREADS")) + label.substr(7);

	return pgettext("system_information", label.c_str());
}

static std::string localizeInformationValue(const std::string& rawValue)
{
	std::string value = Utils::String::trim(rawValue);
	std::string translated = pgettext("system_information_value", value.c_str());
	if (translated != value)
		return translated;

	const std::string coresSuffix = " Cores)";
	if (Utils::String::endsWith(value, coresSuffix))
		value.replace(value.size() - coresSuffix.size(), coresSuffix.size(), " " + std::string(pgettext("system_information_value", "Cores")) + ")");

	value = Utils::String::replace(value, "(community)", "(" + std::string(pgettext("system_information_value", "community")) + ")");
	return value;
}


GuiSystemInformation::GuiSystemInformation(Window* window) : GuiSettings(window, _("INFORMATION").c_str())
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	bool warning = ApiSystem::getInstance()->isFreeSpaceLimit();

#if !defined(ROCKNIX)
	addGroup(_("INFORMATION"));

	addWithLabel(_("VERSION"), std::make_shared<TextComponent>(window, ApiSystem::getInstance()->getVersion(), font, color));
	addWithLabel(_("USER DISK USAGE"), std::make_shared<TextComponent>(window, ApiSystem::getInstance()->getFreeSpaceUserInfo(), font, warning ? 0xFF0000FF : color));
	addWithLabel(_("SYSTEM DISK USAGE"), std::make_shared<TextComponent>(window, ApiSystem::getInstance()->getFreeSpaceSystemInfo(), font, color));

	#ifndef WIN32
		std::string path = "/media";
		for (const auto & entry : Utils::FileSystem::getDirContent(path)) {
			if (entry != "/media/SHARE" && entry != "/media/BATOCERA") {
				addWithLabel(_("DISK USAGE") + " " + entry, std::make_shared<TextComponent>(window, ApiSystem::getInstance()->getFreeSpaceInfo(entry), font, warning ? 0xFF0000FF : color));
			}
		}
	#endif
#endif

	std::vector<std::string> infos = ApiSystem::getInstance()->getSystemInformations();
	if (infos.size() > 0)
	{
		addGroup(_("SYSTEM"));

		for (auto info : infos)
		{
			std::vector<std::string> tokens = Utils::String::split(info, ':');
			if (tokens.size() >= 2)
			{
				// concatenat the ending words
				std::string vname;
				for (unsigned int i = 1; i < tokens.size(); i++)
				{
					if (i > 1)
					{
						if (tokens.at(0).find("NETWORK IP ADDRESS"))
							vname += ":";
						else
							vname += " ";
					}
					vname += tokens.at(i);
				}

				addWithLabel(localizeInformationLabel(tokens.at(0)), std::make_shared<TextComponent>(window, localizeInformationValue(vname), font, color));
			}
		}
	}

	addGroup(_("VIDEO DRIVER"));
	for (auto info : Renderer::getDriverInformation())
		addWithLabel(_(info.first.c_str()), std::make_shared<TextComponent>(window, info.second, font, color));
}
