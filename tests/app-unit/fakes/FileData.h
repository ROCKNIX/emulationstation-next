#pragma once
// Double for es-app/src/FileData.h: only whether a game holds the screen,
// and how many have started.
class FileData
{
public:
	static FileData* GetRunningGame();
	static unsigned GetGamesStarted();
};
