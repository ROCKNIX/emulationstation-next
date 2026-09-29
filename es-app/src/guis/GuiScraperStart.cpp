#include "guis/GuiScraperStart.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "scrapers/ThreadedScraper.h"
#include "guis/GuiScraperRun.h"
#include "scrapers/ScreenScraper.h"
#include "LocaleES.h"
#include "GuiLoading.h"
#include "views/gamelist/IGameListView.h"
#include <set>

GuiScraperStart::GuiScraperStart(Window* window)
	: GuiSettings(window, _("SCRAPER"), true)
{
	addTab(_("SCRAPE"));
	addTab(_("OPTIONS"));
	addTab(_("ACCOUNTS"));
	setOnTabIndexChanged([&] { loadActivePage(); });

	loadActivePage();

	onFinalize([] { Settings::getInstance()->saveFile(); });

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiScraperStart::loadActivePage()
{	
	save();
	clearSaveFuncs();

	mMenu.clear();
	mMenu.clearButtons();

	switch (this->getTabIndex())
	{
	case 0:
		loadScrapPage();
		break;
	case 1:
		loadSettingsPage();
		break;
	case 2:
		loadAccountsPage();
		break;
	}

	mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });
}

void GuiScraperStart::loadScrapPage() 
{
	mOverwriteMedias = true;

	std::string scraperName = Settings::getInstance()->getString("Scraper");

	// scrape from
	auto scraper_list = std::make_shared< OptionListComponent< std::string > >(mWindow, _("SCRAPING DATABASE"), false);

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for (auto engine : Scraper::getScraperList())
		scraper_list->add(engine, engine, engine == scraperName);


	addGroup(_("SOURCE"));
	addWithLabel(_("SCRAPE FROM"), scraper_list);

	if (!scraper_list->hasSelection())
	{
		scraper_list->selectFirstItem();
		scraperName = scraper_list->getSelected();
	}	

	addGroup(_("FILTERS"));

	auto scraper = Scraper::getScraper(scraperName);

	// GAMES TO SCRAPE FOR, IGNORE RECENTLY SCRAPED GAMES and SYSTEMS INCLUDED are
	// remembered (#67). They used to be rebuilt with hard-coded defaults on every
	// visit and on every tab switch, so a choice made before stepping to OPTIONS
	// was gone on the way back. Each is written by a save function, which runs
	// when the tab changes and when the page closes -- the path the OPTIONS rows
	// already use.

	// Media Filter
	std::string mediaFilter = Settings::getInstance()->getString("ScraperFilter");
	if (mediaFilter != "all" && mediaFilter != "missing-all")
		mediaFilter = "missing-any";

	mFilters = std::make_shared< OptionListComponent<FilterFunc> >(mWindow, _("GAMES TO SCRAPE FOR"), false);
	mFilters->add(_("ALL"), [](FileData*) -> bool { return true; }, mediaFilter == "all");
	mFilters->add(_("GAMES MISSING ANY MEDIA"), [this, scraper](FileData* g) -> bool { mOverwriteMedias = false; return scraper->hasMissingMedia(g); }, mediaFilter == "missing-any");
	mFilters->add(_("GAMES MISSING ALL MEDIA"), [this, scraper](FileData* g) -> bool { mOverwriteMedias = true; return !scraper->hasAnyMedia(g); }, mediaFilter == "missing-all");
	addWithLabel(_("GAMES TO SCRAPE FOR"), mFilters);
	addSaveFunc([this]
	{
		static const char* keys[] = { "all", "missing-any", "missing-all" };
		int i = mFilters->getSelectedIndex();
		if (i >= 0 && i < 3)
			Settings::getInstance()->setString("ScraperFilter", keys[i]);
	});

	// Date Filter
	auto now = Utils::Time::now();
	auto isOlderThan = [now](const std::string& scraper, FileData* f, int days)
	{
		auto date = f->getMetadata().getScrapeDate(scraper);
		if (date == nullptr || !date->isValid())
			return true;

		return date->getTime() <= (now - (days * 86400));
	};

	// Upstream declared this setting (default 3, LAST 15 DAYS) and never read it.
	int idx = Settings::getInstance()->getInt("RecentlyScrappedFilter");
	if (idx < 0 || idx > 6)
		idx = 3;

	mDateFilters = std::make_shared< OptionListComponent<FilterFunc> >(mWindow, _("IGNORE RECENTLY SCRAPED GAMES"), false);
	mDateFilters->add(_("NO"), [](FileData*) -> bool { return true; }, idx == 0);
	mDateFilters->add(_("LAST DAY"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 1); }, idx == 1);
	mDateFilters->add(_("LAST WEEK"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 7); }, idx == 2);
	mDateFilters->add(_("LAST 15 DAYS"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 15); }, idx == 3);
	mDateFilters->add(_("LAST MONTH"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 31); }, idx == 4);
	mDateFilters->add(_("LAST 3 MONTHS"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 90); }, idx == 5);
	mDateFilters->add(_("LAST YEAR"), [this, scraperName, isOlderThan](FileData* g) -> bool { return isOlderThan(scraperName, g, 365); }, idx == 6);
	addWithLabel(_("IGNORE RECENTLY SCRAPED GAMES"), mDateFilters);
	addSaveFunc([this]
	{
		int i = mDateFilters->getSelectedIndex();
		if (i >= 0)
			Settings::getInstance()->setInt("RecentlyScrappedFilter", i);
	});

	// System Filter
	std::string currentSystem;

	if (ViewController::get()->getState().viewing == ViewController::GAME_LIST)
	{
		auto uiSystem = ViewController::get()->getState().getSystem();
		if (uiSystem != nullptr)
		{
			if (uiSystem->isGroupSystem())
			{
				auto gl = ViewController::get()->getGameListView(uiSystem, false);
				if (gl != nullptr && gl->getCursor() != nullptr)
					uiSystem = gl->getCursor()->getSourceFileData()->getSystem();
			}

			if (uiSystem != nullptr)
				currentSystem = uiSystem->getName();
		}
	}

	// Opened from a game list, the page pre-selects that system ("scrape this
	// one"). Opened from the main menu, it restores the set used last time, and
	// falls back to today's default -- every system with a platform id -- when
	// nothing was remembered or the remembered names match no listed system.
	std::set<std::string> remembered;
	for (auto& name : Utils::String::split(Settings::getInstance()->getString("ScraperSystems"), ','))
		if (!name.empty())
			remembered.insert(name);

	std::vector<SystemData*> listed;
	bool anyRemembered = false;
	for (auto system : SystemData::sSystemVector)
		if (system->isGameSystem() && !system->isCollection() && !system->hasPlatformId(PlatformIds::PLATFORM_IGNORE) && scraper->isSupportedPlatform(system))
		{
			listed.push_back(system);
			if (remembered.count(system->getName()))
				anyRemembered = true;
		}

	bool useRemembered = currentSystem.empty() && anyRemembered;

	mSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS INCLUDED"), true);

	for (auto system : listed)
	{
		bool selected;
		if (!currentSystem.empty())
			selected = system->getName() == currentSystem && !system->getPlatformIds().empty();
		else if (useRemembered)
			selected = remembered.count(system->getName()) > 0;
		else
			selected = !system->getPlatformIds().empty();

		mSystems->add(system->getFullName(), system, selected);
	}

	addWithLabel(_("SYSTEMS INCLUDED"), mSystems);

	// A game list's pre-selection is contextual; only a set chosen from the main
	// menu is worth remembering.
	if (currentSystem.empty())
		addSaveFunc([this]
		{
			std::string names;
			for (auto system : mSystems->getSelectedObjects())
				names += (names.empty() ? "" : ",") + system->getName();

			Settings::getInstance()->setString("ScraperSystems", names);
		});

	mMenu.clearButtons();
	mMenu.addButton(_("SCRAPE NOW"), _("START"), std::bind(&GuiScraperStart::pressedStart, this));

	scraper_list->setSelectedChangedCallback([this, scraperName, scraper_list](std::string value) { Settings::getInstance()->setString("Scraper", value); });
}

void GuiScraperStart::loadSettingsPage() 
{
	auto scrap = Scraper::getScraper();
	if (scrap == nullptr)
		return;

	addGroup(_("SETTINGS"));

	std::string scraper = Scraper::getScraperName(scrap);

	std::string imageSourceName = Settings::getInstance()->getString("ScrapperImageSrc");
	auto imageSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("IMAGE SOURCE"), false);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Screenshot) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::Mix) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt) ||
		scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
	{
		// Image source : <image> tag

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Screenshot))
			imageSource->add(_("SCREENSHOT"), "ss", imageSourceName == "ss");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
			imageSource->add(_("TITLE SCREENSHOT"), "sstitle", imageSourceName == "sstitle");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Mix))
		{
			imageSource->add(_("MIX V1"), "mixrbv1", imageSourceName == "mixrbv1");
			imageSource->add(_("MIX V2"), "mixrbv2", imageSourceName == "mixrbv2");
		}

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
			imageSource->add(_("BOX 2D"), "box-2D", imageSourceName == "box-2D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
			imageSource->add(_("BOX 3D"), "box-3D", imageSourceName == "box-3D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt))
			imageSource->add(_("FAN ART"), "fanart", imageSourceName == "fanart");

		imageSource->add(_("NONE"), "", imageSourceName.empty());

		if (!imageSource->hasSelection())
			imageSource->selectFirstItem();

		addWithLabel(_("IMAGE SOURCE"), imageSource);
		addSaveFunc([imageSource] { Settings::getInstance()->setString("ScrapperImageSrc", imageSource->getSelected()); });
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d) || scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
	{
		// Box source : <thumbnail> tag
		std::string thumbSourceName = Settings::getInstance()->getString("ScrapperThumbSrc");
		auto thumbSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("BOX SOURCE"), false);
		thumbSource->add(_("NONE"), "", thumbSourceName.empty());

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
			thumbSource->add(_("BOX 2D"), "box-2D", thumbSourceName == "box-2D");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))//if (scraper == "HfsDB")
			thumbSource->add(_("BOX 3D"), "box-3D", thumbSourceName == "box-3D");

		if (!thumbSource->hasSelection())
			thumbSource->selectFirstItem();

		addWithLabel(_("BOX SOURCE"), thumbSource);
		addSaveFunc([thumbSource] { Settings::getInstance()->setString("ScrapperThumbSrc", thumbSource->getSelected()); });

		imageSource->setSelectedChangedCallback([this, scrap, thumbSource](std::string value)
			{
				if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box2d))
				{
					if (value == "box-2D")
						thumbSource->remove(_("BOX 2D"));
					else
						thumbSource->add(_("BOX 2D"), "box-2D", false);
				}

				if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Box3d))
				{
					if (value == "box-3D")
						thumbSource->remove(_("BOX 3D"));
					else
						thumbSource->add(_("BOX 3D"), "box-3D", false);
				}
			});
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Marquee) || scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel) || scrap->isMediaSupported(Scraper::ScraperMediaSource::WheelHD))
	{
		// Logo source : <marquee> tag
		std::string logoSourceName = Settings::getInstance()->getString("ScrapperLogoSrc");

		auto logoSource = std::make_shared< OptionListComponent<std::string> >(mWindow, _("LOGO SOURCE"), false);
		logoSource->add(_("NONE"), "", logoSourceName.empty());

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel))
			logoSource->add(_("HD WHEEL"), "wheel-hd", logoSourceName == "wheel-hd");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Wheel))
			logoSource->add(_("WHEEL"), "wheel", logoSourceName == "wheel");

		if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Marquee)) // if (scraper == "HfsDB")
			logoSource->add(_("MARQUEE"), "marquee", logoSourceName == "marquee");

		if (!logoSource->hasSelection())
			logoSource->selectFirstItem();

		addWithLabel(_("LOGO SOURCE"), logoSource);
		addSaveFunc([logoSource] { Settings::getInstance()->setString("ScrapperLogoSrc", logoSource->getSelected()); });
	}

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Region))
	{
		std::string region = Settings::getInstance()->getString("ScraperRegion");
		auto regionCtrl = std::make_shared< OptionListComponent<std::string> >(mWindow, _("PREFERED REGION"), false);
		regionCtrl->addRange({ { _("AUTO"), "" }, { "EUROPE", "eu" },  { "USA", "us" }, { "JAPAN", "jp" } , { "WORLD", "wor" } }, region);

		if (!regionCtrl->hasSelection())
			regionCtrl->selectFirstItem();

		addWithLabel(_("PREFERED REGION"), regionCtrl);
		addSaveFunc([regionCtrl] { Settings::getInstance()->setString("ScraperRegion", regionCtrl->getSelected()); });
	}

	addSwitch(_("OVERWRITE NAMES"), "ScrapeNames", true);
	addSwitch(_("OVERWRITE DESCRIPTIONS"), "ScrapeDescription", true);
	addSwitch(_("OVERWRITE MEDIAS"), "ScrapeOverWrite", true);

	addGroup(_("SCRAPE FOR"));

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::ShortTitle))
		addSwitch(_("SHORT NAME"), "ScrapeShortTitle", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Ratings))
		addSwitch(_("COMMUNITY RATING"), "ScrapeRatings", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Video))
		addSwitch(_("VIDEO"), "ScrapeVideos", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::FanArt))
		addSwitch(_("FAN ART"), "ScrapeFanart", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::BoxBack))
		addSwitch(_("BOX BACKSIDE"), "ScrapeBoxBack", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Map))
		addSwitch(_("MAP"), "ScrapeMap", true);

	/*
	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::TitleShot))
	addSwitch(_("SCRAPE TITLESHOT"), "ScrapeTitleShot", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Cartridge))
	addSwitch(_("SCRAPE CARTRIDGE"), "ScrapeCartridge", true);
	*/

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::Manual))
		addSwitch(_("MANUAL"), "ScrapeManual", true);

	if (scrap->isMediaSupported(Scraper::ScraperMediaSource::PadToKey))
		addSwitch(_("PADTOKEY SETTINGS"), "ScrapePadToKey", true);


	addGroup(_("MANUAL SCRAPE"));

	auto excludedScrapers = Utils::String::split(Settings::getInstance()->getString("DisabledManualScrapers"), ';');

	// scrape from
	auto scraper_list = std::make_shared<OptionListComponent<std::string>>(mWindow, _("INCLUDED SCRAPERS"), true);

	// Select either the first entry of the one read from the settings, just in case the scraper from settings has vanished.
	for (auto engine : Scraper::getScraperList())
		scraper_list->add(engine, engine, std::find(excludedScrapers.cbegin(), excludedScrapers.cend(), engine) == excludedScrapers.cend());

	addWithLabel(_("INCLUDED SCRAPERS"), scraper_list);
	addSaveFunc([this, scraper_list]
		{
			Utils::String::stringVector scraperNames = scraper_list->getSelectedObjects();

			std::string exclusion;
			for (auto engine : Scraper::getScraperList())
				if (std::find(scraperNames.cbegin(), scraperNames.cend(), engine) == scraperNames.cend())
					exclusion = exclusion.empty() ? engine : exclusion + ";" + engine;

			Settings::getInstance()->setString("DisabledManualScrapers", exclusion);			
		});
}

void GuiScraperStart::loadAccountsPage() 
{
	addGroup(_("SCREENSCRAPER"));
	addInputTextConfigRow(_("USERNAME"), "ScreenScraperUser", false, true);
	addInputTextConfigRow(_("PASSWORD"), "ScreenScraperPass", true, true);
#if defined(SCREENSCRAPER_RUNTIME_DEV_LOGIN) && !defined(SCREENSCRAPER_DEV_LOGIN)
	// This build carries no developer pair; the player enters theirs here,
	// beside the account it belongs to (#64). Someone with their own developer
	// access types the same pair in both places.
	addInputTextConfigRow(_("DEVELOPER ID"), "ScreenScraperDevId", false, true);
	addInputTextConfigRow(_("DEVELOPER PASSWORD"), "ScreenScraperDevPass", true, true);
#endif

	addGroup(_("IGDB"));
	addInputTextConfigRow(_("CLIENT ID"), "IGDBClientID", false, true);
	addInputTextConfigRow(_("CLIENT SECRET"), "IGDBSecret", true, true);
}

void GuiScraperStart::pressedStart()
{
#if defined(SCREENSCRAPER_RUNTIME_DEV_LOGIN) && !defined(SCREENSCRAPER_DEV_LOGIN)
	// Without a developer pair the API refuses every request with a French
	// login error. Say what is missing and where it goes: the pair's rows
	// are on the ACCOUNTS tab, under the account they belong to
	// (loadAccountsPage), which this and the rejected-pair message both
	// used to place under OPTIONS (#151 PL-15).
	if (Settings::getInstance()->getString("Scraper") == "ScreenScraper" && screenScraperDevLogin().empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("SCREENSCRAPER NEEDS A DEVELOPER ID AND PASSWORD.\nENTER YOURS UNDER SCRAPER > ACCOUNTS.")));
		return;
	}
#endif

	std::vector<SystemData*> systems = mSystems->getSelectedObjects();
	for(auto system : systems)
	{
		if (system->getPlatformIds().empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, 
				_("WARNING: SOME OF YOUR SELECTED SYSTEMS DO NOT HAVE A PLATFORM SET. RESULTS MAY BE FOR DIFFERENT SYSTEMS!\nCONTINUE ANYWAY?"),  
				_("YES"), std::bind(&GuiScraperStart::start, this),  
				_("NO"), nullptr)); 

			return;
		}
	}

	start();
}

void GuiScraperStart::start()
{
	if (ThreadedScraper::isRunning())
	{
		Window* window = mWindow;

		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT?"),
			_("YES"), [this, window] { ThreadedScraper::stop(); },
			_("NO"), nullptr));

		return;
	}

	mWindow->pushGui(new GuiLoading<std::queue<ScraperSearchParams>>(mWindow, _("PLEASE WAIT"),
		[this](IGuiLoadingHandler* gui)
		{
			return getSearches(mSystems->getSelectedObjects(), mFilters->getSelected(), mDateFilters->getSelected(), gui);
		},
		[this](std::queue<ScraperSearchParams> searches)
		{
			if (searches.empty())
				mWindow->pushGui(new GuiMsgBox(mWindow, _("NO GAMES FIT THAT CRITERIA.")));
			else
			{
				// The run's page over this one, then this one goes: the
				// scrape is watched to its end (D-UI-078), and the outcome
				// is dismissed back onto the game lists.
				ThreadedScraper::start(mWindow, searches);
				if (ThreadedScraper::isRunning())
					mWindow->pushGui(new GuiScraperRun(mWindow));
				close();
			}
		}));
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, FilterFunc mediaSelector, FilterFunc dateSelector, IGuiLoadingHandler* handler)
{
	std::queue<ScraperSearchParams> queue;

	int pos = 1;
	for (auto system : systems)
	{		
		if (systems.size() > 0 && handler != nullptr)
		{
			int percent = (pos * 100) / systems.size();
			handler->setText(_("PLEASE WAIT") + " " + std::to_string(percent) + "%");
		}

		auto games = system->getRootFolder()->getFilesRecursive(GAME);
		for(auto game : games)
		{
			if (dateSelector(game) && mediaSelector(game))
			{
				ScraperSearchParams search;
				search.game = game;
				search.system = system;
				search.overWriteMedias = mOverwriteMedias;

				queue.push(search);
			}
		}

		pos++;
	}

	return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiSettings::input(config, input);
	if (consumed)
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		close();
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}

	return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
