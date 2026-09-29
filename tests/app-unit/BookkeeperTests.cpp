// The save state manager's queued deletion and the cloud transfer lock
// (audit #307 PL-068, E2's half): the shipped SaveStateBookkeeper.cpp,
// compiled against doubles for the script runner and the log, with the
// transfer lock at a path of the test's own (CLOUD_SYNC_LOCK_PATH). A
// real lock, held by a real process, as a cloud_backup run holds it.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "ApiSystem.h"
#include "Log.h"
#include "SaveStateBookkeeper.h"
#include "utils/FileSystemUtil.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
	std::mutex gRunLock;
	std::vector<std::string> gRuns;

	bool fileExists(const std::string& p) { struct stat st; return ::stat(p.c_str(), &st) == 0; }

	pid_t holdLock(const std::string& path)
	{
		const pid_t child = fork();
		if (child == 0)
		{
			execl("/bin/sh", "sh", "-c", "exec 9>\"$0\"; flock 9; touch \"$0.held\"; exec sleep 60", path.c_str(), (char*) nullptr);
			_exit(127);
		}
		for (int i = 0; i < 200 && !fileExists(path + ".held"); i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		return child;
	}

	void release(pid_t p, const std::string& path)
	{
		kill(p, SIGKILL);
		waitpid(p, nullptr, 0);
		std::remove((path + ".held").c_str());
	}

	int runsNamed(const std::string& part)
	{
		std::lock_guard<std::mutex> g(gRunLock);
		int n = 0;
		for (auto& r : gRuns)
			if (r.find(part) != std::string::npos)
				n++;
		return n;
	}

	bool waitFor(const std::function<bool()>& done, int ms)
	{
		const auto start = std::chrono::steady_clock::now();
		while (!done())
		{
			if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(ms))
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		return true;
	}

	std::string stateFile(const std::string& name)
	{
		const std::string p = std::string(CLOUD_SYNC_LOCK_PATH) + "." + name + ".state1";
		std::ofstream(p) << "state";
		return p;
	}
}

namespace FakeLog
{
	std::vector<std::string>& lines() { static std::vector<std::string> l; return l; }
	std::mutex& lock() { static std::mutex m; return m; }
}

// How long the script "runs", and what a transfer starting meanwhile would
// have found: the case below asks while the deletion's script is running.
static std::atomic<int> gRunMs{ 0 };
static std::function<void()> gDuringRun;

std::pair<std::string, int> ApiSystem::executeScriptLegacy(const std::string& command, const std::function<void(const std::string)>&)
{
	{
		std::lock_guard<std::mutex> g(gRunLock);
		gRuns.push_back(command);
	}
	if (gDuringRun)
		gDuringRun();
	if (gRunMs > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(gRunMs));
	return std::make_pair(std::string(), 0);
}

namespace Utils { namespace FileSystem {
	bool exists(const std::string& path, bool) { return path == "/usr/bin/cloud_capture" || fileExists(path); }
	std::string readAllText(const std::string&) { return ""; }
	bool removeFile(const std::string& path) { return std::remove(path.c_str()) == 0; }
}}

TEST_CASE("bookkeeper: a queued deletion waits while a transfer holds the lock")
{
	const std::string lock = CLOUD_SYNC_LOCK_PATH;
	const std::string state = stateFile("a");
	const pid_t holder = holdLock(lock);

	SaveStateBookkeeper::deleteLater(state, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK_MESSAGE(fileExists(state), "the state was deleted under a transfer");
	CHECK(runsNamed("--retire --unlink") == 0);
	CHECK(SaveStateBookkeeper::isPending(state));

	release(holder, lock);
	REQUIRE(waitFor([&state] { return !fileExists(state); }, 5000));
	CHECK(runsNamed("--retire --unlink") == 1);
	CHECK_FALSE(SaveStateBookkeeper::isPending(state));
}

TEST_CASE("bookkeeper: with no transfer, a deletion goes at once")
{
	const std::string state = stateFile("b");
	const auto before = runsNamed("--retire --unlink");
	SaveStateBookkeeper::deleteLater(state, "");
	REQUIRE(waitFor([&state] { return !fileExists(state); }, 2000));
	CHECK(runsNamed("--retire --unlink") == before + 1);
}

// A lock the bookkeeper cannot ask is not a free one (engineering-practices.md,
// guards fail closed): a deletion waits on a lock file it cannot open, as it
// waits on a held one, and goes once the file can be asked again.
//
// Unopenable for anyone, root included -- a device runs as root, and root
// reads a mode-0 file, so the case used to fail outright there (audit of
// the fixes, E2 claude G-E2-09): the lock path is a link to itself, which
// open() answers with ELOOP whoever asks.
TEST_CASE("bookkeeper: a lock file that cannot be opened is taken as held")
{
	const std::string lock = CLOUD_SYNC_LOCK_PATH;
	std::remove(lock.c_str());
	REQUIRE(::symlink(lock.c_str(), lock.c_str()) == 0);
	const int fd = ::open(lock.c_str(), O_RDONLY);
	REQUIRE_MESSAGE(fd < 0, "the looped link opened; the case cannot happen");
	const std::string state = stateFile("d");

	SaveStateBookkeeper::deleteLater(state, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	CHECK_MESSAGE(fileExists(state), "the state was deleted past a lock nobody could ask");

	REQUIRE(std::remove(lock.c_str()) == 0);
	{ std::ofstream touch(lock); }
	REQUIRE(waitFor([&state] { return !fileExists(state); }, 5000));
}

// gpt's coverage note on PL-068 (audit of the fixes, E2): the bookkeeper
// asked whether the lock was held and then deleted, so a transfer that
// started between the answer and the deletion ran beside it. The lock is
// now held for the deletion: a transfer that starts meanwhile finds it
// taken -- the scripts give a busy lock a second, longer than a deletion.
TEST_CASE("bookkeeper: a deletion holds the transfer lock while it runs")
{
	const std::string lock = CLOUD_SYNC_LOCK_PATH;
	const std::string state = stateFile("e");
	std::atomic<int> freeDuring{ -1 };
	std::atomic<int> ownDuring{ -1 };
	gDuringRun = [&lock, &freeDuring, &ownDuring]
	{
		// A transfer starting now: flock -n on the lock, from another process.
		const int rc = std::system(("flock -n '" + lock + "' true").c_str());
		freeDuring = WIFEXITED(rc) && WEXITSTATUS(rc) == 0 ? 1 : 0;
		ownDuring = SaveStateBookkeeper::holdsTransferLock() ? 1 : 0;
	};
	SaveStateBookkeeper::deleteLater(state, "");
	REQUIRE(waitFor([&state] { return !fileExists(state); }, 5000));
	gDuringRun = nullptr;
	CHECK_MESSAGE(freeDuring.load() == 0, "a transfer could have taken the lock while the deletion ran");
	CHECK(ownDuring.load() == 1);                          // the manager can tell it is ours
	CHECK_FALSE(SaveStateBookkeeper::holdsTransferLock());
	// And it is let go after.
	const int after = std::system(("flock -n '" + lock + "' true").c_str());
	CHECK((WIFEXITED(after) && WEXITSTATUS(after) == 0));
}

TEST_CASE("bookkeeper: exit does not wait out a transfer, and deletes nothing under it")
{
	const std::string lock = CLOUD_SYNC_LOCK_PATH;
	const std::string state = stateFile("c");
	const pid_t holder = holdLock(lock);
	SaveStateBookkeeper::deleteLater(state, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	const auto start = std::chrono::steady_clock::now();
	SaveStateBookkeeper::shutdown();
	const auto took = std::chrono::steady_clock::now() - start;
	CHECK(took < std::chrono::seconds(10));
	CHECK_MESSAGE(fileExists(state), "the state was deleted under a transfer at exit");
	release(holder, lock);
	std::remove(state.c_str());
}
