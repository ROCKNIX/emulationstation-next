#include "guis/GuiBios.h"

#include "ApiSystem.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "components/ComponentGrid.h"
#include "components/ComponentList.h"
#include "components/MultiLineMenuEntry.h"
#include "components/ButtonComponent.h"
#include "components/TextComponent.h"
#include "GuiLoading.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <cstring>

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.65f))

// Glyphs from the menu font. The check-circle is what the store and the
// theme installer draw for "installed", so present reads the same here. The
// unlink glyph was already this screen's "missing"; the warning triangle was
// drawn for "INVALID", a status the check never emits, while UNTESTED -- the
// file is there, its hash is not one we know -- got the missing icon. It
// gets the triangle now, because it is a different problem with a different
// fix.
#define PRESENT_ICON  _U("")
#define UNTESTED_ICON _U("")
#define MISSING_ICON  _U("")

static std::string biosIcon(const std::string& status)
{
	if (status == "MISSING") return MISSING_ICON;
	if (status == "PRESENT") return PRESENT_ICON;
	return UNTESTED_ICON;
}

void GuiBios::show(Window* window)
{
	// --all: present files too, not only the problems. The launch-time check
	// keeps the default output, which lists problems alone.
	window->pushGui(new GuiLoading<std::vector<BiosSystem>>(window, _("PLEASE WAIT"),
		[](auto gui) { return ApiSystem::getInstance()->getBiosInformations("", true); },
		[window](std::vector<BiosSystem> ra) { window->pushGui(new GuiBios(window, ra)); }));
}

GuiBios::GuiBios(Window* window, const std::vector<BiosSystem> bioses)
	: GuiComponent(window), mGrid(window, Vector2i(1, 4)), mBackground(window, ":/frame.png")
{
	mBios = bioses;

	addChild(&mBackground);
	addChild(&mGrid);

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	// Title
	mTitle = std::make_shared<TextComponent>(mWindow, _("BIOS CHECK"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// Row 1 held a tab strip -- "Installed systems" / "All" -- that filtered
	// the list by whether EmulationStation had loaded a system, which nobody
	// could tell from the label, and drew as a row of words with an
	// underline too thin to see on a 640-wide panel. One list, ordered by
	// what needs doing, replaces it; the row stays in the grid at no height.

	// Entries
	mList = std::make_shared<ComponentList>(mWindow);
	mList->setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);
	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	// Buttons
	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("REFRESH"), _("refresh"), [&] { refresh(); }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("BACK"), _("BACK"), [this] { delete this; }));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool
		{
			if (config->isMappedLike("down", input)) { mGrid.setCursorTo(mList); mList->setCursorIndex(0); return true; }
			if (config->isMappedLike("up", input)) { mList->setCursorIndex(mList->size() - 1); mGrid.moveCursor(Vector2i(0, 1)); return true; }
			return false;
		});

	centerWindow();
	loadList();
}

void GuiBios::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeight(0, titleHeight + titleSubtitleSpacing  + (Renderer::getScreenHeight() * 0.05f));
	mGrid.setRowHeight(1, 0.00001f);
	mGrid.setRowHeight(3, mButtonGrid->getSize().y());
}

void GuiBios::refresh()
{
	mWindow->pushGui(new GuiLoading<std::vector<BiosSystem>>(mWindow, _("PLEASE WAIT"),
		[](auto gui) { return ApiSystem::getInstance()->getBiosInformations("", true); },
		[this](std::vector<BiosSystem> ra) { mBios = ra; loadList(); }));
}

void GuiBios::loadList()
{
	auto theme = ThemeData::getMenuTheme();

	int idx = mList->getCursorIndex();
	mList->clear();

	struct Entry
	{
		BiosSystem  data;
		std::string name;
		bool        hasGames;
		int         missing;
		int         untested;
		int         present;
	};

	std::vector<Entry> entries;
	for (auto& systemBiosData : mBios)
	{
		Entry e;
		e.data = systemBiosData;
		e.name = Utils::String::proper(systemBiosData.name);
		e.hasGames = false;
		e.missing = e.untested = e.present = 0;

		// EmulationStation loads a system only when it has games, so being in
		// this list is "this device has games for it".
		for (auto sys : SystemData::sSystemVector)
		{
			if (sys->getName() == systemBiosData.name)
			{
				e.hasGames = true;
				e.name = sys->getFullName();
			}
		}

		for (auto& f : systemBiosData.bios)
		{
			if (f.status == "MISSING") e.missing++;
			else if (f.status == "PRESENT") e.present++;
			else e.untested++;
		}

		entries.push_back(e);
	}

	// What needs doing first: systems with a problem, and among those the
	// ones there are games for, then by how much is missing. Complete
	// systems follow, games first, so the top of the page is the work and
	// the bottom is the reassurance.
	std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
	{
		const bool ap = a.missing + a.untested > 0;
		const bool bp = b.missing + b.untested > 0;
		if (ap != bp) return ap;
		if (a.hasGames != b.hasGames) return a.hasGames;
		if (a.missing != b.missing) return a.missing > b.missing;
		if (a.untested != b.untested) return a.untested > b.untested;
		return Utils::String::toUpper(a.name) < Utils::String::toUpper(b.name);
	});

	if (entries.empty())
	{
		// Only when the check itself produced nothing: with every system
		// listed, an empty page is a failed check, not a clean one.
		auto text = std::make_shared<TextComponent>(mWindow, _("THE BIOS CHECK RETURNED NOTHING"), theme->Text.font, theme->Text.color);
		if (EsLocale::isRTL())
			text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

		// A TextComponent measures itself as one line when built and the
		// list then keeps that height while widening it, so give the height
		// back (0) and it wraps at the width it is given.
		float wrapWidth = mList->getSize().x();
		if (wrapWidth <= 0)
			wrapWidth = Renderer::ScreenSettings::fullScreenMenus() ? Renderer::getScreenWidth() : WINDOW_WIDTH;
		text->setMultiLine(TextComponent::MultiLineType::MULTILINE);
		text->setSize(wrapWidth * 0.94f, 0);

		ComponentListRow row;
		row.addElement(text, true);
		mList->addRow(row);
	}

	for (auto& e : entries)
	{
		ComponentListRow row;

		auto icon = std::make_shared<TextComponent>(mWindow);
		icon->setText(e.missing > 0 ? std::string(MISSING_ICON) : e.untested > 0 ? std::string(UNTESTED_ICON) : std::string(PRESENT_ICON));
		icon->setColor(theme->Text.color);
		icon->setFont(theme->Text.font);
		icon->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);
		row.addElement(icon, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(theme->Text.font->getLetterHeight() * 0.5f, 0);
		row.addElement(spacer, false);

		const int total = e.missing + e.untested + e.present;
		std::string summary;
		if (e.missing == 0 && e.untested == 0)
			summary = _("ALL PRESENT") + std::string(" (") + std::to_string(total) + ")";
		else
		{
			if (e.missing > 0)
				summary = std::to_string(e.missing) + " " + _("OF") + " " + std::to_string(total) + " " + _("MISSING");
			if (e.untested > 0)
				summary += (summary.empty() ? "" : ", ") + std::to_string(e.untested) + " " + _("NOT A KNOWN VERSION");
		}

		auto line = std::make_shared<MultiLineMenuEntry>(mWindow, e.name, summary);
		row.addElement(line, true);

		Window* window = mWindow;
		BiosSystem data = e.data;
		std::string name = e.name;
		row.makeAcceptInputHandler([window, data, name] { GuiBios::openSystem(window, data, name); });

		mList->addRow(row);
	}

	if (idx >= 0 && idx < mList->size())
		mList->setCursorIndex(idx);

	centerWindow();
	// DETAILS comes and goes with the list (getHelpPrompts).
	updateHelpPrompts();
}

void GuiBios::openSystem(Window* window, const BiosSystem& system, const std::string& displayName)
{
	auto s = new GuiSettings(window, displayName);

	for (auto& f : system.bios)
	{
		std::string status;
		if (f.status == "MISSING")
			status = _("MISSING");
		else if (f.status == "PRESENT")
			status = _("PRESENT");
		else
			status = _("PRESENT, BUT NOT A KNOWN VERSION");

		// The expected hash is how somebody finds the right file; a present,
		// matching one has nothing to look up.
		if (f.status != "PRESENT" && !f.md5.empty() && f.md5 != "-")
			status += " - MD5: " + f.md5;

		// The path as the system expects it, case kept: /storage is
		// case-sensitive, and addWithDescription upper-cases its label, so
		// bios/scph5501.bin read BIOS/SCPH5501.BIN and a file named as the
		// page said still read MISSING (#308 8-es claude F-ES-06). Built by
		// hand, as MANAGE SAVED NETWORKS' rows are for a network's name.
		ComponentListRow row;
		row.addElement(std::make_shared<MultiLineMenuEntry>(window, biosIcon(f.status) + "  " + f.path, status, true), true);
		s->addRow(row);
	}

	window->pushGui(s);
}

void GuiBios::centerWindow()
{
	if (Renderer::ScreenSettings::fullScreenMenus())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
}

bool GuiBios::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		refresh();
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiBios::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	// Only when there is a system to open: the THE BIOS CHECK RETURNED
	// NOTHING row has no action, and a prompt that does nothing is worse than
	// none (#308 8-es claude F-ES-28; es-code-traps.md, the help bar).
	if (!mBios.empty())
		prompts.push_back(HelpPrompt(BUTTON_OK, _("DETAILS")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("REFRESH")));
	return prompts;
}
