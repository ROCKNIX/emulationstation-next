#ifndef EMULATIONSTATION_ALL_SYSTEMCONF_H
#define EMULATIONSTATION_ALL_SYSTEMCONF_H


#include <string>
#include <map>
#include <set>
#include <utility>

class SystemConf 
{
public:
	static SystemConf* getInstance();
	
	static bool getIncrementalSaveStates();
	static bool getIncrementalSaveStatesUseCurrentSlot();

	// Read system.cfg (again). keepPending keeps the changes a save has not
	// made yet -- one the lock refused (PL-024) -- except where the file now
	// holds a key differently than when it was changed (audit of the fixes
	// G-E1-03, Utils::AtomicFile::pendingAfterReload): the Wi-Fi picker's
	// reload after wifictl join wrote the network. A reload that reads
	// nothing keeps them all and answers false (audit of the fix round,
	// G2-E-core-06). Without it every change is dropped and the file's
	// values stand, which is what a reload after a settings restore or a
	// factory reset is for.
    bool loadSystemConf(bool keepPending = false);
    bool saveSystemConf();

	// True when this start found system.cfg missing, empty or damaged and
	// loaded its last-known-good record (system.cfg.backup) in its place,
	// writing that back as the live file. main() tells the player once, after
	// the interface is up (D-CLOUD-079).
	static bool wasRecovered() { return sRecovered; }

    std::string get(const std::string &name);
    bool set(const std::string &name, const std::string &value);

	bool getBool(const std::string &name, bool defaultValue = false);
	bool setBool(const std::string &name, bool value);

private:
	SystemConf();
	static SystemConf* sInstance;
	static bool sRecovered;

	// The shell's settings lock (wait_lock in profile.d/001-functions) and
	// how long the interface waits for it. Members rather than literals so
	// the unit tests can point them at a scratch directory and a short
	// budget; nothing else sets them.
	static std::string sLockPath;
	static int sLockBudgetMs;
	// es-app/tests/unit/SystemConfTests.cpp: a fresh instance per case,
	// against real files in a scratch directory (the audit of the fix round).
	friend struct SystemConfTestAccess;

	// Parse key=value lines into confMap. Comments and blank lines skipped,
	// as the file has always been read.
	void parseSystemConf(const std::string& text);
	// The last-known-good record beside the live file: written only from
	// text that has just been read whole and found well-formed, or that this
	// process has just written -- never from whatever happens to be on disk.
	// Made with the live file's mode, or with `recoveredMode` when a
	// recovery gives one (the mode every copy on disk shares).
	void recordLastGood(const std::string& text, int recoveredMode = -1);
	// changedConf's keys applied to the file's text as read under the lock.
	std::string applyChanges(const std::string& current);

	// The file as read, and the load itself (loadSystemConf wraps it).
	bool loadFromDisk();
	// The last loadFromDisk read a live file the check found unusable, with
	// no whole record and no whole temporary (Source::Damaged): it parsed
	// what it could and returned true, so a reload that keeps its pending
	// changes treats it as nothing read (the audit of the fixes, claude
	// G3-E-06 -- every change looked removed by somebody).
	bool mLoadDamaged = false;

	std::map<std::string, std::string> confMap;
	std::set<std::string> changedConf;
	// For each key in changedConf: what it held when it was first changed
	// since the last save (false: nothing), for a reload to tell the player's
	// change from somebody else's newer write (G-E1-03).
	std::map<std::string, std::pair<bool, std::string>> mPendingBase;
	// The values of the text the last load parsed or the last save wrote,
	// alone -- confMap also carries keys the file no longer has. A change's
	// base comes from here (G2-E-core-07).
	std::map<std::string, std::string> mOnDisk;


	std::string mSystemConfFile;
};


#endif //EMULATIONSTATION_ALL_SYSTEMCONF_H
