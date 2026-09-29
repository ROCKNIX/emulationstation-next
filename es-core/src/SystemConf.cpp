#include "SystemConf.h"
#include <iostream>
#include <fstream>
#include "Log.h"
#include "utils/StringUtil.h"
#include "utils/FileSystemUtil.h"
#include "Settings.h"
#include "Paths.h"
#include "utils/AtomicFileUtil.h"

#include <cstdio>
#include <set>
#include <regex>
#include <string>
#include <sstream>
#include <iostream>
#include <SDL_timer.h>

static std::string mapSettingsName(const std::string& name)
{
	if (name == "system.language")
		return "Language";

	return name;
}

SystemConf *SystemConf::sInstance = NULL;
bool SystemConf::sRecovered = false;
std::string SystemConf::sLockPath = "/tmp/.system.cfg.lock";
int SystemConf::sLockBudgetMs = 5000;

static std::set<std::string> dontRemoveValue
{
	{ "audio.device" },
	{ "updates.branch" }
};

static std::map<std::string, std::string> defaults =
{
	{ "kodi.enabled", "1" },
	{ "kodi.atstartup", "0" },
	{ "audio.bgmusic", "1" },
	{ "wifi.enabled", "0" },
	{ "system.hostname", "BATOCERA" }, // batocera
	{ "global.retroachievements", "0" },
	{ "global.retroachievements.hardcore", "0" },
	{ "global.retroachievements.leaderboards", "0" },
	{ "global.retroachievements.verbose", "0" },
	{ "global.retroachievements.screenshot", "0" },
	{ "global.retroachievements.username", "" },
	{ "global.retroachievements.password", "" },
	{ "global.netplay_public_announce", "1" },
	{ "global.ai_service_enabled", "0" },
};

SystemConf::SystemConf() 
{
	mSystemConfFile = Paths::getSystemConfFilePath();
	if (mSystemConfFile.empty())
		return;

	loadSystemConf();	
}

SystemConf *SystemConf::getInstance() 
{
    if (sInstance == NULL)
        sInstance = new SystemConf();

    return sInstance;
}

void SystemConf::parseSystemConf(const std::string& text)
{
	// The same reading as ever (Utils::AtomicFile::parseKeyValues), kept
	// alone as well for a reload's comparison (G-E1-03).
	mOnDisk = Utils::AtomicFile::parseKeyValues(text);
	for (const auto& kv : mOnDisk)
		confMap[kv.first] = kv.second;
}

// One record at rest, under the name the boot scripts already use
// (system.cfg.backup, D-CLOUD-079). Written whole through a temporary and a
// rename, like the live file, and only when it would change -- this runs at
// every start and after every save, and most of those change nothing.
void SystemConf::recordLastGood(const std::string& text, int recoveredMode)
{
	// No less private than the live file it copies (#308 F-ES-08): a record
	// an earlier build made 0644 beside a 0600 file is rewritten for its mode
	// even when its text is the same. A recovery passes the mode every copy
	// on disk shares (chooseConfig, G-E1-05): the live file may be the copy
	// that could not be written back, and its mode -- or 0644 where there
	// is no file -- made a 0600 temporary's text a 0644 record (audit of the
	// fix round, gpt G2-E-core-05).
	const std::string backup = mSystemConfFile + ".backup";
	const int mode = recoveredMode >= 0 ? recoveredMode : Utils::AtomicFile::modeOf(mSystemConfFile, 0644);
	if (Utils::AtomicFile::readText(backup) == text && Utils::AtomicFile::modeOf(backup, mode) == mode)
		return;
	if (!Utils::AtomicFile::writeText(backup, text, mode))
		LOG(LogWarning) << "Unable to write the last-known-good record " << backup;
}

// Read the live file if it is usable; else an unfinished save's whole
// temporary where the live file was cut (PL-064); else the last-known-good
// record, and put that back as the live file; else whatever is there, so a
// device whose file fails this check but was fine for years reads exactly as
// before, and nothing is written over it (D-CLOUD-078: never defaults over a
// file that failed to parse before the recovery was tried). Which one is
// Utils::AtomicFile::chooseConfig's call; the writes and the words are here.
bool SystemConf::loadSystemConf(bool keepPending)
{
	if (mSystemConfFile.empty())
		return true;

	// The changes not saved yet, with what each key held when it was
	// changed (G-E1-03).
	std::map<std::string, Utils::AtomicFile::PendingChange> pending;
	if (keepPending)
	{
		for (const auto& key : changedConf)
		{
			Utils::AtomicFile::PendingChange change;
			change.value = confMap[key];
			const auto base = mPendingBase.find(key);
			if (base != mPendingBase.cend())
			{
				change.hadBase = base->second.first;
				change.base = base->second.second;
			}
			pending[key] = change;
		}
	}
	const auto bases = mPendingBase;
	const std::set<std::string> unsaved = changedConf;
	const auto onDiskBefore = mOnDisk;

	changedConf.clear();
	mPendingBase.clear();
	mOnDisk.clear();
	mLoadDamaged = false;
	const bool loaded = loadFromDisk();

	// Nothing read is no evidence about any key (audit of the fix round,
	// gpt G2-E-core-06): the empty reading made every change to a key the
	// file held look removed by somebody since, and it was dropped from the
	// pending set and from memory before the false came back. The changes
	// stay, with their bases and the last reading of the file, for the
	// next save or the next reload that reads something.
	// A damaged reading is no evidence either (claude G3-E-06): its parse has
	// no usable line, and the comparison below would drop every change.
	if ((!loaded || mLoadDamaged) && keepPending)
	{
		changedConf = unsaved;
		mPendingBase = bases;
		mOnDisk = onDiskBefore;
		if (!unsaved.empty())
			LOG(LogWarning) << "loadSystemConf: " << mSystemConfFile << (mLoadDamaged ? " was read with no usable line" : " could not be read") << " -- the " << unsaved.size()
				<< " change(s) not saved yet are kept for the next save";
		return false;
	}

	const auto kept = Utils::AtomicFile::pendingAfterReload(pending, mOnDisk);
	// A change dropped here must not linger in memory where the file has no
	// such key: the parse only adds, so the interface would go on showing a
	// value nothing will ever save.
	for (const auto& key : unsaved)
		if (kept.find(key) == kept.cend() && mOnDisk.find(key) == mOnDisk.cend())
			confMap.erase(key);

	if (!pending.empty())
	{
		for (const auto& change : pending)
			if (kept.find(change.first) == kept.cend())
				LOG(LogInfo) << "loadSystemConf: " << change.first << " was changed in " << mSystemConfFile
					<< " since this interface changed it -- the file's value stands";
		for (const auto& kv : kept)
		{
			confMap[kv.first] = kv.second;
			changedConf.insert(kv.first);
			const auto base = bases.find(kv.first);
			mPendingBase[kv.first] = base != bases.cend() ? base->second : std::make_pair(false, std::string());
		}
		if (!kept.empty())
			LOG(LogInfo) << "loadSystemConf: " << kept.size() << " change(s) not saved yet are kept for the next save";
	}
	return loaded;
}

bool SystemConf::loadFromDisk()
{

	// The record's own temporary, from a build before PL-063 gave every
	// write a temporary of its own, is litter: nothing else writes that name.
	// system.cfg.tmp is not removed any more (PL-064): it is read below, and
	// it is also the name the shell's set_setting writes and renames -- a
	// start that deleted it under a script's write made the script's rename
	// fail and its key vanish. chksysconfig sweeps it at boot, before any
	// script runs.
	std::remove((mSystemConfFile + ".backup.tmp").c_str());

	// The choice, and a recovery's write-back under the settings lock the
	// save and the shell's set_setting take (audit of the fix round, gpt
	// G2-E-core-02, claude G2-E-core-07): the write-back used to run without
	// it, and a script's write that landed between the choice and the write
	// was lost under it.
	Utils::AtomicFile::RecoveryWrite wrote = Utils::AtomicFile::RecoveryWrite::None;
	const Utils::AtomicFile::LoadedConfig chosen = Utils::AtomicFile::loadUnderLock(mSystemConfFile, sLockPath, sLockBudgetMs, &wrote);
	// No less private than any copy it came from (G-E1-05).
	const int mode = chosen.mode;
	const auto reportWriteBack = [this, wrote](const char* from)
	{
		if (wrote == Utils::AtomicFile::RecoveryWrite::WriteFailed)
			LOG(LogError) << "Unable to write " << mSystemConfFile << " back from " << from;
		else if (wrote == Utils::AtomicFile::RecoveryWrite::LockBusy)
			LOG(LogWarning) << "loadFromDisk: the settings lock was not free within " << sLockBudgetMs << " ms; "
				<< mSystemConfFile << " is read from " << from << " and not written back -- the next save writes it";
	};

	switch (chosen.source)
	{
	case Utils::AtomicFile::LoadedConfig::Source::Live:
		parseSystemConf(chosen.text);
		// "Usable" for the record means complete too: a file that stops part
		// way through a line is read -- nothing better is there -- but never
		// replaces a whole record (PL-064, PL-065).
		if (chosen.record)
			recordLastGood(chosen.text);
		else
			LOG(LogWarning) << mSystemConfFile << " does not end in a line end -- read as it is, and not recorded as the last known good";
		return true;

	case Utils::AtomicFile::LoadedConfig::Source::Temporary:
		LOG(LogWarning) << mSystemConfFile << " is cut short or unreadable beside a whole " << mSystemConfFile
			<< ".tmp that an interrupted save left -- loading that and writing it back";
		parseSystemConf(chosen.text);
		reportWriteBack("its .tmp");
		recordLastGood(chosen.text, mode);
		sRecovered = true;
		return true;

	case Utils::AtomicFile::LoadedConfig::Source::Backup:
		LOG(LogWarning) << mSystemConfFile << " is missing, empty, unreadable, damaged or cut short of the record -- loading the last-known-good record "
			<< mSystemConfFile << ".backup and writing it back";
		parseSystemConf(chosen.text);
		reportWriteBack("its last-known-good record");
		// The same text: only the record's mode can change, to the one every
		// copy shares, which the live file is written with (claude
		// G2-E-core-04 b: a record an earlier build made 0644 stayed so beside
		// the 0600 file until the next save).
		recordLastGood(chosen.text, mode);
		sRecovered = true;
		return true;

	case Utils::AtomicFile::LoadedConfig::Source::Damaged:
		// Unusable by the check, with no whole record and no whole
		// temporary (a record cut short is not taken, PL-018). Read the live file as it always was read -- a file this
		// check is wrong about still works -- and record nothing: there is no
		// good copy to record.
		LOG(LogError) << mSystemConfFile << " has no usable key=value line and no whole last-known-good record; reading it as it is";
		parseSystemConf(chosen.text);
		mLoadDamaged = true;
		return true;

	case Utils::AtomicFile::LoadedConfig::Source::Missing:
		break;
	}

	LOG(LogError) << "Unable to open " << mSystemConfFile;
	return false;
}

bool SystemConf::saveSystemConf()
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->saveFile();	

	if (changedConf.empty())
		return false;

	// The shell's settings lock (wait_lock in profile.d/001-functions), held
	// across the read-modify-write: set_setting reads the file, writes a
	// temporary and renames it under the same lock, and a save that read or
	// renamed beside it put one writer's snapshot over the other's. Five
	// seconds is the budget, on the interface thread. A live holder still on
	// the lock past it is logged and nothing is written (PL-024): this used to
	// save anyway, and lost either the script's key or the player's. The
	// changes stay in changedConf, and the next save -- any page closing,
	// any set-and-save -- writes them over whatever the script left.
	std::string out;
	bool baseWhole = true;
	const Utils::AtomicFile::LockedSave saved = Utils::AtomicFile::saveUnderLock(mSystemConfFile, sLockPath, sLockBudgetMs,
		[this](const std::string& current) { return applyChanges(current); }, &out, &baseWhole);

	switch (saved)
	{
	case Utils::AtomicFile::LockedSave::LockBusy:
		LOG(LogWarning) << "saveSystemConf: the settings lock was not free within 5s; nothing written -- the changes are kept for the next save";
		return false;
	case Utils::AtomicFile::LockedSave::Unreadable:
		LOG(LogError) << "Unable to open for saving :  " << mSystemConfFile << " -- the changes are kept for the next save";
		return false;
	case Utils::AtomicFile::LockedSave::WriteFailed:
		// Written to a temporary of its own, synced, and renamed over the
		// live file (D-CLOUD-079, PL-063); a failure leaves the file whole.
		LOG(LogError) << "Unable to write " << mSystemConfFile << " -- the changes are kept for the next save";
		return false;
	case Utils::AtomicFile::LockedSave::Written:
		break;
	}

	changedConf.clear();
	mPendingBase.clear();
	// The file is now what this save wrote: the base the next change is
	// compared with (G2-E-core-07).
	mOnDisk = Utils::AtomicFile::parseKeyValues(out);

	// What was just written is, by construction, the newest good state, so
	// it becomes the record (D-CLOUD-078: a success becomes the last known
	// good). Outside the lock: the shell never takes it for the record.
	// Unless it was merged onto a file cut short: the record then keeps the
	// keys the cut lost (audit of the fixes G-E1-04).
	if (baseWhole)
		recordLastGood(out);
	else
		LOG(LogWarning) << "saveSystemConf: " << mSystemConfFile << " was cut short when this save read it -- written, and not recorded as the last known good";

	return true;
}

// This save's keys, applied to the file as it is on disk now (read under
// the lock): a key already there is rewritten in place, or removed when it
// went back to its default or to nothing; a new one goes at the end. Every
// other line is left exactly as the file had it -- the shell's writes
// included.
std::string SystemConf::applyChanges(const std::string& current)
{
	/* Read all lines in a vector */
	std::vector<std::string> fileLines;
	std::string line;

	std::istringstream filein(current);
	while (std::getline(filein, line))
		fileLines.push_back(line);

	static std::string removeID = "$^�(p$^mpv$�rpver$^vper$vper$^vper$vper$vper$^vperv^pervncvizn";

	int lastTime = SDL_GetTicks();

	/* Save new value if exists */
	for (auto& it : changedConf)
	{
		std::string key = it + "=";
		char key0 = key[0];

		bool lineFound = false;

		for (auto& currentLine : fileLines)
		{
			if (currentLine.size() < 3)
				continue;

			char fc = currentLine[0];
			if (fc != key0 && currentLine[1] != key0)
				continue;

			int idx = currentLine.find(key);
			if (idx == std::string::npos)
				continue;

			if (idx == 0 || (idx == 1 && (fc == ';' || fc == '#')))
			{
				std::string val = confMap[it];
				if ((!val.empty() && val != "auto" && val != "default") || dontRemoveValue.find(it) != dontRemoveValue.cend())
				{
					auto defaultValue = defaults.find(key);
					if (defaultValue != defaults.cend() && defaultValue->second == val)
						currentLine = removeID;
					else
						currentLine = key + val;
				}
				else 
					currentLine = removeID;

				lineFound = true;
			}
		}

		if (!lineFound)
		{
			std::string val = confMap[it];
			if (!val.empty() && val != "auto")
				fileLines.push_back(key + val);
		}
	}

	lastTime = SDL_GetTicks() - lastTime;

	LOG(LogDebug) << "saveSystemConf :  " << lastTime;

	std::string out;
	for (int i = 0; i < fileLines.size(); i++) 
	{
		if (fileLines[i] != removeID)
			out += fileLines[i] + "\n";
	}
	return out;
}

std::string SystemConf::get(const std::string &name) 
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->getString(mapSettingsName(name));
	
	auto it = confMap.find(name);
	if (it != confMap.cend())
		return it->second;

	auto dit = defaults.find(name);
	if (dit != defaults.cend())
		return dit->second;

    return "";
}

bool SystemConf::set(const std::string &name, const std::string &value) 
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->setString(mapSettingsName(name), value == "auto" ? "" : value);

	if (confMap.count(name) == 0 || confMap[name] != value)
	{
		// What the key held before this run of changes, once: a reload
		// compares it with the file (G-E1-03). Held in the file, as last
		// read or written -- not in confMap, which keeps keys the file no
		// longer has: one another writer removed, one the last save wrote
		// away. The reload found such a key missing where the base said it
		// was, and dropped the change as removed by somebody since (audit of
		// the fix round, gpt G2-E-core-07, claude G2-E-core-09).
		if (changedConf.find(name) == changedConf.cend())
		{
			const auto it = mOnDisk.find(name);
			mPendingBase[name] = it == mOnDisk.cend() ? std::make_pair(false, std::string()) : std::make_pair(true, it->second);
		}
		confMap[name] = value;
		changedConf.insert(name);
		return true;
	}

	return false;
}

bool SystemConf::getBool(const std::string &name, bool defaultValue)
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->getBool(mapSettingsName(name));

	if (defaultValue)
		return get(name) != "0";

	return get(name) == "1";
}

bool SystemConf::setBool(const std::string &name, bool value)
{
	if (mSystemConfFile.empty())
		return Settings::getInstance()->setBool(mapSettingsName(name), value);

	return set(name, value  ? "1" : "0");
}

bool SystemConf::getIncrementalSaveStates()
{
	auto valGSS = SystemConf::getInstance()->get("global.incrementalsavestates");
	return valGSS != "0" && valGSS != "2";
}

bool SystemConf::getIncrementalSaveStatesUseCurrentSlot()
{
	return SystemConf::getInstance()->get("global.incrementalsavestates") == "2";
}
