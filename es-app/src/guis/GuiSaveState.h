#pragma once

#include <functional>
#include <string>
#include <vector>

#include "GuiComponent.h"
#include "Window.h"
#include "components/ImageGridComponent.h"
#include "components/NinePatchComponent.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "SaveState.h"

class ThemeData;
class FileData;
class SaveStateRepository;

struct SaveStateItem
{
	SaveStateItem() { saveState = nullptr; }
	SaveStateItem(SaveState* save) { saveState = save; }

	SaveState* saveState;
};

class GuiSaveState : public GuiComponent
{
public:
	GuiSaveState(Window* window, FileData* game, const std::function<void(SaveState* state)>& callback);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;
	void render(const Transform4x4f& parentTrans) override;
	float helpRowPerc(float sheetHeight, const HelpStyle& help);
	std::vector<HelpPrompt> getHelpPrompts() override;

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	bool onMouseClick(int button, bool pressed, int x, int y);


protected:
	void centerWindow();
	void loadGrid();

	// The files of the states the page would show now -- what the repository
	// lists minus what is queued for deletion -- sorted, so it compares as a
	// set with mShown.
	std::vector<std::string> filesOnDisk();

	std::shared_ptr<ImageGridComponent<SaveStateItem>> mGrid;
	std::shared_ptr<ThemeData> mTheme;
	std::shared_ptr<TextComponent>	mTitle;

	NinePatchComponent				mBackground;
	ComponentGrid					mLayout;
	
	std::function<void(SaveState* state)>			mRunCallback;

	FileData* mGame;
	SaveStateRepository* mRepository;

	// SaveStateBookkeeper::completed() as last read: when it moves, a job this
	// page (or another) queued has landed -- a deletion's files gone, a copy's
	// record written (D-UI-073). A number, never a pointer -- the worker
	// outlives pages.
	unsigned mDeletionsSeen;

	// The files of the tiles the grid was last built from, sorted. When a job
	// lands the page is rebuilt only if filesOnDisk() differs from this: in
	// the normal case the tile already went (or came) the frame the player
	// pressed, and a rebuild would only tear every tile down and replay the
	// selection animation a second later -- the flash of #207 (D-UI-074).
	std::vector<std::string> mShown;
};