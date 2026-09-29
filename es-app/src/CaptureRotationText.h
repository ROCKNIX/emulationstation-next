#pragma once
#ifndef ES_APP_CAPTURE_ROTATION_TEXT_H
#define ES_APP_CAPTURE_ROTATION_TEXT_H

#include <string>

// How many quarter turns counter-clockwise RetroArch turned a game's frame
// for the display, read from what it logged and configured (fork #245,
// D-UI-081). The capture -- a save-state thumbnail, a screenshot -- is the
// frame as the core drew it, so the interface turns it by the same amount.
namespace CaptureRotationText
{
	// The last "[Environ] SET_ROTATION: "n" (deg)." line of a launch log
	// AFTER its last "=== Build" banner (RetroArch prints one per process),
	// as 0-3; -1 when that launch has none (a core that never asked), and
	// -1 when the log has no banner at all. Only the last launch's section
	// counts: the file held every launch since it was last removed on a
	// device whose log level was none (fork #280), and a vertical game's
	// line at its end turned every game exited after it.
	int turnsFromLog(const std::string& launchLog);

	// What the display did with the core's request: nothing when
	// video_allow_rotate is off, plus the player's own video_rotation,
	// mod 4. A core that asked for nothing counts as 0.
	int fold(int coreTurns, const std::string& retroarchConfig);

	// A core's table -- "<romname> <turns>" per line, generated from the
	// core's own driver flags at build time (fork #248) -- read for one
	// game: the turns, or 0 when the game is not in it.
	int turnsFromTable(const std::string& table, const std::string& romName);

	// The record's text -- "turns=N", then "from=checked-launch" -- and reading
	// it back: N is 0-3 and anything else reads as 0. A bare digit reads
	// too. The first line is longer than three bytes on purpose --
	// readAllText skips a UTF-8 byte-order mark by reading three bytes
	// first, and a shorter file comes back empty.
	std::string recordText(int turns);
	int parseRecord(const std::string& text);

	// Whether the record says its turn was read from the game's own launch:
	// a line "from=checked-launch" of its own. The reader before fork #288
	// took the last rotation line of a log that held every launch since the
	// file was last removed (fork #280), so a record it wrote may carry
	// another game's turn -- Ms. Pac-Man's 3 on Dr. Mario -- and it wrote no
	// such line. The builds after it wrote "from=own-launch", also over a
	// launch that failed and a log with no launch in it (audit of the fixes,
	// E2 gpt G-E2-06). A record without the checked line is not trusted: the
	// core's table stands in until the game's next exit rewrites it. The
	// line is the writer's claim about what it read, which is the one thing
	// a migration could not know.
	bool recordFromOwnLaunch(const std::string& text);

	// Whether the launch log holds a launch at all: RetroArch's build banner.
	bool logHasLaunch(const std::string& launchLog);

	// Whether a session's reading is written as the game's record (#308
	// 8-es claude F-ES-08, 8a gpt F-ES-08). `existingRecord` is "" when there
	// is none; `turns` is what fold made of the log; `tableTurns` what the
	// core's table would say without a record. Never from a log with no
	// launch in it -- turnsFromLog's -1 there is "no evidence", which fold
	// makes 0, and a record written from it said from=own-launch over a
	// turn nobody read. A zero with no record is written only when the table
	// would say otherwise: rotation turned off in RetroArch, a core that did
	// not ask, and the table's 3 went on answering. Unchanged, never rewritten.
	bool shouldRecord(const std::string& launchLog, const std::string& existingRecord, int turns, int tableTurns);
}

#endif // ES_APP_CAPTURE_ROTATION_TEXT_H
