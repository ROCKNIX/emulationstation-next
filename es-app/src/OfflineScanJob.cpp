#include "OfflineScanJob.h"
#include "AppWindow.h"
#include "CloudText.h"

#include "Window.h"
#include "Log.h"
#include "utils/StringUtil.h"

#include <cctype>
#include <cstdio>
#include <thread>
#include <signal.h>
#include <sys/wait.h>

std::mutex OfflineScanJob::sMutex;
std::shared_ptr<OfflineScanJob> OfflineScanJob::sCurrent;

OfflineScanJob::OfflineScanJob(Window* window, const std::string& command)
	: mWindow(window), mCommand(command), mStarted(std::chrono::steady_clock::now())
{
}

std::shared_ptr<OfflineScanJob> OfflineScanJob::current()
{
	std::unique_lock<std::mutex> lock(sMutex);
	return sCurrent;
}

bool OfflineScanJob::running()
{
	auto job = current();
	return job != nullptr && !job->state().finished;
}

std::shared_ptr<OfflineScanJob> OfflineScanJob::start(Window* window, const std::string& command)
{
	std::unique_lock<std::mutex> lock(sMutex);
	if (sCurrent != nullptr && !sCurrent->state().finished)
		return sCurrent;

	std::shared_ptr<OfflineScanJob> job(new OfflineScanJob(window, command));
	sCurrent = job;
	// Detached, and holding its own reference: the run ends when the ctl
	// does, not when a page closes -- the same shape as the top-up's thread
	// (OfflineAchievements::topUpWhenOnline).
	std::thread([job] { job->run(); }).detach();
	return job;
}

OfflineScanJob::State OfflineScanJob::state() const
{
	std::unique_lock<std::mutex> lock(mMutex);
	State s = mState;
	if (!s.finished)
		s.elapsedMs = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStarted).count();
	return s;
}

void OfflineScanJob::setOnChanged(const std::function<void()>& onChanged)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mOnChanged = onChanged;
}

void OfflineScanJob::cancel()
{
	pid_t pid = 0;
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mState.finished || mState.cancelled)
			return;
		mState.cancelled = true;
		pid = mPid;
		if (pid <= 0)
			mCancelWanted = true;   // the pid line has not arrived: handleLine delivers it
	}
	if (pid > 0)
		::kill(-pid, SIGINT);
	LOG(LogInfo) << "OfflineScanJob: cancelled by the player (SIGINT, group " << pid << ")";
	changed();
}

// One refresh waiting on the interface thread at a time: a game in
// progress pauses the interface's loop, and a thousand queued refreshes
// running back to back when it returns would be a thousand for nothing.
void OfflineScanJob::changed()
{
	std::function<void()> onChanged;
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mPostPending || !mOnChanged)
			return;
		mPostPending = true;
		onChanged = mOnChanged;
	}
	// This run, not current(): a TRY AGAIN may have started the next one
	// between the last line and here.
	//
	// Through AppWindow (#308 8-es claude F-ES-26's rule): this thread is
	// detached and a scan runs for minutes, so its end can come after main()
	// has let the window go. A refresh nobody can draw is dropped, and with
	// it the pending mark, so nothing waits on a post that was not made.
	std::shared_ptr<OfflineScanJob> self = shared_from_this();
	const bool posted = AppWindow::post(mWindow, [self, onChanged]
	{
		{
			std::unique_lock<std::mutex> lock(self->mMutex);
			self->mPostPending = false;
		}
		onChanged();
	});
	if (!posted)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mPostPending = false;
	}
}

void OfflineScanJob::run()
{
	int ret = -1;
	// Braces around the whole command: a trailing redirection binds to the
	// last element of a sequence only (GuiCloudTransfer learnt it). SIGPIPE
	// ignored inside them, and so in the ctl, which inherits the disposition:
	// should this process end while the ctl is mid-scan, its ">>> " lines
	// meet a closed pipe and fail without killing it, so the scan runs to
	// its end and writes its stamp rather than dying at the next line.
	//
	// In a session of its own, saying so on its first line: setsid makes the
	// shell a process group leader and ">>> pid N" tells cancel() which group
	// to signal, so the shell, the ctl and its helper go together -- the
	// shape CloudTransferJob gives its commands (D-CLOUD-129). shellQuote, so
	// a command with a quote in it survives the trip.
	const std::string wrapped = "setsid sh -c "
		+ Utils::String::shellQuote("echo \">>> pid $$\"; { trap '' PIPE; " + mCommand + " ; }") + " 2>&1";
	FILE* pipe = popen(wrapped.c_str(), "r");
	if (pipe != nullptr)
	{
		std::string buf;
		int c;
		while ((c = fgetc(pipe)) != EOF)
		{
			if (c != '\n' && c != '\r')
			{
				if (buf.size() < 1024)
					buf += (char) c;
				continue;
			}
			handleLine(cleanLine(buf));
			buf.clear();
		}
		if (!buf.empty())
			handleLine(cleanLine(buf));

		int status = pclose(pipe);
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
	}

	{
		std::unique_lock<std::mutex> lock(mMutex);
		mState.exit = ret;
		mState.elapsedMs = (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStarted).count();
		mState.finished = true;
		LOG(LogInfo) << "OfflineScanJob: " << Utils::String::maskSecrets(mCommand) << " exited " << ret
			<< " (cached " << mState.cached << ", skipped " << mState.skipped << ", ready " << mState.ready << ")";
	}
	changed();
}

// The ctl talks to the page through ">>> " lines (raofflineproxy-ctl's
// header is the contract), after run()'s own first line:
//   ">>> pid <n>"              the run's process group, from the shell run() starts
//   ">>> doing listing"        the library is being walked
//   ">>> total <n>"            how many ROMs will be looked at
//   ">>> game <i>|<n>|<name>"  the one it is on now
//   ">>> cached <c>|<s>"       added so far, and passed over so far
//   ">>> note <TOKEN>"         LIMIT_REACHED, NOTHING_NEW, TRUNCATED
//   ">>> errors <n>"           games a fetch failed for (audit #186 PL-24; the
//                              ctl says it as the run ends, before why)
//   ">>> why <TOKEN>"          why it stopped, in the ctl's token (CANCELLED
//                              after cancel(): the ctl's INT trap says it)
//   ">>> done <c>|<s>|<ready>|<limit>"
// Everything else on stdout is the client's own and is not shown. TRUNCATED
// is read should the ctl ever say it; at ad25fdeb22 it logs the fact and
// tells the page nothing.
void OfflineScanJob::handleLine(const std::string& line)
{
	if (line.rfind(">>> ", 0) != 0)
		return;
	const std::string body = line.substr(4);
	const size_t space = body.find(' ');
	const std::string word = body.substr(0, space);
	const std::string rest = space == std::string::npos ? "" : Utils::String::trim(body.substr(space + 1));
	const std::vector<std::string> fields = Utils::String::split(rest, '|', false);
	auto num = [](const std::string& s) -> int
	{
		const std::string t = Utils::String::trim(s);
		if (t.empty() || t.size() > 9)
			return 0;
		for (char c : t)
			if (c < '0' || c > '9')
				return 0;
		return std::stoi(t);
	};

	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (word == "pid")
		{
			// run()'s own first line: the group cancel() signals. A cancel
			// that came first is delivered now.
			mPid = num(rest);
			if (mCancelWanted && mPid > 0)
			{
				mCancelWanted = false;
				::kill(-mPid, SIGINT);
			}
			return;
		}
		if (word == "doing")
			mState.listing = rest == "listing";
		else if (word == "total")
			mState.total = num(rest);
		else if (word == "game" && fields.size() >= 3)
		{
			mState.listing = false;
			mState.index = num(fields[0]);
			mState.total = num(fields[1]);
			// The name may itself contain '|', so everything past the second is it.
			std::string name = fields[2];
			for (size_t i = 3; i < fields.size(); i++)
				name += "|" + fields[i];
			mState.game = name;
		}
		else if (word == "cached" && fields.size() >= 2)
		{
			mState.cached = num(fields[0]);
			mState.skipped = num(fields[1]);
		}
		else if (word == "note")
		{
			if (rest == "LIMIT_REACHED") mState.limit = true;
			else if (rest == "NOTHING_NEW") mState.nothingNew = true;
			else if (rest == "TRUNCATED") mState.truncated = true;
		}
		else if (word == "errors")
			mState.errors = num(rest);
		else if (word == "why")
			mState.why = rest;
		else if (word == "done" && fields.size() >= 4)
		{
			mState.cached = num(fields[0]);
			mState.skipped = num(fields[1]);
			mState.ready = num(fields[2]);
			mState.limit = num(fields[3]) != 0;
		}
		else
			return;
	}
	changed();
}

// One line of a script's output: ANSI escapes, C0 controls and DEL dropped,
// every UTF-8 byte kept, then trimmed -- CloudText::cleanLine, the rule the
// sync card reads by since #308 F-CS-19. This page carried a copy of its
// own; the two gave the same line for every input, and one copy is one
// rule to change.
std::string OfflineScanJob::cleanLine(const std::string& raw)
{
	return CloudText::cleanLine(raw);
}
