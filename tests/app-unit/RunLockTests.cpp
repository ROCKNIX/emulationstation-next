// Who holds the offline achievements' run lock (#308 1-raoffline claude
// F-RA-08 / gpt F-RA-16): the pid in the file is signalled only while the
// lock is held, and only when it is the program that takes it. Real files,
// real processes: a lock file a finished run left, and one a live process
// holds.
#include "doctest/doctest.h"
#include "RunLock.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace
{
	std::string tempLock()
	{
		char name[] = "/tmp/runlock-test-XXXXXX";
		const int fd = mkstemp(name);
		close(fd);
		return name;
	}

	// A process that takes the lock and writes its pid, as the ctl's
	// take_lock does, then becomes `sleep` (its command line from then on).
	pid_t holdLock(const std::string& path)
	{
		const pid_t child = fork();
		if (child == 0)
		{
			execl("/bin/sh", "sh", "-c", "exec 9<>\"$0\"; flock 9; printf '%s\\n' $$ >&9; exec sleep 30", path.c_str(), (char*) nullptr);
			_exit(127);
		}
		// Until the pid is in the file.
		for (int i = 0; i < 200; i++)
		{
			std::ifstream in(path);
			std::string line;
			if (std::getline(in, line) && std::atol(line.c_str()) == child)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return child;
	}

	void stop(pid_t p) { kill(p, SIGKILL); waitpid(p, nullptr, 0); }
}

TEST_CASE("run lock: a pid left in a lock nobody holds is not a run")
{
	const std::string path = tempLock();
	// A finished run's pid, and a live process under it -- the recycled pid.
	const pid_t other = fork();
	if (other == 0) { execl("/bin/sleep", "sleep", "30", (char*) nullptr); _exit(127); }
	{ std::ofstream out(path); out << other << "\n"; }
	CHECK(RunLock::holder(path, "sleep") == 0);
	stop(other);
	std::remove(path.c_str());
}

TEST_CASE("run lock: the holder is found while it holds the lock")
{
	const std::string path = tempLock();
	const pid_t child = holdLock(path);
	CHECK(RunLock::holder(path, "sleep") == child);
	// The same holder under a program name it is not: not signalled.
	CHECK(RunLock::holder(path, "raofflineproxy-ctl") == 0);
	stop(child);
	CHECK(RunLock::holder(path, "sleep") == 0);   // released with the process
	std::remove(path.c_str());
}

TEST_CASE("run lock: no file, an empty file, and no number are no run")
{
	CHECK(RunLock::holder("/tmp/runlock-test-does-not-exist", "sleep") == 0);
	const std::string path = tempLock();
	CHECK(RunLock::holder(path, "sleep") == 0);
	{ std::ofstream out(path); out << "not-a-pid\n"; }
	CHECK(RunLock::holder(path, "sleep") == 0);
	std::remove(path.c_str());
}
