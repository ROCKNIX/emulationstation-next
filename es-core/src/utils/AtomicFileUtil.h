#pragma once
#ifndef ES_CORE_UTILS_ATOMIC_FILE_UTIL_H
#define ES_CORE_UTILS_ATOMIC_FILE_UTIL_H

#include <functional>
#include <map>
#include <string>

// Whole-file writes that leave either the old file or the new one on disk,
// never a torn one, and the pid lock the shell's settings functions take.
//
// The two configuration files EmulationStation owns -- system.cfg and
// es_settings.cfg -- used to be rewritten in place: truncate, then write.
// A process killed between the two, or a battery that dies there, leaves an
// empty or half-written file, and the boot-time check then replaced the good
// copy with it (fork #102, D-CLOUD-078). Written to a temporary name, synced,
// and renamed into place, the file on disk is always a complete one; the
// directory is synced afterwards so the rename itself survives a power cut.
//
// Every guarantee in this file is POSIX's, and holds on Linux, which is all
// ROCKNIX builds (#307 PL-075). The _WIN32 branches keep upstream's Windows
// build compiling and nothing more: writeText there removes `path` before it
// renames the temporary into place, so a crash between the two leaves no file
// at all; its temporary is the shared `path`.tmp; the lock is never taken
// (acquire answers true); and the mode is not kept.
namespace Utils
{
	namespace AtomicFile
	{
		// Write `text` to `path` through a temporary of this call's own beside
		// it (`path`.tmp.<pid>.<n>, never the shared `path`.tmp the shell's
		// set_setting writes): write, fsync, rename over `path`, fsync the
		// directory. True only once the rename has landed; on any failure the
		// temporary is removed and `path` is untouched. Two calls at once, from
		// two threads or two processes, each leave one whole file (PL-063).
		// With mode -1, a file being replaced keeps its mode, and its owner
		// where this process may set it (#308 F-ES-08), and a new one is made
		// 0644 before umask, as the shell makes files. A mode given is the
		// file's mode whether it is new or replaced: a record written beside
		// a private file passes that file's (modeOf), so the copy is never
		// less private than what it copies.
		bool writeText(const std::string& path, const std::string& text, int mode = -1);

		// The permission bits of the regular file at `path`, or `fallback`
		// when there is none: what a record written beside a file should be
		// made with, so the copy is no less private than the original.
		int modeOf(const std::string& path, int fallback);

		// Read `path` whole. `ok` (when given) says whether it was: opened,
		// read to the end without an error, and -- for a regular file -- no
		// shorter than its size at the open (PL-065). An empty file and a
		// missing one both read ""; only the first is ok. On failure "".
		std::string readText(const std::string& path, bool* ok = nullptr);

		// readText(src) then writeText(dst): dst is replaced whole or not at
		// all, with src's mode. False when src could not be read or dst could
		// not be written.
		bool copy(const std::string& src, const std::string& dst);

		// Whether a key=value configuration text is one worth reading and
		// worth keeping as the last-known-good record: something in it, no
		// NUL bytes (a file cut by a power failure reads as a run of them),
		// and at least one key=value line that is not a comment.
		bool isUsableKeyValues(const std::string& text);

		// Where a configuration's text comes from at load (SystemConf's
		// system.cfg), and whether it is whole enough to become the record.
		struct LoadedConfig
		{
			enum class Source
			{
				Live,        // the file itself
				Temporary,   // `path`.tmp: an unfinished save's whole text, the file cut or unusable
				Backup,      // `path`.backup: the last-known-good record, whole (a cut one is never taken, PL-018)
				Damaged,     // the file, unusable, with nothing better: read as it is, recorded nowhere
				Missing      // nothing could be read at all
			};
			Source source = Source::Missing;
			std::string text;
			bool record = false;   // may replace the last-known-good record
			// The mode to write the chosen text back with, and the record:
			// never less private than any copy of it on disk (G-E1-05).
			int mode = 0644;
		};

		// The choice, from what is on disk now. Reads; writes nothing.
		LoadedConfig chooseConfig(const std::string& path);

		// What the load's write-back of a recovery came to.
		enum class RecoveryWrite
		{
			None,          // nothing to write back: the live file was taken, or nothing could be read
			Written,       // written back as the live file, with the settings lock held
			WriteFailed,   // the lock was held and the write did not land: the file is as it was
			LockBusy       // the lock stayed with a live holder past the budget: nothing written
		};

		// SystemConf's load: chooseConfig, and where it names a recovery --
		// the whole temporary or the whole record, to be written back as the
		// live file -- the choice made again with the settings lock at
		// `lockPath` held, and written back under it with the mode every copy
		// shares (audit of the fix round G2-E-core-02). The write-back used to
		// run without the lock every other writer of the file takes, so a
		// set_setting that landed between the choice and the write was lost
		// under it. The answer is the choice the text was read from. With the
		// lock busy past `timeoutMs` it is the first choice and nothing is
		// written: the next save, under the lock, merges onto the same
		// recovery (saveUnderLock).
		LoadedConfig loadUnderLock(const std::string& path, const std::string& lockPath, int timeoutMs,
			RecoveryWrite* wrote = nullptr);

		// Whether another process holds an flock on `path` now: the cloud
		// scripts' transfer lock, /var/run/cloud_sync.lock, which every
		// cloud_backup, cloud_restore and content run takes for its whole
		// length (PL-068). Asked the way `flock -n <path> true` asks: an
		// exclusive lock tried without waiting and let go at once -- the
		// scripts give a busy lock a second before they give up, so the
		// instant this holds it costs them nothing. No file is nobody's lock;
		// a file that cannot be opened or asked is taken as held, since the
		// callers refuse on held.
		bool isFlockHeld(const std::string& path);

		// A key=value text as SystemConf reads it: comment lines (# or ;)
		// and lines without a key or a value skipped, the last of a repeated
		// key kept.
		std::map<std::string, std::string> parseKeyValues(const std::string& text);

		// A change the interface made and has not saved yet (a save refused
		// for the lock keeps it, PL-024): its value, and what the key held
		// when the change was made -- hadBase false when it held nothing.
		struct PendingChange
		{
			std::string value;
			bool hadBase = false;
			std::string base;
		};

		// Which pending changes a reload of the file keeps (audit of the fixes
		// G-E1-03): each whose key the file still holds as it was when the
		// change was made, which is then made again at the next save. A key
		// the file now holds differently was written by somebody else since --
		// wifictl join's wifi.ssid, a script's set_setting -- and that newer
		// write wins. The answer is the changes to keep, key to value.
		std::map<std::string, std::string> pendingAfterReload(const std::map<std::string, PendingChange>& pending,
			const std::map<std::string, std::string>& reloaded);

		// What a read-modify-write under the settings lock came to.
		enum class LockedSave
		{
			Written,       // read, merged, written whole, with the lock held throughout
			LockBusy,      // the lock stayed with a live holder past the budget: nothing read, nothing written
			Unreadable,    // the lock was held and the file could not be read whole: nothing written
			WriteFailed    // read and merged, and the write did not land: the file is as it was
		};

		// system.cfg's save (SystemConf): take the lock at `lockPath` within
		// `timeoutMs`, read `path`, hand its text to `merge`, write what comes
		// back whole, release. `written`, when given, gets the text that went
		// to disk on Written. Nothing is written without the lock (PL-024):
		// the shell's set_setting reads, writes a temporary and renames it
		// under that lock, and a save made beside it -- the interface's
		// snapshot of the file renamed over the shell's new one, or the other
		// way round -- lost one of the two writers' keys. The caller keeps its
		// changes on any answer but Written and makes them at its next save.
		// A live file that is empty, cut part way through a line or unusable
		// is not merged onto where chooseConfig names a recovery beside it --
		// the whole temporary it is the start of, the whole record: the save
		// merges onto that, with the mode every copy shares (audit of the fix
		// round G2-E-core-03). `baseComplete`, when given, says whether the
		// text merged onto was whole (not empty, ending in a line end): a save
		// merged onto a cut file is written, and must not become the
		// last-known-good record (audit of the fixes G-E1-04).
		LockedSave saveUnderLock(const std::string& path, const std::string& lockPath, int timeoutMs,
			const std::function<std::string(const std::string& current)>& merge, std::string* written = nullptr,
			bool* baseComplete = nullptr);

		// The settings lock the shell takes around every get_setting and
		// set_setting (wait_lock in profile.d/001-functions): a file holding
		// the owner's pid, whose creation fails when it is already there.
		// Same rules, so both sides can wait on each other: a lock whose pid
		// is alive is waited for; one whose pid is gone, or that names no pid,
		// is stale and removed. Released only while it still carries this
		// process's pid.
		//
		// This side's lock is linked into place already holding the pid, so
		// no waiter ever sees it empty (PL-041); and a stale lock is removed
		// under an flock on `path`.reap, re-read first, so two waiters that
		// both judged one dead holder's lock stale cannot remove each other's
		// new one; the shell's wait_lock takes the same flock. The guard is
		// asked without waiting past the budget, and a guard that cannot be
		// had removes nothing (audit of the fixes G-E1-01/02). The shell
		// waits forever; acquire() takes a budget, because it runs on the
		// interface thread.
		class PidLock
		{
		public:
			explicit PidLock(const std::string& path);
			~PidLock();

			// True when the lock is held. False when the budget ran out with
			// a live holder still on it, or when the lock file could not be
			// created at all (its directory missing) -- the caller decides
			// whether to go on without it.
			bool acquire(int timeoutMs);
			void release();
			bool held() const { return mHeld; }

		private:
			std::string mPath;
			bool mHeld;
		};
	}
}

#endif // ES_CORE_UTILS_ATOMIC_FILE_UTIL_H
