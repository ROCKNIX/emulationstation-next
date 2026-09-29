#include "GuiRetroAchievementsSettings.h"
#include "ThreadedHasher.h"
#include "GuiHashStart.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "RetroAchievements.h"
#include "CheevosRetry.h"
#include "NetworkThread.h"
#include "utils/Platform.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include "guis/GuiMsgBox.h"
#include "guis/GuiOfflineScan.h"
#include "OfflineScanJob.h"
#include "CloudText.h"
#include "OfflineAchievements.h"
#include "Settings.h"
#include "components/ComponentList.h"
#include "components/MultiLineMenuEntry.h"
#include "utils/TimeUtil.h"
#include "components/SwitchComponent.h"
#include "components/OptionListComponent.h"
#include "components/TextComponent.h"
#include "components/MenuComponent.h"
#include "renderers/Renderer.h"
#include "math/Misc.h"
#include "ThemeData.h"
#include "Window.h"
#include "LocaleES.h"
#include "Log.h"
#include <memory>
#include <thread>

#if defined(ROCKNIX)
// A line of the page's own text, at the standard text size, wrapped to the
// page's width when it needs a second line: what the OFFLINE ACHIEVEMENTS
// page is for is room to read (D-RA-003, D-UI-023: two lines, never three).
// Not selectable, and inset like the selectable rows so it lines up.
//
// The width is settled here rather than left to the list, because
// TextComponent measures its wrap at its full width and draws at its padded
// one (es-code-traps.md): given the width without the inset, the height it
// budgets is for the lines it will actually draw, and only then is the inset
// put back with the height fixed. The width is the menu's own rule
// (MenuComponent::updateSize): the whole screen on a handheld panel, else
// the shorter of the screen's height and nine tenths of its width.
//
// The row's height is measured on longestText -- the longest words it may
// ever carry -- because a ComponentList sizes a row as it is added and a
// text set afterwards that needs one more line hangs below the row's slot.
// Returned so the words can change later (a sentence that becomes true).
static std::shared_ptr<TextComponent> addInfoRow(GuiSettings* s, Window* window, const std::string& text, const std::string& longestText)
{
	auto theme = ThemeData::getMenuTheme();
	const float inset = 10.0f;
	const float width = Renderer::ScreenSettings::fullScreenMenus()
		? (float) Renderer::getScreenWidth()
		: (float) Math::min((int) Renderer::getScreenHeight(), (int) (Renderer::getScreenWidth() * 0.90f));

	auto tc = std::make_shared<TextComponent>(window, longestText, theme->Text.font, theme->Text.color, ALIGN_LEFT,
		Vector3f::Zero(), Vector2f(width - 2 * inset, 0));
	const float height = tc->getSize().y();
	tc->setPadding(Vector4f(inset, 0, inset, 0));
	tc->setSize(width, height);
	tc->setVerticalAlignment(ALIGN_TOP);
	if (text != longestText)
		tc->setText(text);

	ComponentListRow row;
	row.selectable = false;
	row.addElement(tc, true);
	s->addRow(row);
	return tc;
}

// The page's block of text (D-RA-003), with or without its last sentence.
// The last sentence is D-RA-013's: the cache follows the interface's own
// game index, so a game added later is cached by the top-up that runs when
// the device is next connected (raofflineproxy-ctl topup) -- once the index
// knows it, which is INDEX NEW GAMES AT STARTUP's job. So the sentence is
// shown only while that setting is on (audit #186 PL-06, D-UI-055: a
// sentence must be true of what happens); turning the switch on turns the
// setting on, below, so on a device set up through this page it is.
static std::string offlineInfoText(bool indexAtStartup)
{
	std::string text = _("EARN CASUAL ACHIEVEMENTS WITHOUT A CONNECTION. THEY ARE SENT WHEN YOU'RE BACK ONLINE. CASUAL ACHIEVEMENTS ONLY, SO TURNING IT ON TURNS HARDCORE MODE OFF. '!RA!' IN A GAME'S CORNER MEANS AN ACHIEVEMENT HASN'T REACHED RETROACHIEVEMENTS YET.");
	if (indexAtStartup)
		text += " " + _("NEW GAMES ARE ADDED THE NEXT TIME YOU'RE CONNECTED.");
	return text;
}

// A little air between the options and the text under them: an empty,
// non-selectable row half a text line tall (RC-5 round, D-UI-054).
static void addSpacerRow(GuiSettings* s, Window* window)
{
	auto theme = ThemeData::getMenuTheme();
	auto gap = std::make_shared<GuiComponent>(window);
	gap->setSize(0, theme->Text.font->getHeight() * 0.5f);
	ComponentListRow row;
	row.selectable = false;
	row.addElement(gap, true);
	s->addRow(row);
}

// SCAN GAMES FOR OFFLINE ACHIEVEMENTS (fork #179, D-RA-010): the proxy
// serves a game offline only from what it cached while online, so a game
// never started with a connection earned nothing offline. The row runs
// raofflineproxy-ctl scan on a page of its own (GuiOfflineScan, the fourth
// surface tier) and carries one line underneath (D-UI-023): why it cannot
// run now, else how the last scan went and how many games are ready.
//
// A row that cannot run yet is dimmed, not hidden (es-ui-style-guide.md),
// and the dim has to be applied by the entry itself: ComponentList::render
// sets every element's colour every frame from the theme, so a colour set
// once at construction is gone by the first frame (the same reason a QR
// code in a row needs UntintedImageComponent, es-native-ui.md). That is
// MultiLineMenuEntry::setDimmed, and for the OFFLINE ACHIEVEMENTS switch
// -- dimmed while raofflineproxy-ctl is enabling or disabling on its
// behalf (audit #186 PL-27) -- SwitchComponent::setDimmed: the rule first
// lived in two classes here and moved into es-core so the cloud rows
// share it (fork #182).

static bool offlineScanOn()
{
	return SystemConf::getInstance()->getBool("global.retroachievements.offlineproxy");
}

// The cheap question, for a row built at page open: an address on any
// interface. Whether RetroAchievements itself answers is the ctl's to ask
// when the row is pressed -- a network round trip never runs at page build
// (es-ui-style-guide.md, Gating).
static bool offlineScanOnline()
{
	return !Utils::Platform::queryIPAddress().empty();
}

// Whether the row is showing a run the ctl runs on its own (fork #189),
// which keeps the row undimmed whatever the gates say now. A scan from this
// page has its page over it for its whole length (D-UI-078), so the row
// never reports one.
static bool offlineScanShowsRun()
{
	return OfflineAchievements::runningProgress().running;
}

// The line under the row. A run the ctl started on its own first (fork
// #189, below); else why it cannot run now, else how the last run went and
// the count that is the point of the row -- longest form first, and the
// row's own small font decides which fits (D-UI-035). An automatic top-up
// says so in place of the date (D-UI-032).
static std::string offlineScanDetail(bool on, bool online)
{
	const std::string ready = GuiOfflineScan::readyPhrase(OfflineAchievements::readyCount());
	std::vector<std::string> candidates;
	if (const CloudText::RunningProgress run = OfflineAchievements::runningProgress(); run.running)
	{
		// The top-up the ctl runs on its own -- at link-up (D-RA-010) or
		// after the startup index (D-RA-013) -- reports to no job of this
		// process, and the row went on showing the last finished run while
		// 147 games were being cached (fork #189). The ctl's own progress
		// file says how far it has got, and the row says it in the scan's
		// words: the head alone while the ctl is still listing, GAME i OF n
		// once a game is known, the ready count where it fits, and the head
		// as the form that fits any panel (D-UI-035). Ahead of the gates
		// below: the run is in flight whatever the switch or the link says
		// now.
		const std::string head = _("SAVING GAMES FOR OFFLINE PLAY...");
		std::string counted = head;
		if (run.index > 0)
		{
			counted += " - " + std::string(_("GAME")) + " " + std::to_string(run.index);
			if (run.total > 0)
				counted += " " + std::string(_("OF")) + " " + std::to_string(run.total);
		}
		candidates.push_back(counted + "  ·  " + ready);
		if (counted != head)
			candidates.push_back(counted);
		candidates.push_back(head);
	}
	else if (!on)
		return _("TURN ON OFFLINE ACHIEVEMENTS FIRST.");
	else if (!online)
		return _("YOU'RE NOT ONLINE.");
	else if (const CloudText::ScanStamp last = OfflineAchievements::lastScan(); !last.ran)
		candidates = { _("NOT SCANNED YET") + std::string("  ·  ") + ready, ready };
	else
	{
		// The player's cancel is the page's word for it (D-UI-078), not a
		// failure: the row says SKIPPED as the page did.
		const std::string outcome = last.code == 0 ? _("COMPLETED")
			: last.why == "CANCELLED" ? _("SKIPPED") : _("COULDN'T FINISH");
		std::string head;
		if (last.topup)
			head = _("WHEN YOU CAME ONLINE");
		else
		{
			// The player's own date format and clock, as the cloud rows.
			const std::string fmt = Utils::Time::getSystemDateFormat()
				+ (Settings::ClockMode12() ? " %I:%M %p" : " %H:%M");
			head = _("LAST") + std::string(" ") + Utils::Time::timeToString(last.when, fmt);
		}
		candidates = { head + "  -  " + outcome + "  ·  " + ready, outcome + "  ·  " + ready, ready };
	}

	// The description is drawn in the menu's small font, in a row that
	// spans the menu less its insets and the arrow; 0.86 of the menu width
	// is what a 640x480 frame showed the line to have.
	auto theme = ThemeData::getMenuTheme();
	const float menuWidth = Renderer::ScreenSettings::fullScreenMenus()
		? (float) Renderer::getScreenWidth()
		: (float) Math::min((int) Renderer::getScreenHeight(), (int) (Renderer::getScreenWidth() * 0.90f));
	std::shared_ptr<Font> font = theme->TextSmall.font;
	return CloudText::chooseThatFits(candidates, menuWidth * 0.86f,
		[font](const std::string& t) { return font ? font->sizeText(t).x() : 0.0f; });
}

static void offlineScanRefresh(const std::weak_ptr<MultiLineMenuEntry>& weak)
{
	auto entry = weak.lock();
	if (!entry)
		return;
	const bool on = offlineScanOn();
	const bool online = offlineScanOnline();
	// A run in flight keeps the row lit whatever the gates say now.
	entry->setDimmed((!on || !online) && !offlineScanShowsRun());
	// Only when the words change: the refresher below asks once a second,
	// and a line set to itself would still lay the row out again.
	const std::string text = offlineScanDetail(on, online);
	if (text != entry->getDescription())
		entry->setDescription(text);
}

// The row's line follows a run the ctl started on its own -- the top-up at
// link-up (D-RA-010) or after the startup index (D-RA-013) -- through the
// progress file the ctl keeps while it runs (fork #189), and nothing in
// this process says when that file changes. So the page carries
// one component that is never drawn and never focused, asks once a second
// while the page is open, and refreshes the row -- which changes its words
// only when they differ, and keeps its height, since only the words change
// (addOfflineScanRow). Owned by the page (EXTRACHILDREN: GuiComponent's
// destructor deletes it), so it dies with the page; ticked by the page's
// update, which Window gives to the top page alone, so a dialog over the
// page pauses it and the row catches up the second the dialog closes. The
// row is held weakly, as every row on this page is.
class OfflineRowRefresher : public GuiComponent
{
public:
	OfflineRowRefresher(Window* window, const std::weak_ptr<MultiLineMenuEntry>& row)
		: GuiComponent(window), mRow(row), mElapsedMs(0)
	{
		setVisible(false);
		setExtraType(ExtraType::EXTRACHILDREN);
	}

	void update(int deltaTime) override
	{
		GuiComponent::update(deltaTime);
		mElapsedMs += deltaTime;
		if (mElapsedMs < 1000)
			return;
		mElapsedMs = 0;
		offlineScanRefresh(mRow);
	}

private:
	std::weak_ptr<MultiLineMenuEntry> mRow;
	int mElapsedMs;
};

// A press on the row: the reason when it cannot run, else the confirmation
// (D-UI-023: what the scan does and costs is read at the moment of
// deciding, with why the last one could not finish as its second paragraph,
// D-UI-029), then the page. YES first, NO last so B answers NO.
// The scan page itself, from the row's confirmation and from the prompt
// that follows turning the switch on (D-RA-012). The refresh it is handed
// runs as the scan reports, when it ends and when the page closes, so the
// row's line is right the moment the page is gone.
static void offlineScanStart(Window* window, std::weak_ptr<MultiLineMenuEntry> weak)
{
	window->pushGui(new GuiOfflineScan(window, "/usr/bin/raofflineproxy-ctl scan",
		[weak] { offlineScanRefresh(weak); }));
}

static void offlineScanPressed(Window* window, std::weak_ptr<MultiLineMenuEntry> weak)
{
	if (!offlineScanOn())
	{
		window->pushGui(new GuiMsgBox(window, _("TURN ON OFFLINE ACHIEVEMENTS FIRST."), _("OK")));
		return;
	}
	if (!offlineScanOnline())
	{
		window->pushGui(new GuiMsgBox(window, _("YOU'RE NOT ONLINE. TRY AGAIN WHEN YOU'RE ONLINE."), _("OK")));
		return;
	}

	std::string text = _("SCAN GAMES FOR OFFLINE ACHIEVEMENTS?") + std::string("\n\n")
		+ _("THIS LOOKS AT EVERY GAME ON THIS CONSOLE AND SAVES ITS ACHIEVEMENT DATA SO ACHIEVEMENTS CAN BE EARNED WHILE OFFLINE. THIS CAN TAKE A WHILE FOR A LARGE LIBRARY.");
	const CloudText::ScanStamp last = OfflineAchievements::lastScan();
	// A cancelled run is not one that could not finish (D-UI-078): nothing
	// to explain, the next scan carries on.
	if (last.ran && last.code != 0 && !last.why.empty() && last.why != "CANCELLED")
	{
		// The why as a clause, then a full stop -- unless the why is a
		// sentence already (SOME GAMES COULDN'T BE SAVED. TRY THE SCAN
		// AGAIN.), which brings its own.
		const std::string why = OfflineAchievements::scanWhy(last.why);
		text += "\n\n" + _("LAST TIME IT COULDN'T FINISH:") + " " + why
			+ (Utils::String::endsWith(why, ".") ? "" : ".");
	}

	window->pushGui(new GuiMsgBox(window, text,
		_("YES"), [window, weak] { offlineScanStart(window, weak); },
		_("NO"), nullptr));
}

static std::shared_ptr<MultiLineMenuEntry> addOfflineScanRow(GuiSettings* s, Window* window)
{
	// The line goes in at construction: ComponentList sizes the row from the
	// entry as it is added, and an entry made with no substring is one line
	// tall for good -- a description set afterwards hangs below the row's
	// slot, under the selector bar (frame 02 of the first 640x480 run).
	// Later refreshes only change the words, so the height holds.
	const bool on = offlineScanOn();
	const bool online = offlineScanOnline();
	auto entry = std::make_shared<MultiLineMenuEntry>(window, _("SCAN GAMES FOR OFFLINE ACHIEVEMENTS"),
		offlineScanDetail(on, online), false);
	entry->setDimmed((!on || !online) && !offlineScanShowsRun());
	std::weak_ptr<MultiLineMenuEntry> weak = entry;

	ComponentListRow row;
	row.addElement(entry, true);
	row.addElement(makeArrow(window), false);
	row.makeAcceptInputHandler([window, weak] { offlineScanPressed(window, weak); });
	s->addRow(row);
	return entry;
}

// The OFFLINE ACHIEVEMENTS page (fork #165, #173; D-RA-001..003): the
// switch, with what it does, that it is beta and casual-only and what that
// does to hardcore, and what the badge means, each with room. One
// system-wide switch, off by default, backed by raofflineproxy-ctl: enable
// starts the RAOfflineProxy service, records hardcore as it was and turns it
// off -- the proxy is casual-only and refuses hardcore awards -- and sets
// the toggle the launch scripts read at every game start; disable stops the
// service and puts hardcore back as recorded. Written by hand rather than
// through addSwitch: the script is the writer, not this page's save, and
// turning it on asks first, because it changes the HARDCORE MODE row on the
// page below (never silently). The script's last stdout line,
// hardcore=<0|1>, is what that row is set from afterwards, so the row shows
// what system.cfg holds; the same values are mirrored into SystemConf so the
// parent page's own save at close writes what the script wrote rather than
// what it read at open.
//
// hardcoreRow is the parent page's HARDCORE MODE switch, weak on purpose:
// this page's callbacks must never keep a row of the page below alive.
// indexRow is the parent's INDEX NEW GAMES AT STARTUP switch, boxed because
// that row is built after the row that opens this page: turning the switch
// on turns that setting on too (below), and the row has to show it, or the
// parent's save at close would write the row's old state back.
static void openOfflineAchievements(Window* window, std::weak_ptr<SwitchComponent> hardcoreRow,
	std::shared_ptr<std::weak_ptr<SwitchComponent>> indexRow)
{
	auto s = new GuiSettings(window, _("OFFLINE ACHIEVEMENTS (BETA)").c_str());

	auto offline = std::make_shared<SwitchComponent>(window);
	offline->setState(SystemConf::getInstance()->getBool("global.retroachievements.offlineproxy"));
	s->addWithLabel(_("OFFLINE ACHIEVEMENTS (BETA)"), offline);

	// The two options first -- the switch above, the scan under it -- then a
	// little space, then one block of text that explains both (RC-5 round,
	// D-UI-054: "the two options at the top of the page, a little space, and
	// then some text explaining it"). The scan row is dimmed with its reason
	// until the switch is on and the device has an address (fork #179,
	// D-RA-010). RetroArch's disconnected badge is explained here because
	// rcheevos shows it while an award is waiting to reach the server and
	// says nothing about what it means (fork #162); it is quoted so it reads
	// as a thing on screen and not as a typo (D-RA-003).
	std::weak_ptr<MultiLineMenuEntry> scanRow = addOfflineScanRow(s, window);
	// The row follows a top-up the ctl runs while the page is open (fork
	// #189); the page owns the component and deletes it with itself.
	s->addChild(new OfflineRowRefresher(window, scanRow));
	addSpacerRow(s, window);
	std::weak_ptr<TextComponent> infoRow = addInfoRow(s, window,
		offlineInfoText(Settings::CheevosCheckIndexesAtStart()), offlineInfoText(true));

	// Weak on purpose: the callback lives inside the switch it names, so a
	// shared_ptr here would be a cycle that keeps the page alive forever --
	// and the ctl's answer arrives from a thread after the page may have
	// been closed. setState fires the change callback too, so a revert made
	// from inside it would re-enter it: quiet while the code, not the
	// player, sets the state.
	std::weak_ptr<SwitchComponent> offlineWeak = offline;
	auto quiet = std::make_shared<bool>(false);
	auto setQuietly = [offlineWeak, quiet](bool state)
	{
		if (auto sw = offlineWeak.lock())
		{
			*quiet = true;
			sw->setState(state);
			*quiet = false;
		}
	};

	// The ctl runs from a thread of its own (audit #186 PL-27): disable
	// waits on systemctl stop, up to the unit's fifteen seconds, and the
	// interface thread never blocks on a process (es-native-ui.md). While it
	// runs the switch is dimmed and refuses a second press; the rows are set
	// from the ctl's answer on the interface thread (postToUiThread), the
	// pattern sayAfterGame uses. Every row is held weakly: the player may
	// have left the page, or the page below it, before the answer comes,
	// and each row that is gone is simply not set -- system.cfg already
	// holds what the ctl wrote, and the page below saves only what changed.
	auto pending = std::make_shared<bool>(false);
	auto apply = [window, offlineWeak, hardcoreRow, indexRow, infoRow, setQuietly, scanRow, pending](bool on)
	{
		if (*pending)
			return;
		*pending = true;
		if (auto sw = offlineWeak.lock())
			sw->setDimmed(true);

		std::thread([window, on, offlineWeak, hardcoreRow, indexRow, infoRow, setQuietly, scanRow, pending]
		{
			std::string last;
			// executeScriptLegacy: the public route that hands back the real
			// exit status and every line, as the cloud pages use it.
			auto result = ApiSystem::executeScriptLegacy(std::string("/usr/bin/raofflineproxy-ctl ") + (on ? "enable" : "disable") + " 2>/dev/null",
				[&last](const std::string line) { last = line; });
			const bool ok = result.second == 0 && Utils::String::startsWith(last, "hardcore=");
			const bool hardcoreNow = (last == "hardcore=1");

			window->postToUiThread([window, on, ok, hardcoreNow, offlineWeak, hardcoreRow, indexRow, infoRow, setQuietly, scanRow, pending]
			{
				*pending = false;
				if (auto sw = offlineWeak.lock())
					sw->setDimmed(false);

				if (!ok)
				{
					setQuietly(!on);
					window->pushGui(new GuiMsgBox(window,
						on ? _("OFFLINE ACHIEVEMENTS COULDN'T BE TURNED ON.") : _("OFFLINE ACHIEVEMENTS COULDN'T BE TURNED OFF."),
						_("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
					return;
				}
				if (auto row = hardcoreRow.lock())
					row->setState(hardcoreNow);
				SystemConf::getInstance()->set("global.retroachievements.offlineproxy", on ? "1" : "0");
				SystemConf::getInstance()->set("global.retroachievements.hardcore", hardcoreNow ? "1" : "0");
				// The scan row reads the switch: on, it offers the scan; off, it
				// says to turn the switch on first.
				offlineScanRefresh(scanRow);

				// The cache follows the index (D-RA-013), and the index is off
				// by default: turning the switch on turns INDEX NEW GAMES AT
				// STARTUP on as well, said in the confirmation above (audit
				// #186 PL-06). Saved here, not left to the page below: its save
				// writes the row's state, so the row is set too where it is
				// still there, and a page already closed has already saved.
				// The block of text gains its last sentence now that it is
				// true. Turning the switch off leaves the setting as it is --
				// an index is the player's, and costs nothing offline.
				if (on && !Settings::CheevosCheckIndexesAtStart())
				{
					Settings::getInstance()->setBool("CheevosCheckIndexesAtStart", true);
					Settings::getInstance()->saveFile();
					if (auto row = indexRow->lock())
						row->setState(true);
					if (auto info = infoRow.lock())
						info->setText(offlineInfoText(true));
				}

				// Turning it on offers the scan at once (D-RA-012): without it a
				// player who skips the row plays offline with nothing cached.
				// Offline, one line says when to come back to it.
				if (on)
				{
					if (offlineScanOnline())
					{
						std::string text = _("SCAN GAMES FOR OFFLINE ACHIEVEMENTS NOW?") + std::string("\n\n")
							+ _("THIS LOOKS AT EVERY GAME ON THIS CONSOLE AND SAVES ITS ACHIEVEMENT DATA SO ACHIEVEMENTS CAN BE EARNED WHILE OFFLINE. THIS CAN TAKE A WHILE FOR A LARGE LIBRARY.");
						window->pushGui(new GuiMsgBox(window, text,
							_("SCAN NOW"), [window, scanRow] { offlineScanStart(window, scanRow); },
							_("LATER"), nullptr));
					}
					else
					{
						window->pushGui(new GuiMsgBox(window,
							_("YOU'RE NOT ONLINE. SCAN GAMES FOR OFFLINE ACHIEVEMENTS WHEN YOU'RE CONNECTED, SO ACHIEVEMENTS CAN BE EARNED WHILE OFFLINE."),
							_("OK")));
					}
				}
			});
		}).detach();
	};

	offline->setOnChangedCallback([window, offlineWeak, quiet, setQuietly, apply, pending]
	{
		if (*quiet)
			return;
		auto sw = offlineWeak.lock();
		if (!sw)
			return;

		// A press while the ctl is still at work on the last one: the switch
		// goes back to the state the ctl is making, and nothing else happens.
		if (*pending)
		{
			setQuietly(!sw->getState());
			return;
		}

		if (!sw->getState())
		{
			apply(false);
			return;
		}

		// The consequence, read at the moment of deciding (D-UI-023); the
		// page above already says what the feature does. With the startup
		// index off, one more sentence says it goes on too (PL-06). NOT NOW
		// is the last button, so B answers NOT NOW (GuiMsgBox's accelerator)
		// and the switch goes back to off.
		std::string text = _("THIS IS A BETA FEATURE. IT WORKS FOR CASUAL ACHIEVEMENTS ONLY, AND TURNING IT ON TURNS HARDCORE MODE OFF.");
		if (!Settings::CheevosCheckIndexesAtStart())
			text += " " + _("IT ALSO TURNS ON INDEX NEW GAMES AT STARTUP, SO GAMES YOU ADD LATER ARE SAVED FOR OFFLINE PLAY TOO.");
		window->pushGui(new GuiMsgBox(window, text,
			_("TURN ON"), [apply] { apply(true); },
			_("NOT NOW"), [setQuietly] { setQuietly(false); }));
	});

	window->pushGui(s);
}
#endif

GuiRetroAchievementsSettings::GuiRetroAchievementsSettings(Window* window) : GuiSettings(window, _("RETROACHIEVEMENTS SETTINGS").c_str())
{
	addGroup(_("SETTINGS"));

	bool retroachievementsEnabled = SystemConf::getInstance()->getBool("global.retroachievements");
	std::string username = SystemConf::getInstance()->get("global.retroachievements.username");
	std::string password = SystemConf::getInstance()->get("global.retroachievements.password");

	// retroachievements_enable
	auto retroachievements_enabled = std::make_shared<SwitchComponent>(mWindow);
	retroachievements_enabled->setState(retroachievementsEnabled);
	addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);
	
	// retroachievements, username, password
	addInputTextConfigRow(_("USERNAME"), "global.retroachievements.username", false);
	addInputTextConfigRow(_("PASSWORD"), "global.retroachievements.password", true);
#ifndef CHEEVOS_DEV_LOGIN
	// A fork build compiles no developer pair in, so the menu pages authenticate
	// with the player's own web API key, from the account's settings page on
	// retroachievements.org (#68). Held back from backups like the password.
	addInputTextConfigRow(_("WEB API KEY"), "global.retroachievements.key", true);
#endif

	addGroup(_("OPTIONS"));

	auto hardcore = addSwitch(_("HARDCORE MODE"), _("Disable loading states, rewind and cheats for more points."), "global.retroachievements.hardcore", false, nullptr);

#if defined(ROCKNIX)
	// OFFLINE ACHIEVEMENTS: a row that opens a page (D-RA-003; es-player-text
	// "A row that leads somewhere is a label"). The switch and what it means
	// need more room than one line under a row gives them on a 3.5" panel,
	// so the row carries the label and one short line -- sentence case like
	// every description on this page (#166) -- and the page carries the
	// switch with the explanation beside it. Shown only where the backend
	// is: an image without the package has no toggle to offer.
	// indexRow is filled once the GAME INDEXES rows exist, below.
	auto indexRow = std::make_shared<std::weak_ptr<SwitchComponent>>();
	if (Utils::FileSystem::exists("/usr/bin/raofflineproxy-ctl"))
	{
		std::weak_ptr<SwitchComponent> hardcoreRow = hardcore;
		addWithDescription(_("OFFLINE ACHIEVEMENTS (BETA)"), _("Casual achievements only."), makeArrow(mWindow),
			[window, hardcoreRow, indexRow] { openOfflineAchievements(window, hardcoreRow, indexRow); }, "", false, true);
	}
#endif
	addSwitch(_("LEADERBOARDS"), _("Compete in high-score and best time leaderboards (requires hardcore)."), "global.retroachievements.leaderboards", false, nullptr);
	addSwitch(_("VERBOSE MODE"), _("Show achievement progression on game launch and other notifications."), "global.retroachievements.verbose", false, nullptr);
	addSwitch(_("RICH PRESENCE"), "global.retroachievements.richpresence", false);
	addSwitch(_("ENCORE MODE"), _("Unlocked achievements can be earned again."), "global.retroachievements.encore", false, nullptr);
	addSwitch(_("AUTOMATIC SCREENSHOT"), _("Automatically take a screenshot when an achievement is earned."), "global.retroachievements.screenshot", false, nullptr);
	addSwitch(_("CHALLENGE INDICATORS"), _("Shows icons in the bottom right corner when eligible achievements can be earned."), "global.retroachievements.challenge_indicators", false, nullptr);
	// RetroArch's progress tracker is a separate widget from the challenge
	// indicators -- it counts measured progress (34/99 rings) while an
	// indicator marks a live "do X without Y" achievement -- and it had no
	// switch here, so it could not be turned off (maintainer, 2026-09-07).
	// On by default, as RetroArch ships it: absent reads as on, so an
	// upgraded device shows the state it is actually in. Written by hand
	// rather than through addSwitch, whose bool means "store in Settings",
	// not "default" -- the first version passed true there and the value
	// went to es_settings.cfg, where the launch script never looks.
	auto progressTracker = std::make_shared<SwitchComponent>(mWindow);
	progressTracker->setState(SystemConf::getInstance()->getBool("global.retroachievements.progress_tracker", true));
	addWithDescription(_("PROGRESS TRACKER"), _("Shows how far you are toward an achievement while you play."), progressTracker);
	addSaveFunc([progressTracker] { SystemConf::getInstance()->setBool("global.retroachievements.progress_tracker", progressTracker->getState()); });
	addSwitch(_("UNOFFICIAL ACHIEVEMENTS"), _("Enable unlocking of unofficial achievements."), "global.retroachievements.unofficial", false, nullptr);

	// Unlock sound
	auto installedRSounds = ApiSystem::getInstance()->getRetroachievementsSoundsList();
	if (installedRSounds.size() > 0)
	{
		std::string currentSound = SystemConf::getInstance()->get("global.retroachievements.sound");

		auto rsounds_choices = std::make_shared<OptionListComponent<std::string> >(mWindow, _("RETROACHIEVEMENTS UNLOCK SOUND"), false);
		rsounds_choices->add(_("none"), "none", currentSound.empty() || currentSound == "none");

		for (auto snd : installedRSounds)
			rsounds_choices->add(_(Utils::String::toUpper(snd).c_str()), snd, currentSound == snd);

		if (!rsounds_choices->hasSelection())
			rsounds_choices->selectFirstItem();

		addWithLabel(_("UNLOCK SOUND"), rsounds_choices);
		addSaveFunc([rsounds_choices] { SystemConf::getInstance()->set("global.retroachievements.sound", rsounds_choices->getSelected()); });
	}

#if defined(ROCKNIX)
        if (Utils::Platform::GetEnv("DEVICE_ANALOG_STICKS_LED_CONTROL") == "true") {
                // Enable LED Notifications
                auto cheevos_led_enabled = std::make_shared<SwitchComponent>(mWindow);
                bool cheevosledenabled = SystemConf::getInstance()->get("global.retroachievements.leds") == "1";
                cheevos_led_enabled->setState(SystemConf::getInstance()->getBool("global.retroachievements.leds"));
                //s->addWithLabel(_("LED NOTIFICATIONS"), cheevos_led_enabled);
                addWithLabel(_("LED NOTIFICATIONS"), cheevos_led_enabled);
                cheevos_led_enabled->setOnChangedCallback([cheevos_led_enabled] {
                        bool cheevosledenabled = cheevos_led_enabled->getState();
                                SystemConf::getInstance()->set("global.retroachievements.leds", cheevosledenabled ? "1" : "0");
                });
        }
#endif
	// retroachievements_hardcore_mode
	addSwitch(_("SHOW RETROACHIEVEMENTS ENTRY IN MAIN MENU"), _("View your RetroAchievements stats right from the main menu!"), "RetroachievementsMenuitem", true, nullptr);

	addGroup(_("GAME INDEXES"));
	// With OFFLINE ACHIEVEMENTS on, the index feeds the offline cache (fork
	// #184, D-RA-013): as the hasher finishes, the games it identified are
	// cached for offline play from their id and hash, and a game the index
	// does not know is not cached at all. Maintainer: "we need to be clear
	// that scanning for retro achievements [...] will cache new games
	// discovered as well [...] perhaps even changing the text of that option
	// when offline achievements are enabled." So each row says so in one
	// line while the switch is on (D-UI-023), and reads as upstream's when
	// it is off. Read at page build: the switch lives on the page below,
	// and this page is built again on the way back to it.
	bool indexFeedsOffline = false;
#if defined(ROCKNIX)
	indexFeedsOffline = OfflineAchievements::available() && OfflineAchievements::toggleOn();
#endif
	auto indexAtStartup = addSwitch(_("INDEX NEW GAMES AT STARTUP"),
		indexFeedsOffline ? _("Also saves new games' achievement data for offline play.") : std::string(),
		"CheevosCheckIndexesAtStart", true, nullptr);
#if defined(ROCKNIX)
	*indexRow = indexAtStartup;
#endif
	auto indexGames = [this]
	{
		if (ThreadedHasher::checkCloseIfRunning(mWindow))
			mWindow->pushGui(new GuiHashStart(mWindow, ThreadedHasher::HASH_CHEEVOS_MD5));
	};
	if (indexFeedsOffline)
		addWithDescription(_("INDEX GAMES"), _("Also saves their achievement data for offline play."), makeArrow(mWindow), indexGames, "", false, true);
	else
		addEntry(_("INDEX GAMES"), true, indexGames);

	// The switch is the player's choice, and this save writes that choice
	// and nothing else (#175). The sign-in below decides the token only. It
	// used to decide the switch as well: a sign-in that failed because the
	// device was offline -- this page closed on a boot whose first attempt
	// had run before the network was up, so no token was there to skip it
	// -- wrote the switch off, and the RETROACHIEVEMENTS entry left the
	// main menu with nothing on screen to say why.
	//
	// What the player is told depends on why it failed and on what they
	// did. RetroAchievements turned the account down: said every time, the
	// credentials are wrong and no retry changes that. RetroAchievements
	// could not be reached after the switch was turned on or the account
	// changed: said once, with what happens next. Could not be reached and
	// nothing was changed: nothing to say -- the sign-in that runs when the
	// network comes up (NetworkThread) finishes this on its own.
	addSaveFunc([retroachievementsEnabled, retroachievements_enabled, username, password, window]
	{
		bool newState = retroachievements_enabled->getState();
		std::string newUsername = SystemConf::getInstance()->get("global.retroachievements.username");
		std::string newPassword = SystemConf::getInstance()->get("global.retroachievements.password");
		std::string token = SystemConf::getInstance()->get("global.retroachievements.token");

		bool accountChanged = !retroachievementsEnabled || username != newUsername || password != newPassword;
		// On the interface thread only for a change the player made and an
		// address to send it from (#308 1-raoffline claude F-RA-19): nothing
		// changed and no token was a sign-in at every close while offline,
		// the screen waiting on the request -- its connect timeout on a
		// hotspot with a route and no DNS (#242). That case is the token
		// check's, on the watchers' thread, which retries when the network
		// comes up; CheevosRetry::saveSignIn decides.
		const CheevosRetry::SaveSignIn signIn = CheevosRetry::saveSignIn(newState, accountChanged, token.empty(),
			!Utils::Platform::queryIPAddress().empty());
		if (signIn == CheevosRetry::SaveSignIn::InBackground)
			LOG(LogInfo) << "retroachievements: no token; the token check is asked to sign in, off the interface thread";
		else if (signIn == CheevosRetry::SaveSignIn::Offline)
		{
			// The account changed and there is nowhere to send it from: the
			// old token is not this account's, and the answer is the one a
			// request would have come back with, without the wait.
			SystemConf::getInstance()->set("global.retroachievements.token", "");
			window->pushGui(new GuiMsgBox(window,
				_("COULDN'T REACH RETROACHIEVEMENTS TO SIGN YOU IN.\n\nRETROACHIEVEMENTS STAYS ON. IT'LL SIGN IN WHEN YOU'RE ONLINE."),
				_("OK")));
		}
		else if (signIn == CheevosRetry::SaveSignIn::Now)
		{
			std::string tokenOrError;
			bool refused = false;
			if (RetroAchievements::testAccount(newUsername, newPassword, tokenOrError, &refused))
				SystemConf::getInstance()->set("global.retroachievements.token", tokenOrError);
			else
			{
				SystemConf::getInstance()->set("global.retroachievements.token", "");

				if (refused)
					window->pushGui(new GuiMsgBox(window,
						_("RETROACHIEVEMENTS DIDN'T ACCEPT YOUR SIGN-IN:") + "\n" + tokenOrError + "\n\n"
						+ _("RETROACHIEVEMENTS STAYS ON. CHECK YOUR USERNAME AND PASSWORD, THEN TRY AGAIN."),
						_("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				else if (accountChanged)
					window->pushGui(new GuiMsgBox(window,
						_("COULDN'T REACH RETROACHIEVEMENTS TO SIGN YOU IN.\n\nRETROACHIEVEMENTS STAYS ON. IT'LL SIGN IN WHEN YOU'RE ONLINE."),
						_("OK")));
				else
					LOG(LogWarning) << "retroachievements: could not reach RetroAchievements for a token (" << tokenOrError << "); the switch stays as set, and the sign-in runs when the network is up";
			}
		}
		else if (!newState)
			SystemConf::getInstance()->set("global.retroachievements.token", "");

		if (SystemConf::getInstance()->setBool("global.retroachievements", newState))
			if (!ThreadedHasher::isRunning() && newState)
				ThreadedHasher::start(window, ThreadedHasher::HASH_CHEEVOS_MD5, false, true);

		// After the switch is written: the check reads it, and one that ran
		// before a switch just turned on would find it off and do nothing
		// -- nor retry when the network comes up.
		if (signIn == CheevosRetry::SaveSignIn::InBackground || signIn == CheevosRetry::SaveSignIn::Offline)
			NetworkThread::checkCheevosTokenSoon();
	});
}
