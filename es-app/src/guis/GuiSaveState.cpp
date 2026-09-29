#include "GuiSaveState.h"
#include "DisplayAspect.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "utils/TimeUtil.h"
#include "utils/FileSystemUtil.h"
#include "ApiSystem.h"
#include "HelpStyle.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "SaveStateRepository.h"
#include "ThreadedCloudSync.h"
#include "Log.h"
#include "SaveStateBookkeeper.h"
#include "utils/AtomicFileUtil.h"
#include <algorithm>

// Every writer of the saves tree is gated by the transfer lock (D-CLOUD-053),
// and the interface's own sync card is not the only holder of it: a
// cloud_backup started from a shell, a restore or a content run holds
// /var/run/cloud_sync.lock for its whole length with no card on screen
// (#307 PL-068). So DELETE and COPY ask both -- the card first, for its
// words, which point at the card; then the lock, whose words cannot. "" when
// the saves tree is free; else what the refusal says.
static std::string savesTreeBusy(const std::string& cardWords)
{
	if (ThreadedCloudSync::isRunning())
		return cardWords;
	// Not while it is this process's own: a deletion holds the lock for its
	// moment (SaveStateBookkeeper, runDelete), and a press then is queued
	// behind it, not refused as a transfer.
	if (Utils::AtomicFile::isFlockHeld("/var/run/cloud_sync.lock") && !SaveStateBookkeeper::holdsTransferLock())
		return _("A SYNC IS ALREADY RUNNING.") + std::string("\n\n") + _("WAIT FOR IT TO FINISH, THEN TRY AGAIN.");
	return "";
}

// 0.55 of the screen: the sheet has to hold a tile whose label is two
// lines of the small font over a thumbnail still worth looking at. At 0.40
// a 640x480 panel left a 92 px grid row, one line of label, and START NEW
// GAME / START NEW AUTO SAVE ending in "..." (#27); at 0.50 two lines left
// the thumbnail 74 px tall there.
#define WINDOW_HEIGHT Renderer::getScreenHeight() * 0.55f

static int slots = 6; // 5;

GuiSaveState::GuiSaveState(Window* window, FileData* game, const std::function<void(SaveState* state)>& callback) :
	GuiComponent(window), mGrid(nullptr), mBackground(window, ":/frame.png"),
	mLayout(window, Vector2i(3, 5)), mTitle(nullptr)
{
	mGame = game;
	mRepository = game->getSourceFileData()->getSystem()->getSaveStateRepository();
	mRunCallback = callback;

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path); // ":/frame.png"
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	mTitle = std::make_shared<TextComponent>(mWindow, _("SAVE STATE MANAGER"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mLayout.setEntry(mTitle, Vector2i(1, 1), false, true);

	mGrid = std::make_shared<ImageGridComponent<SaveStateItem>>(mWindow);
	// RetroArch writes a thumbnail at the core's native size, which for the
	// NES and the SNES is not the screen's shape (fork #243, D-UI-080): the
	// tiles are drawn at the system's aspect, the picture scaled to it.
	{
		// A tile that shows one of this game's captures takes the game's
		// display transform -- its system's aspect and the turn the display
		// gave its frame (fork #243, #245). Only that tile: START NEW GAME
		// and START NEW AUTO SAVE carry the arrow, and a grid-wide transform
		// turned and fitted the arrow with the rest (fork #250).
		const DisplayAspect::Transform t = DisplayAspect::forGame(game);
		mGrid->setTileDecorator([t](GridTileComponent* tile, const SaveStateItem& item)
		{
			const bool capture = item.saveState != nullptr && !item.saveState->getScreenShot().empty();
			tile->setDisplayAspect(capture ? t.aspect : 0.0f);
			tile->setDisplayRotation(capture ? t.turns : 0);
		});
	}
	mLayout.setEntry(mGrid, Vector2i(1, 3), true, true);

	addChild(&mBackground);
	addChild(&mLayout);
	
	float cellProportion = 1.77;
	float screenProportion = (float)Renderer::getScreenWidth() / (float)Renderer::getScreenHeight();

	float sh = (float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth());
	sh = (float) theme->TextSmall.font->getSize() / sh;

	// The label's share of a tile is whatever two lines of the tile's font
	// are of the tile's height, never less than the 0.30 it was nor more than half (#27). The
	// tile is the grid's one row: the sheet less its spacing, title and help
	// rows, laid out by onSizeChanged with the same arithmetic. TextComponent
	// wraps on its own once its height clears 1.8 lines, so two full lines
	// make START NEW GAME, START NEW AUTO SAVE and a slot's number-and-date
	// wrap where they used to end in "...". The strings themselves are
	// untouched: they are msgids in every locale.
	// Font::getHeight is the tallest glyph rasterised SO FAR, so measured
	// before the labels have drawn it under-reports: at 640x480 the share
	// computed from it fell to the 0.30 floor while the renderer, measuring
	// later with more glyphs loaded, found the area under its two-line
	// threshold and ellipsised every label (878ec8863b, 2026-09-13).
	// Rasterise the printable range first, so this height is the one the
	// labels are laid out with; and the tile text is told to wrap outright
	// (<multiLine>true</multiLine>) rather than left to that threshold.
	std::string ascii;
	for (int c = 32; c < 127; c++)
		ascii.push_back((char)c);
	theme->TextSmall.font->sizeText(ascii);

	// The widest line a tile shows is a slot's date and time. A tile is the
	// grid's width over the columns it lays out (the same arithmetic as
	// ImageGridComponent::calcGridDimension: the sheet less its two 0.01
	// side columns, less a margin between columns), and on a 640x480 panel
	// that is about 152 px while the date in the small font is about 150:
	// the labels of neighbouring tiles ran into each other ("21:4709/12/
	// 2026", 02f368914e). Where the date would take more than 0.86 of the
	// tile, the label font shrinks so it fits with a gap either side; the
	// two-line height below is then measured on that smaller font.
	const int columns = (int)(slots * screenProportion / cellProportion);
	const float marginX = 0.01f * (float)Renderer::getScreenWidth();
	const float tileWidth = ((float)Renderer::getScreenWidth() * 0.98f - marginX * (columns - 1)) / (float)Math::max(1, columns);
	//
	// The font a tile ends up with is not the theme's object: the tile makes
	// its own from the size it is handed, through Font::get with the screen's
	// font scale (1.31 under 720 px), and the theme's small font was ALREADY
	// made with the menu scale. Handing the theme font's size straight
	// through -- what this page did from the start -- scaled it twice: on a
	// 640x480 panel a 16 px small font became 20, then 26, which is half of
	// why the labels never fit and why a date measured on the theme's font
	// read narrower than the tile drew it (5776ede212). Ask for the size that
	// comes out equal to the theme's, and measure with the font the tile
	// will use. At 1280x800 the scale is 1 and nothing changes.
	const float minSide = (float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth());
	const float screenScale = Renderer::ScreenSettings::fontScale() > 0.0f ? Renderer::ScreenSettings::fontScale() : 1.0f;
	//
	// One point under the theme's small font (maintainer, 2026-09-15, on
	// the RG SP with RC-12: "the save state manager looks great, but the
	// text could be a tiny bit smaller, maybe one point or so"): 15 px
	// where the 640x480 panel's small font is 16, the date-width shrink
	// below still applying after it.
	const int pointsUnderSmall = 1;
	int requested = Math::max(1, (int)((float)theme->TextSmall.font->getSize() / screenScale + 0.5f) - pointsUnderSmall);
	std::shared_ptr<Font> labelFont = Font::get(requested, theme->TextSmall.font->getPath());
	labelFont->sizeText(ascii);
	{
		// now()'s text is as long as any tile's: the locale's date is one
		// width, and the time half writes the same number of characters at
		// every hour on either clock, AM/PM included (fork #195; TimeText's
		// test holds it to that), so the sample measures the row's shape
		// whatever the switch says when the manager opens. A tile's second
		// line is TODAY at 17:07, YESTERDAY at 14:03 or 09/24/26 at 14:03
		// (fork #195, D-UI-089), so the sample is the widest of the three
		// words with the same "at" and time -- in French AUJOURD'HUI is the
		// longest, in English the date is.
		const std::string at = " " + _("at") + " " + Utils::Time::DateTime::localClockText(Utils::Time::DateTime::now().getTimeStruct());
		const std::string candidates[] = { _("TODAY") + at, _("YESTERDAY") + at, Utils::Time::DateTime::now().toShortLocalDateString() + at };
		std::string widest = candidates[0];
		for (const std::string& c : candidates)
			if (labelFont->sizeText(c).x() > labelFont->sizeText(widest).x())
				widest = c;
		const float widestPx = labelFont->sizeText(widest).x();
		if (widestPx > tileWidth * 0.86f && widestPx > 0)
		{
			requested = Math::max(1, (int)((float)requested * tileWidth * 0.86f / widestPx));
			labelFont = Font::get(requested, theme->TextSmall.font->getPath());
			labelFont->sizeText(ascii);
		}
	}
	sh = (float)requested / minSide;
	const float sheetHeight = WINDOW_HEIGHT;
	const float titlePerc = theme->Title.font->getHeight(2.0f) / sheetHeight;
	// The same style onSizeChanged measures, so the two agree (see
	// helpRowPerc); the debug line is how that is checked in es_log.txt.
	const float helpPerc = helpRowPerc(sheetHeight, getHelpStyle());
	LOG(LogDebug) << "GuiSaveState: help row " << helpPerc << " of a " << sheetHeight << " px sheet (constructor)";
	const float gridHeight = sheetHeight * (1.0f - 0.02f - titlePerc - 0.02f - helpPerc);
	const float twoLines = 2.0f * labelFont->getHeight(1.5f) + 4.0f;
	float labelPerc = gridHeight > 0 ? twoLines / gridHeight : 0.30f;
	labelPerc = Math::max(0.30f, Math::min(0.50f, labelPerc));

	std::string xml =
		"<theme defaultView=\"Tiles\">"
		"<formatVersion>7</formatVersion>"
		"<view name = \"grid\">"
		"<imagegrid name=\"gamegrid\">"
		"  <margin>0.01 0.02</margin>"
		"  <padding>0 0</padding>"
		"  <scrollDirection>horizontal</scrollDirection>"
		"  <autoLayout>" + std::to_string(slots * screenProportion / cellProportion) +" 1</autoLayout>"
		"  <autoLayoutSelectedZoom>1</autoLayoutSelectedZoom>"
		"  <animateSelection>false</animateSelection>"
		"  <centerSelection>false</centerSelection>"		
		"</imagegrid>"		
		"<gridtile name=\"default\">"
		"  <backgroundColor>FFFFFF00</backgroundColor>"
		"  <padding>8 8</padding>"
		"  <imageColor>FFFFFFFF</imageColor>"
		"</gridtile>"		
		"<gridtile name=\"selected\">"
		"  <backgroundColor>" + Utils::String::toHexString(theme->Text.selectorColor) + "</backgroundColor>"
		"</gridtile>"
		"<text name=\"gridtile\">"
		"  <color>" + Utils::String::toHexString(theme->Text.color) + "</color>"
		"  <backgroundColor>00000000</backgroundColor>"
		"  <fontPath>" + theme->Text.font->getPath() +"</fontPath>"
		"  <fontSize>" + std::to_string(sh) + "</fontSize>"
		"  <alignment>center</alignment>"
		"  <singleLineScroll>false</singleLineScroll>"
		"  <multiLine>true</multiLine>"
		"  <size>1 " + std::to_string(labelPerc) + "</size>"
		"</text>"
		"<text name=\"gridtile:selected\">"
		"  <color>" + Utils::String::toHexString(theme->Text.selectedColor) + "</color>"
		"</text>"
		"<image name=\"gridtile.image\">"
		"  <linearSmooth>true</linearSmooth>"		
		"  <color>D0D0D0D0</color>"
		"  <roundCorners>0.02</roundCorners>"		
		"</image>"
		"<image name=\"gridtile.image:selected\">"
		"  <color>FFFFFFFF</color>"
		"</image>"
		"</view>"
		"</theme>";

	mTheme = std::shared_ptr<ThemeData>(new ThemeData());
	std::map<std::string, std::string> emptyMap;
	mTheme->loadFile("imageviewer", emptyMap, xml, false);

	//mGrid->setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	mGrid->applyTheme(mTheme, "grid", "gamegrid", 0);
	mGrid->setCursorChangedCallback([&](const CursorState& /*state*/) { updateHelpPrompts(); });

	mDeletionsSeen = SaveStateBookkeeper::completed();
	loadGrid();
	centerWindow();
}

void GuiSaveState::loadGrid()
{
	mGrid->clear();
	mGrid->onSizeChanged(); // To Rebuild tiles

	bool supportsIncrementalSaveStates = SystemConf::getIncrementalSaveStates();
	bool incrementalSaveStates = supportsIncrementalSaveStates && mRepository->supportsIncrementalSaveStates();

	auto states = mRepository->getSaveStates(mGame);

	// A slot the player has just deleted is gone from the grid the same frame;
	// its file follows on the worker, retire first (D-UI-073). Until then the
	// repository still lists it, so it is dropped here rather than shown as a
	// tile that would come back for a second and vanish again.
	states.erase(std::remove_if(states.begin(), states.end(),
		[](const SaveState* x) { return SaveStateBookkeeper::isPending(x->fileName); }), states.end());

	// What this build shows, for update() to compare the disk against.
	mShown.clear();
	for (auto state : states)
		mShown.push_back(state->fileName);
	std::sort(mShown.begin(), mShown.end());
	
	std::sort(states.begin(), states.end(), [&, supportsIncrementalSaveStates, incrementalSaveStates](const SaveState* file1, const SaveState* file2)
		{ 
			// Show active emulator first
			if (file1->config != nullptr && file2->config != nullptr && !file1->config->equals(file2->config))
				return file1->config->isActiveConfig(mGame);
			
			if (supportsIncrementalSaveStates && file1->config != nullptr ? file1->config->incremental : incrementalSaveStates)
				return file1->creationDate >= file2->creationDate;

			return file1->slot < file2->slot; 
		});


	mGrid->add(_("START NEW GAME"), ":/freeslot.svg", SaveStateItem(mRepository->getDefaultNewGameSaveState()));

	if (mRepository->supportsAutoSave() && mGame->getCurrentGameSetting("autosave") == "1")
	{
		auto autoSave = std::find_if(states.cbegin(), states.cend(), [](SaveState* x) { return x->slot == -1; });
		if (autoSave == states.cend())
			mGrid->add(_("START NEW AUTO SAVE"), ":/freeslot.svg", SaveStateItem(mRepository->getDefaultAutoSaveSaveState()));
	}

	for (auto item : states)
	{
		std::string coreinfo;
		
		if (item->config != nullptr && !item->config->emulator.empty() && !item->config->isActiveConfig(mGame))
		{
			if (item->config->core.empty() || item->config->emulator == item->config->core)
				coreinfo = std::string("\r\n") + item->config->emulator;
			else
				coreinfo = std::string("\r\n") + item->config->emulator + ": " + item->config->core;
		}

		if (item->slot == -1)
			mGrid->add(_("AUTO SAVE") + std::string("\r\n") + item->creationDate.toRelativeLocalTimeString(_("TODAY"), _("YESTERDAY"), _("at")) + coreinfo, item->getScreenShot(), SaveStateItem(item));
		else if (supportsIncrementalSaveStates && item->config != nullptr ? item->config->incremental : incrementalSaveStates)
			mGrid->add(item->creationDate.toRelativeLocalTimeString(_("TODAY"), _("YESTERDAY"), _("at")) + coreinfo, item->getScreenShot(), SaveStateItem(item));
		else 
			mGrid->add(_("SLOT") + std::string(" ") + std::to_string(item->slot) + std::string("\r\n") + item->creationDate.toRelativeLocalTimeString(_("TODAY"), _("YESTERDAY"), _("at")) + coreinfo, item->getScreenShot(), SaveStateItem(item));
	}

	// The help bar follows the cursor, and a rebuild moves the cursor
	// without a cursor event: after the last slot was deleted the bar still
	// offered DELETE and COPY TO FREE SLOT over START NEW GAME (#93). Read
	// the prompts of whatever is under the cursor now. A no-op before the
	// page is on screen (updateHelpPrompts acts only on the top page), so
	// the constructor's call costs nothing.
	updateHelpPrompts();
}

std::vector<std::string> GuiSaveState::filesOnDisk()
{
	// The same list loadGrid() builds from: the repository hands out nothing
	// whose file is gone (a8e274598), and what is queued for deletion is
	// hidden until it is.
	std::vector<std::string> files;
	for (auto state : mRepository->getSaveStates(mGame))
		if (!SaveStateBookkeeper::isPending(state->fileName))
			files.push_back(state->fileName);
	std::sort(files.begin(), files.end());
	return files;
}

// The help row's share of a sheet of the given height -- shared by the
// layout and by the label arithmetic in the constructor, so the two agree.
// The style is the caller's, and both pass getHelpStyle(): the one Window
// draws the bar with (the default theme's helpsystem), which needs nothing
// of this page. It used to be built here from mTheme, which the
// constructor has not made yet when it needs this number, behind a null
// guard that let the two calls measure different styles without a word
// (#151 PL-17). Both callers log the result at debug level, so the
// acceptance -- equal values at 640x480 and 1280x800 -- reads straight
// out of es_log.txt with --debug.
float GuiSaveState::helpRowPerc(float sheetHeight, const HelpStyle& help)
{
	float helpSize = 0.02;

	if (Settings::getInstance()->getBool("ShowHelpPrompts"))
	{
		const float height = Math::round(help.font->getLetterHeight() * 1.25f);

		float helpBottom = help.position.y() + (height * mOrigin.y());
		float helpBottomSpace = 0; // Renderer::getScreenHeight() - helpBottom;

		float helpTop = help.position.y() - (height * mOrigin.y()); // +height / 2;
		
		helpSize = helpTop;
		helpSize = Renderer::getScreenHeight() - helpSize + helpBottomSpace;
		helpSize = helpSize / sheetHeight + 0.06;
	}

	return helpSize;
}

void GuiSaveState::onSizeChanged()
{	
	GuiComponent::onSizeChanged();

	float helpSize = helpRowPerc(mSize.y(), getHelpStyle());
	LOG(LogDebug) << "GuiSaveState: help row " << helpSize << " of a " << mSize.y() << " px sheet (onSizeChanged)";

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mLayout.setColWidthPerc(0, 0.01);
	mLayout.setColWidthPerc(2, 0.01);

	mLayout.setRowHeightPerc(0, 0.02);

	if (mTitle != nullptr &&  mTitle->getFont() != nullptr)
		mLayout.setRowHeightPerc(1, mTitle->getFont()->getHeight(2.0f) / mSize.y());

	mLayout.setRowHeightPerc(2, 0.02);
	mLayout.setRowHeightPerc(4, helpSize );

	mLayout.setSize(mSize);
}

void GuiSaveState::render(const Transform4x4f& parentTrans)
{
	GuiComponent::render(parentTrans);

	// The layout keeps its bottom row free for the help prompts
	// (onSizeChanged), but with full-screen menus on -- every handheld
	// panel -- Window draws no help while a second page is open, so on a
	// 640x480 panel this page never showed BACK / LAUNCH / DELETE / COPY TO
	// FREE SLOT at all (2026-09-13, found framing #27). Draw them here, as
	// ViewController does when it is the top page, into the row kept for
	// them; Window then skips its own draw for this frame.
	if (mWindow->peekGui() == this && Renderer::ScreenSettings::fullScreenMenus())
		mWindow->renderHelpPromptsEarly(parentTrans);
}

void GuiSaveState::centerWindow()
{
	setSize(Renderer::getScreenWidth(), WINDOW_HEIGHT);
	animateTo(
		Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, Renderer::getScreenHeight()),
		Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, Renderer::getScreenHeight() - WINDOW_HEIGHT),
		AnimateFlags::OPACITY | AnimateFlags::POSITION);
}

bool GuiSaveState::input(InputConfig* config, Input input)
{
	if (input.value != 0 && (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("l3", input)))
	{
		delete this;
		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_OK, input))
	{
		if (mGrid->size())
		{
			const SaveStateItem& item = mGrid->getSelected();
			mRunCallback(item.saveState);
		}

		delete this;
		return true;
	}

	if (input.value != 0 && config->isMappedTo("y", input))
	{
		// Every writer of the saves tree is gated by the transfer lock
		// (D-CLOUD-053): a deletion landing while a sync is reading that
		// tree is the race the maintainer
		// named -- exit a game, the backup starts, delete a save under it.
		// Refused, never waited for (nobody waits, #22 R6), in the words the
		// launch gate uses for the same state.
		const std::string deleteBusy = _("YOUR SAVES ARE SYNCING WITH THE CLOUD.\n\nWAIT FOR IT TO FINISH BEFORE DELETING A SAVE STATE - THE NOTIFICATION AT THE TOP SAYS WHEN IT IS DONE.");
		const std::string busy = savesTreeBusy(deleteBusy);
		if (!busy.empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, busy));
			return true;
		}

		if (mGrid->size())
		{
			Window* window = mWindow;
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO DELETE THIS ITEM?"), _("YES"), 
				[this, window, deleteBusy]
				{
					// Asked again at YES: a sync can start while the question
					// is up, and the deletion is queued from here (#307 PL-068).
					const std::string busyNow = savesTreeBusy(deleteBusy);
					if (!busyNow.empty())
					{
						window->pushGui(new GuiMsgBox(window, busyNow));
						return;
					}

					const SaveStateItem& toDelete = mGrid->getSelected();

					// The grid also holds the START NEW GAME / START NEW AUTO SAVE
					// placeholders, which have no file: nothing to delete.
					if (toDelete.saveState == nullptr || !toDelete.saveState->isSlotValid() || toDelete.saveState->fileName.empty())
						return;

					// The tile goes now; the disk follows on the worker, in the order
					// D-CLOUD-053 asks for -- the deletion recorded by --retire, then
					// the files unlinked, both inside the script (D-CLOUD-133) -- one
					// deletion at a time (D-UI-073). This
					// used to run both scripts here, on the interface thread, and the
					// dialog hung after YES for as long as they took: a third of a
					// second on x86_64, a second on the RG SP (#205). The worker is
					// handed two strings and nothing else: not this page, which B or
					// LAUNCH may delete before the retire returns, and not the
					// repository's SaveState, which the next refresh() frees. The
					// other slots keep their numbers (D-UI-069), so there is no
					// rescan to run (D-CLOUD-132).
					SaveStateBookkeeper::deleteLater(toDelete.saveState->fileName, toDelete.saveState->getScreenShot());

					// No refresh(): the file is still on disk for the moment, and
					// loadGrid() hides what is pending. update() compares the disk
					// with the page when the worker reports the deletion done, and
					// rebuilds only if they differ (#207).
					loadGrid();
				}, 
				_("NO"), nullptr));
		}

		return true;
	}
	
	if (input.value != 0 && config->isMappedTo("x", input))
	{
		// The same gate DELETE carries (D-CLOUD-053): a copy is a writer of
		// the saves tree, and a sync reading that tree must not meet it. It
		// had none until #206.
		const std::string busy = savesTreeBusy(
			_("YOUR SAVES ARE SYNCING WITH THE CLOUD.\n\nWAIT FOR IT TO FINISH BEFORE COPYING A SAVE STATE - THE NOTIFICATION AT THE TOP SAYS WHEN IT IS DONE."));
		if (!busy.empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, busy));
			return true;
		}

		if (mGrid->size())
		{
			const SaveStateItem& toCopy = mGrid->getSelected();
			if (toCopy.saveState == nullptr || !toCopy.saveState->isSlotValid() || toCopy.saveState->fileName.empty())
				return true;

			int slot = mRepository->getNextFreeSlot(mGame, toCopy.saveState->config);
			if (slot >= 0)
			{
				if (toCopy.saveState->copyToSlot(slot))
				{
					// The copy is a file no capture mode would ever record
					// (#206): exit mode takes only what a launch wrote, the
					// verify passes adopt nothing new. The manager made it, so
					// the manager records it -- on the worker, as the source's
					// version at the new path; the tile is real already.
					FileData* game = mGame->getSourceFileData();
					SaveStateBookkeeper::recordCopy(toCopy.saveState->makeStateFilename(slot),
						toCopy.saveState->fileName, game->getSystem()->getName(), game->getPath());
					mRepository->refresh();
					loadGrid();
				}
			}
		}

		return true;
	}
	
	return GuiComponent::input(config, input);
}

void GuiSaveState::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	// A job has landed (D-UI-073): a deletion's files are gone, or a copy's
	// record is written. The page already showed the outcome the frame the
	// player pressed -- the tile hidden, the copy's tile added -- so in the
	// normal case the disk now agrees with the page and there is nothing to
	// draw. Rebuilding anyway tore every tile down and replayed the selection
	// animation a second after YES: the flash of #207. So the page is rebuilt
	// only when the disk disagrees with it (D-UI-074) -- a deletion whose
	// unlink failed brings its tile back, which is the one sign the player
	// gets -- and then keeps the cursor where it is, because a page that
	// jumps to its first tile a second after a press is a page that looks
	// broken. The repository keeps the SaveState of a gone file until its
	// next refresh() (a copy, the next open of the page); onDisk() hands it
	// to nobody in the meantime.
	const unsigned done = SaveStateBookkeeper::completed();
	if (done != mDeletionsSeen)
	{
		mDeletionsSeen = done;
		const std::vector<std::string> files = filesOnDisk();
		if (files != mShown)
		{
			// The exception path, so it is logged: a deletion that did not
			// take, or a file that arrived while the page was open -- seen
			// here, when a job lands, since the disk is read at no other
			// moment (audit #258, the seat's G-07).
			LOG(LogInfo) << "save state manager: a landed job left the disk differing from the page ("
				<< mShown.size() << " shown, " << files.size() << " on disk); rebuilt";
			const int cursor = mGrid->getCursorIndex();
			mRepository->refresh();
			loadGrid();
			if (cursor > 0 && cursor < mGrid->size())
				mGrid->setCursorIndex(cursor);
		}
	}
}

std::vector<HelpPrompt> GuiSaveState::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK"), [&] { delete this; }));
	prompts.push_back(HelpPrompt(BUTTON_OK, _("LAUNCH")));

	if (mGrid->size())
	{
		const SaveStateItem& item = mGrid->getSelected();
		if (!item.saveState->fileName.empty())
		{
			prompts.push_back(HelpPrompt("y", _("DELETE")));
			prompts.push_back(HelpPrompt("x", _("COPY TO FREE SLOT")));
		}
	}	

	return prompts;
}

bool GuiSaveState::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult) pResult->push_back(this); // Always return this as it's a fake fullscreen, so we always have click events
	GuiComponent::hitTest(x, y, parentTransform, pResult);
	return true;
}

bool GuiSaveState::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mBackground.isMouseOver())
	{
		delete this;
		return true;
	}

	return (button == 1);
}
