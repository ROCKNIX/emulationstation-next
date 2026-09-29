#include "SaveStateJobQueue.h"

bool SaveStateJobQueue::enqueueDelete(const std::string& stateFile, const std::string& screenshot)
{
	if (stateFile.empty())
		return false;
	if (mPending.count(stateFile))
		return false;

	SaveStateJob job;
	job.kind = SaveStateJob::Kind::Delete;
	job.stateFile = stateFile;
	job.screenshot = screenshot;
	mPending.insert(stateFile);
	mQueue.push_back(job);
	return true;
}

bool SaveStateJobQueue::enqueueCopy(const std::string& stateFile, const std::string& source,
	const std::string& system, const std::string& rom)
{
	if (stateFile.empty() || source.empty())
		return false;

	SaveStateJob job;
	job.kind = SaveStateJob::Kind::Copy;
	job.stateFile = stateFile;
	job.source = source;
	job.system = system;
	job.rom = rom;
	mQueue.push_back(job);
	return true;
}

bool SaveStateJobQueue::take(SaveStateJob& out)
{
	if (mRunning || mQueue.empty())
		return false;

	mCurrent = mQueue.front();
	mQueue.pop_front();
	mRunning = true;
	out = mCurrent;
	return true;
}

void SaveStateJobQueue::finish()
{
	if (!mRunning)
		return;

	if (mCurrent.kind == SaveStateJob::Kind::Delete)
		mPending.erase(mCurrent.stateFile);
	mCurrent = SaveStateJob();
	mRunning = false;
	mCompleted++;
}

bool SaveStateJobQueue::isPending(const std::string& stateFile) const
{
	return !stateFile.empty() && mPending.count(stateFile) != 0;
}
