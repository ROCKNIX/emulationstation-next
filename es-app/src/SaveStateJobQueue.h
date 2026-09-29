#pragma once
//
// SaveStateJobQueue - the order book for the save state manager's
// bookkeeping (fork #205 / #206, D-UI-073): the deletions on their way out
// and the copies to be recorded, in order, and how many have been done.
//
// Pure on purpose: no thread, no file, no script, so es-app/tests/unit can
// hold it to its rules. SaveStateBookkeeper wraps one of these in the
// worker that runs the jobs. The rules this class carries:
//
//   - one job at a time: take() hands out nothing while a job runs, so two
//     jobs never put two writers on the manifest;
//   - a file queued or running for deletion is pending, and the manager
//     hides its tile; a copy being recorded hides nothing -- its tile is
//     real from the moment the file was copied;
//   - a file is never queued for deletion twice while it is pending;
//   - completed() counts finished jobs of either kind, so a page can
//     remember the number it last saw and reload when it moves, holding no
//     pointer to anything.

#include <deque>
#include <set>
#include <string>

struct SaveStateJob
{
	enum class Kind { Delete, Copy };

	Kind kind = Kind::Delete;
	std::string stateFile;    // Delete: the slot's file; Copy: the new file
	std::string screenshot;   // Delete: its thumbnail, or empty
	std::string source;       // Copy: the file it was copied from
	std::string system;       // Copy: the game's system name
	std::string rom;          // Copy: the game's ROM path
};

class SaveStateJobQueue
{
public:
	// Queue a deletion. Returns false when nothing was queued: an empty
	// state path, or a file that is already pending deletion.
	bool enqueueDelete(const std::string& stateFile, const std::string& screenshot);

	// Queue a copy to be recorded. Returns false on an empty path.
	bool enqueueCopy(const std::string& stateFile, const std::string& source,
		const std::string& system, const std::string& rom);

	// Move the next queued job to running and hand it out. False when
	// nothing is queued or a job is already running.
	bool take(SaveStateJob& out);

	// The running job is done: a deletion's file stops being pending, and
	// the count of finished jobs goes up by one. Nothing running: no change.
	void finish();

	// Queued or running for deletion.
	bool isPending(const std::string& stateFile) const;

	// Finished jobs since this queue was made.
	unsigned completed() const { return mCompleted; }

	bool running() const { return mRunning; }
	size_t queued() const { return mQueue.size(); }
	bool idle() const { return mQueue.empty() && !mRunning; }

private:
	std::deque<SaveStateJob> mQueue;
	std::set<std::string> mPending;
	SaveStateJob mCurrent;
	bool mRunning = false;
	unsigned mCompleted = 0;
};
