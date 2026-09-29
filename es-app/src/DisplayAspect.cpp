#include "DisplayAspect.h"

#include "CaptureRotation.h"
#include "DisplayAspectText.h"
#include "FileData.h"
#include "SystemData.h"
#include "PlatformId.h"
#include "components/ImageComponent.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <map>
#include <mutex>

namespace DisplayAspect
{
	float forSystem(SystemData* system)
	{
		if (system == nullptr)
			return 0.0f;
		return DisplayAspectText::forSystem(system->getThemeFolder());
	}

	Transform forGame(FileData* game)
	{
		Transform t;
		if (game == nullptr)
			return t;
		FileData* source = game->getSourceFileData();
		if (source == nullptr)
			return t;
		t.aspect = forSystem(source->getSystem());
		t.turns = CaptureRotation::read(source);
		return t;
	}

	// The content name RetroArch wrote into the file name is the ROM's
	// file name without its extension. The game is looked for once per
	// screenshot across the game systems -- a list of a few hundred games
	// is a few hundred string compares, and the answer is kept, so moving
	// the cursor back over a screenshot costs nothing. A screenshot whose
	// game is in no list (a ROM since removed, a name RetroArch shortened)
	// keeps the file's own proportions and turn.
	// The screenshot -> game cache, and its lock: which system and which
	// game path a screenshot's content name resolved to ("" when none),
	// never the Transform itself. The turn is asked of CaptureRotation on
	// every look, which stats the record, so a record written by a session
	// or by hand over ssh is seen on the next look without a restart; the
	// cache used to hold the whole Transform and kept the turn it was built
	// with until the process ended (audit #258 PL-019). A FileData pointer
	// is not cached either: the list that owns it is rebuilt by a rescan
	// (#246's lesson), so the game is found again by path each time.
	// forgetScreenshots empties it when a session records a rotation, which
	// also covers a screenshot whose game arrived after the first look.
	struct ShotGame { std::string system; std::string gamePath; };
	static std::mutex sShotLock;
	static std::map<std::string, ShotGame> sShots;
	// The library by stem, built on the first screenshot that misses sShots
	// and kept until forgetScreenshots: a list of S never-seen screenshots
	// used to walk every game S times on the interface thread (audit #258,
	// the seat's G-04); now the library is walked once. The first game of
	// the first system to carry a stem wins, as the walk did.
	static std::map<std::string, ShotGame> sStems;
	static bool sStemsBuilt = false;

	void forgetScreenshots()
	{
		std::unique_lock<std::mutex> lock(sShotLock);
		sShots.clear();
		sStems.clear();
		sStemsBuilt = false;
	}

	static void buildStems()
	{
		sStems.clear();
		for (SystemData* system : SystemData::sSystemVector)
		{
			if (system == nullptr || system->isCollection() || !system->isGameSystem() || system->getRootFolder() == nullptr)
				continue;
			if (system->hasPlatformId(PlatformIds::IMAGEVIEWER))
				continue;
			for (FileData* game : system->getRootFolder()->getFilesRecursive(GAME))
				sStems.emplace(Utils::FileSystem::getStem(game->getPath()), ShotGame{ system->getName(), game->getPath() });
		}
		sStemsBuilt = true;
	}

	static FileData* findGame(const ShotGame& sg)
	{
		if (sg.system.empty())
			return nullptr;
		SystemData* system = SystemData::getSystem(sg.system);
		if (system == nullptr || system->getRootFolder() == nullptr)
			return nullptr;
		return system->getRootFolder()->FindByPath(sg.gamePath);
	}

	Transform forScreenshotPath(const std::string& path)
	{
		{
			std::unique_lock<std::mutex> lock(sShotLock);
			auto it = sShots.find(path);
			if (it != sShots.cend())
			{
				if (it->second.system.empty())
					return Transform();
				if (FileData* game = findGame(it->second))
					return forGame(game);
				// The list was rebuilt under it: look again below.
			}
		}
		const std::string content = DisplayAspectText::screenshotContent(Utils::FileSystem::getFileName(path));
		Transform t;
		ShotGame sg;
		std::unique_lock<std::mutex> lock(sShotLock);
		if (!sStemsBuilt)
			buildStems();
		if (!content.empty())
		{
			auto it = sStems.find(content);
			if (it != sStems.cend())
			{
				sg = it->second;
				if (FileData* game = findGame(sg))
					t = forGame(game);
			}
		}
		sShots[path] = sg;
		return t;
	}

	Transform forScreenshot(FileData* screenshot)
	{
		if (screenshot == nullptr)
			return Transform();
		return forScreenshotPath(screenshot->getPath());
	}

	void applyToBoundImages(GuiComponent* root, const std::string& imagePath, const Transform& transform)
	{
		if (root == nullptr)
			return;
		if (ImageComponent* image = dynamic_cast<ImageComponent*>(root))
		{
			const bool bound = !imagePath.empty() && image->getImagePath() == imagePath;
			image->setDisplayAspect(bound ? transform.aspect : 0.0f);
			image->setDisplayRotation(bound ? transform.turns : 0);
		}
		for (unsigned int i = 0; i < root->getChildCount(); i++)
			applyToBoundImages(root->getChild(i), imagePath, transform);
	}
}
