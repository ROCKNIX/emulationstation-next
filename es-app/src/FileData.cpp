#include "FileData.h"
#include "AppWindow.h"
#include "CaptureRotation.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "AudioManager.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "Log.h"
#include "MameNames.h"
#include "utils/Platform.h"
#include "Scripting.h"
#include "SystemData.h"
#include "VolumeControl.h"
#include "Window.h"
#include "views/UIModeController.h"
#include <assert.h>
#include "SystemConf.h"
#include "InputManager.h"
#include "scrapers/ThreadedScraper.h"
#include "Gamelist.h" 
#include "ApiSystem.h"
#include <time.h>
#include <algorithm>
#include "LangParser.h"
#include "resources/ResourceManager.h"
#include "RetroAchievements.h"
#include "SaveStateRepository.h"
#include "Genres.h"
#include "TextToSpeech.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "ThreadedCloudSync.h"
#include "CloudTransferJob.h"
#include "ProxyCards.h"
#include "guis/GuiCloudTransfer.h"
#include "guis/GuiLoading.h"
#include "views/ViewController.h"
#include <chrono>
#include <thread>
#include <atomic>
#include "OfflineAchievements.h"
#include "Paths.h"
#include "resources/TextureData.h"
#include "views/gamelist/GameNameFormatter.h"
#include "LaunchCommand.h"
#include "watchers/WatchersManager.h"

using namespace Utils::Platform;

static std::map<std::string, std::function<BindableProperty(FileData*)>> properties =
{
	{ "name",				[](FileData* file) { return file->getName(); } },
	{ "rom",				[](FileData* file) { return BindableProperty(Utils::FileSystem::getFileName(file->getPath()), BindablePropertyType::String); } },
	{ "stem",				[](FileData* file) { return BindableProperty(Utils::FileSystem::getStem(file->getPath()), BindablePropertyType::String); } },
	{ "path",				[](FileData* file) { return BindableProperty(file->getPath(), BindablePropertyType::Path); } },
	{ "image",				[](FileData* file) { return BindableProperty(file->getImagePath(), BindablePropertyType::Path); } },
	{ "thumbnail",			[](FileData* file) { return BindableProperty(file->getThumbnailPath(false), BindablePropertyType::Path); } },
	{ "video",				[](FileData* file) { return BindableProperty(file->getVideoPath(), BindablePropertyType::Path); } },
	{ "marquee",			[](FileData* file) { return BindableProperty(file->getMarqueePath(), BindablePropertyType::Path); } },
	{ "favorite",			[](FileData* file) { return file->getFavorite(); } },
	{ "hidden",				[](FileData* file) { return file->getHidden(); } },
	{ "kidGame",			[](FileData* file) { return file->getKidGame(); } },
	{ "gunGame",			[](FileData* file) { return file->isLightGunGame(); } },
	{ "wheelGame",			[](FileData* file) { return file->isWheelGame(); } },
	{ "trackballGame",		[](FileData* file) { return file->isTrackballGame(); } },
	{ "spinnerGame",		[](FileData* file) { return file->isSpinnerGame(); } },
	{ "cheevos",			[](FileData* file) { return file->hasCheevos(); } },
	{ "genre",			    [](FileData* file) { return file->getGenre(); } },
	{ "hasKeyboardMapping", [](FileData* file) { return file->hasKeyboardMapping(); } },	
	{ "systemName",			[](FileData* file) { return file->getSourceFileData()->getSystem()->getFullName(); } },
	{ "fullName",			[](FileData* file) { return GameNameFormatter(file->getSystem()).getDisplayName(file); } },
	{ "fullNameNoFavorite",	[](FileData* file) { return GameNameFormatter(file->getSystem()).getDisplayName(file, false, false); } },
	{ "fullNameNoTags",		[](FileData* file) { return GameNameFormatter(file->getSystem()).getDisplayName(file, false, false, false); } },
};

FileData* FileData::mRunningGame = nullptr;

// Counts game exits, so the work an exit hands to a worker thread (the
// capture, then the exit sync -- fork #290) can tell whether another game
// has been launched and left since it was posted. See launchGame.
static std::atomic<unsigned> sExitGeneration{ 0 };
// Counts game starts (GetGamesStarted).
static std::atomic<unsigned> sGamesStarted{ 0 };

unsigned FileData::GetGamesStarted()
{
	return sGamesStarted.load();
}
// PLAY NOW through the offline achievements' send card: the launch it
// leads to comes back through this function (launchNow goes through
// ViewController::launch), so the answer is kept for that one launch or
// the question would be asked again on the next frame, without end (the
// #292 proof on the VM, 2026-09-26).
static std::atomic<bool> sPlayThroughSend{ false };
// The exit capture still recording (audit #307 PL-061): the exit generation
// whose cloud_capture is running, 0 when none. The launch waits for it, as
// it waits for a sync or a transfer to be gone: a game launched under it
// writes a save the capture is hashing, and the manifest records a hash the
// file no longer has.
static std::atomic<unsigned> sCaptureInFlight{ 0 };
// When it started (steady clock, ms), for the one case the gate lets a
// launch go over it: a capture so old it is hung, not slow.
static std::atomic<long long> sCaptureStartedMs{ 0 };
// The capture a launch is waiting on behind the spinner: its exit sync is
// the gate's to decide (a launch that goes owns it; one that is refused
// leaves it to the capture), so the capture's own post leaves it alone.
static std::atomic<unsigned> sCaptureWaitedOn{ 0 };
static const long CaptureHungSeconds = 120;

static long long steadyMs()
{
	return (long long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static long captureAgeSeconds()
{
	return (long) ((steadyMs() - sCaptureStartedMs.load()) / 1000);
}

FileData::FileData(FileType type, const std::string& path, SystemData* system)
	: mPath(path), mType(type), mSystem(system), mParent(nullptr), mDisplayName(nullptr), mMetadata(type == GAME ? GAME_METADATA : FOLDER_METADATA) // metadata is REALLY set in the constructor!
{
	// metadata needs at least a name field (since that's what getName() will return)
	if (mMetadata.get(MetaDataId::Name).empty() && !mPath.empty())
		mMetadata.set(MetaDataId::Name, getDisplayName());
	
	mMetadata.resetChangedFlag();
}

const std::string FileData::getPath() const
{
	if (mPath.empty())
		return getSystemEnvData()->mStartPath;

	return mPath;
}

const std::string FileData::getBreadCrumbPath()
{
	std::vector<std::string> paths;

	FileData* root = getSystem()->getParentGroupSystem() != nullptr ? getSystem()->getParentGroupSystem()->getRootFolder() : getSystem()->getRootFolder();

	FileData* parent = (getType() == GAME ? getParent() : this);
	parent = (getType() == GAME ? getParent() : this);
	while (parent != nullptr)
	{
		if (parent == root->getSystem()->getRootFolder() && !parent->getSystem()->isCollection())
			break;
		
		if (parent->getSystem()->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
			break;

		if (parent->getSystem()->isGroupChildSystem() && 
			parent->getSystem()->getParentGroupSystem() != nullptr && 
			parent->getParent() == parent->getSystem()->getParentGroupSystem()->getRootFolder() && 			
			parent->getSystem()->getName() != "windows_installers")
			break;

		paths.push_back(parent->getName());
		parent = parent->getParent();
	}

	std::reverse(paths.begin(), paths.end());
	return Utils::String::join(paths, " > ");
}


const std::string FileData::getConfigurationName()
{
	std::string gameConf = Utils::FileSystem::getFileName(getPath());
	gameConf = Utils::String::replace(gameConf, "=", "");
	gameConf = Utils::String::replace(gameConf, "#", "");
	gameConf = getSourceFileData()->getSystem()->getName() + std::string("[\"") + gameConf + std::string("\"]");
	return gameConf;
}

inline SystemEnvironmentData* FileData::getSystemEnvData() const
{ 
	return mSystem->getSystemEnvData(); 
}

std::string FileData::getSystemName() const
{
	return mSystem->getName();
}

FileData::~FileData()
{
	if (mDisplayName)
		delete mDisplayName;

	if (mParent)
		mParent->removeChild(this);

	if (mType == GAME)
		mSystem->removeFromIndex(this);
}

std::string& FileData::getDisplayName()
{
	if (mDisplayName == nullptr)
	{
		std::string stem = Utils::FileSystem::getStem(getPath());
		if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
			stem = MameNames::getInstance()->getRealName(stem);

		mDisplayName = new std::string(stem);
	}

	return *mDisplayName;
}

std::string FileData::getCleanName()
{
	return Utils::String::removeParenthesis(getDisplayName());
}

std::string FileData::findLocalArt(const std::string& type, std::vector<std::string> exts)
{
	if (Settings::getInstance()->getBool("LocalArt"))
	{
		for (auto ext : exts)
		{
			std::string path = getSystemEnvData()->mStartPath + "/images/" + getDisplayName() + (type.empty() ? "" :  "-" + type) + ext;
			if (Utils::FileSystem::exists(path))
				return path;

			if (type == "video")
			{
				path = getSystemEnvData()->mStartPath + "/videos/" + getDisplayName() + "-" + type + ext;
				if (Utils::FileSystem::exists(path))
					return path;

				path = getSystemEnvData()->mStartPath + "/videos/" + getDisplayName() + ext;
				if (Utils::FileSystem::exists(path))
					return path;
			}
		}
	}

	return "";
}

const std::string FileData::getThumbnailPath(bool fallbackWithImage)
{
	std::string thumbnail = getMetadata(MetaDataId::Thumbnail);

	// no thumbnail, try image
	if (thumbnail.empty())
	{
		thumbnail = findLocalArt("thumb");
		if (!thumbnail.empty())
			setMetadata(MetaDataId::Thumbnail, thumbnail);

		// no image, try to use local image
		if (fallbackWithImage)
		{
			if (thumbnail.empty())
				thumbnail = getMetadata(MetaDataId::Image);

			if (thumbnail.empty())
				thumbnail = findLocalArt("image");

			if (thumbnail.empty())
				thumbnail = findLocalArt();
		}

		if (thumbnail.empty() && getType() == GAME && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getThumbnailPath();
			else if (getType() == GAME)
			{
				thumbnail = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(thumbnail));
				if (TextureData::PdfHandler == nullptr && ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";
			}
		}

	}

	return thumbnail;
}

const bool FileData::getFavorite() const
{
	return getMetadata(MetaDataId::Favorite) == "true";
}

const bool FileData::getHidden() const
{
	return getMetadata(MetaDataId::Hidden) == "true";
}

const bool FileData::getKidGame() const
{
	auto data = getMetadata(MetaDataId::KidGame);
	return data != "false" && !data.empty();
}

const bool FileData::hasCheevos()
{
	if (Utils::String::toInteger(getMetadata(MetaDataId::CheevosId)) > 0)
		return getSourceFileData()->getSystem()->isCheevosSupported();

	return false;
}

bool FileData::hasAnyMedia()
{
	if (Utils::FileSystem::exists(getImagePath()) || Utils::FileSystem::exists(getThumbnailPath(false)) || Utils::FileSystem::exists(getVideoPath()))
		return true;

	for (auto mdd : mMetadata.getMDD())
	{
		if (mdd.type != MetaDataType::MD_PATH)
			continue;

		std::string path = mMetadata.get(mdd.key);
		if (path.empty())
			continue;

		if (mdd.id == MetaDataId::Manual || mdd.id == MetaDataId::Magazine)
		{
			if (Utils::FileSystem::exists(path))
				return true;
		}
		else if (mdd.id != MetaDataId::Image && mdd.id != MetaDataId::Thumbnail)
		{
			if (Utils::FileSystem::isImage(path))
				continue;

			if (Utils::FileSystem::exists(path))
				return true;
		}
	}

	return false;
}

std::vector<std::string> FileData::getFileMedias()
{
	std::vector<std::string> ret;

	for (auto mdd : mMetadata.getMDD())
	{
		if (mdd.type != MetaDataType::MD_PATH)
			continue;

		if (mdd.id == MetaDataId::Video || mdd.id == MetaDataId::Manual || mdd.id == MetaDataId::Magazine)
			continue;

		std::string path = mMetadata.get(mdd.key);
		if (path.empty())
			continue;

		if (!Utils::FileSystem::isImage(path))
			continue;
		
		if (Utils::FileSystem::exists(path))
			ret.push_back(path);
	}

	return ret;
}

void FileData::resetSettings() 
{
	
}

const std::string& FileData::getName()
{
	if (mSystem != nullptr && mSystem->getShowFilenames())
		return getDisplayName();

	return mMetadata.getName();
}

const std::string FileData::getVideoPath()
{
	std::string video = getMetadata(MetaDataId::Video);
	
	// no video, try to use local video
	if (video.empty())
	{
		video = findLocalArt("video", { ".mp4" });
		if (!video.empty())
			setMetadata(MetaDataId::Video, video);
	}
	
	if (video.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
	{
		if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
			return ((FolderData*)this)->mChildren[0]->getVideoPath();
		else if (getType() == GAME)
		{
			if (Utils::FileSystem::isVideo(getPath()))
				return getPath();

			if (Utils::FileSystem::isAudio(getPath()))
				return getPath();
		}
	}

	return video;
}

const std::string FileData::getMarqueePath()
{
	std::string marquee = getMetadata(MetaDataId::Marquee);

	// no marquee, try to use local marquee
	if (marquee.empty())
	{
		marquee = findLocalArt("marquee");
		if (!marquee.empty())
			setMetadata(MetaDataId::Marquee, marquee);
	}
	
	return marquee;
}

const std::string FileData::getImagePath()
{
	std::string image = getMetadata(MetaDataId::Image);

	// no image, try to use local image
	if (image.empty())
	{
		auto romExt = Utils::String::toLower(Utils::FileSystem::getExtension(getPath()));
		if (romExt == ".png" || (getSystemName() == "pico8" && romExt == ".p8"))
			return getPath();

		if (image.empty())
			image = findLocalArt("image");

		if (image.empty())
			image = findLocalArt();

		if (!image.empty())
			setMetadata(MetaDataId::Image, image);

		if (image.empty() && getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
		{
			if (getType() == FOLDER && ((FolderData*)this)->mChildren.size())
				return ((FolderData*)this)->mChildren[0]->getImagePath();
			else if (getType() == GAME)
			{
				image = getPath();

				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(image));
				if (TextureData::PdfHandler == nullptr && ext == ".pdf" && ResourceManager::getInstance()->fileExists(":/pdf.jpg"))
					return ":/pdf.jpg";

				if (Utils::FileSystem::isAudio(image) && ResourceManager::getInstance()->fileExists(":/mp3.jpg"))
					return ":/mp3.jpg";
			}
		}
	}

	return image;
}

std::string FileData::getKey()
{
	return getFileName();
}

const bool FileData::isArcadeAsset()
{
	if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
	{	
		const std::string stem = Utils::FileSystem::getStem(getPath());
		return MameNames::getInstance()->isBiosOrDevice(stem);		
	}

	return false;
}

const bool FileData::isVerticalArcadeGame()
{
	if (mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE))
		return MameNames::getInstance()->isVertical(Utils::FileSystem::getStem(getPath()));

	return false;
}

const bool FileData::isLightGunGame()
{
	return MameNames::getInstance()->isLightgun(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_LIGHTGUN);
}

const bool FileData::isWheelGame()
{
	return MameNames::getInstance()->isWheel(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_WHEEL);
}

const bool FileData::isTrackballGame()
{
	return MameNames::getInstance()->isTrackball(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_TRACKBALL);
}

const bool FileData::isSpinnerGame()
{
	return MameNames::getInstance()->isSpinner(Utils::FileSystem::getStem(getPath()), mSystem->getName(), mSystem && mSystem->hasPlatformId(PlatformIds::ARCADE));
	//return Genres::genreExists(&getMetadata(), GENRE_SPINNER);
}

FileData* FileData::getSourceFileData()
{
	return this;
}

static std::string formatCommandLineArgument(const std::string& name)
{
	if (name.find(" ") != std::string::npos)
		return "\"" + Utils::String::replace(name, "\"", "\\\"") + "\"";

	return Utils::String::replace(name, "\"", "\\\"");
};

std::string FileData::getlaunchCommand(LaunchGameOptions& options, bool includeControllers)
{
	FileData* gameToUpdate = getSourceFileData();
	if (gameToUpdate == nullptr)
		return "";

	SystemData* system = gameToUpdate->getSystem();
	if (system == nullptr)
		return "";

	// must really;-) be done before window->deinit while it closes joysticks
	std::string controllersConfig = InputManager::getInstance()->configureEmulators();

	if (gameToUpdate->isLightGunGame())
		controllersConfig = controllersConfig + "-lightgun ";

        if (gameToUpdate->isWheelGame())
		controllersConfig = controllersConfig + "-wheel ";

        if (gameToUpdate->isTrackballGame())
		controllersConfig = controllersConfig + "-trackball ";

        if (gameToUpdate->isSpinnerGame())
		controllersConfig = controllersConfig + "-spinner ";

	std::string systemName = system->getName();
	std::string emulator = getEmulator();
	std::string core = getCore();

	bool forceCore = false;

	if (options.netPlayMode == CLIENT && !options.core.empty() && core != options.core)
	{
		for (auto& em : system->getEmulators())
		{
			for (auto& cr : em.cores)
			{
				if (cr.name == options.core)
				{
					emulator = em.name;
					core = cr.name;
					forceCore = true;
					break;
				}
			}

			if (forceCore)
				break;
		}
	}	
	/*else if (!isExtensionCompatible())
	{
		auto extension = Utils::String::toLower(Utils::FileSystem::getExtension(gameToUpdate->getPath()));

		for (auto emul : system->getEmulators())
		{
			if (std::find(emul.incompatibleExtensions.cbegin(), emul.incompatibleExtensions.cend(), extension) == emul.incompatibleExtensions.cend())
			{
				for (auto coreZ : emul.cores)
				{
					if (std::find(coreZ.incompatibleExtensions.cbegin(), coreZ.incompatibleExtensions.cend(), extension) == coreZ.incompatibleExtensions.cend())
					{
						emulator = emul.name;
						core = coreZ.name;
						break;
					}
				}
			}
		}
	}*/
	
	std::string command = system->getLaunchCommand(emulator, core);

	if (forceCore)
	{
		if (command.find("%EMULATOR%") == std::string::npos && command.find("-emulator") == std::string::npos)
			command = command + " -emulator %EMULATOR%";

		if (command.find("%CORE%") == std::string::npos && command.find("-core") == std::string::npos)
			command = command + " -core %CORE%";
	}

	const std::string rom = Utils::FileSystem::getEscapedPath(getPath());
	const std::string basename = Utils::FileSystem::getStem(getPath());
	const std::string rom_raw = Utils::FileSystem::getPreferredPath(getPath());
	
	command = Utils::String::replace(command, "%SYSTEM%", systemName);
	command = Utils::String::replace(command, "%ROM%", rom);
	command = Utils::String::replace(command, "%BASENAME%", basename);
	command = Utils::String::replace(command, "%ROM_RAW%", rom_raw);
	command = Utils::String::replace(command, "%EMULATOR%", emulator);
	command = Utils::String::replace(command, "%CORE%", core);
	command = Utils::String::replace(command, "%HOME%", Paths::getHomePath());
	command = Utils::String::replace(command, "%GAMENAME%", formatCommandLineArgument(gameToUpdate->getName()));
	command = Utils::String::replace(command, "%SYSTEMNAME%", formatCommandLineArgument(system->getFullName()));

	// Export Game info XML is requested
#ifdef WIN32
	std::string fileInfo = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), "game.xml");
#else
	std::string fileInfo = "/tmp/game.xml";
#endif

	if (command.find("%GAMEINFOXML%") != std::string::npos && saveToXml(gameToUpdate, fileInfo, true))
		command = Utils::String::replace(command, "%GAMEINFOXML%", Utils::FileSystem::getEscapedPath(fileInfo));
	else
	{
		command = Utils::String::replace(command, "%GAMEINFOXML%", "");
		Utils::FileSystem::removeFile(fileInfo);
	}
	
	if (includeControllers)
		command = Utils::String::replace(command, "%CONTROLLERSCONFIG%", controllersConfig);

	if (options.netPlayMode != DISABLED && (forceCore || gameToUpdate->isNetplaySupported()) && command.find("%NETPLAY%") == std::string::npos)
		command = command + " %NETPLAY%"; // Add command line parameter if the netplay option is defined at <core netplay="true"> level

	if (SystemConf::getInstance()->get("global.netplay.nickname").empty())
	{
		SystemConf::getInstance()->set("global.netplay.nickname", ApiSystem::getInstance()->getApplicationName() + " Player");
		SystemConf::getInstance()->saveSystemConf();
	}

	if (options.netPlayMode == CLIENT || options.netPlayMode == SPECTATOR)
	{
		std::string mode = (options.netPlayMode == SPECTATOR ? "spectator" : "client");
		std::string pass;

		if (!options.netplayClientPassword.empty())
			pass = " -netplaypass " + options.netplayClientPassword;

		// mitm_session: Should inject this into retroarch -> --mitm-session=ID
		std::string session;
		if (!options.session.empty())
			session = " -netplaysession " + options.session;

#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--connect " + options.ip + " --port " + std::to_string(options.port) + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
#if ROCKNIX
			command = Utils::String::replace(command, "%NETPLAY%", "--connect " + Utils::String::shellQuote(options.ip) + " --port " + std::to_string(options.port) + " --nick " + Utils::String::shellQuote(SystemConf::getInstance()->get("global.netplay.nickname"))); // fork #275
#else
			command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode " + mode + " -netplayport " + std::to_string(options.port) + " -netplayip " + options.ip + session + pass);
#endif
	}
	else if (options.netPlayMode == SERVER)
	{
#if WIN32
		if (Utils::String::toLower(command).find("retroarch.exe") != std::string::npos)
			command = Utils::String::replace(command, "%NETPLAY%", "--host --port " + SystemConf::getInstance()->get("global.netplay.port") + " --nick " + SystemConf::getInstance()->get("global.netplay.nickname"));
		else
#endif
#if ROCKNIX
			command = Utils::String::replace(command, "%NETPLAY%", "--host --port " + Utils::String::shellQuote(SystemConf::getInstance()->get("global.netplay.port")) + " --nick " + Utils::String::shellQuote(SystemConf::getInstance()->get("global.netplay.nickname"))); // fork #275
#else
			command = Utils::String::replace(command, "%NETPLAY%", "-netplaymode host");
#endif
	}
	else
		command = Utils::String::replace(command, "%NETPLAY%", "");

	int monitorId = Settings::getInstance()->getInt("MonitorID");
	if (monitorId >= 0 && command.find(" -system ") != std::string::npos)
		command = command + " -monitor " + std::to_string(monitorId);

	if (SaveStateRepository::isEnabled(this))
	{
		if (options.saveStateInfo == nullptr)
		{
			if (getCurrentGameSetting("autosave") == "1")
				options.saveStateInfo = system->getSaveStateRepository()->getGameAutoSave(this);
			else
				options.saveStateInfo = SaveStateRepository::getEmptySaveState();
		}

		command = options.saveStateInfo->setupSaveState(this, command);		
	}

	// The pair the emulator will actually be handed, read from the finished
	// command (fork #21 R5): a savestate config's rewrite above and the netplay
	// client override both diverge from getEmulator()/getCore().
	options.launchedEmulator = launchArgument(command, "--emulator", emulator);
	options.launchedCore = launchArgument(command, "--core", core);

	return command;
}

std::string FileData::getMessageFromExitCode(int exitCode)
{
	switch (exitCode)
	{
	case 200:
		return _("THE EMULATOR EXITED UNEXPECTEDLY");
	case 201:
		return _("BAD COMMAND LINE ARGUMENTS");
	case 202:
		return _("INVALID CONFIGURATION");
	case 203:
		return _("UNKNOWN EMULATOR");
	case 204:
		return _("EMULATOR IS MISSING");
	case 205:
		return _("CORE IS MISSING");
	case 299:
	case 250:
		{
	#if WIN32
			std::string messageFile = Utils::FileSystem::combine(Utils::FileSystem::getTempPath(), "launch_error.log");
	#else
			std::string messageFile = "/tmp/launch_error.log";
	#endif
			if (Utils::FileSystem::exists(messageFile))
			{
				auto message = Utils::FileSystem::readAllText(messageFile);
				Utils::FileSystem::removeFile(messageFile);

				if (!message.empty())
					return message;
			}
		}
	}

	return _("UKNOWN ERROR") + " : " + std::to_string(exitCode);
}

// The launch a STOP IT AND PLAY leads to, on the next frame: the dialog or
// spinner that asked is off the stack first, and the launch effect and the
// emulator take the screen from the view, as a press on the game would.
//
// A save state chosen in the manager is carried by its file (options.
// saveStateFile, rememberSaveState) and found again here, when the launch
// runs: the repository's refresh -- the manager's jobs call it as they land
// -- deletes every state object, and a launch deferred behind a gate held a
// copy of the pointer for as long as the gate took (#308 8-es-menus-and-
// core claude F-ES-11). A state gone by then is not handed over: the game
// starts as it would with none chosen, and the log says so.
static void launchNow(Window* window, FileData* game, const LaunchGameOptions& options)
{
	window->postToUiThread([game, options]
	{
		LaunchGameOptions now = options;
		if (!now.saveStateFile.empty())
		{
			now.saveStateInfo = nullptr;
			if (SaveStateRepository::isEnabled(game))
				for (auto* state : game->getSourceFileData()->getSystem()->getSaveStateRepository()->getSaveStates(game))
					if (state->fileName == now.saveStateFile)
					{
						now.saveStateInfo = state;
						break;
					}
			if (now.saveStateInfo == nullptr)
				LOG(LogWarning) << "launch: the save state chosen before the wait is gone (" << now.saveStateFile << "); the game starts without it";
		}
		ViewController::get()->launch(game, now);
	});
}

// The save state the launch was handed, by its file, while the pointer is
// the repository's live object (launchNow finds it again by it). The three
// shared states -- none, the auto-save, a new game -- are never deleted and
// need no name.
static void rememberSaveState(FileData* game, LaunchGameOptions& options)
{
	options.saveStateFile.clear();
	SaveState* state = options.saveStateInfo;
	if (state == nullptr || !SaveStateRepository::isEnabled(game))
		return;
	SaveStateRepository* repo = game->getSourceFileData()->getSystem()->getSaveStateRepository();
	if (state == SaveStateRepository::getEmptySaveState() || state == repo->getDefaultAutoSaveSaveState() || state == repo->getDefaultNewGameSaveState())
		return;
	options.saveStateFile = state->fileName;
}

// Wait behind a spinner for a sync or transfer that has been told to stop,
// then launch (D-CLOUD-129). stillRunning is asked every 50 ms; hardStop,
// when given, is sent once at five seconds for an rclone slow to act on
// SIGTERM; at twenty the wait gives up and says so -- a run that will not
// die is not one to start a game over (cancelForLaunch's rule, on a longer
// leash and behind a spinner rather than a frozen menu).
static void launchWhenGone(Window* window, FileData* game, const LaunchGameOptions& options,
	const std::function<bool()>& stillRunning, const std::function<void()>& hardStop)
{
	window->pushGui(new GuiLoading<bool>(window, _("STOPPING IT SO YOU CAN PLAY..."),
		[stillRunning, hardStop](IGuiLoadingHandler*)
		{
			const auto started = std::chrono::steady_clock::now();
			bool hard = false;
			while (stillRunning())
			{
				const long ms = (long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
				if (ms >= 20000)
					return false;
				if (!hard && ms >= 5000 && hardStop != nullptr)
				{
					hardStop();
					hard = true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			return true;
		},
		[window, game, options](bool gone)
		{
			if (!gone)
			{
				LOG(LogWarning) << "launch: the sync or transfer did not stop within 20 s; the game was not started";
				window->pushGui(new GuiMsgBox(window, _("IT DIDN'T STOP IN TIME. TRY AGAIN IN A MOMENT.")));
				return;
			}
			launchNow(window, game, options);
		}));
}

// The last game's saves still being recorded (the capture in launchGame, on
// a thread of its own since fork #290; audit #307 PL-061): true when the
// launch may go on now. Bookkeeping with no network, so no question
// (D-UI-095): the launch waits for it. Most captures are done within a
// moment, and a moment is waited for here, on this thread, so a spinner
// does not flash (es-ui-style-guide.md, Waiting). A longer one is waited
// for behind the spinner, bounded.
//
// At the bound the launch is refused and says why (audit of the fixes, E2
// gpt G-E2-03): it used to start anyway, under the capture it was waiting
// on, and every later launch skipped the same capture -- the overlap the
// gate exists to prevent, only later. A capture is local hashing of one
// game's saves and ends in a second or two; one still running after ten
// is stalled, and the next press waits for it again. Only a capture so old
// it is hung (CaptureHungSeconds) stops holding the device, with a warning:
// a player kept from every game for bookkeeping is the worse failure.
//
// The exit sync is the gate's to decide while a launch waits (claude
// G-E2-06): a launch that goes owns it -- the generation moves at the
// moment it goes, so the capture's post leaves the sync to that game's
// exit and the player is not asked about a sync that started while they
// waited -- and a launch refused at the bound leaves it to the capture,
// whose post runs it when the capture ends. The generation used to move as
// the spinner went up, before the launch was certain.
static bool captureGate(Window* window, FileData* game, const LaunchGameOptions& options)
{
	const unsigned capturing = sCaptureInFlight.load();
	if (capturing == 0)
		return true;
	if (captureAgeSeconds() >= CaptureHungSeconds)
	{
		LOG(LogWarning) << "launch: the last game's capture has run " << captureAgeSeconds() << " s and is taken as hung; the game starts";
		return true;
	}

	const auto started = std::chrono::steady_clock::now();
	while (sCaptureInFlight.load() == capturing && std::chrono::steady_clock::now() - started < std::chrono::milliseconds(300))
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	if (sCaptureInFlight.load() != capturing)
	{
		LOG(LogInfo) << "launch: the last game's saves were recorded first";
		return true;
	}

	LOG(LogInfo) << "launch: waiting for the last game's saves to be recorded";
	sCaptureWaitedOn = capturing;
	window->pushGui(new GuiLoading<bool>(window, _("RECORDING YOUR LAST GAME'S SAVES..."),
		[capturing](IGuiLoadingHandler*)
		{
			const auto waited = std::chrono::steady_clock::now();
			while (sCaptureInFlight.load() == capturing)
			{
				if (std::chrono::steady_clock::now() - waited >= std::chrono::seconds(10))
					return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			return true;
		},
		[window, game, options, capturing](bool)
		{
			// Decided by the capture itself, not the wait's answer: one that
			// ended as the bound came has ended.
			if (sCaptureInFlight.load() == capturing)
			{
				sCaptureWaitedOn = 0;
				LOG(LogWarning) << "launch: the last game's capture did not finish within 10 s; the game was not started";
				window->pushGui(new GuiMsgBox(window, _("YOUR LAST GAME'S SAVES ARE STILL BEING RECORDED. TRY AGAIN IN A MOMENT.")));
				return;
			}
			++sExitGeneration;
			sCaptureWaitedOn = 0;
			launchNow(window, game, options);
		}));
	return false;
}

bool FileData::launchGame(Window* window, LaunchGameOptions options)
{
	LOG(LogInfo) << "Attempting to launch game...";

	// PLAY NOW's answer is for the launch it leads to and no other (#308
	// 8-es-menus-and-core claude F-ES-07): taken here, on the way in,
	// whether or not the send is still running. It was taken only where the
	// send was, so a send that ended between the press and the relaunch left
	// it set, and a later launch over a later send asked nothing. A relaunch
	// that meets another question first asks the send's again after it.
	const bool playThroughSend = sPlayThroughSend.exchange(false);
	rememberSaveState(this, options);

	// Not while saves are moving -- unless the sync is one EmulationStation
	// started on its own. A cloud sync reads and writes the same save files
	// the emulator is about to open, and the archive step tars up /storage
	// while it runs: starting a game in the middle of that can upload a
	// half-written save or restore over one the game has already loaded.
	//
	// The startup sync and the after-a-game sync run without a press. From
	// #101 (maintainer, 2026-09-09) a launch cancelled them on its own --
	// picking a game was the choice -- and since 2026-09-16 (D-CLOUD-130)
	// it asks first, like every other sync: the maintainer met the exit
	// sync's stop as "it told me it was stopping so I couldn't play" and
	// wants one behaviour for every sync or transfer a launch would
	// interrupt. Stopping is safe because the scripts are rclone copy --
	// each file written under a temporary name and renamed when complete,
	// nothing deleted -- so a copy cut short leaves no partial file and the
	// next run finishes it; the wait for the process to be gone is what
	// keeps a rename from landing after the emulator has the save open.
	// Before #101 only the network wait could be cancelled (fork #94), and a
	// transfer that had lost its link was refused for as long as rclone's
	// own timeouts let it run (#103).
	//
	// A sync the player asked for, and a back up, restore or match they
	// started on the transfer page (fork #187, D-CLOUD-113; while that page
	// could be left with the run going, before D-UI-078), used to refuse
	// the launch outright: theirs to wait for.
	// Maintainer, 2026-09-15, having met the refusal on the RG SP: "We
	// should say for the user, would you like to cancel and play, or would
	// you like to stop the sync or continue ... If it shows a modal anyways
	// when the sync is in progress and someone tries to start the new game,
	// we might as well give them the option as to whether they'd like to
	// cancel or keep waiting." So each is a question with two answers
	// (D-CLOUD-129), and since D-CLOUD-130 the automatic syncs ask the same
	// question: "If we want consistent behavior on how user-started syncs or
	// transfers deal with a potential interruption by a game launch, we
	// should have a consistent behavior."
	// STOP IT AND PLAY sends the run's process group SIGTERM,
	// waits behind a spinner for it to be gone -- the automatic sync's wait,
	// for the same reason: a rename must not land under a game that has the
	// save open -- and then launches through ViewController, launch effect
	// and all; KEEP WAITING (B as well) leaves it running. Stopping is safe
	// for the reason the automatic cancel is: the scripts are rclone copy,
	// each file renamed into place whole, so the next run finishes what this
	// one did not, and a match's deletions stop where they are. The run's
	// outcome reads SKIPPED - YOU STARTED A GAME, as the card's does.
	//
	// One question for any sync on the card, whoever started it. STOP IT AND
	// PLAY cancels it (cancelForLaunch with the player's answer, which sends
	// the group SIGTERM and waits its two seconds) and launches; a sync slow
	// to die is waited for behind the spinner rather than refused.
	if (!captureGate(window, this, options))
		return false;

	if (ThreadedCloudSync::isRunning())
	{
		window->pushGui(new GuiMsgBox(window,
			_("YOUR SAVES ARE SYNCING WITH THE CLOUD.") + "\n\n" + _("IF YOU STOP IT, THE NEXT SYNC FINISHES WHAT THIS ONE DID NOT."),
			_("STOP IT AND PLAY"), [this, window, options]
			{
				LOG(LogInfo) << "launch: the player chose to stop the sync for a game";
				ThreadedCloudSync::CancelRefusal again = ThreadedCloudSync::CancelRefusal::Stopping;
				if (ThreadedCloudSync::cancelForLaunch(&again, true) || !ThreadedCloudSync::isRunning())
					launchNow(window, this, options);
				else
					launchWhenGone(window, this, options, [] { return ThreadedCloudSync::isRunning(); }, nullptr);
			},
			_("KEEP WAITING"), nullptr));
		return false;
	}

	if (const std::shared_ptr<CloudTransferJob> transfer = CloudTransferJob::current(); transfer != nullptr && !transfer->finished())
	{
		window->pushGui(new GuiMsgBox(window,
			GuiCloudTransfer::stillRunningSentence(transfer) + "\n\n" + _("IF YOU STOP IT, WHAT IT HAS NOT MOVED YET WAITS FOR THE NEXT RUN."),
			_("STOP IT AND PLAY"), [this, window, options]
			{
				LOG(LogInfo) << "launch: the player chose to stop the transfer for a game";
				CloudTransferJob::stopForLaunch(false);
				launchWhenGone(window, this, options, [] { return CloudTransferJob::running(); }, [] { CloudTransferJob::stopForLaunch(true); });
			},
			_("KEEP WAITING"), nullptr));
		return false;
	}

	// The offline achievements' two jobs ask the same question (fork #293,
	// D-UI-095). The send is the proxy's and cannot be stopped from here,
	// so its question offers to play through it: PLAY NOW launches at once
	// and the proxy goes on sending behind the game. The top-up can be
	// stopped: STOP IT AND PLAY signals the ctl and waits for it to be
	// gone, and the ctl runs again next time the device is connected. The
	// safe verb is last in both (D-UI-096).
	if (ProxyCards::sendRunning() && !playThroughSend)
	{
		window->pushGui(new GuiMsgBox(window,
			_("OFFLINE ACHIEVEMENTS ARE BEING SENT.") + "\n\n" + _("IT'LL BE A MOMENT."),
			_("PLAY NOW"), [this, window, options]
			{
				LOG(LogInfo) << "launch: the player chose to play through the send";
				sPlayThroughSend = true;
				launchNow(window, this, options);
			},
			_("KEEP WAITING"), nullptr));
		return false;
	}

	if (ProxyCards::topUpRunning())
	{
		window->pushGui(new GuiMsgBox(window,
			_("YOUR OFFLINE ACHIEVEMENTS ARE BEING UPDATED.") + "\n\n" + _("IF YOU STOP IT, IT'LL TRY AGAIN NEXT TIME YOU'RE CONNECTED."),
			_("STOP IT AND PLAY"), [this, window, options]
			{
				LOG(LogInfo) << "launch: the player chose to stop the top-up for a game";
				ProxyCards::stopTopUp();
				launchWhenGone(window, this, options, [] { return ProxyCards::topUpRunning(); }, nullptr);
			},
			_("KEEP WAITING"), nullptr));
		return false;
	}

	FileData* gameToUpdate = getSourceFileData();
	if (gameToUpdate == nullptr)
		return false;

	SystemData* system = gameToUpdate->getSystem();
	if (system == nullptr)
		return false;

	std::string command = getlaunchCommand(options);
	if (command.empty())
		return false;

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	bool hideWindow = Settings::getInstance()->getBool("HideWindow");
	window->deinit(hideWindow);
	
	const std::string rom = Utils::FileSystem::getEscapedPath(getPath());
	const std::string basename = Utils::FileSystem::getStem(getPath());

	Scripting::fireEvent("game-start", rom, basename, getName());

	time_t tstart = time(NULL);

	LOG(LogInfo) << "	" << Utils::String::maskSecrets(command);

	auto p2kConv = convertP2kFile();

	mRunningGame = gameToUpdate;
	sGamesStarted++;

	// Pause watchers before game launch
	WatchersManager::pause();

	ProcessStartInfo process(command);
	process.window = hideWindow ? NULL : window;
	
	int exitCode = process.run();
	
	// Resume watchers when returning to ES
	WatchersManager::resume();
	
	if (exitCode != 0)
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";

	mRunningGame = nullptr;

	Utils::FileSystem::FileSystemCache::reset();

	if (SaveStateRepository::isEnabled(this))
	{
		if (options.saveStateInfo != nullptr)
			options.saveStateInfo->onGameEnded(this);

		getSourceFileData()->getSystem()->getSaveStateRepository()->refresh();
	}

	// Record what this session wrote (fork #21 R5). On every exit: whatever
	// the exit code, whatever the cloud toggle says, whether or not another
	// sync holds the lock -- a save written and not recorded is exactly what
	// the reconciler cannot explain later. Network-free, takes no lock and
	// spawns no rclone: a handful of stats and at most a few hundred KB of
	// hashing. It ran here, synchronously, while the window was still down,
	// and on the RG35XX SP that was three seconds of nothing on screen
	// between the emulator's exit and the interface's first frame (fork
	// #290); the command is built here and run below, on the worker that
	// then starts the exit sync, so it still finishes before the sync
	// starts and the manifest the push carries is current. The paths are
	// read here, after onGameEnded above, which renumbers slot files
	// and restores .state.auto -- the paths only settle there. The emulator
	// and core are the ones the command carried (launchedEmulator/
	// launchedCore), never getEmulator()/getCore() re-read now. --started is
	// tstart: a member with no entry is recorded only if this session wrote
	// it, so nothing stamps a file it did not write. A system with no
	// <emulators> element (tools, imageviewer) reaches here with an empty
	// pair; nothing there writes a save, so there is nothing to record and
	// no failure to stamp -- an empty --emulator would otherwise be a usage
	// row in capture-failures on every run of such a system.
	std::string capture;
	if (Utils::FileSystem::exists("/usr/bin/cloud_capture") && !options.launchedEmulator.empty())
	{
		capture = std::string("/usr/bin/cloud_capture")
			+ " --system "   + Utils::String::shellQuote(system->getName())
			+ " --rom "      + Utils::String::shellQuote(gameToUpdate->getPath())
			+ " --emulator " + Utils::String::shellQuote(options.launchedEmulator)
			+ " --core "     + Utils::String::shellQuote(options.launchedCore)
			+ " --started "  + std::to_string(static_cast<long long>(tstart))
			+ " --exit "     + std::to_string(exitCode);
	}

	// What the display did with this game's frame, for its captures (fork
	// #245, D-UI-081): the core's rotation request is in the launch log
	// only while the log is this session's, so it is read here. Only after a
	// session that ran: a launch that failed before RetroArch's banner (a
	// missing core, a bad ROM) exits non-zero, and its log is the previous
	// game's section or no banner at all -- recordAfterSession then wrote
	// that game's turn, or 0, as this game's own, from=own-launch (#308
	// 8-es claude F-ES-08). A crash after a real session keeps the record it
	// had. CaptureRotation checks the rest: the log's age against the
	// launch, and no record from a log with no launch in it.
	if (exitCode == 0)
		CaptureRotation::recordAfterSession(gameToUpdate, options.launchedEmulator, tstart);
	else
		LOG(LogInfo) << "capture rotation: not recorded, the launch exited " << exitCode;

	if (!p2kConv.empty()) // delete .keys file if it has been converted from p2k
		Utils::FileSystem::removeFile(p2kConv);

	Scripting::fireEvent("game-end");

	if (!hideWindow && Settings::getInstance()->getBool("HideWindowFullReinit"))
	{
		ResourceManager::getInstance()->reloadAll();
		window->deinit();
		window->init();
		window->setCustomSplashScreen(gameToUpdate->getImagePath(), gameToUpdate->getName(), gameToUpdate);
	}
	else
		window->init(hideWindow);
	
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->init();

	window->normalizeNextUpdate();

	//update number of times the game has been launched
	if (exitCode == 0)
	{
		int timesPlayed = gameToUpdate->getMetadata().getInt(MetaDataId::PlayCount) + 1;
		gameToUpdate->setMetadata(MetaDataId::PlayCount, std::to_string(static_cast<long long>(timesPlayed)));

		// How long have you played that game? (more than 10 seconds, otherwise
		// you might have experienced a loading problem)
		time_t tend = time(NULL);
		long elapsedSeconds = difftime(tend, tstart);
		long gameTime = gameToUpdate->getMetadata().getInt(MetaDataId::GameTime) + elapsedSeconds;
		if (elapsedSeconds >= 10)
			gameToUpdate->setMetadata(MetaDataId::GameTime, std::to_string(static_cast<long>(gameTime)));

		//update last played time
		gameToUpdate->setMetadata(MetaDataId::LastPlayed, Utils::Time::DateTime(Utils::Time::now()));
		CollectionSystemManager::get()->refreshCollectionSystems(gameToUpdate);
		saveToGamelistRecovery(gameToUpdate);
	}

	window->reactivateGui();

	// A screenshot taken in this session is in a folder the viewer scanned
	// at boot. Re-read the folders that changed, once the launch has fully
	// unwound (#82; the rescan may delete this very FileData when the game
	// was an image in the viewer).
	window->postToUiThread([] { SystemData::rescanChangedFolders(); });

	// Sync saves to the cloud, visibly.
	//
	// This was an OS event hook -- /usr/bin/scripts/game-end/, run by the
	// fireEvent above -- which backgrounded cloud_backup with its output sent
	// to /dev/null. It worked, and no player could ever tell: a silent
	// background job that a reboot kills looks exactly like one that never
	// started, and the first question after exiting a game is whether the save
	// is safe. Running it here puts the answer on the progress card and leaves
	// the outcome on it, which is what every other cloud operation does.
	//
	// It replaces the hook rather than joining it. The hook had no caller but
	// this line, so there is nothing else to keep working -- and two paths to
	// one operation is what the cloud menu was just collapsed to avoid.
	//
	// Not gated on exitCode: an emulator that crashed may still have written a
	// save, and that is the copy most worth having.
	//
	// --saves-only --recent: the after-every-game job is the saves this
	// session touched, pushed now. The system-settings archive is the
	// occasional job and has its own row. Comparing every save on the device
	// against the cloud was 18 seconds of somebody's time to move nothing,
	// most of it remote round trips that had nothing to do with the game
	// just played. With no network, cloud_backup answers
	// CloudExit::NoNetwork at once rather than waiting for a probe to time
	// out.
	//
	// The capture first, then the sync, both off the interface thread (fork
	// #290): the window is back and drawing before either starts, where the
	// capture used to run before the window came back and the RG35XX SP
	// showed nothing for its three seconds. The sync card is created on the
	// interface thread, so the worker posts it there once the capture is
	// done. A game launched and left in between owns the outcome: the loop
	// that runs posted tasks does not turn while a game runs, so this task
	// runs only after that game's exit, when that exit's own task is the
	// one to act -- its --recent run carries this session's saves as well.
	const bool exitSync = SystemConf::getInstance()->get("cloudsaves.gameexit") == "1"
		&& Utils::FileSystem::exists("/usr/bin/cloud_backup");
	const unsigned generation = ++sExitGeneration;
	if (!capture.empty())
	{
		sCaptureStartedMs = steadyMs();
		sCaptureInFlight = generation;   // before the thread, so a launch on the next frame sees it
	}
	std::thread([window, capture, exitSync, generation]
	{
		bool captureFailed = false;
		if (!capture.empty())
		{
			const int captureCode = ApiSystem::executeScriptLegacy(capture, nullptr).second;
			if (captureCode != 0)
			{
				LOG(LogWarning) << "cloud_capture exited " << captureCode << " -- see /var/log/cloud_sync.log and /storage/.cache/cloud_sync/capture-failures";
				captureFailed = true;
			}
			// Only this exit's: a later exit's capture owns the slot now.
			unsigned mine = generation;
			sCaptureInFlight.compare_exchange_strong(mine, 0);
		}
		AppWindow::post(window, [window, exitSync, generation, captureFailed]
		{
			// A capture that could not record says so once, as a toast, in
			// the player's words (fork #293 item 3, D-UI-095): what did not
			// happen and what is in place. Said before the sync card starts,
			// so it shows after the card (D-UI-093 puts a toast's words back
			// on the queue when a card takes the screen). And said whether or
			// not another game has been launched since: a later game makes
			// this exit's sync its own, not this capture's failure (#308
			// 8a-es-app gpt F-ES-05; the check below returned before it).
			if (captureFailed)
				window->displayNotificationMessage(_U("\uF0C2  ") + _("COULDN'T RECORD THIS SESSION'S SAVES. THEY'RE STILL ON THIS DEVICE."));
			if (generation != sExitGeneration.load() || mRunningGame != nullptr)
			{
				LOG(LogInfo) << "exit: another game was launched since; its exit syncs";
				return;
			}
			// A launch is waiting on this capture behind the spinner: if it
			// goes it owns the sync, as above; it can only be refused while
			// the capture runs, and this capture has ended (captureGate).
			if (sCaptureWaitedOn.load() == generation)
			{
				LOG(LogInfo) << "exit: a launch waited for this capture; its game's exit syncs";
				return;
			}
			if (exitSync && !ThreadedCloudSync::isRunning())
			{
				ThreadedCloudSync::start(window, "/usr/bin/cloud_backup --yes --saves-only --recent --automatic",
					_("SYNC SAVES"), _("SYNCING SAVES TO THE CLOUD"), ThreadedCloudSync::Origin::Exit);
			}
			// Nothing else is said at exit (fork #292, D-RA-030): the offline
			// achievements go up when RetroAchievements answers, with a card
			// of their own on the link's return (ProxyCards).
		});
	}).detach();

	if (system != nullptr && system->getTheme() != nullptr)
		AudioManager::getInstance()->changePlaylist(system->getTheme(), true);
	else
		AudioManager::getInstance()->playRandomMusic();

	if (exitCode >= 200 && exitCode <= 300)
		window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURRED") + ":\r\n" + getMessageFromExitCode(exitCode), _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));

	return exitCode == 0;
}


bool FileData::hasContentFiles()
{
	if (mPath.empty())
		return false;

	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));
	if (ext == ".m3u" || ext == ".cue" || ext == ".ccd" || ext == ".gdi")
		return getSourceFileData()->getSystemEnvData()->isValidExtension(ext) && getSourceFileData()->getSystemEnvData()->mSearchExtensions.size() > 1;

	return false;
}

static std::vector<std::string> getTokens(const std::string& string)
{
	std::vector<std::string> tokens;

	bool inString = false;
	int startPos = 0;
	int i = 0;
	for (;;)
	{
		char c = string[i];

		switch (c)
		{
		case '\"':
			inString = !inString;
			if (inString)
				startPos = i + 1;

		case '\0':
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (!inString)
			{
				std::string value = string.substr(startPos, i - startPos);
				if (!value.empty())
					tokens.push_back(value);

				startPos = i + 1;
			}
			break;
		}

		if (c == '\0')
			break;

		i++;
	}

	return tokens;
}

std::set<std::string> FileData::getContentFiles()
{
	std::set<std::string> files;

	if (mPath.empty())
		return files;

	if (Utils::FileSystem::isDirectory(mPath))
	{
		for (auto file : Utils::FileSystem::getDirContent(mPath, true, true))
			files.insert(file);
	}
	else if (hasContentFiles())
	{
		auto path = Utils::FileSystem::getParent(mPath);
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(mPath));

		if (ext == ".cue")
		{
			std::string start = "FILE";

			std::ifstream cue(WINSTRINGW(mPath));
			if (cue && cue.is_open())
			{
				std::string line;
				while (std::getline(cue, line))
				{
					if (!Utils::String::startsWith(line, start))
						continue;

					auto tokens = getTokens(line);
					if (tokens.size() > 1)
						files.insert(path + "/" + tokens[1]);
				}

				cue.close();
			}
		}
		else if (ext == ".ccd")
		{
			std::string stem = Utils::FileSystem::getStem(mPath);
			files.insert(path + "/" + stem + ".cue");
			files.insert(path + "/" + stem + ".img");
			files.insert(path + "/" + stem + ".bin");
			files.insert(path + "/" + stem + ".sub");
		}
		else if (ext == ".m3u" || ext == ".xbox360")
		{
			std::ifstream m3u(WINSTRINGW(mPath));
			if (m3u && m3u.is_open())
			{
				std::string line;
				while (std::getline(m3u, line))
				{
					auto trim = Utils::String::trim(line);
					if (trim.empty() || trim[0] == '#')
						continue;

					trim = Utils::String::replace(trim, "\\", "/");

					if (Utils::FileSystem::isAbsolute(trim))
						files.insert(Utils::FileSystem::getGenericPath(trim));
					else if (trim[0] == '/')
						continue;
					else
						files.insert(Utils::FileSystem::getCanonicalPath(path + "/" + trim));
				}

				m3u.close();
			}
		}
		else if (ext == ".gdi")
		{
			std::ifstream gdi(WINSTRINGW(mPath));
			if (gdi && gdi.is_open())
			{
				std::string line;
				while (std::getline(gdi, line))
				{
					auto tokens = getTokens(line);
					if (tokens.size() > 5 && tokens[4].find(".") != std::string::npos)
						files.insert(path + "/" + tokens[4]);
				}

				gdi.close();
			}			
		}
	}

	return files;
}

void FileData::deleteGameFiles()
{
	if (mType != FileType::GAME)
		return;

	std::string path = getPath();
	if (path.empty() || getSystemEnvData()->mStartPath == path)
		return;

	for (auto mdd : mMetadata.getMDD())
	{
		if (mMetadata.getType(mdd.id) != MetaDataType::MD_PATH)
			continue;

		std::string path = mMetadata.get(mdd.id);
		if (Utils::FileSystem::exists(path) && !Utils::FileSystem::isDirectory(path))
			Utils::FileSystem::removeFile(mMetadata.get(mdd.id));
	}

	if (Utils::FileSystem::isDirectory(path))
	{
		Utils::FileSystem::deleteDirectoryFiles(path, true);
		return;
	}

	Utils::FileSystem::removeFile(path);

	for (auto contentFile : getContentFiles())
		Utils::FileSystem::removeFile(contentFile);
}

CollectionFileData::CollectionFileData(FileData* file, SystemData* system)
	: FileData(file->getSourceFileData()->getType(), "", system)
{
	mSourceFileData = file->getSourceFileData();
	mParent = NULL;	
}

SystemEnvironmentData* CollectionFileData::getSystemEnvData() const
{ 
	return mSourceFileData->getSystemEnvData();
}

const std::string CollectionFileData::getPath() const
{
	return mSourceFileData->getPath();
}

std::string CollectionFileData::getSystemName() const
{
	return mSourceFileData->getSystem()->getName();
}

CollectionFileData::~CollectionFileData()
{
	// need to remove collection file data at the collection object destructor
	if(mParent)
		mParent->removeChild(this);

	mParent = NULL;
}

std::string CollectionFileData::getKey() 
{
	return getFullPath();
}

FileData* CollectionFileData::getSourceFileData()
{
	return mSourceFileData;
}

const std::string& CollectionFileData::getName()
{
	return mSourceFileData->getName();
}

const std::vector<FileData*> FolderData::getChildrenListToDisplay() 
{
	std::vector<FileData*> ret;

	std::string showFoldersMode = getSystem()->getFolderViewMode();
	
	bool showHiddenFiles = Settings::ShowHiddenFiles();

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1") showHiddenFiles = true;
	else if (shv == "0") showHiddenFiles = false;

	bool filterKidGame = false;

	if (!Settings::getInstance()->getBool("ForceDisableFilters"))
	{
		if (UIModeController::getInstance()->isUIModeKiosk())
			showHiddenFiles = false;

		if (UIModeController::getInstance()->isUIModeKid())
			filterKidGame = true;
	}

	auto sys = CollectionSystemManager::get()->getSystemToView(mSystem);

	std::vector<std::string> hiddenExts;
	if (mSystem->isGameSystem() && !mSystem->isCollection())
		hiddenExts = Utils::String::split(Utils::String::toLower(Settings::getInstance()->getString(mSystem->getName() + ".HiddenExt")), ';');

	FileFilterIndex* idx = sys->getIndex(false);
	if (idx != nullptr && !idx->isFiltered())
		idx = nullptr;

  	std::vector<FileData*>* items = &mChildren;
	
	std::vector<FileData*> flatGameList;
	if (showFoldersMode == "never")
	{
		flatGameList = getFlatGameList(false, sys);
		items = &flatGameList;		
	}

	std::map<FileData*, int> scoringBoard;

	bool refactorUniqueGameFolders = (showFoldersMode == "having multiple games");

	for (auto it = items->cbegin(); it != items->cend(); it++)
	{
		if (!showHiddenFiles && (*it)->getHidden())
			continue;

		if (filterKidGame && (*it)->getType() == GAME && !(*it)->getKidGame())
			continue;

		if (hiddenExts.size() > 0 && (*it)->getType() == GAME)
		{
			std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension((*it)->getFileName(), false));
			if (std::find(hiddenExts.cbegin(), hiddenExts.cend(), extlow) != hiddenExts.cend())
				continue;
		}

		if (idx != nullptr)
		{
			int score = idx->showFile(*it);
			if (score == 0)
				continue;

			scoringBoard[*it] = score;
		}

		if ((*it)->getType() == FOLDER && refactorUniqueGameFolders)
		{
			FolderData* pFolder = (FolderData*)(*it);
			if (pFolder->getChildren().size() == 0)
				continue;

			if (pFolder->isVirtualStorage() && pFolder->getSourceFileData()->getSystem()->isGroupChildSystem() && pFolder->getSourceFileData()->getSystem()->getName() == "windows_installers")
			{
				ret.push_back(*it);
				continue;
			}

			auto fd = pFolder->findUniqueGameForFolder();
			if (fd != nullptr)
			{
				if (idx != nullptr && !idx->showFile(fd))
					continue;

				if (!showHiddenFiles && fd->getHidden())
					continue;

				if (filterKidGame && !fd->getKidGame())
					continue;

				ret.push_back(fd);

				continue;
			}
		}

		ret.push_back(*it);
	}

	unsigned int currentSortId = sys->getSortId();
	if (currentSortId > FileSorts::getSortTypes().size())
		currentSortId = 0;

	const FileSorts::SortType& sort = FileSorts::getSortTypes().at(currentSortId);

	if (idx != nullptr && idx->hasRelevency())
	{
		auto compf = sort.comparisonFunction;

		std::sort(ret.begin(), ret.end(), [scoringBoard, compf](const FileData* file1, const FileData* file2) -> bool
		{ 
			auto s1 = scoringBoard.find((FileData*) file1);
			auto s2 = scoringBoard.find((FileData*) file2);		

			if (s1 != scoringBoard.cend() && s2 != scoringBoard.cend() && s1->second != s2->second)
				return s1->second < s2->second;
			
			return compf(file1, file2);
		});
	}
	else
	{
		bool foldersFirst = Settings::ShowFoldersFirst();
		bool favoritesFirst = getSystem()->getShowFavoritesFirst();

		std::stable_sort(ret.begin(), ret.end(), [sort, foldersFirst, favoritesFirst](const FileData* file1, const FileData* file2) -> bool
			{
				if (favoritesFirst && file1->getFavorite() != file2->getFavorite())
					return file1->getFavorite();

				if (foldersFirst && file1->getType() != file2->getType())
					return (file1->getType() == FOLDER);

				return sort.comparisonFunction(file1, file2) == sort.ascending;
			});
	}

	return ret;
}

std::shared_ptr<std::vector<FileData*>> FolderData::findChildrenListToDisplayAtCursor(FileData* toFind, std::stack<FileData*>& stack)
{
	auto items = getChildrenListToDisplay();

	for (auto item : items)
		if (toFind == item)
			return std::make_shared<std::vector<FileData*>>(items);

	for (auto item : items)
	{
		if (item->getType() != FOLDER)
			continue;
		
		stack.push(item);

		auto ret = ((FolderData*)item)->findChildrenListToDisplayAtCursor(toFind, stack);
		if (ret != nullptr)
			return ret;

		stack.pop();		
	}

	if (stack.empty())
		return std::make_shared<std::vector<FileData*>>(items);

	return nullptr;
}

FileData* FolderData::findUniqueGameForFolder()
{
	auto games = this->getFilesRecursive(GAME);

	FileData* found = nullptr;

	int count = 0;
	for (auto game : games)
	{
		if (game->getHidden())
		{
			bool showHiddenFiles = Settings::ShowHiddenFiles() && !UIModeController::getInstance()->isUIModeKiosk();

			auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
			if (shv == "1") showHiddenFiles = true;
			else if (shv == "0") showHiddenFiles = false;

			if (!showHiddenFiles)
				continue;
		}

		found = game;
		count++;
		if (count > 1)
			break;
	}
	
	if (count == 1)
		return found;
	/*{
		auto it = games.cbegin();
		if ((*it)->getType() == GAME)
			return (*it);
	}
	*/
	return nullptr;
}



void FolderData::getFilesRecursiveWithContext(std::vector<FileData*>& out, unsigned int typeMask, GetFileContext* filter, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const
{
	if (filter == nullptr)
		return;

	auto isVirtualFolder = [](FileData* file)
	{
		if (file->getType() == GAME)
			return false;

		FolderData* fld = (FolderData*)file;
		return fld->isVirtualStorage();
	};

	SystemData* pSystem = (system != nullptr ? system : mSystem);
	
	FileFilterIndex* idx = pSystem->getIndex(false);

	for (auto it : mChildren)
	{
		if (it->getType() & typeMask)
		{
			if (!displayedOnly || idx == nullptr || !idx->isFiltered() || idx->showFile(it))
			{
				if (displayedOnly)
				{
					if (!filter->showHiddenFiles && it->getHidden())
						continue;

					if (filter->filterKidGame && !it->getKidGame())
						continue;

					if (typeMask == GAME && filter->hiddenExtensions.size() > 0)
					{
						std::string extlow = Utils::String::toLower(Utils::FileSystem::getExtension(it->getFileName(), false));
						if (filter->hiddenExtensions.find(extlow) != filter->hiddenExtensions.cend())
							continue;
					}
				}

				if (includeVirtualStorage || !isVirtualFolder(it))
					out.push_back(it);
			}
		}

		if (it->getType() != FOLDER)
			continue;

		FolderData* folder = (FolderData*)it;
		if (folder->getChildren().size() > 0)
		{
			if (includeVirtualStorage || !isVirtualFolder(folder))
			{
				if (folder->isVirtualStorage() && folder->getSourceFileData()->getSystem()->isGroupChildSystem() && folder->getSourceFileData()->getSystem()->getName() == "windows_installers")
					out.push_back(it);
				else
					folder->getFilesRecursiveWithContext(out, typeMask, filter, displayedOnly, system, includeVirtualStorage);
			}
		}
	}
}

std::vector<FileData*> FolderData::getFlatGameList(bool displayedOnly, SystemData* system) const
{
	return getFilesRecursive(GAME, displayedOnly, system);
}

std::vector<FileData*> FolderData::getFilesRecursive(unsigned int typeMask, bool displayedOnly, SystemData* system, bool includeVirtualStorage) const
{
	SystemData* pSystem = (system != nullptr ? system : mSystem);
	
	GetFileContext ctx;
	ctx.showHiddenFiles = Settings::ShowHiddenFiles() && !UIModeController::getInstance()->isUIModeKiosk();

	auto shv = Settings::getInstance()->getString(getSystem()->getName() + ".ShowHiddenFiles");
	if (shv == "1")
		ctx.showHiddenFiles = true;
	else if (shv == "0")
		ctx.showHiddenFiles = false;

	if (pSystem->isGameSystem() && !pSystem->isCollection())
	{
		for (auto ext : Utils::String::split(Utils::String::toLower(Settings::getInstance()->getString(pSystem->getName() + ".HiddenExt")), ';'))
			if (ctx.hiddenExtensions.find(ext) == ctx.hiddenExtensions.cend())
				ctx.hiddenExtensions.insert(ext);
	}

	ctx.filterKidGame = UIModeController::getInstance()->isUIModeKid();

	std::vector<FileData*> out;
	getFilesRecursiveWithContext(out, typeMask, &ctx, displayedOnly, system, includeVirtualStorage);
	return out;
}

void FolderData::addChild(FileData* file, bool assignParent)
{
#if DEBUG
	assert(file->getParent() == nullptr || !assignParent);
#endif

	mChildren.push_back(file);

	if (assignParent)
		file->setParent(this);	
}

void FolderData::removeChild(FileData* file)
{
#if DEBUG
	assert(mType == FOLDER);
	assert(file->getParent() == this);
#endif

	auto it = std::find(mChildren.begin(), mChildren.end(), file);
	if (it != mChildren.end())
	{
		file->setParent(nullptr);
		std::iter_swap(it, mChildren.end() - 1);
		mChildren.pop_back();
	}

	// File somehow wasn't in our children.
#if DEBUG
	assert(false);
#endif
}

void FolderData::bulkRemoveChildren(std::vector<FileData*>& mChildren, const std::unordered_set<FileData*>& filesToRemove)
{
	mChildren.erase(
		std::remove_if(
			mChildren.begin(),
			mChildren.end(),
			[&filesToRemove](FileData* file)
			{
				if (filesToRemove.count(file))
				{
					file->setParent(nullptr);
					return true;
				}
				return false;
			}
		),
		mChildren.end()
	);
}

FileData* FolderData::FindByPath(const std::string& path)
{
	std::stack<FileData*> stack;	
	for (FileData* c : mChildren)
		if (c != nullptr)
			stack.push(c);

	while (!stack.empty())
	{
		FileData* item = stack.top();
		stack.pop();

		if (item->getPath() == path)
			return item;

		if (item->getType() != FOLDER)
			continue;

		const auto& sub = static_cast<FolderData*>(item)->mChildren;
		for (FileData* s : sub)
			if (s != nullptr)
				stack.push(s);
	}

	return nullptr;
}

void FolderData::createChildrenByFilenameMap(std::unordered_map<std::string, FileData*>& map)
{
	std::vector<FileData*> children = getChildren();

	for (std::vector<FileData*>::const_iterator it = children.cbegin(); it != children.cend(); ++it)
	{
		if ((*it)->getType() == FOLDER)
			((FolderData*)(*it))->createChildrenByFilenameMap(map);			
		else 
			map[(*it)->getKey()] = (*it);
	}	
}

const std::string FileData::getCore(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string core = getMetadata(MetaDataId::Core);
#else
	std::string core = SystemConf::getInstance()->get(getConfigurationName() + ".core");	
#endif

	if (core == "auto")
		core = "";

	if (!core.empty())
	{
		// Check core exists 
		std::string emulator = getEmulator();
		if (emulator.empty())
			core = "";
		else
		{
			bool exists = false;

			for (auto emul : getSourceFileData()->getSystem()->getEmulators())
			{
				if (emul.name == emulator)
				{
					for (auto cr : emul.cores)
					{
						if (cr.name == core)
						{
							exists = true;
							break;
						}
					}

					if (exists)
						break;
				}
			}

			if (!exists)
				core = "";
		}
	}

	if (resolveDefault && core.empty())
		core = getSourceFileData()->getSystem()->getCore();

	return core;
}

const std::string FileData::getEmulator(bool resolveDefault)
{
#if WIN32 && !_DEBUG
	std::string emulator = getMetadata(MetaDataId::Emulator);
#else
	std::string emulator = SystemConf::getInstance()->get(getConfigurationName() + ".emulator");
#endif

	if (emulator == "auto")
		emulator = "";

	if (!emulator.empty())
	{
		// Check emulator exists 
		bool exists = false;

		for (auto emul : getSourceFileData()->getSystem()->getEmulators())
			if (emul.name == emulator) { exists = true; break; }

		if (!exists)
			emulator = "";
	}

	if (resolveDefault && emulator.empty())
		emulator = getSourceFileData()->getSystem()->getEmulator();

	return emulator;
}

void FileData::setCore(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata(MetaDataId::Core, value == "auto" ? "" : value);
#else
	SystemConf::getInstance()->set(getConfigurationName() + ".core", value);
#endif
}

void FileData::setEmulator(const std::string value)
{
#if WIN32 && !_DEBUG
	setMetadata(MetaDataId::Emulator, value == "auto" ? "" : value);
#else
	SystemConf::getInstance()->set(getConfigurationName() + ".emulator", value);
#endif
}

bool FileData::isNetplaySupported()
{
	if (!SystemConf::getInstance()->getBool("global.netplay"))
		return false;

	auto file = getSourceFileData();
	if (file->getType() != GAME)
		return false;

	auto system = file->getSystem();
	if (system == nullptr)
		return false;
	
	std::string emulName = getEmulator();
	std::string coreName = getCore();

	if (!CustomFeatures::FeaturesLoaded)
	{
		std::string command = system->getLaunchCommand(emulName, coreName);
		if (command.find("%NETPLAY%") != std::string::npos)
			return true;
	}
	
	for (auto emul : system->getEmulators())
		if (emulName == emul.name)
			for (auto core : emul.cores)
				if (coreName == core.name)
					return core.netplay;
					
	return false;
}

void FileData::detectLanguageAndRegion(bool overWrite)
{
	if (!overWrite && (!getMetadata(MetaDataId::Language).empty() || !getMetadata(MetaDataId::Region).empty()))
		return;

	if (getSystem()->isCollection() || getType() == FOLDER)
		return;

	auto info = LangInfo::parse(getSourceFileData()->getPath(), getSourceFileData()->getSystem());
	if (info.languages.size() > 0)
		mMetadata.set(MetaDataId::Language, info.getLanguageString());
	if (!info.region.empty())
		mMetadata.set(MetaDataId::Region, info.region);
}

void FolderData::removeVirtualFolders() {
	if (!mOwnsChildrens)
		return;

	std::unordered_set<FileData*> filesToRemove;

	for (auto file : mChildren)
	{
		if (file->getType() != FOLDER)
			continue;

		auto folder = static_cast<FolderData*>(file);
		if (!folder->mOwnsChildrens)
			filesToRemove.insert(file);
	}

	bulkRemoveChildren(mChildren, filesToRemove);

	for (auto file : filesToRemove)
		delete file;
}

void FileData::checkCrc32(bool force)
{
	if (getSourceFileData() != this && getSourceFileData() != nullptr)
	{
		getSourceFileData()->checkCrc32(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::Crc32).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = ApiSystem::getInstance()->getCRC32(getPath(), system->shouldExtractHashesFromArchives());
	if (!crc.empty())
	{
		getMetadata().set(MetaDataId::Crc32, Utils::String::toUpper(crc));
		saveToGamelistRecovery(this);
	}
}

void FileData::checkMd5(bool force)
{
	if (getSourceFileData() != this && getSourceFileData() != nullptr)
	{
		getSourceFileData()->checkMd5(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::Md5).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = ApiSystem::getInstance()->getMD5(getPath(), system->shouldExtractHashesFromArchives());
	if (!crc.empty())
	{
		getMetadata().set(MetaDataId::Md5, Utils::String::toUpper(crc));
		saveToGamelistRecovery(this);
	}
}


void FileData::checkCheevosHash(bool force)
{
	if (getSourceFileData() != this)
	{
		getSourceFileData()->checkCheevosHash(force);
		return;
	}

	if (!force && !getMetadata(MetaDataId::CheevosHash).empty())
		return;

	SystemData* system = getSystem();
	if (system == nullptr)
		return;

	auto crc = RetroAchievements::getCheevosHash(system, getPath());
	getMetadata().set(MetaDataId::CheevosHash, Utils::String::toUpper(crc));
	saveToGamelistRecovery(this);
}

std::string FileData::getKeyboardMappingFilePath()
{
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		return getSourceFileData()->getPath() + "/padto.keys";

	return getSourceFileData()->getPath() + ".keys";
}

bool FileData::hasP2kFile()
{
	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	return Utils::FileSystem::exists(p2kPath);
}

void FileData::importP2k(const std::string& p2k)
{
	if (p2k.empty())
		return;

	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	Utils::FileSystem::writeAllText(p2kPath, p2k);

	std::string keysPath = getKeyboardMappingFilePath();
	if (Utils::FileSystem::exists(keysPath))
		Utils::FileSystem::removeFile(keysPath);
}

std::string FileData::convertP2kFile()
{
	std::string p2kPath = getSourceFileData()->getPath() + ".p2k.cfg";
	if (Utils::FileSystem::isDirectory(getSourceFileData()->getPath()))
		p2kPath = getSourceFileData()->getPath() + "/.p2k.cfg";

	if (!Utils::FileSystem::exists(p2kPath))
		return "";

	std::string keysPath = getKeyboardMappingFilePath();
	if (Utils::FileSystem::exists(keysPath))
		return "";

	auto map = KeyMappingFile::fromP2k(p2kPath);
	if (map.isValid())
	{
		map.save(keysPath);
		return keysPath;
	}

	return "";
}

bool FileData::hasKeyboardMapping()
{
	if (!Utils::FileSystem::exists(getKeyboardMappingFilePath()))
		return hasP2kFile();

	return true;
}

KeyMappingFile FileData::getKeyboardMapping()
{
	KeyMappingFile ret;
	auto path = getKeyboardMappingFilePath();

	// If pk2.cfg file but no .keys file, then convert & load
	if (!Utils::FileSystem::exists(path) && hasP2kFile())
	{
		convertP2kFile();

		ret = KeyMappingFile::load(path);
		Utils::FileSystem::removeFile(path);
		return ret;
	}
		
	if (Utils::FileSystem::exists(path))
		ret = KeyMappingFile::load(path);
	else
		ret = getSystem()->getKeyboardMapping(); // if .keys file does not exist, take system config as predefined mapping

	ret.path = path;
	return ret;
}

bool FileData::isFeatureSupported(EmulatorFeatures::Features feature)
{
	auto system = getSourceFileData()->getSystem();
	return system->isFeatureSupported(getEmulator(), getCore(), feature);
}

bool FileData::isExtensionCompatible()
{
	auto game = getSourceFileData();
	auto extension = Utils::String::toLower(Utils::FileSystem::getExtension(game->getPath()));

	auto system = game->getSystem();
	auto emulName = game->getEmulator();
	auto coreName = game->getCore();

	for (auto emul : system->getEmulators())
	{
		if (emulName == emul.name)
		{
			if (std::find(emul.incompatibleExtensions.cbegin(), emul.incompatibleExtensions.cend(), extension) != emul.incompatibleExtensions.cend())
				return false;

			for (auto core : emul.cores)
			{
				if (coreName == core.name)
					return std::find(core.incompatibleExtensions.cbegin(), core.incompatibleExtensions.cend(), extension) == core.incompatibleExtensions.cend();
			}

			break;
		}
	}

	return true;
}

FolderData::FolderData(const std::string& startpath, SystemData* system, bool ownsChildrens) : FileData(FOLDER, startpath, system)
{
	mIsDisplayableAsVirtualFolder = false;
	mOwnsChildrens = ownsChildrens;
}

FolderData::~FolderData()
{
	clear();
}

void FolderData::clear() {
	if (mOwnsChildrens)
		for (auto* child : mChildren)
		{
			child->setParent(nullptr); // prevent each child from inefficiently removing itself from our mChildren vector, since we're about to clear it anyway
			delete child;
		}
	mChildren.clear();
}

void FolderData::removeFromVirtualFolders(FileData* game)
{
	for (auto it = mChildren.begin(); it != mChildren.end(); ++it) 
	{		
		if ((*it)->getType() == FOLDER)
		{
			((FolderData*)(*it))->removeFromVirtualFolders(game);
			continue;
		}

		if ((*it) == game)
		{
			mChildren.erase(it);
			return;
		}
	}
}

std::string FileData::getCurrentGameSetting(const std::string& settingName)
{
	FileData* src = getSourceFileData();

	std::string value = SystemConf::getInstance()->get(getConfigurationName() + "." + settingName);
	if (!value.empty() && value != "auto")
		return value;

	value = SystemConf::getInstance()->get(src->getSystem()->getName() + "." + settingName);
	if (!value.empty() && value != "auto")
		return value;

	return SystemConf::getInstance()->get("global." + settingName);
}

void FileData::setSelectedGame()
{
	TextToSpeech::getInstance()->say(getName(), false);

	Scripting::fireEvent("game-selected", getSourceFileData()->getSystem()->getName(), getPath(), getName());

	std::string desc = getMetadata(MetaDataId::Desc);
	if (!desc.empty())
		TextToSpeech::getInstance()->say(desc, true);	
}

std::string FileData::getGenre()
{
	std::string genre = Genres::genreStringFromIds(Utils::String::split(getMetadata(MetaDataId::GenreIds), ',', true));
	if (genre.empty())
		genre = getMetadata(MetaDataId::Genre);

	return genre;
}

std::string FileData::getSortName() const
{
	// Try to get sortname first to get precedence over (scraped) name
	std::string sortName = getMetadata().get(MetaDataId::SortName);

	// If empty, fallback to the standard display name
	if (sortName.empty())
		sortName = const_cast<FileData*>(this)->getName();

	return sortName;
}

BindableProperty FileData::getProperty(const std::string& name)
{
	auto it = properties.find(name);
	if (it != properties.cend())
		return it->second(this);

	if (name == "nameShort")
	{
		auto name = getName();

		for (int i = 0; i < name.size(); i++)
			if (name[i] == '(' || name[i] == '[')
				return name.substr(0, i);

		return name;
	}

	if (name == "nameExtra")
	{
		auto name = getName();

		for (int i = 0; i < name.size(); i++)
			if (name[i] == '(' || name[i] == '[')
				return name.substr(i);

		return "";
	}	

	if (name == "collection")
	{
		if (getSystem()->isCollection() || getSystem()->isGroupChildSystem())
		{
			FolderData* parent = getParent();

			if (getType() == FOLDER)
			{				
				if (parent != nullptr && (parent->getSystem()->isCollection() || getSystem()->isGroupChildSystem() || getSystem()->isGroupSystem()))
					return BindableProperty(parent->getSystem());
			}

			if (getSystem()->isGroupChildSystem() && parent != nullptr)
			{
				auto group = getSystem()->getParentGroupSystem();
				if (group != nullptr)
				{
					std::string showFoldersMode = group->getFolderViewMode();
					if (showFoldersMode == "never")
						return BindableProperty(getBindableParent());
					else if (showFoldersMode == "having multiple games" && parent && parent->findUniqueGameForFolder() != nullptr)
						return BindableProperty(getBindableParent());
				}
			}

			if (getSystem()->isCollection() || getSystem()->isGroupChildSystem() || getSystem()->isGroupSystem())
				return BindableProperty(getSystem());
		}
			
		return BindableProperty::Null; // getProperty("system");
	}

	if (name == "system")
	{
		auto sys = getSourceFileData()->getSystem();
		if (mPath == ".." && sys->isGroupChildSystem())
		{
			SystemData* group = sys->getParentGroupSystem();
			if (group != nullptr)
				sys = group;
		}

		return BindableProperty(sys);
	}

	if (name == "directory")
	{				
		if (!getSystem()->isCollection() && getSystem()->isGroupChildSystem())
		{
			SystemData* group = getSystem()->getParentGroupSystem();
			if (group != nullptr)
				return BindableProperty::EmptyString;
		}

		std::string showFoldersMode = getSystem()->getFolderViewMode();
		if (showFoldersMode == "never")
			return BindableProperty::EmptyString;
		
		auto parent = getParent();
		if (parent != nullptr)
		{
			if (showFoldersMode == "having multiple games")
			{
				auto fd = parent->findUniqueGameForFolder();
				if (fd != nullptr) 
					return BindableProperty::EmptyString;
			}

			if (getSystem()->isCollection() && parent == getSystem()->getRootFolder())
				return BindableProperty::EmptyString;

			if (!parent->isVirtualFolderDisplay())
				return parent->getBreadCrumbPath();
		}

		return BindableProperty::EmptyString;
	}

	if (name == "type")
	{
		switch (getType())
		{
		case FOLDER: return BindableProperty("folder", BindablePropertyType::String);
		case PLACEHOLDER: return BindableProperty("placeholder", BindablePropertyType::String);
		default: return BindableProperty("game", BindablePropertyType::String);
		}
	}

	if (name == "stars")
	{
		#define RATINGSTAR _U("\uF005")

		int stars = (int)Math::round(Math::clamp(0.0f, 1.0f, Utils::String::toFloat(getMetadata(MetaDataId::Rating))) * 5.0);

		std::string str;
		for (int i = 0; i < stars; i++)
			str += RATINGSTAR;

		return str;
	}

	if (name == "folder" || name == "isFolder")
		return getType() == FOLDER; 

	if (name == "virtualfolder")
		return getType() == FOLDER && (getPath() == ".." || ((FolderData*)this)->isVirtualFolderDisplay());

	if (name == "placeHolder" || name == "isPlaceHolder" || name == "placeholder")
		return getType() == PLACEHOLDER;

	if (name == "playerCount" || name == "playercount")
	{
		std::string value = getMetadata().get("players");
		auto split = value.rfind("+");
		if (split != std::string::npos)
			return value.substr(0, split);

		split = value.rfind("-");
		if (split != std::string::npos)
			return value.substr(split + 1);

		return (int) Math::clamp(Utils::String::toInteger(value), 1, 9);
	}

	if (name == "hasManual" || name == "hasmanual")
	{
		if (Settings::getInstance()->getBool("PreloadMedias"))
			return !getMetadata(MetaDataId::Manual).empty() || !getMetadata(MetaDataId::Magazine).empty(); // ? _("YES") : _("NO");
		
		return Utils::FileSystem::exists(getMetadata(MetaDataId::Manual)) || Utils::FileSystem::exists(getMetadata(MetaDataId::Magazine)); // ? _("YES") : _("NO");
	}

	if (name == "hasSaveState" || name == "hassavestate" || name == "savestate")
	{
		bool hasSaveState = SaveStateRepository::isEnabled(this) && getSourceFileData()->getSystem()->getSaveStateRepository()->hasSaveStates(this);
		return hasSaveState;
	}

	if (name == "releaseyear" || name == "releaseYear")
	{
		std::string releaseDateMeta = getMetadata(MetaDataId::ReleaseDate);
		if (releaseDateMeta.empty())
			return releaseDateMeta;

		Utils::Time::DateTime date = releaseDateMeta;
		if (!date.isValid())
			return std::string();

		return Utils::Time::timeToString(date.getTime(), "%Y");
	}

	MetaDataList& md = getMetadata();

	if (!md.exists(name))
		return BindableProperty::Null;

	std::string finalValue = md.get(name);

	auto type = md.getType(name);

	switch (type)
	{
	case MetaDataType::MD_PATH:				
		return BindableProperty(finalValue, BindablePropertyType::Path);
	case MetaDataType::MD_INT:
		return Utils::String::toInteger(finalValue);
	case MetaDataType::MD_RATING:
		return Utils::String::toFloat(finalValue);
	case MetaDataType::MD_BOOL:
		return finalValue == "1" || finalValue == "true";
	case MetaDataType::MD_TIME:
	case MetaDataType::MD_DATE:
		return finalValue.empty() || finalValue == "0" ? "" : Utils::Time::timeToString(Utils::Time::DateTime(finalValue).getTime(), type == MetaDataType::MD_TIME ? "%Y%m%dT%H%M%S" : "%Y%m%d");
	}

	return finalValue;
}

std::pair<int, int> FileData::parsePlayersRange()
{
	std::string players = getMetadata(MetaDataId::Players);
	if (players.empty())
		return std::pair<int, int>(-1,-1);

	auto key = players;

	int min = 1;

	auto split = key.rfind("+");
	if (split != std::string::npos)
		key = Utils::String::replace(key, "+", "-99999");

	split = key.rfind("-");
	if (split != std::string::npos)
	{
		min = Utils::String::toInteger(key.substr(0, split));
		key = key.substr(split + 1);
	}

	int max = Utils::String::toInteger(key);

	return std::pair<int, int>(min, max);
}

IBindable* FileData::getBindableParent()
{ 
	SystemData* sys = getSystem();

	SystemData* group = sys->getParentGroupSystem();

	if (group != nullptr && sys->isGroupChildSystem())
		return group;

	if (group != nullptr && group != sys)
	{
		std::string showFoldersMode = group->getFolderViewMode();
		if (showFoldersMode == "never")
			return group;

		if (showFoldersMode == "having multiple games")
		{
			for (auto child : group->getRootFolder()->getChildren())
			{
				if (child->getType() == FOLDER && ((FolderData*)child)->getChildren().size() == 1)
					if (((FolderData*)child)->getChildren()[0] == this)
						return group;
			}
		}
	}

	FolderData* parent = getParent();	
	
	if (mPath == "..")
	{
		if (sys->isCollection())
		{
			parent = sys->getRootFolder()->getParent();
			if (parent != nullptr)
				return parent->getSystem();
		}

		if (group != nullptr && sys->isGroupChildSystem())
			return group;		
	}
	
	while (parent != nullptr)
	{
		sys = parent->getSystem();

	//	if (sys->isCollection())
	//		return sys;

		parent = parent->getParent();
	}

	return sys;
}
