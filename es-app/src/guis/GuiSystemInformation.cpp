#include "GuiSystemInformation.h"
#include "SystemConf.h"
#include "components/SwitchComponent.h"
#include "ThemeData.h"
#include "ApiSystem.h"
#include "views/UIModeController.h"

#include <rapidjson/document.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <sstream>
#include "components/TextComponent.h"


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

				addWithLabel(_(tokens.at(0).c_str()), std::make_shared<TextComponent>(window, vname, font, color));
			}
		}
	}

	addGroup(_("VIDEO DRIVER"));
	for (auto info : Renderer::getDriverInformation())
		addWithLabel(_(info.first.c_str()), std::make_shared<TextComponent>(window, info.second, font, color));

	// --- ROCKNIX MEMORY STATUS ---
	{
		std::string json;
		std::array<char, 128> buf;
		// Run status check
		std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("rocknix-memory-manager --json", "r"), pclose);
		
		if (pipe) {
			while (fgets(buf.data(), buf.size(), pipe.get())) json += buf.data();
			
			rapidjson::Document doc;
			doc.Parse(json.c_str());
			
			if (!doc.HasParseError() && doc.IsObject()) {
				addGroup(_("MEMORY SUBSYSTEM"));

				// ZRAM
				std::string zramText = "Inactive";
				if (doc.HasMember("zram") && doc["zram"].IsObject() && doc["zram"]["enabled"].GetBool()) {
					int size = doc["zram"]["size_mb"].GetInt();
					int comp = doc["zram"]["compressed_size_mb"].GetInt();
					std::string algo = doc["zram"]["algo"].GetString();
					std::stringstream ss;
					ss << "Active (" << size << "MB, " << algo << ")";
					// Only show compression detail if space is actually used
					if (comp > 0) ss << " [RAM: " << comp << "MB]";
					zramText = ss.str();
				}
				addWithLabel(_("ZRAM STATUS"), std::make_shared<TextComponent>(window, zramText, font, color));

				// Swap
				std::string swapText = "Inactive";
				if (doc.HasMember("swap") && doc["swap"].IsObject()) {
					int sSize = doc["swap"]["size_mb"].GetInt();
					if (sSize > 0) {
						swapText = std::to_string(sSize) + " MB";
					}
				}
				addWithLabel(_("SWAP FILE"), std::make_shared<TextComponent>(window, swapText, font, color));

				// KSM
				std::string ksmText = "Inactive";
				if (doc.HasMember("ksm") && doc["ksm"].IsObject() && doc["ksm"]["enabled"].GetBool()) {
					int saved = doc["ksm"]["saved_mb"].GetInt();
					ksmText = "Active";
					if (saved > 0) ksmText += " (Saved: " + std::to_string(saved) + "MB)";
				}
				addWithLabel(_("KSM DEDUPLICATION"), std::make_shared<TextComponent>(window, ksmText, font, color));
			}
		}
	}
}
