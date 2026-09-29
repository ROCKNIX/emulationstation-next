// The settings files' writer, reader and lock, against real files in a
// scratch directory (audit #307: PL-024, PL-041, PL-063, PL-064, PL-065;
// #308 F-ES-07, F-ES-08).
//
// A binary of its own, es-file-tests, because es-unit-tests is the pure
// code and touches no file (README.md). This one touches nothing but a
// directory it makes under $TMPDIR (or /tmp) and removes again, and the
// processes it forks to play the other writer -- the shell's set_setting,
// a second waiter on a stale lock. POSIX only, like the code it checks.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "utils/AtomicFileUtil.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace
{
	// A directory of its own per case, removed with everything in it.
	struct ScratchDir
	{
		std::string path;
		ScratchDir()
		{
			const char* base = getenv("TMPDIR");
			std::string tmpl = std::string(base != nullptr && *base != '\0' ? base : "/tmp") + "/es-file-tests.XXXXXX";
			std::vector<char> buf(tmpl.begin(), tmpl.end());
			buf.push_back('\0');
			REQUIRE(mkdtemp(buf.data()) != nullptr);
			path = buf.data();
		}
		~ScratchDir()
		{
			const std::string cmd = "rm -rf '" + path + "'";
			if (std::system(cmd.c_str()) != 0)
				fprintf(stderr, "could not remove %s\n", path.c_str());
		}
		std::string operator/(const std::string& name) const { return path + "/" + name; }
	};

	void put(const std::string& path, const std::string& text)
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << text;
	}

	std::string get(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}

	// Several lines of one letter: big enough that a write takes more than
	// one system call and two writers overlap, small enough to run quickly.
	std::string lines(char c, size_t count)
	{
		std::string line(99, c);
		line += '\n';
		std::string out;
		out.reserve(line.size() * count);
		for (size_t i = 0; i < count; i++)
			out += line;
		return out;
	}

	// A pid nobody holds: a child that has exited and been reaped.
	pid_t deadPid()
	{
		pid_t pid = fork();
		if (pid == 0)
			_exit(0);
		int status = 0;
		waitpid(pid, &status, 0);
		return pid;
	}

	// The line that starts with `key` replaced by `line`, the rest as it was:
	// the merge a save makes of one changed key.
	std::string withLine(const std::string& text, const std::string& key, const std::string& line)
	{
		std::string out;
		size_t pos = 0;
		while (pos < text.size())
		{
			size_t end = text.find('\n', pos);
			const std::string current = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
			out += (current.rfind(key, 0) == 0 ? line : current) + "\n";
			if (end == std::string::npos)
				break;
			pos = end + 1;
		}
		return out;
	}

	// Run fn in a child with a deadline; the child's exit code, or -1 when
	// the deadline killed it (a loop that never returns).
	template <typename F>
	int inChildWithin(int seconds, F fn)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			alarm((unsigned) seconds);
			_exit(fn());
		}
		int status = 0;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		return -1;
	}
}

using namespace Utils::AtomicFile;

// ------------------------------------------------------------------ PL-063

TEST_CASE("two writers at once leave one whole file, never a mix of both (PL-063)")
{
	// Every writer shared path.tmp under O_TRUNC: the second truncated the
	// first's temporary under it, the first renamed the shared file into
	// place -- empty, or half one writer's bytes -- and the second's rename
	// then failed on a name that was gone, so a save that should have landed
	// reported failure. Two threads here; the shell's set_setting (awk into
	// system.cfg.tmp) was the same name from another process.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string start = "start\n";
	const std::string a = lines('a', 200);
	const std::string b = lines('b', 200);
	put(path, start);

	std::atomic<bool> stop(false);
	std::atomic<int> torn(0);
	std::thread reader([&] {
		while (!stop)
		{
			// Never absent, never empty, never a mix: the rename is atomic,
			// so the name always holds one whole file.
			const std::string seen = get(path);
			if (seen != start && seen != a && seen != b)
				torn++;
		}
	});

	std::atomic<int> failed(0);
	auto writer = [&](const std::string& text) {
		for (int i = 0; i < 400; i++)
			if (!writeText(path, text))
				failed++;
	};
	std::thread wa(writer, a);
	std::thread wb(writer, b);
	wa.join();
	wb.join();
	stop = true;
	reader.join();

	const std::string last = get(path);
	CHECK((last == a || last == b));
	CHECK(torn.load() == 0);
	CHECK(failed.load() == 0);
	// Nothing left behind: every temporary was renamed or removed.
	CHECK(std::system(("test -z \"$(ls -A '" + dir.path + "' | grep -v '^system.cfg$')\"").c_str()) == 0);
}

// ------------------------------------------------------------------ PL-065

TEST_CASE("a read that fails after the file opened is not a read (PL-065)")
{
	// readText said ok once the stream opened, whatever the read did, and
	// SystemConf took the prefix it got as the whole file -- and recorded it
	// as the last known good. A directory opens for reading on Linux and
	// then fails with EISDIR; /proc/self/mem opens and fails with EIO at
	// offset 0. Neither is a file's text.
	ScratchDir dir;
	bool ok = true;
	readText(dir.path, &ok);
	CHECK_FALSE(ok);

	ok = true;
	readText("/proc/self/mem", &ok);
	CHECK_FALSE(ok);

	// And the two answers that were always right stay right.
	put(dir / "f", "a=1\n");
	ok = false;
	CHECK(readText(dir / "f", &ok) == "a=1\n");
	CHECK(ok);
	ok = true;
	CHECK(readText(dir / "missing", &ok).empty());
	CHECK_FALSE(ok);
	ok = false;
	put(dir / "empty", "");
	CHECK(readText(dir / "empty", &ok).empty());
	CHECK(ok);   // empty and missing are different answers
}

// A read() that fails part way, for readText's case below: this binary's
// own read(), which AtomicFileUtil.cpp's calls resolve to, passes every
// call through to the kernel unless a test on this thread has armed it, and
// then lets `readsLeft` reads through and fails the next with EIO -- the
// failing card the finding describes, on a file that opened and began to
// read (the audit of the fixes: PL-065's fixture failed on the first read).
namespace
{
	thread_local int readsLeft = -1;   // -1: not armed
}
extern "C" ssize_t read(int fd, void* buf, size_t count)
{
	if (readsLeft == 0)
	{
		readsLeft = -1;
		errno = EIO;
		return -1;
	}
	if (readsLeft > 0)
		readsLeft--;
	return (ssize_t) syscall(SYS_read, fd, buf, count);
}

TEST_CASE("a read that fails after part of the file came is not a read (PL-065)")
{
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	put(path, lines('a', 2000));   // 200000 bytes: more than one 64 KiB read
	bool ok = true;
	readsLeft = 1;                 // the first read succeeds, the second fails
	const std::string text = readText(path, &ok);
	const int left = readsLeft;
	readsLeft = -1;
	CHECK(left == -1);             // the failure was reached: the fixture ran
	CHECK_FALSE(ok);
	CHECK(text.empty());           // not the 65536 bytes that came first

	// Unarmed, the same file reads whole.
	ok = false;
	CHECK(readText(path, &ok).size() == 200000);
	CHECK(ok);
}

// ------------------------------------------------------------ F-ES-08

TEST_CASE("replacing a file keeps its mode (#308 8b gpt F-ES-08)")
{
	// The temporary was created 0644 and renamed over the file, so a
	// system.cfg somebody had made 0600 -- it carries wifi.key and the
	// RetroAchievements password -- came back 0644 from the next save.
	ScratchDir dir;
	const mode_t before = umask(022);
	const std::string path = dir / "system.cfg";
	put(path, "wifi.key=x\n");
	REQUIRE(chmod(path.c_str(), 0600) == 0);

	CHECK(writeText(path, "wifi.key=y\n"));
	struct stat st;
	REQUIRE(stat(path.c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0600);
	CHECK(get(path) == "wifi.key=y\n");

	// A file that did not exist is made as it always was: 0644.
	const std::string fresh = dir / "fresh.cfg";
	CHECK(writeText(fresh, "a=1\n"));
	REQUIRE(stat(fresh.c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0644);

	// And the last-known-good record beside a private file is as private
	// as the file, the first time it is made and every time after -- one an
	// earlier build left 0644 included.
	const std::string record = dir / "system.cfg.backup";
	CHECK(writeText(record, "wifi.key=y\n", modeOf(path, 0644)));
	REQUIRE(stat(record.c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0600);
	REQUIRE(chmod(record.c_str(), 0644) == 0);
	CHECK(writeText(record, "wifi.key=y\n", modeOf(path, 0644)));
	REQUIRE(stat(record.c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0600);
	CHECK(copy(path, dir / "copied.cfg"));
	REQUIRE(stat((dir / "copied.cfg").c_str(), &st) == 0);
	CHECK((st.st_mode & 0777) == 0600);
	CHECK(modeOf(dir / "missing", 0640) == 0640);
	umask(before);
}

// ------------------------------------------------------------------ PL-041

TEST_CASE("the lock carries its holder's pid from the moment it exists (PL-041)")
{
	// The lock was created empty (O_EXCL) and the pid written after, and the
	// write's result was thrown away. A waiter that read it in between saw
	// an empty lock -- which both sides treat as nobody's, and remove. A
	// reader spinning beside a thousand acquire/release pairs must never
	// see the file there and empty.
	ScratchDir dir;
	const std::string path = dir / ".system.cfg.lock";
	std::atomic<bool> stop(false);
	std::atomic<int> emptySeen(0);
	std::thread reader([&] {
		while (!stop)
		{
			int fd = ::open(path.c_str(), O_RDONLY);
			if (fd < 0)
				continue;
			char buf[32];
			const ssize_t n = ::read(fd, buf, sizeof(buf));
			::close(fd);
			if (n == 0)
				emptySeen++;
		}
	});
	for (int i = 0; i < 3000; i++)
	{
		PidLock lock(path);
		REQUIRE(lock.acquire(2000));
		lock.release();
	}
	stop = true;
	reader.join();
	CHECK(emptySeen.load() == 0);
}

TEST_CASE("two waiters on a stale lock never both hold it (PL-041)")
{
	// Both read the dead holder's pid, both decide the lock is stale; the
	// first removes it and takes it, and the second's remove -- two steps
	// after its read -- could take the first one's new lock with it. Forked
	// waiters, released together at a stale lock, each marking the inside of
	// its hold with an O_EXCL file: a second mark while one is there is two
	// holders.
	ScratchDir dir;
	const std::string lockPath = dir / ".system.cfg.lock";
	const std::string inside = dir / "inside";
	int overlaps = 0;
	int failures = 0;

	for (int round = 0; round < 200; round++)
	{
		put(lockPath, std::to_string((long long) deadPid()) + "\n");
		int gate[2];
		REQUIRE(pipe(gate) == 0);
		std::vector<pid_t> waiters;
		for (int w = 0; w < 3; w++)
		{
			pid_t pid = fork();
			if (pid == 0)
			{
				::close(gate[1]);
				char c;
				if (::read(gate[0], &c, 1) < 0)
					_exit(4);
				PidLock lock(lockPath);
				if (!lock.acquire(3000))
					_exit(2);
				int fd = ::open(inside.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
				if (fd < 0)
					_exit(3);
				::close(fd);
				usleep(1000);
				::unlink(inside.c_str());
				lock.release();
				_exit(0);
			}
			waiters.push_back(pid);
		}
		::close(gate[0]);
		::close(gate[1]);   // everyone goes at once
		for (pid_t pid : waiters)
		{
			int status = 0;
			waitpid(pid, &status, 0);
			if (WIFEXITED(status) && WEXITSTATUS(status) == 3)
				overlaps++;
			else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
				failures++;
		}
		::unlink(inside.c_str());
		::unlink(lockPath.c_str());
	}
	CHECK(overlaps == 0);
	CHECK(failures == 0);
}

// --------------------------------------------- audit of the fixes G-E1-01/02

TEST_CASE("a reap guard somebody else holds cannot stretch the wait past its budget (G-E1-01)")
{
	// The stale-lock remover took `lock`.reap with a blocking flock and read
	// the clock only after it: a holder of the guard that never let go --
	// stopped, or a shell reaper waiting on something of its own -- kept the
	// interface's settings save waiting for good.
	ScratchDir dir;
	const std::string lockPath = dir / ".system.cfg.lock";
	put(lockPath, std::to_string((long long) deadPid()) + "\n");   // stale: a reaper is needed

	int held[2];
	REQUIRE(pipe(held) == 0);
	pid_t guard = fork();
	if (guard == 0)
	{
		::close(held[0]);
		int fd = ::open((lockPath + ".reap").c_str(), O_RDWR | O_CREAT, 0644);
		if (fd < 0 || ::flock(fd, LOCK_EX) != 0)
			_exit(2);
		if (::write(held[1], "x", 1) != 1)
			_exit(2);
		pause();   // holds the guard until killed
		_exit(0);
	}
	::close(held[1]);
	char c;
	REQUIRE(::read(held[0], &c, 1) == 1);
	::close(held[0]);

	const auto started = std::chrono::steady_clock::now();
	const int rc = inChildWithin(5, [&] {
		PidLock lock(lockPath);
		return lock.acquire(300) ? 1 : 0;
	});
	const long ms = (long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
	kill(guard, SIGKILL);
	int status = 0;
	waitpid(guard, &status, 0);

	CHECK(rc == 0);          // -1: the deadline killed an acquire still blocked on the guard
	CHECK(ms < 3000);
}

TEST_CASE("a reap guard that cannot be taken removes nothing (G-E1-02)")
{
	// Without the guard the remover used to go on to its re-read and unlink,
	// which is the race the guard exists to close: a check that could not
	// run went ahead as if it had passed. A directory where the guard goes
	// cannot be opened for writing; the stale lock must stay, and the wait
	// end false within its budget.
	ScratchDir dir;
	const std::string lockPath = dir / ".system.cfg.lock";
	const std::string stale = std::to_string((long long) deadPid()) + "\n";
	put(lockPath, stale);
	REQUIRE(mkdir((lockPath + ".reap").c_str(), 0755) == 0);

	const int rc = inChildWithin(5, [&] {
		PidLock lock(lockPath);
		return lock.acquire(300) ? 1 : 0;
	});
	CHECK(rc == 0);
	CHECK(get(lockPath) == stale);   // not removed, not taken
}

// ------------------------------------------------------------ F-ES-07

TEST_CASE("a lock path that can never be a lock ends the wait within its budget (#308 8b gpt F-ES-07)")
{
	// A directory where the lock goes: the create fails with EEXIST, the
	// read fails, and the loop retried at once without looking at the clock
	// -- forever, on the interface thread. It must answer false in its
	// budget and leave the directory alone.
	ScratchDir dir;
	const std::string lockPath = dir / ".system.cfg.lock";
	REQUIRE(mkdir(lockPath.c_str(), 0755) == 0);

	const auto started = std::chrono::steady_clock::now();
	const int rc = inChildWithin(5, [&] {
		PidLock lock(lockPath);
		return lock.acquire(300) ? 1 : 0;
	});
	const long ms = (long) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
	CHECK(rc == 0);          // -1 is the deadline killing a loop that never returned
	CHECK(ms < 3000);
	struct stat st;
	REQUIRE(stat(lockPath.c_str(), &st) == 0);
	CHECK(S_ISDIR(st.st_mode));
}

// ------------------------------------------------------------------ PL-024

TEST_CASE("a save that cannot get the lock writes nothing, and both writers' keys survive (PL-024)")
{
	// The interface waited five seconds for the settings lock and then saved
	// anyway. The shell's set_setting holds it across a read, a write of
	// system.cfg.tmp and a rename -- so a save made beside it put one
	// writer's snapshot over the other's: the interface's key, or the
	// script's, was gone. The other writer here is that shape, stuck for
	// longer than the interface waits; the interface's save must write
	// nothing, keep its change, and make it once the lock is free.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string lockPath = dir / ".system.cfg.lock";
	put(path, "a=1\nb=1\n");

	int ready[2];
	REQUIRE(pipe(ready) == 0);
	pid_t other = fork();
	if (other == 0)
	{
		::close(ready[0]);
		// wait_lock's shape: noclobber create, then the pid.
		int fd = ::open(lockPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd < 0)
			_exit(2);
		const std::string pid = std::to_string((long long) getpid()) + "\n";
		if (::write(fd, pid.data(), pid.size()) != (ssize_t) pid.size())
			_exit(2);
		::close(fd);
		// write_setting_line's: read, write the temporary, rename.
		put(path + ".tmp", withLine(get(path), "b=", "b=2"));
		if (::write(ready[1], "x", 1) != 1)
			_exit(2);
		usleep(800 * 1000);
		if (::rename((path + ".tmp").c_str(), path.c_str()) != 0)
			_exit(3);
		::unlink(lockPath.c_str());
		_exit(0);
	}
	::close(ready[1]);
	char c;
	REQUIRE(::read(ready[0], &c, 1) == 1);
	::close(ready[0]);

	auto setA = [](const std::string& current) { return withLine(current, "a=", "a=2"); };
	const LockedSave first = saveUnderLock(path, lockPath, 200, setA);

	int status = 0;
	waitpid(other, &status, 0);
	CHECK(WIFEXITED(status));
	CHECK(WEXITSTATUS(status) == 0);   // the other writer's rename landed
	CHECK(first == LockedSave::LockBusy);

	// The interface still has its change, and saves it once the lock is free.
	if (first != LockedSave::Written)
		CHECK(saveUnderLock(path, lockPath, 1000, setA) == LockedSave::Written);
	const std::string last = get(path);
	CHECK(last.find("b=2\n") != std::string::npos);   // the other writer's key
	CHECK(last.find("a=2\n") != std::string::npos);   // and the interface's
}

// ------------------------------------------------------------------ PL-064

TEST_CASE("a cut live file beside a whole temporary and no record loads the temporary (PL-064)")
{
	// The writer before #102 wrote system.cfg.tmp whole, then truncated the
	// live file and copied the temporary into it. Killed in the copy, it
	// left a live file cut short -- empty, or a prefix -- beside a whole
	// .tmp, and a device that had never run the build that keeps .backup had
	// no record. The start deleted the .tmp unread and took the fragment:
	// empty read as nothing (defaults), a prefix with one key=value line in
	// it passed as usable and was recorded as the last known good.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string whole = "system.hostname=RG35XXSP\nwifi.ssid=Home\nwifi.key=secret\naudio.volume=70\n";
	put(path + ".tmp", whole);

	SUBCASE("the live file truncated to nothing")
	{
		put(path, "");
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Temporary);
		CHECK(c.text == whole);
		CHECK(c.record);
	}
	SUBCASE("the live file cut part way through a line")
	{
		put(path, whole.substr(0, 46));   // "system.hostname=RG35XXSP\nwifi.ssid=Home\nwifi.k"
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Temporary);
		CHECK(c.text == whole);
	}
	SUBCASE("the live file missing")
	{
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Temporary);
		CHECK(c.text == whole);
	}
}

TEST_CASE("usable means complete: a live file cut mid-line is read, and never recorded (PL-064)")
{
	// No temporary to compare it with: the cut file is still the best there
	// is and is read as it always was, but it is not whole, so it must not
	// replace a good record.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	put(path, "system.hostname=RG35XXSP\nwifi.ssid=Ho");
	LoadedConfig c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Live);
	CHECK_FALSE(c.record);

	// A whole one is read and recorded, as ever.
	put(path, "system.hostname=RG35XXSP\nwifi.ssid=Home\n");
	c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Live);
	CHECK(c.record);
}

TEST_CASE("the choices that were already right stay right (PL-064)")
{
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string good = "system.hostname=A\n";

	// A whole live file wins over a leftover temporary: a temporary beside
	// a whole file is a save that never reached its rename, or the shell's
	// own write in flight, and neither is the file.
	put(path, good);
	put(path + ".tmp", "system.hostname=A\nwifi.ssid=B\n");
	CHECK(chooseConfig(path).source == LoadedConfig::Source::Live);
	::unlink((path + ".tmp").c_str());

	// An unusable live file and a record: the record.
	put(path, std::string("\0\0\0", 3));
	put(path + ".backup", good);
	LoadedConfig c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Backup);
	CHECK(c.text == good);

	// An unusable live file and nothing else: read as it is, recorded nowhere.
	::unlink((path + ".backup").c_str());
	put(path, "# only a comment\n");
	c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Damaged);
	CHECK_FALSE(c.record);

	// Nothing at all.
	::unlink(path.c_str());
	CHECK(chooseConfig(path).source == LoadedConfig::Source::Missing);

	// A temporary that is not whole itself is never taken.
	put(path, "");
	put(path + ".tmp", "system.hostname=A\nwifi.ss");
	CHECK(chooseConfig(path).source == LoadedConfig::Source::Damaged);
}

// ------------------------------------------------------------------ PL-068

TEST_CASE("the transfer lock reads as held while a script holds it, and only then (PL-068)")
{
	// The save state manager's DELETE and COPY asked only whether the
	// interface's own sync card was running. A cloud_backup started from a
	// shell, or anything else holding /var/run/cloud_sync.lock, was
	// invisible to them. The script's side is `exec 9>lock; flock -n 9` in a
	// process of its own; the child here holds the lock the same way.
	ScratchDir dir;
	const std::string lockPath = dir / "cloud_sync.lock";

	CHECK_FALSE(isFlockHeld(lockPath));   // no file: nobody's

	int held[2], done[2];
	REQUIRE(pipe(held) == 0);
	REQUIRE(pipe(done) == 0);
	pid_t script = fork();
	if (script == 0)
	{
		::close(held[0]);
		::close(done[1]);
		int fd = ::open(lockPath.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
		if (fd < 0 || ::flock(fd, LOCK_EX | LOCK_NB) != 0)
			_exit(2);
		if (::write(held[1], "x", 1) != 1)
			_exit(2);
		char c;
		if (::read(done[0], &c, 1) < 0)
			_exit(2);
		_exit(0);   // the lock goes with the process
	}
	::close(held[1]);
	::close(done[0]);
	char c;
	REQUIRE(::read(held[0], &c, 1) == 1);

	CHECK(isFlockHeld(lockPath));
	CHECK(isFlockHeld(lockPath));   // asking twice does not take it from the holder

	REQUIRE(::write(done[1], "x", 1) == 1);
	int status = 0;
	waitpid(script, &status, 0);
	CHECK(WIFEXITED(status));
	CHECK(WEXITSTATUS(status) == 0);

	// The file stays behind after every run; it is the flock that says
	// whether a run is on.
	CHECK_FALSE(isFlockHeld(lockPath));
	::close(held[0]);
	::close(done[1]);
}

// ------------------------------------------------------------------ PL-069

namespace
{
	long vmSizeKiB()
	{
		std::ifstream status("/proc/self/status");
		std::string line;
		while (std::getline(status, line))
			if (line.rfind("VmSize:", 0) == 0)
				return atol(line.c_str() + 7);
		return -1;
	}

	// The entries under /proc/self/task: this process's threads, as
	// `ls /proc/<pid>/task | wc -l` counts them.
	int taskCount()
	{
		DIR* dir = opendir("/proc/self/task");
		if (dir == nullptr)
			return -1;
		int n = 0;
		while (struct dirent* entry = readdir(dir))
			if (entry->d_name[0] != '.')
				n++;
		closedir(dir);
		return n;
	}
}

TEST_CASE("a thread that ends unjoined keeps its stack; a detached one gives it back (PL-069)")
{
	// ThreadedCloudSync did `mHandle = new std::thread(run)` and ended run()
	// with `delete this`, never joining, detaching or deleting the thread: a
	// joinable thread that has ended keeps its stack mapping and its control
	// block until somebody joins it, so every sync -- two a game -- left one
	// behind for the life of the interface. This is the pattern it used and
	// the one it uses now, sequentially, as the syncs run; the case below
	// holds ThreadedCloudSync itself to the second. It also says what to
	// measure on the VM: the task count does not move (the kernel reaps the
	// thread either way); VmSize does, by a stack a sync.
	//
	// The thresholds are this host's own stack size, not a number
	// (audit of the fixes G-E1-07 gpt / G-E1-05 claude): glibc gives a new
	// thread RLIMIT_STACK's size, 8 MiB on most hosts and 2 MiB where the
	// limit is unlimited, and a fixed 4 MiB floor was one host's.
	pthread_attr_t attr;
	REQUIRE(pthread_attr_init(&attr) == 0);
	size_t stack = 0;
	REQUIRE(pthread_attr_getstacksize(&attr, &stack) == 0);
	pthread_attr_destroy(&attr);
	const long stackKiB = (long) (stack / 1024);
	REQUIRE(stackKiB > 0);

	const int tasksBefore = taskCount();
	const long before = vmSizeKiB();
	for (int i = 0; i < 20; i++)
	{
		std::atomic<bool> ended(false);
		new std::thread([&ended] { ended = true; });   // the old shape: never joined, never deleted
		while (!ended)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	const long leaked = vmSizeKiB() - before;
	const int tasksAfterLeak = taskCount();

	const long before2 = vmSizeKiB();
	for (int i = 0; i < 20; i++)
	{
		std::atomic<bool> ended(false);
		std::thread([&ended] { ended = true; }).detach();   // the new one
		while (!ended)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	const long detached = vmSizeKiB() - before2;

	INFO("thread stack " << stackKiB << " KiB; VmSize growth, 20 unjoined threads: " << leaked << " KiB; 20 detached: " << detached << " KiB");
	CHECK(leaked >= 20 * stackKiB / 2);   // most of a stack each, kept
	CHECK(detached <= 4 * stackKiB);      // a stack or a few, reused
	CHECK(tasksAfterLeak == tasksBefore); // why `ls /proc/<pid>/task` cannot see it
}

TEST_CASE("ThreadedCloudSync starts its thread detached and keeps no handle to leak (PL-069, G-E1-07)")
{
	// The case above is the mechanism; this one holds the class to it. The
	// class needs a Window and cannot be built here, so its source is read:
	// no `new std::thread`, no thread member, and the one thread it starts is
	// detached where it is made. Reverting to the leaking shape fails here.
	// ES_SOURCE_ROOT overrides the tree read (to show this fail on an old one).
	const char* root = getenv("ES_SOURCE_ROOT");
	const std::string base = root != nullptr && *root != '\0' ? root : ES_ROOT_DIR;
	// The code, not its comments: the comment that tells the story names
	// the old shape.
	auto code = [](const std::string& text) {
		std::string out;
		std::istringstream in(text);
		std::string line;
		while (std::getline(in, line))
			out += line.substr(0, line.find("//")) + "\n";
		return out;
	};
	const std::string cpp = code(get(base + "/es-app/src/ThreadedCloudSync.cpp"));
	const std::string h = code(get(base + "/es-app/src/ThreadedCloudSync.h"));
	REQUIRE_FALSE(cpp.empty());
	REQUIRE_FALSE(h.empty());
	CHECK(cpp.find("new std::thread") == std::string::npos);
	CHECK(h.find("std::thread*") == std::string::npos);
	CHECK(cpp.find("std::thread(&ThreadedCloudSync::run, this).detach();") != std::string::npos);
}

// ------------------------------------------------------ G-E1-03

TEST_CASE("a reload keeps the changes still waiting to be saved, unless the file moved under them (G-E1-03)")
{
	// A save refused for the lock keeps its changes for the next save
	// (PL-024) -- and the Wi-Fi picker's reload after a join cleared them, so
	// the next save had nothing to make. A change survives a reload when
	// the file still holds the key as it was when the change was made; a
	// key the file now holds differently was written by somebody since
	// (wifictl join wrote wifi.ssid), and theirs is the newer write.
	std::map<std::string, PendingChange> pending;
	pending["audio.volume"] = { "40", true, "70" };              // the player's; the file still says 70
	pending["wifi.ssid"] = { "Old Cafe", true, "Home" };         // the join rewrote it: Library
	pending["global.retroachievements"] = { "1", false, "" };    // new key; the file still has none
	pending["system.language"] = { "fr_FR", false, "" };         // new key; a script added one since

	const std::map<std::string, std::string> reloaded = {
		{ "audio.volume", "70" },
		{ "wifi.ssid", "Library" },
		{ "system.language", "de_DE" },
		{ "system.hostname", "RG35XXSP" },
	};

	const auto kept = pendingAfterReload(pending, reloaded);
	CHECK(kept.size() == 2);
	REQUIRE(kept.count("audio.volume") == 1);
	CHECK(kept.at("audio.volume") == "40");
	REQUIRE(kept.count("global.retroachievements") == 1);
	CHECK(kept.at("global.retroachievements") == "1");
	CHECK(kept.count("wifi.ssid") == 0);
	CHECK(kept.count("system.language") == 0);

	// Nothing pending, nothing kept.
	CHECK(pendingAfterReload({}, reloaded).empty());
}

TEST_CASE("parseKeyValues reads a system.cfg as SystemConf always has")
{
	const auto v = parseKeyValues("# comment\n;also\nsystem.hostname=RG\nempty=\n=novalue\nwifi.key=a=b\naudio.volume=70\naudio.volume=40\n");
	CHECK(v.size() == 3);
	CHECK(v.at("system.hostname") == "RG");
	CHECK(v.at("wifi.key") == "a=b");      // the value is everything after the first =
	CHECK(v.at("audio.volume") == "40");   // the last of a repeated key
	CHECK(v.count("empty") == 0);
}

// ------------------------------------------------------ G-E1-04 / G-E1-05

namespace
{
	void setMtime(const std::string& path, time_t when)
	{
		struct timespec times[2];
		times[0].tv_sec = when; times[0].tv_nsec = 0;
		times[1].tv_sec = when; times[1].tv_nsec = 0;
		utimensat(AT_FDCWD, path.c_str(), times, 0);
	}
}

TEST_CASE("a live file cut short of its own record loads the record (G-E1-04)")
{
	// No temporary to compare with, and a whole record beside the live file
	// whose text is the live file's start: the file was cut after the record
	// was written from it. It used to be loaded as it was -- one key=value
	// line is "usable" -- and the next save recorded the fragment over the
	// record that held the rest.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string whole = "system.hostname=A\nwifi.ssid=Home\nwifi.key=secret\n";
	put(path + ".backup", whole);
	put(path, whole.substr(0, 38));   // "...wifi.ssid=Home\nwifi.k"
	const time_t now = time(nullptr);
	setMtime(path, now - 60);
	setMtime(path + ".backup", now - 60);

	LoadedConfig c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Backup);
	CHECK(c.text == whole);

	// A live file edited after the record -- a hand edit that left off the
	// last line end and dropped the last lines -- is the owner's, and stays.
	setMtime(path, now);
	c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Live);
	CHECK_FALSE(c.record);
}

TEST_CASE("a save merged onto a cut file is written and not recorded (G-E1-04)")
{
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string lockPath = dir / ".system.cfg.lock";
	auto setA = [](const std::string& current) { return withLine(current + (current.empty() || current.back() == '\n' ? "" : "\n"), "a=", "a=2"); };

	put(path, "a=1\nb=par");
	bool whole = true;
	CHECK(saveUnderLock(path, lockPath, 1000, setA, nullptr, &whole) == LockedSave::Written);
	CHECK_FALSE(whole);

	put(path, "a=1\nb=2\n");
	whole = false;
	CHECK(saveUnderLock(path, lockPath, 1000, setA, nullptr, &whole) == LockedSave::Written);
	CHECK(whole);
}

TEST_CASE("a recovery is written no less private than any copy it came from (G-E1-05)")
{
	// A whole system.cfg.tmp made 0600, and neither the live file nor the
	// record: the recovery wrote it back 0644 -- its mode came from the live
	// file and the record only -- and the record after it the same.
	ScratchDir dir;
	const mode_t before = umask(022);
	const std::string path = dir / "system.cfg";
	put(path + ".tmp", "system.hostname=A\nwifi.key=secret\n");
	REQUIRE(chmod((path + ".tmp").c_str(), 0600) == 0);
	LoadedConfig c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Temporary);
	CHECK(c.mode == 0600);

	// A 0600 record beside a 0644 cut live file: the record's privacy wins.
	::unlink((path + ".tmp").c_str());
	put(path + ".backup", "system.hostname=A\n");
	REQUIRE(chmod((path + ".backup").c_str(), 0600) == 0);
	put(path, "");
	c = chooseConfig(path);
	CHECK(c.source == LoadedConfig::Source::Backup);
	CHECK(c.mode == 0600);

	// Nothing private anywhere: 0644, as ever.
	REQUIRE(chmod((path + ".backup").c_str(), 0644) == 0);
	CHECK(chooseConfig(path).mode == 0644);
	umask(before);
}


// ------------------------------------------------------ the audit of the fix round

TEST_CASE("a record cut short beside a whole temporary: the temporary recovers (audit of the fix round PL-018)")
{
	// chooseConfig worked out whether the record was whole and then took it
	// on one key=value line alone: a cut system.cfg.backup -- the cp at boot
	// before #102 left such records -- beat a whole system.cfg.tmp and was
	// written back as the live file. A whole temporary is the recovery.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string cut = "system.hostname=A\nwifi.ssid=Home\nwifi.key=sec";
	const std::string whole = "system.hostname=A\nwifi.ssid=Home\nwifi.key=secret\naudio.volume=70\n";
	put(path + ".backup", cut);
	put(path + ".tmp", whole);

	SUBCASE("no live file")
	{
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Temporary);
		CHECK(c.text == whole);
		CHECK(c.record);
	}
	SUBCASE("an unusable live file")
	{
		put(path, std::string("\0\0\0", 3));
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Temporary);
		CHECK(c.text == whole);
		CHECK(c.record);
	}
}

TEST_CASE("a record cut short with no whole temporary: the defaults answer, and the cut record is not the record (audit of the fix round PL-018)")
{
	// With nothing better the cut record was loaded and written back as the
	// live file. A record is whole or it is not the record: nothing is
	// loaded and the defaults answer, as the boot's own check has it
	// (chksysconfig: the image defaults last).
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string cut = "system.hostname=A\nwifi.ssid=Home\nwifi.key=sec";
	put(path + ".backup", cut);

	SUBCASE("no live file: nothing is loaded")
	{
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Missing);
		CHECK(c.text.empty());
		CHECK_FALSE(c.record);
	}
	SUBCASE("an unusable live file: read as it is, the cut record neither loaded nor recorded")
	{
		put(path, std::string("\0\0\0", 3));
		const LoadedConfig c = chooseConfig(path);
		CHECK(c.source == LoadedConfig::Source::Damaged);
		CHECK(c.text != cut);
		CHECK(parseKeyValues(c.text).empty());
		CHECK_FALSE(c.record);
	}
	// chooseConfig writes nothing: the cut record is on disk as it was.
	CHECK(get(path + ".backup") == cut);
}

TEST_CASE("a save never merges onto a file the load would not have taken (audit of the fix round, gpt G2-E-core-03)")
{
	// saveUnderLock merged onto whatever the live file held and called an
	// empty one whole, so a save onto an empty system.cfg wrote the changed
	// keys alone and they became the record over a whole one; and a cut
	// file the load would have replaced by its whole temporary or record
	// was merged onto as it was, and one save later the result -- its lines
	// completed -- became the record. Under the lock the save now asks
	// chooseConfig, as the load does, and merges onto the recovery it names.
	ScratchDir dir;
	const std::string path = dir / "system.cfg";
	const std::string lockPath = dir / ".system.cfg.lock";
	const std::string whole = "system.hostname=A\nwifi.ssid=Home\nwifi.key=secret\n";
	auto addVolume = [](const std::string& current)
	{
		std::string out = current;
		if (!out.empty() && out.back() != '\n')
			out += '\n';
		return out + "audio.volume=40\n";
	};

	SUBCASE("an empty live file beside a whole record: the record, and the change")
	{
		put(path + ".backup", whole);
		put(path, "");
		std::string written;
		bool baseWhole = false;
		CHECK(saveUnderLock(path, lockPath, 1000, addVolume, &written, &baseWhole) == LockedSave::Written);
		CHECK(written == whole + "audio.volume=40\n");
		CHECK(get(path) == whole + "audio.volume=40\n");
		CHECK(baseWhole);
	}
	SUBCASE("an empty live file and nothing else: not whole, so not a record")
	{
		put(path, "");
		bool baseWhole = true;
		CHECK(saveUnderLock(path, lockPath, 1000, addVolume, nullptr, &baseWhole) == LockedSave::Written);
		CHECK(get(path) == "audio.volume=40\n");
		CHECK_FALSE(baseWhole);
	}
	SUBCASE("a live file cut short of a whole temporary: the temporary, with its mode")
	{
		const mode_t before = umask(022);
		put(path + ".tmp", whole);
		REQUIRE(chmod((path + ".tmp").c_str(), 0600) == 0);
		put(path, whole.substr(0, 30));   // "system.hostname=A\nwifi.ssid=Ho"
		bool baseWhole = false;
		CHECK(saveUnderLock(path, lockPath, 1000, addVolume, nullptr, &baseWhole) == LockedSave::Written);
		CHECK(get(path) == whole + "audio.volume=40\n");
		CHECK(baseWhole);
		struct stat st;
		REQUIRE(::stat(path.c_str(), &st) == 0);
		CHECK((st.st_mode & 07777) == 0600);
		umask(before);
	}
	SUBCASE("a cut live file with nothing whole beside it: merged onto, and not whole")
	{
		put(path, "system.hostname=A\nwifi.ssid=Ho");
		bool baseWhole = true;
		CHECK(saveUnderLock(path, lockPath, 1000, addVolume, nullptr, &baseWhole) == LockedSave::Written);
		CHECK(get(path) == "system.hostname=A\nwifi.ssid=Ho\naudio.volume=40\n");
		CHECK_FALSE(baseWhole);
	}
	SUBCASE("a whole live file beside a leftover temporary: the live file")
	{
		put(path, "system.hostname=A\n");
		put(path + ".tmp", whole);
		bool baseWhole = false;
		CHECK(saveUnderLock(path, lockPath, 1000, addVolume, nullptr, &baseWhole) == LockedSave::Written);
		CHECK(get(path) == "system.hostname=A\naudio.volume=40\n");
		CHECK(baseWhole);
	}
}
