#pragma once

#include <string>
#include <vector>
#include <map>

#include "SaveState.h"

class SystemData;
class FileData;
class SaveStateConfig;

class SaveStateRepository
{
public:
	SaveStateRepository(SystemData* system);
	~SaveStateRepository();

	static bool isEnabled(FileData* game);
	static int	getNextFreeSlot(FileData* game, std::shared_ptr<SaveStateConfig> config);
	static void renumberSlots(FileData* game, std::shared_ptr<SaveStateConfig> config);

	bool supportsAutoSave();
	bool supportsIncrementalSaveStates();
	bool hasSaveStates(FileData* game);

	std::vector<SaveState*> getSaveStates(FileData* game, std::shared_ptr<SaveStateConfig> config = nullptr);

	void clear();
	void refresh();

	SaveState* getGameAutoSave(FileData* game);
	SaveState* getDefaultAutoSaveSaveState();
	SaveState* getDefaultNewGameSaveState();	

	static SaveState* getEmptySaveState();

private:
	// std::string getDefaultSavesPath();

	// A state whose file is gone is not handed out (fork #205, D-UI-073): the
	// manager's deletions land on a worker, and the page that refreshes this
	// repository may be gone before one does.
	static bool onDisk(const SaveState* state);
	static bool anyOnDisk(const std::vector<SaveState*>& states);

	SystemData* mSystem;
	std::map<std::string, std::vector<SaveState*>> mStates;

	static SaveState* _empty;
	SaveState* _autosave;
	SaveState* _newGame;
};
