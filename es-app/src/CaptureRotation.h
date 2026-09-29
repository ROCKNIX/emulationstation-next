#pragma once
#ifndef ES_APP_CAPTURE_ROTATION_H
#define ES_APP_CAPTURE_ROTATION_H

#include <ctime>
#include <string>

class FileData;

// A game's display rotation, learned at the end of each RetroArch session
// from the launch log and kept beside its save states as <rom>.rotation
// (fork #245, D-UI-081) -- so it travels with the states, and a game that
// was never played here, which has no captures either, simply has none.
namespace CaptureRotation
{
	// Quarter turns counter-clockwise, 0-3: the record when there is one
	// and it says its turn came from the game's own launch (fork #288),
	// else the core's own table (fork #248) for the game's ROM name, else 0.
	int read(FileData* game);

	// After a session: read the launch log and RetroArch's config, and
	// write the record when it is new or has changed. Only RetroArch
	// sessions write one; nothing else captures through the interface.
	// `started` is when the launch began: a launch log older than that is
	// not this session's, and nothing is read from it (#308 F-ES-08).
	void recordAfterSession(FileData* game, const std::string& emulator, time_t started);

	// Where the record lives, "" when the game's system keeps no states.
	std::string recordPath(FileData* game);

	// Forget every turn read so far, so the next read asks the record (or
	// the table) again. Called when a record is written by something other
	// than recordAfterSession -- the harness's synthetic line, a restore
	// that brought records down -- and by the screenshot cache when a
	// session has just recorded (audit #258 PL-019).
	void forgetAll();
}

#endif // ES_APP_CAPTURE_ROTATION_H
