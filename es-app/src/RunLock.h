#pragma once
#ifndef ES_APP_RUN_LOCK_H
#define ES_APP_RUN_LOCK_H

// Who holds a script's run lock (#308 1-raoffline claude F-RA-08, gpt
// F-RA-16). raofflineproxy-ctl takes an flock on its lock file for a run and
// writes its pid into it; the file stays, pid and all, after the run ends,
// and a run killed -9 leaves it too. So the pid in the file names a run
// only while the lock is held -- the ctl's own stop path asks the lock first
// (stop_running_run: flock -n) -- and, since the pid is written just after
// the lock is taken, only when the process under it is the program that
// takes it. OfflineAchievements::stopRun signals what this returns.

#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/file.h>
#include <unistd.h>

namespace RunLock
{
	// Whether a process holds an flock on `path` right now -- the cloud
	// scripts' /var/run/cloud_sync.lock while a transfer runs (engineering-
	// practices.md: the file stays behind after every run, so its existence
	// means nothing). A shared lock taken for an instant fails while one is
	// held, and is let go at once. No file is nobody's lock.
	inline bool held(const std::string& lockPath)
	{
		const int fd = ::open(lockPath.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return false;
		const bool free = ::flock(fd, LOCK_SH | LOCK_NB) == 0;
		if (free)
			::flock(fd, LOCK_UN);
		::close(fd);
		return !free;
	}

	// The pid holding `lockPath`, when its command line names `program`;
	// 0 when nobody holds the lock, the file names no pid, or the pid is
	// some other process.
	inline long holder(const std::string& lockPath, const std::string& program)
	{
		const int fd = ::open(lockPath.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return 0;
		// Taken shared for an instant and let go: a lock nobody holds is a
		// file a finished run left. The ctl's own check does the same.
		if (::flock(fd, LOCK_SH | LOCK_NB) == 0)
		{
			::flock(fd, LOCK_UN);
			::close(fd);
			return 0;
		}
		char buf[32];
		const ssize_t n = ::pread(fd, buf, sizeof(buf) - 1, 0);
		::close(fd);
		if (n <= 0)
			return 0;
		buf[n] = '\0';
		const long pid = std::strtol(buf, nullptr, 10);
		if (pid <= 1)
			return 0;
		std::ifstream cmd("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
		const std::string line((std::istreambuf_iterator<char>(cmd)), std::istreambuf_iterator<char>());
		if (line.find(program) == std::string::npos)
			return 0;
		return pid;
	}
}

#endif // ES_APP_RUN_LOCK_H
