#include "utils/AtomicFileUtil.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace Utils
{
	namespace AtomicFile
	{
#if !defined(_WIN32)
		// fsync the directory holding `path`, so the rename that just landed
		// in it is on disk too. Best effort: a filesystem that refuses to
		// open a directory for reading still has the rename in its journal.
		static void syncDirectoryOf(const std::string& path)
		{
			auto slash = path.find_last_of('/');
			const std::string dir = slash == std::string::npos ? "." : slash == 0 ? "/" : path.substr(0, slash);
			int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
			if (fd < 0)
				return;
			::fsync(fd);
			::close(fd);
		}

		// A name beside `path` that no other call -- in this process or any
		// other -- is using: `path`.tmp.<pid>.<n>, created O_EXCL so a name
		// left behind by a process that died and whose pid came round again
		// is stepped over rather than truncated. Every writer used to share
		// `path`.tmp under O_TRUNC (PL-063), and so did the shell: set_setting
		// writes system.cfg.tmp with awk and renames it. Two saves at once
		// truncated each other's temporary; the first renamed the shared file
		// into place -- empty or half written -- and the second's rename found
		// the name gone and reported a save that had not happened. The fd is
		// returned open for writing, -1 on failure.
		static int createTemporary(const std::string& path, std::string& tmp, int mode)
		{
			static std::atomic<unsigned long> counter(0);
			const std::string stem = path + ".tmp." + std::to_string((long long) ::getpid()) + ".";
			for (int attempt = 0; attempt < 100; attempt++)
			{
				tmp = stem + std::to_string(counter++);
				int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, (mode_t) mode);
				if (fd >= 0)
					return fd;
				if (errno != EEXIST)
					return -1;
			}
			return -1;
		}
#endif

		int modeOf(const std::string& path, int fallback)
		{
#if defined(_WIN32)
			(void) path;
			return fallback;
#else
			struct stat st;
			if (::stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
				return fallback;
			return (int) (st.st_mode & 07777);
#endif
		}

		bool writeText(const std::string& path, const std::string& text, int mode)
		{
#if defined(_WIN32)
			const std::string tmp = path + ".tmp";
			{
				std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
				if (!out)
					return false;
				out.write(text.data(), (std::streamsize) text.size());
				if (!out)
				{
					out.close();
					std::remove(tmp.c_str());
					return false;
				}
			}
			std::remove(path.c_str());
			if (std::rename(tmp.c_str(), path.c_str()) != 0)
			{
				std::remove(tmp.c_str());
				return false;
			}
			return true;
#else
			// The rename puts the temporary's mode and owner on the live name,
			// so a file being replaced gives the temporary its own first
			// (#308 F-ES-08): system.cfg and es_settings.cfg carry the Wi-Fi
			// key and the account passwords, and a file somebody had made 0600
			// came back 0644 from the next save. A new file is made 0644,
			// before umask, as the shell's redirections make one -- unless the
			// caller names the mode, which a record of a private file does.
			struct stat existing;
			const bool replacing = ::stat(path.c_str(), &existing) == 0 && S_ISREG(existing.st_mode);

			std::string tmp;
			int fd = createTemporary(path, tmp, mode >= 0 ? 0600 : replacing ? 0600 : 0644);
			if (fd < 0)
				return false;
			if (mode >= 0)
			{
				if (::fchmod(fd, (mode_t) (mode & 07777)) != 0)
				{
					::close(fd);
					::unlink(tmp.c_str());
					return false;
				}
			}
			else if (replacing)
			{
				// The owner only where it differs and only as root, which is
				// how ROCKNIX runs the interface; best effort, since a caller
				// that cannot give a file away still wrote what it meant to.
				// The mode is not best effort: a file that would come out less
				// private than it was is not written.
				if ((existing.st_uid != ::geteuid() || existing.st_gid != ::getegid()) && ::geteuid() == 0)
				{
					if (::fchown(fd, existing.st_uid, existing.st_gid) != 0)
					{
						// kept as it is: root that cannot chown is a filesystem without owners
					}
				}
				if (::fchmod(fd, existing.st_mode & 07777) != 0)
				{
					::close(fd);
					::unlink(tmp.c_str());
					return false;
				}
			}

			const char* data = text.data();
			size_t left = text.size();
			bool ok = true;
			while (left > 0)
			{
				ssize_t n = ::write(fd, data, left);
				if (n < 0)
				{
					if (errno == EINTR)
						continue;
					ok = false;
					break;
				}
				data += n;
				left -= (size_t) n;
			}
			// The data must be on disk before the rename makes it the file
			// everything reads: rename first and a power cut leaves a
			// complete-looking name over empty blocks.
			if (ok && ::fsync(fd) != 0)
				ok = false;
			if (::close(fd) != 0)
				ok = false;
			if (!ok)
			{
				::unlink(tmp.c_str());
				return false;
			}
			if (std::rename(tmp.c_str(), path.c_str()) != 0)
			{
				::unlink(tmp.c_str());
				return false;
			}
			syncDirectoryOf(path);
			return true;
#endif
		}

		// Read whole, or say it was not (PL-065). This used to say ok once the
		// stream had opened, whatever the read then did: an I/O error part way
		// through a failing card returned the prefix as the whole file, and
		// SystemConf parsed it and recorded it as the last known good. Now ok
		// means every read succeeded to the end, and for a regular file that
		// the end was no shorter than the size it had when it was opened -- a
		// file cut under the reader is not the file. On any failure the text
		// is "", as for a file that would not open.
		std::string readText(const std::string& path, bool* ok)
		{
			if (ok != nullptr)
				*ok = false;
#if defined(_WIN32)
			// C stdio, whose ferror says whether a read failed. The stream
			// version read failbit on its ostringstream as a failure, and
			// `ss << in.rdbuf()` sets it when nothing is inserted: an empty
			// file came back not read (audit of the fixes G-E1-06, claude).
			FILE* file = std::fopen(path.c_str(), "rb");
			if (file == nullptr)
				return "";
			std::string text;
			char buffer[65536];
			size_t n;
			while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
				text.append(buffer, n);
			const bool failed = std::ferror(file) != 0;
			std::fclose(file);
			if (failed)
				return "";
			if (ok != nullptr)
				*ok = true;
			return text;
#else
			int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
			if (fd < 0)
				return "";
			struct stat st;
			if (::fstat(fd, &st) != 0)
			{
				::close(fd);
				return "";
			}

			std::string text;
			if (S_ISREG(st.st_mode) && st.st_size > 0)
				text.reserve((size_t) st.st_size);
			char buffer[65536];
			for (;;)
			{
				const ssize_t n = ::read(fd, buffer, sizeof(buffer));
				if (n < 0)
				{
					if (errno == EINTR)
						continue;
					::close(fd);   // EIO part way, EISDIR on a directory: not a read
					return "";
				}
				if (n == 0)
					break;
				text.append(buffer, (size_t) n);
			}
			::close(fd);

			if (S_ISREG(st.st_mode) && (off_t) text.size() < st.st_size)
				return "";
			if (ok != nullptr)
				*ok = true;
			return text;
#endif
		}

		bool copy(const std::string& src, const std::string& dst)
		{
			bool ok = false;
			const std::string text = readText(src, &ok);
			if (!ok)
				return false;
			return writeText(dst, text, modeOf(src, 0644));
		}

		bool isUsableKeyValues(const std::string& text)
		{
			if (text.empty() || text.find('\0') != std::string::npos)
				return false;
			std::istringstream in(text);
			std::string line;
			while (std::getline(in, line))
			{
				auto idx = line.find('=');
				if (idx != std::string::npos && idx > 0 && line[0] != '#' && line[0] != ';')
					return true;
			}
			return false;
		}

		// Whole, as every writer of these files leaves one: each line ends in
		// a line end -- ours, the shell's awk and sed, echo >>. A text that
		// stops part way through a line was cut. (A cut exactly at a line end
		// cannot be told from a shorter file; nothing here pretends to.)
		static bool isComplete(const std::string& text)
		{
			return !text.empty() && text.back() == '\n';
		}

		LoadedConfig chooseConfig(const std::string& path)
		{
			// PL-064. The start used to delete `path`.tmp unread and take the
			// live file whenever one key=value line survived in it. The writer
			// before #102 wrote the temporary whole, then truncated the live
			// file and copied the temporary in: killed in the copy, it left the
			// live file cut short beside a whole temporary, on a device that
			// may never have run a build that keeps the record. So the
			// temporary is read first, and wins when it is whole and the live
			// file is gone, unusable, or a cut prefix of it; and "usable" for
			// the record means complete as well -- for the record's own load
			// too (PL-018).
			LoadedConfig out;
			bool liveOk = false, tmpOk = false, backupOk = false;
			const std::string live = readText(path, &liveOk);
			const std::string tmp = readText(path + ".tmp", &tmpOk);
			const std::string backup = readText(path + ".backup", &backupOk);
			const bool tmpWhole = tmpOk && isUsableKeyValues(tmp) && isComplete(tmp);
			const bool backupWhole = backupOk && isUsableKeyValues(backup) && isComplete(backup);

			// The mode the chosen text goes back with, and the record: the
			// permission bits every copy on disk shares, so a recovery is never
			// less private than a copy it came from (audit of the fixes
			// G-E1-05: a 0600 temporary came back as a 0644 file and record).
			int mode = 07777;
			bool any = false;
			for (const std::string& copy : { path, path + ".tmp", path + ".backup" })
			{
				const int m = modeOf(copy, -1);
				if (m >= 0)
				{
					mode &= m;
					any = true;
				}
			}
			out.mode = any ? mode : 0644;

			// The copy stopped part way: what is there is the start of the
			// temporary, short of its end.
			const bool liveCut = liveOk && tmpWhole && !isComplete(live)
				&& live.size() < tmp.size() && tmp.compare(0, live.size(), live) == 0;

			// Or the live file stops part way through a line and is the start
			// of the whole record beside it, written no earlier than the live
			// file was (G-E1-04): the file was cut after the record was made
			// from it, and the record is the file. A live file changed after
			// the record -- a hand edit that left off the last line end -- is
			// the owner's, and is read as it is.
			bool cutOfRecord = false;
#if defined(_WIN32)
			(void) backupWhole;   // the mtime test below is POSIX's
#else
			if (liveOk && backupWhole && !isComplete(live) && live.size() < backup.size()
				&& backup.compare(0, live.size(), live) == 0)
			{
				struct stat ls, bs;
				cutOfRecord = ::stat(path.c_str(), &ls) == 0 && ::stat((path + ".backup").c_str(), &bs) == 0
					&& ls.st_mtime <= bs.st_mtime + 1;
			}
#endif

			if (liveOk && isUsableKeyValues(live) && !liveCut && !cutOfRecord)
			{
				// A temporary beside a whole file is a save that never reached
				// its rename, or the shell's own write in flight; neither is the
				// file. A live file that is not whole is still read -- there is
				// nothing better -- but it does not become the record.
				out.source = LoadedConfig::Source::Live;
				out.text = live;
				out.record = isComplete(live);
				return out;
			}
			if (liveCut)
			{
				out.source = LoadedConfig::Source::Temporary;
				out.text = tmp;
				out.record = true;
				return out;
			}

			// The record only when it is whole (audit of the fix round PL-018):
			// one key=value line was enough, so a record cut short -- the cp at
			// boot before #102 left such records -- beat a whole temporary,
			// and with nothing better was written back as the live file. A cut
			// record is not the last known good; with nothing else, nothing is
			// loaded and the defaults answer, as chksysconfig has it.
			if (backupWhole)
			{
				out.source = LoadedConfig::Source::Backup;
				out.text = backup;
				return out;
			}
			if (tmpWhole)
			{
				out.source = LoadedConfig::Source::Temporary;
				out.text = tmp;
				out.record = true;
				return out;
			}
			if (liveOk)
			{
				out.source = LoadedConfig::Source::Damaged;
				out.text = live;
			}
			return out;
		}

		// A choice that is to be written back as the live file.
		static bool isRecovery(const LoadedConfig& chosen)
		{
			return chosen.source == LoadedConfig::Source::Temporary || chosen.source == LoadedConfig::Source::Backup;
		}

		LoadedConfig loadUnderLock(const std::string& path, const std::string& lockPath, int timeoutMs,
			RecoveryWrite* wrote)
		{
			if (wrote != nullptr)
				*wrote = RecoveryWrite::None;

			LoadedConfig chosen = chooseConfig(path);
			if (!isRecovery(chosen))
				return chosen;

			PidLock lock(lockPath);
			if (!lock.acquire(timeoutMs))
			{
				if (wrote != nullptr)
					*wrote = RecoveryWrite::LockBusy;
				return chosen;
			}

			// Again, with the lock: a writer that held it while this waited may
			// have put a whole file in place, and that file is the one to read.
			chosen = chooseConfig(path);
			if (!isRecovery(chosen))
				return chosen;

			const bool written = writeText(path, chosen.text, chosen.mode);
			if (wrote != nullptr)
				*wrote = written ? RecoveryWrite::Written : RecoveryWrite::WriteFailed;
			return chosen;
		}

		bool isFlockHeld(const std::string& path)
		{
#if defined(_WIN32)
			(void) path;
			return false;
#else
			int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
			if (fd < 0)
				return errno != ENOENT;
			int r;
			do
				r = ::flock(fd, LOCK_EX | LOCK_NB);
			while (r != 0 && errno == EINTR);
			const bool held = r != 0;   // EWOULDBLOCK: somebody has it; anything else: cannot tell
			::close(fd);                // and the lock, if this took it, with it
			return held;
#endif
		}

		std::map<std::string, std::string> parseKeyValues(const std::string& text)
		{
			std::map<std::string, std::string> values;
			std::istringstream in(text);
			std::string line;
			while (std::getline(in, line))
			{
				const size_t idx = line.find('=');
				if (idx == std::string::npos || line.find('#') == 0 || line.find(';') == 0)
					continue;
				const std::string key = line.substr(0, idx);
				const std::string value = line.substr(idx + 1);
				if (!key.empty() && !value.empty())
					values[key] = value;
			}
			return values;
		}

		std::map<std::string, std::string> pendingAfterReload(const std::map<std::string, PendingChange>& pending,
			const std::map<std::string, std::string>& reloaded)
		{
			std::map<std::string, std::string> kept;
			for (const auto& change : pending)
			{
				const auto now = reloaded.find(change.first);
				const bool hasNow = now != reloaded.cend();
				if (hasNow != change.second.hadBase)
					continue;   // added or removed by somebody since
				if (hasNow && now->second != change.second.base)
					continue;   // rewritten by somebody since
				kept[change.first] = change.second.value;
			}
			return kept;
		}

		LockedSave saveUnderLock(const std::string& path, const std::string& lockPath, int timeoutMs,
			const std::function<std::string(const std::string& current)>& merge, std::string* written,
			bool* baseComplete)
		{

			// Not without the lock (PL-024). This used to log and go ahead once
			// the budget ran out -- "the player's change dropped on the floor
			// is the worse outcome" -- and the change was dropped anyway,
			// under the other writer's rename, or it dropped theirs. Refused,
			// the change is not lost: the caller keeps it and writes it at the
			// next save, which reads the other writer's file first.
			PidLock lock(lockPath);
			if (!lock.acquire(timeoutMs))
				return LockedSave::LockBusy;

			bool opened = false;
			const std::string current = readText(path, &opened);
#if !defined(_WIN32)
			if (!opened)
				return LockedSave::Unreadable;
#endif
			// (On Windows a file not there yet is merged onto nothing and made,
			// as SystemConf always did there.)

			// Never onto a file the load would not have taken as the file (audit
			// of the fix round, gpt G2-E-core-03). A live file that is empty,
			// cut part way through a line or unusable is asked about as the
			// load asks, and where chooseConfig names the recovery -- the whole
			// temporary it is the start of, the whole record -- the save merges
			// onto that, written with the mode every copy shares. An empty file
			// used to count as whole: a save onto one wrote the changed keys
			// alone, and they became the record over a whole one. A missing
			// file is still refused above, so a factory reset's empty
			// /storage is not refilled from memory.
			std::string base = current;
			bool whole = isComplete(current);
			int mode = -1;
			if (!whole || !isUsableKeyValues(current))
			{
				const LoadedConfig chosen = chooseConfig(path);
				if (isRecovery(chosen))
				{
					base = chosen.text;
					whole = true;
					mode = chosen.mode;
				}
			}

			// A save merged onto a cut file lands (the change is the player's)
			// but is not a record (G-E1-04).
			if (baseComplete != nullptr)
				*baseComplete = whole;

			const std::string out = merge(base);
			if (!writeText(path, out, mode))
				return LockedSave::WriteFailed;
			if (written != nullptr)
				*written = out;
			return LockedSave::Written;
		}

		PidLock::PidLock(const std::string& path) : mPath(path), mHeld(false)
		{
		}

		PidLock::~PidLock()
		{
			release();
		}

#if !defined(_WIN32)
		// The first line of a lock file, without its line end: the holder's
		// pid as both sides write it ("$$\n" from the shell).
		static std::string lockHolder(const std::string& text)
		{
			std::string holder = text.substr(0, text.find('\n'));
			while (!holder.empty() && (holder.back() == '\r' || holder.back() == ' '))
				holder.pop_back();
			return holder;
		}

		// Remove the lock at `path` only if it still names `holder`, with no
		// other remover between that look and the unlink (PL-041). Every
		// remover of a stale lock -- this process on any thread, and the
		// shell's wait_lock, which takes the same guard -- takes an flock on
		// `path`.reap for the few instructions this lasts. Under it nothing
		// else can change what `path` names: a stale holder is gone and cannot
		// release it, a live holder only ever releases a lock naming itself,
		// and every other remover waits. So the re-read and the unlink are one
		// step as far as the lock's users are concerned. The guard is an
		// flock, so a remover that dies holding it frees it with its last
		// descriptor -- there is no stale guard to clear.
		//
		// The guard is asked without waiting, until `deadline` (audit of the
		// fixes G-E1-01: a blocking flock here held the interface thread past
		// its budget for as long as another holder kept the guard). And a
		// guard that cannot be had -- the file cannot be opened, the flock
		// fails, the deadline comes -- removes nothing (G-E1-02): the re-read
		// and unlink without it are the race the guard is for. False then;
		// the caller waits on as for a live holder, and its budget ends it.
		static bool removeIfStill(const std::string& path, const std::string& holder,
			std::chrono::steady_clock::time_point deadline)
		{
			const std::string guard = path + ".reap";
			int gfd = ::open(guard.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
			if (gfd < 0)
				return false;
			for (;;)
			{
				if (::flock(gfd, LOCK_EX | LOCK_NB) == 0)
					break;
				if (errno == EINTR)
					continue;
				if (errno != EWOULDBLOCK || std::chrono::steady_clock::now() >= deadline)
				{
					::close(gfd);
					return false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			bool again = false;
			const std::string current = readText(path, &again);
			if (again && lockHolder(current) == holder)
				::unlink(path.c_str());
			::close(gfd);   // and the flock with it
			return true;
		}
#endif

		bool PidLock::acquire(int timeoutMs)
		{
#if defined(_WIN32)
			mHeld = true;
			return true;
#else
			if (mHeld)
				return true;

			const auto started = std::chrono::steady_clock::now();
			const std::string mine = std::to_string((long long) ::getpid());

			// The lock is made whole before anybody can see it (PL-041): the
			// pid goes into a file of this call's own beside it, and that file
			// is hard-linked to the lock's name, which fails with EEXIST exactly
			// as the O_EXCL create did. The lock used to be created empty and
			// the pid written after, with the write's result thrown away, so a
			// waiter reading in between saw an empty lock -- which both sides
			// take for nobody's -- and a failed write left one for good.
			static std::atomic<unsigned long> counter(0);
			const std::string staging = mPath + ".new." + mine + "." + std::to_string(counter++);
			bool linkable = true;
			{
				int fd = ::open(staging.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
				if (fd < 0)
					return false;   // the directory is missing or not writable: nothing to hold
				const std::string line = mine + "\n";
				ssize_t n;
				do
					n = ::write(fd, line.data(), line.size());
				while (n < 0 && errno == EINTR);
				const bool written = n == (ssize_t) line.size();
				if (::close(fd) != 0 || !written)
				{
					::unlink(staging.c_str());
					return false;
				}
			}

			// The clock is read on every pass, whichever way the pass went
			// (#308 F-ES-07): the budget used to be checked only after a live
			// holder was found, so a lock that could never be read or never
			// be removed -- a directory where the lock goes -- kept the
			// interface thread in this loop for good.
			auto expired = [&]() {
				const long elapsed = (long) std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - started).count();
				if (elapsed < timeoutMs)
					return false;
				::unlink(staging.c_str());
				return true;
			};

			for (;;)
			{
				int made = -1;
				if (linkable)
				{
					made = ::link(staging.c_str(), mPath.c_str());
					if (made != 0 && errno != EEXIST)
					{
						// A filesystem with no hard links (EPERM, ENOTSUP): the
						// create the shell makes, then the pid, and the write's
						// result checked this time.
						if (errno == EPERM || errno == ENOTSUP || errno == EOPNOTSUPP || errno == EXDEV || errno == EMLINK)
							linkable = false;
						else
						{
							::unlink(staging.c_str());
							return false;
						}
					}
				}
				if (!linkable)
				{
					int fd = ::open(mPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
					if (fd >= 0)
					{
						const std::string line = mine + "\n";
						const bool written = ::write(fd, line.data(), line.size()) == (ssize_t) line.size();
						if (::close(fd) != 0 || !written)
						{
							::unlink(mPath.c_str());
							::unlink(staging.c_str());
							return false;
						}
						made = 0;
					}
					else if (errno != EEXIST)
					{
						::unlink(staging.c_str());
						return false;
					}
				}
				if (made == 0)
				{
					::unlink(staging.c_str());
					mHeld = true;
					return true;
				}

				// Held -- or was. Something that is not a file where the lock
				// goes is nobody's lock and never will be: nothing here or in
				// the shell removes a directory, so there is nothing to wait
				// for.
				struct stat st;
				if (::lstat(mPath.c_str(), &st) == 0 && !S_ISREG(st.st_mode))
				{
					::unlink(staging.c_str());
					return false;
				}

				// Read who by, and ask the kernel.
				bool ok = false;
				const std::string holder = lockHolder(readText(mPath, &ok));
				if (!ok)
				{
					if (expired())
						return false;
					// Released between the create and the read: try again at
					// once. Anything else that stops the read -- a lock this
					// process may not read -- is tried again after a pause,
					// until the budget is spent.
					if (::access(mPath.c_str(), F_OK) == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}

				bool numeric = !holder.empty();
				for (char c : holder)
					if (c < '0' || c > '9')
						numeric = false;

				bool stale;
				if (!numeric)
				{
					// Empty, or not a pid: nobody can be holding it. An
					// empty file is also what the shell's own create looks
					// like between its open and its write, for a few
					// microseconds -- so give it a moment before deciding,
					// and re-read before removing (below). This process's
					// own locks are never empty (above).
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					stale = true;
				}
				else
				{
					const pid_t pid = (pid_t) atol(holder.c_str());
					// kill 0 asks without signalling. ESRCH is the one
					// answer that means gone; EPERM is a live process this
					// one may not signal, which is still a live holder.
					stale = pid > 0 && ::kill(pid, 0) != 0 && errno == ESRCH;
				}

				if (stale)
				{
					// Only if it still says what was read above, and with no
					// other waiter between that look and the remove: two
					// waiters that both judged one dead holder's lock stale
					// used to be able to remove each other's new one -- the
					// first removed it and took it, and the second's remove,
					// two steps after its re-read, took the first's new lock
					// with it (PL-041).
					const bool reaped = removeIfStill(mPath, holder, started + std::chrono::milliseconds(timeoutMs));
					if (expired())
						return false;
					// Retry at once, there is nothing to wait for -- unless the
					// guard could not be had (G-E1-02), or the remove did not
					// take (a directory that will not let go of the name),
					// which a pause keeps from spinning.
					bool still = false;
					if (!reaped || (lockHolder(readText(mPath, &still)) == holder && still))
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}

				if (expired())
					return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
#endif
		}

		void PidLock::release()
		{
			if (!mHeld)
				return;
			mHeld = false;
#if !defined(_WIN32)
			// Only a lock this process still holds: another process may own
			// the file by now if this one's was removed as stale. A lock that
			// is not there at all is looked for again for a moment: a waiter
			// that moves a lock aside to judge it (the shell's own reaper may)
			// puts a live holder's back within microseconds, and a release
			// that gave up in that instant would leave a lock naming this
			// process -- alive, and never going to release it.
			const std::string mine = std::to_string((long long) ::getpid());
			for (int attempt = 0; attempt < 20; attempt++)
			{
				bool ok = false;
				const std::string holder = lockHolder(readText(mPath, &ok));
				if (ok)
				{
					if (holder == mine)
						::unlink(mPath.c_str());
					return;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
#endif
		}
	}
}
