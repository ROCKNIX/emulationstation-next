#include "guis/GuiMoonlight.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <map>
#include <vector>

#include "ApiSystem.h"
#include "Scripting.h"
#include "Window.h"
#include "components/OptionListComponent.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "utils/Platform.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "SystemConf.h"
#include "HttpReq.h"
#include <pugixml.hpp>

class MoonlightClient {
 public:
  MoonlightClient(const std::string& server_ip)
   : server_ip_(server_ip) {}
  
  // Requests to Moonlight server
  std::string GetAppListXml() { return MakeRequest(MakeUrl("applist")); }
  void QuitApp() { MakeRequest(MakeUrl("cancel")); }
  bool WriteBoxArtFile(const std::string& app_id, std::string filename);

  // Helpers
  bool UpdateMoonlightGames();

 private:
  std::string MakeUrl(const std::string& command, std::string args={});
  std::string MakeRequest(const std::string& url, std::string* filename = nullptr);
  static std::string CreateNewGuid();

  std::string server_ip_;
};

// Result of a pairing attempt, passed back to the UI thread
struct MoonlightPairResult {
  bool paired = false;
  std::string server_ip;
};

// Moonlight version check function
bool isEmbedded(void) {
  FILE* mlver = popen ("moonlight -v", "r");
  if (mlver == nullptr) {
    return false;
  }

  std::string output;
  char buffer[256];
  while (fgets(buffer, sizeof buffer, mlver) != nullptr) {
    output += buffer;
  }
  pclose(mlver);

  return output.find("Embedded") != std::string::npos;
}

// SSL extraction function (client.pem)
void extractClient() {
  std::ifstream confFile("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf");
  std::ofstream clientFile("/storage/.cache/Moonlight Game Streaming Project/client.pem");
  std::string line;
  do{
    std::getline(confFile, line);
  } 
  while (line.find("certificate=") == std::string::npos);
  if (line.find("certificate=\"") != std::string::npos) {
    line.erase(0, 24);
    line.erase(line.size()-2);
  } else {
    line.erase(0, 23);
    line.erase(line.size()-1);
  }
  std::string::size_type pos = 0;
  do{
    pos = line.find("\\n", pos);
    line.replace(pos, 2, "\n");
  }
  while (line.find("\\n", pos) != std::string::npos);
  clientFile << line;
}

// SSL extraction function (key.pem)
void extractKey() {
  std::ifstream confFile("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf");
  std::ofstream keyFile("/storage/.cache/Moonlight Game Streaming Project/key.pem");
  std::string line;
  do{
    std::getline(confFile, line);
  }
  while (line.find("key=") == std::string::npos);
  if (line.find("key=\"") != std::string::npos) {
    line.erase(0, 16);
    line.erase(line.size()-2);
  } else {
    line.erase(0, 15);
    line.erase(line.size()-1);
  }
  std::string::size_type pos = 0;
  do{
    pos = line.find("\\n", pos);
    line.replace(pos, 2, "\n");
  }
  while (line.find("\\n", pos) != std::string::npos);
  keyFile << line;
}

// Streaming options live in moonlight's own config, not SystemConf.
// Only managed keys are rewritten, comments and everything else stay.
static const char* MOONLIGHT_CONF = "/storage/.config/moonlight/moonlight.conf";

std::map<std::string, std::string> readMoonlightConf() {
  std::map<std::string, std::string> values;
  std::ifstream in(MOONLIGHT_CONF);
  std::string line;

  while (std::getline(in, line)) {
    const size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }

    const size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }

    const std::string key = Utils::String::trim(line.substr(0, equals));
    if (!key.empty()) {
      values[key] = Utils::String::trim(line.substr(equals + 1));
    }
  }

  return values;
}

void updateMoonlightConf(const std::map<std::string, std::string>& updates) {
  std::vector<std::string> lines;
  std::map<std::string, bool> replaced;
  std::string line;

  std::ifstream in(MOONLIGHT_CONF);
  while (std::getline(in, line)) {
    const size_t equals = line.find('=');
    const size_t comment = line.find('#');

    if (equals != std::string::npos && (comment == std::string::npos || comment > equals)) {
      const std::string key = Utils::String::trim(line.substr(0, equals));
      auto it = updates.find(key);
      if (it != updates.end()) {
        line = key + " = " + it->second;
        replaced[key] = true;
      }
    }

    lines.push_back(line);
  }
  in.close();

  // Append keys the file did not have
  for (const auto& update : updates) {
    if (!replaced[update.first]) {
      lines.push_back(update.first + " = " + update.second);
    }
  }

  std::ofstream out(MOONLIGHT_CONF);
  for (const auto& l : lines) {
    out << l << "\n";
  }
}

// File existence check function
bool fileExists (const std::string& name) {
  if (FILE *file = fopen(name.c_str(), "r")) {
      fclose(file);
      return true;
  } else {
      return false;
  }   
}

std::string MoonlightClient::CreateNewGuid() {
  srand(time(NULL));

  char strUuid[256];
  snprintf(strUuid, sizeof strUuid, "%x%x-%x-%x-%x-%x%x%x", 
      rand(), rand(),                 // Generates a 64-bit Hex number
      rand(),                         // Generates a 32-bit Hex number
      ((rand() & 0x0fff) | 0x4000),   // Generates a 32-bit Hex number of the form 4xxx (4 indicates the UUID version)
      rand() % 0x3fff + 0x8000,       // Generates a 32-bit Hex number in the range [0x8000, 0xbfff]
      rand(), rand(), rand());
  return strUuid;
}

std::string MoonlightClient::MakeUrl(
    const std::string& command, std::string args) {
  if (!args.empty()) args += "&";
  args += "uniqueid=0123456789ABCDEF&uuid=" + CreateNewGuid();

  char url[1024];
  snprintf(url, sizeof url, "https://%s:47984/%s?%s",
      server_ip_.c_str(), command.c_str(), args.c_str());
  return std::string(url);
}

std::string MoonlightClient::MakeRequest(const std::string& url, std::string* filename) {
  std::string cert_path;
  if (fileExists("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf") == true) {
    cert_path = "/storage/.cache/Moonlight Game Streaming Project/";
  } else {
    cert_path = "/storage/.cache/moonlight/";
  }

  HttpReqOptions opts;
  opts.clientCert = cert_path + "client.pem";
  opts.clientKey = cert_path + "key.pem";

  if (filename != nullptr)
    opts.outputFilename = *filename;

	HttpReq req(url, &opts);
  std::cout << "MoonlightClient::MakeRequest : " << url << std::endl;
	req.wait();
	
	if (req.status() != HttpReq::REQ_SUCCESS) return "";
  if (filename) {
    std::cout << "MoonlightClient::MakeRequest saved to : " << *filename << std::endl;
    return {};
  } else {
    auto res = req.getContent();
    std::cout << "MoonlightClient::MakeRequest got : " << res << std::endl;
    return res;
  }
}

bool MoonlightClient::WriteBoxArtFile(const std::string& app_id, std::string filename) {
  char args[256];
  snprintf(args, sizeof args, "appid=%s&AssetType=2&AssetIdx=0", app_id.c_str());
  MakeRequest(MakeUrl("appasset", args), &filename);
  return true;
}

bool MoonlightClient::UpdateMoonlightGames() {
  ApiSystem::executeScriptLegacy("rm /storage/roms/moonlight/images/*");
  ApiSystem::executeScriptLegacy("rm /storage/roms/moonlight/*");
  ApiSystem::executeScriptLegacy("mkdir -p /storage/roms/moonlight");

  auto xml = GetAppListXml();
  if (xml.empty()) return false;

	pugi::xml_document doc;
	auto result = doc.load_string(xml.c_str());
 	if (!result) return false;

  // list api returns the following xml
  // <?xml version="1.0" encoding="UTF-16"?>
  // <root protocol_version="0.1" query="applist" status_code="200" status_message="OK">
  //   <App>
  //     <AppInstallPath>C:\Program Files (x86)\Steam\</AppInstallPath>
  //     <AppTitle>Steam</AppTitle>
  //     <CmsId>100021711</CmsId>
  //     <Distributor>Steam</Distributor>
  //     <ID>1088017781</ID>
  //     <IsAppCollectorGame>0</IsAppCollectorGame>
  //     <IsHdrSupported>1</IsHdrSupported>
  //     <MaxControllersForSingleSession>1</MaxControllersForSingleSession>
  //     <ShortName>steam</ShortName>
  //     <SupportedSOPS>
  //       <SOPS>
  //         <Height>2160</Height>
  //         <RefreshRate>60</RefreshRate>
  //         <Width>3840</Width>
  //       </SOPS>
  //       ...
  //     </SupportedSOPS>
  //     <UniqueId>20225001</UniqueId>
  //     <simulateControllers>0</simulateControllers>
  //   </App>
  //   ...
  // </root>

	pugi::xml_node root = doc.child("root");
  if (!root) return false;

  pugi::xml_document gamelist_doc;
  auto gamelist = gamelist_doc.append_child("gameList");

	for (auto app : root.children())
	{
    if (std::string(app.name()) != "App") continue;

    std::string title = app.child("AppTitle").text().get();
    std::cout << "MoonlightClient::UpdateMoonlightGames: " << title << std::endl;
    auto filename = title;
    std::replace(filename.begin(), filename.end(), '/', ' ');

    // Write bash script
    if (fileExists("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf") == true) {
      std::ofstream app_file("/storage/roms/moonlight/" + filename + ".sh");
      app_file << "#!/bin/sh" << std::endl;
      app_file << "/usr/bin/controller-layout moonlight \"" << filename + ".sh" << "\" /storage/.config/moonlight/gamecontrollerdb.txt" << std::endl;
      app_file << "QT_QPA_PLATFORM=wayland moonlight stream " << server_ip_ << " \"" << title << "\" --quit-after" << std::endl;
      app_file.close();
    } else {
      std::ofstream app_file("/storage/roms/moonlight/" + filename + ".sh");
      app_file << "#!/bin/sh" << std::endl;
      app_file << "/usr/bin/controller-layout moonlight \"" << filename + ".sh" << "\" /storage/.config/moonlight/gamecontrollerdb.txt" << std::endl;
      app_file << "moonlight stream -app \"" << title << "\" -platform sdl " << server_ip_ << std::endl;
      app_file.close();
    }

    // Write box art image
    std::string app_id = app.child("ID").text().get();
    if (!app_id.empty()) {
      WriteBoxArtFile(app_id, "/storage/roms/moonlight/images/" + filename + ".png");
    }

    // Update gamelist
    auto game = gamelist.append_child("game");
    game.append_child("path").text().set(std::string("./" + filename + ".sh").c_str());
    game.append_child("name").text().set(title.c_str());
    game.append_child("image").text().set(std::string("./images/" + filename + ".png").c_str());
  }

  gamelist_doc.save_file("/storage/roms/moonlight/gamelist.xml");
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GuiMoonlight::show(Window* window)
{
	window->pushGui(new GuiMoonlight(window));
}

GuiMoonlight::GuiMoonlight(Window* window)
 : GuiSettings(window, "MOONLIGHT GAME STREAMING")
{
  char pinBuffer[5];
  snprintf(pinBuffer, sizeof pinBuffer, "%04d", rand() % 10000);
  const std::string pin(pinBuffer);

	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;
	auto pinUI = std::make_shared<TextComponent>(window, pin, font, color);

	// GuiSettings::save() does nothing unless a save func is registered
	addSaveFunc([] { SystemConf::getInstance()->saveSystemConf(); });

	addGroup(_("TOOLS"));

  addEntry(_("QUIT CURRENT GAME"), false, [window] {
    std::string server_ip = SystemConf::getInstance()->get("moonlight.host");
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "moonlight quit %s", server_ip.c_str());
    ApiSystem::executeScriptLegacy(cmd);
  });

  addEntry(_("UPDATE MOONLIGHT GAMES"), false, [window] {
    std::string server_ip = SystemConf::getInstance()->get("moonlight.host");
    
    MoonlightClient client(server_ip);
    if (!server_ip.empty() && client.UpdateMoonlightGames()) {
      Scripting::fireEvent("quit", "restart");
			Utils::Platform::quitES(Utils::Platform::QuitMode::QUIT);
    } else {
      window->pushGui(new GuiMsgBox(window, _("Unable to connect to server")));
    }
  });

	addGroup(_("SETTINGS"));
  addInputTextConfigRow(_("SERVER IP"), "moonlight.host", false);
  addWithLabel(_("PAIRING PIN"), pinUI);

	addGroup(_("STREAMING"));

  // Seeded from moonlight.conf, written back by the save func below
  const auto conf = readMoonlightConf();

  auto valueOr = [&conf](const std::string& key, const std::string& fallback) {
    auto it = conf.find(key);
    return (it == conf.end() || it->second.empty()) ? fallback : it->second;
  };

  // Steps match moonlight's bitrate tiers. Not every device has a 1080p screen.
  const std::string currentRes = valueOr("width", "1280") + "x" + valueOr("height", "720");
  auto resolution = std::make_shared<OptionListComponent<std::string>>(window, _("RESOLUTION"), false);
  bool knownRes = false;
  for (const std::string& res : { "640x360", "854x480", "1280x720", "1920x1080", "2560x1440", "3840x2160" }) {
    const bool selected = (res == currentRes);
    knownRes |= selected;
    resolution->add(res, res, selected);
  }
  // Keep a hand-edited value instead of snapping to a preset
  if (!knownRes) {
    resolution->add(currentRes, currentRes, true);
  }
  addWithLabel(_("RESOLUTION"), resolution);

  const std::string currentFps = valueOr("fps", "60");
  auto fps = std::make_shared<OptionListComponent<std::string>>(window, _("FRAME RATE"), false);
  bool knownFps = false;
  for (const std::string& f : { "30", "60", "90", "120" }) {
    const bool selected = (f == currentFps);
    knownFps |= selected;
    fps->add(f + " FPS", f, selected);
  }
  if (!knownFps) {
    fps->add(currentFps + " FPS", currentFps, true);
  }
  addWithLabel(_("FRAME RATE"), fps);

  // -1 is moonlight's default: it works the rate out from resolution and fps
  const std::string currentBitrate = valueOr("bitrate", "-1");
  auto bitrate = std::make_shared<OptionListComponent<std::string>>(window, _("BITRATE"), false);
  bool knownBitrate = (currentBitrate == "-1");
  bitrate->add(_("AUTO"), "-1", knownBitrate);
  for (const std::string& b : { "1000", "3000", "5000", "10000", "15000", "20000", "30000", "40000", "50000" }) {
    const bool selected = (b == currentBitrate);
    knownBitrate |= selected;
    bitrate->add(std::to_string(std::stoi(b) / 1000) + " Mbps", b, selected);
  }
  if (!knownBitrate) {
    bitrate->add(currentBitrate + " Kbps", currentBitrate, true);
  }
  addWithLabel(_("BITRATE"), bitrate);

  // moonlight treats "hevc" as "h265" and falls back to auto on anything else
  std::string currentCodec = valueOr("codec", "auto");
  if (currentCodec == "hevc") {
    currentCodec = "h265";
  }
  if (currentCodec != "auto" && currentCodec != "h264" && currentCodec != "h265" && currentCodec != "av1") {
    currentCodec = "auto";
  }

  auto codec = std::make_shared<OptionListComponent<std::string>>(window, _("VIDEO CODEC"), false);
  codec->add(_("AUTO"), "auto", currentCodec == "auto");
  codec->add("H.264", "h264", currentCodec == "h264");
  codec->add("H.265 (HEVC)", "h265", currentCodec == "h265");
  codec->add("AV1", "av1", currentCodec == "av1");
  addWithLabel(_("VIDEO CODEC"), codec);

  addSaveFunc([resolution, fps, bitrate, codec] {
    const std::string res = resolution->getSelected();
    const size_t x = res.find('x');
    if (x == std::string::npos) {
      return;
    }

    updateMoonlightConf({
      { "width",   res.substr(0, x) },
      { "height",  res.substr(x + 1) },
      { "fps",     fps->getSelected() },
      { "bitrate", bitrate->getSelected() },
      { "codec",   codec->getSelected() },
    });
  });

	addGroup(_("PAIRING"));

  addEntry(_("PAIR WITH SERVER"), false, [window, pin] {
    std::string server_ip = SystemConf::getInstance()->get("moonlight.host");
    if (server_ip.empty()) {
      window->pushGui(new GuiMsgBox(window, _("Unable to connect to server")));
      return;
    }

    char cmd[1024];
    if (isEmbedded() == false) {
      snprintf(cmd, sizeof cmd, "QT_QPA_PLATFORM=wayland moonlight pair -pin %s %s", pin.c_str(), server_ip.c_str());
    } else {
      snprintf(cmd, sizeof cmd, "moonlight pair -pin %s %s", pin.c_str(), server_ip.c_str());
    }

    // moonlight blocks until the host accepts the PIN, so keep it off the UI thread
    const std::string command(cmd);
    const std::string waitText = _("PAIRING PIN") + ": " + pin;

    // Cancelling must kill the pairing too, or it keeps running against a PIN
    // the menu has thrown away.
    char killCmd[1024];
    snprintf(killCmd, sizeof killCmd, "pkill -f \"moonlight pair -pin %s\"", pin.c_str());
    const std::string killCommand(killCmd);
    auto cancelled = std::make_shared<bool>(false);

    window->pushGui(new GuiLoading<MoonlightPairResult>(window, waitText,
      [command, server_ip](IGuiLoadingHandler*) {
        MoonlightPairResult result;
        result.server_ip = server_ip;

        ApiSystem::executeScriptLegacy(command, [&result](std::string line) {
          std::string new_server_ip;
          if (ParseServerIp(line, &new_server_ip)) {
            result.server_ip = new_server_ip;
          }
          if (line == "Succesfully paired") {
            result.paired = true;
          }
        });

        return result;
      },
      [window, server_ip, cancelled](MoonlightPairResult result) {
        // Cancelled, nothing to report
        if (*cancelled) {
          return;
        }

        if (!result.server_ip.empty() && result.server_ip != server_ip) {
          SystemConf::getInstance()->set("moonlight.host", result.server_ip);
          SystemConf::getInstance()->saveSystemConf();
        }

        if (!result.paired) {
          window->pushGui(new GuiMsgBox(window, _("Unable to connect to server")));
          return;
        }

        if (fileExists("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf") == true) {
          const std::string cert_path = "/storage/.cache/Moonlight Game Streaming Project/";
          if (fileExists(cert_path + "client.pem") == false) {
            extractClient();
          }
          if (fileExists(cert_path + "key.pem") == false) {
            extractKey();
          }
        }

        window->pushGui(new GuiMsgBox(window, _("Succesfully paired with server")));
      },
      [cancelled, killCommand]() {
        *cancelled = true;
        Utils::Platform::runSystemCommand(killCommand, "", nullptr);
      }));
	});

  addEntry(_("UNPAIR WITH SERVER"), false, [this, window] {
		if (fileExists("/storage/.config/Moonlight Game Streaming Project/Moonlight.conf") == true) {
      Utils::Platform::runSystemCommand("rm -r \"~/.cache/Moonlight Game Streaming Project\"", "", nullptr);
      Utils::Platform::runSystemCommand("rm -r \"~/.config/Moonlight Game Streaming Project\"", "", nullptr);
      window->pushGui(new GuiMsgBox(window, _("Unpaired and settings cleared")));
    } else {
      Utils::Platform::runSystemCommand("rm -r ~/.cache/moonlight", "", nullptr);
      window->pushGui(new GuiMsgBox(window, _("Unpaired from server")));
    }
	});
}

std::vector<std::string> GuiMoonlight::ParseAppList(const std::vector<std::string>& vec) {
  std::vector<std::string> apps;
  for (auto line : vec) {
    int pos = line.find(". ");
    if (pos == -1) continue;
    apps.push_back(line.substr(pos+2));
  }
  return apps;
}

bool GuiMoonlight::ParseServerIp(const std::string& line, std::string* server_ip) {
  // Watching for "Connect to 10.1.10.217..." line.
  const std::string prompt = "Connect to ";
  if (line.find(prompt) != 0) return false;

  const std::string ip = line.substr(prompt.length());
  if (ip.substr(ip.length() - 3) != "...") return false;

  *server_ip = ip.substr(0, ip.length() - 3);
  return true;
}
