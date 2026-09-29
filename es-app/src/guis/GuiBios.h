#pragma once

#include "GuiComponent.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "ApiSystem.h"

class TextComponent;
class ComponentList;

// One page, every system the check knows about, one row each: an icon for
// the worst of its files, the system's name, and what is missing. Systems
// with something to fix come first, and among those the ones this device
// has games for. A press on a system opens its file-by-file list.
class GuiBios : public GuiComponent
{
public:
	static void show(Window* window);

	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void onSizeChanged() override;

protected:
	GuiBios(Window* window, const std::vector<BiosSystem> bioses);

private:
	static void openSystem(Window* window, const BiosSystem& system, const std::string& displayName);

	void refresh();
	void loadList();
	void centerWindow();

	std::vector<BiosSystem> mBios;

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<ComponentList>	mList;
	std::shared_ptr<ComponentGrid>	mButtonGrid;
};
