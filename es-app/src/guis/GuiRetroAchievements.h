#pragma once

#include "GuiSettings.h"
#include "RetroAchievements.h"

class FileData;

class GuiRetroAchievements : public GuiSettings 
{
public:
	static void show(Window* window);
	
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	static FileData* getFileData(const std::string& cheevosGameId);

protected:
	GuiRetroAchievements(Window *window, RetroAchievementInfo ra);    
	void	centerWindow();
};

// The completion bar with its label: softcore in blue over hardcore in gold,
// on a track drawn in the text colour so the empty part shows on any theme.
class RetroAchievementProgress : public GuiComponent
{
public:
	RetroAchievementProgress(Window* window, int valueSoftcore, int valueHardcore, int max, const std::string& label);

	// Where the label sits. Below the bar, both centred in the width (the
	// default): a column taller than it is wide, the summary page's per-game
	// bars. Beside the bar, on one centre line, the label at the right and
	// the bar filling what it leaves: a row one text line tall, the game
	// page's header (fork #193).
	void setLabelBeside(bool beside);

	void onSizeChanged() override;
	void render(const Transform4x4f& parentTrans) override;
	void setColor(unsigned int color) override;

private:
	int mValueSoftCore;
	int mValueHardCore;
	int mMax;

	bool mLabelBeside;
	// The colour the row last gave us (the theme's text colour until a list
	// says otherwise): the label's, and the track's at a fraction of it.
	unsigned int mColor;
	// The track's rectangle, laid out with the size; the fill shares it.
	float mBarX, mBarY, mBarW, mBarH;

	std::shared_ptr<TextComponent> mText;
};
