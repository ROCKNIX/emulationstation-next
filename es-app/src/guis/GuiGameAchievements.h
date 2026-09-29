#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "GuiSettings.h"
#include "RetroAchievements.h"
#include "GuiRetroAchievements.h"

class FileData;

class GuiGameAchievements : public GuiSettings
{
public:
	// cheevosHash is the game's hash from the gamelist, the offline proxy's
	// key for a game started once through RetroArch (fork #180).
	static void show(Window* window, int gameId, const std::string& cheevosHash = "");

	void	render(const Transform4x4f& parentTrans) override;
	bool	input(InputConfig* config, Input input) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	GuiGameAchievements(Window *window, GameInfoAndUserProgress ra);

	void	centerWindow();
	float	headerTextColumn();

	FileData* mFile;
	std::shared_ptr<RetroAchievementProgress> mProgress;
	// The completion bar has its own row under the header lines, because they
	// end past where it would sit beside them (#160).
	bool mProgressBelow = false;
};
