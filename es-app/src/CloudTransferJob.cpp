#include "CloudTransferJob.h"

#include "CloudExit.h"
#include "CloudText.h"
#include "ThreadedCloudSync.h"
#include "LocaleES.h"
#include "Log.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <signal.h>
#include <sys/wait.h>

std::mutex CloudTransferJob::sMutex;
std::function<void()> CloudTransferJob::testPauseInStop;
std::shared_ptr<CloudTransferJob> CloudTransferJob::sCurrent;

// The run's starting state: every counter at its starting value. Once, at
// construction -- a TRY AGAIN is a new run of the same command, not this
// one reset under a page that might still be reading it.
CloudTransferJob::CloudTransferJob(const std::string& command, const std::string& title, int itemsExpected, int itemsAfterContent)
	: mCommand(command), mTitle(title),
	  mItemsExpected(itemsExpected > 0 ? itemsExpected : 0), mItemsAfterContent(itemsAfterContent > 0 ? itemsAfterContent : 0),
	  mStarted(std::chrono::steady_clock::now())
{
	mItemIndex = 0; mItemCount = mItemsExpected; mTrailing = mItemsAfterContent; mScriptBase = 0; mLastScriptIndex = 0;
	mUnitBytes = 0; mUnitFiles = 0; mRunBytes = 0; mRunFiles = 0; mRunSized = false;
	mRemovedFiles = 0; mRemovedBytes = 0; mAnyTransferred = false;
	mFilesThisBlock = 0; mSeenBlock = false;
	mChecksDone = 0; mChecksTotal = 0; mListed = 0; mChecksThisBlock = 0;
	mBytePercent = -1; mFilePercent = -1; mCheckPercent = -1; mPercent = -1;
	mFinished = false; mExit = -1;
	mElapsedMs = 0; mFinishedAt = 0;
	mStartedAt = time(nullptr);
}

std::shared_ptr<CloudTransferJob> CloudTransferJob::current()
{
	std::unique_lock<std::mutex> lock(sMutex);
	return sCurrent;
}

bool CloudTransferJob::running()
{
	auto job = current();
	return job != nullptr && !job->finished();
}

std::shared_ptr<CloudTransferJob> CloudTransferJob::start(const std::string& command, const std::string& title,
	int itemsExpected, int itemsAfterContent)
{
	std::unique_lock<std::mutex> lock(sMutex);
	if (sCurrent != nullptr && !sCurrent->finished())
		return sCurrent;

	std::shared_ptr<CloudTransferJob> job(new CloudTransferJob(command, title, itemsExpected, itemsAfterContent));
	sCurrent = job;
	// Detached, and holding its own reference: the run ends when the command
	// does, not when a page closes -- the scan job's shape.
	std::thread([job] { job->run(); }).detach();
	return job;
}

// Only the run that was shown: a TRY AGAIN may have made the next run
// current between the outcome and the press that dismissed it, and that
// one is still somebody's to see.
void CloudTransferJob::dismiss(const std::shared_ptr<CloudTransferJob>& job)
{
	std::unique_lock<std::mutex> lock(sMutex);
	if (job != nullptr && sCurrent == job && job->finished())
		sCurrent = nullptr;
}

// The look and the mark are one step (audit of the fixes, E2 gpt
// G-E2-05): under the run's own lock, which its end holds while it
// decides whether it was stopped and says it has finished. A stop that
// looked, found the run going, and was descheduled while it finished on
// its own used to mark a completed run stopped after the end had cleared
// the flags; now the end waits for the mark, sees it with the command's
// own 0, and says COMPLETED. The signal is sent under the lock too:
// deliverStop takes nothing, and a kill does not block.
bool CloudTransferJob::stopForLaunch(bool hard)
{
	auto job = current();
	if (job == nullptr)
		return false;
	pid_t pid = 0;
	{
		std::unique_lock<std::mutex> lock(job->mMutex);
		if (job->mFinished)
			return false;
		if (testPauseInStop)
			testPauseInStop();
		job->mStoppedForGame = true;
		job->requestStop(hard ? SIGKILL : SIGTERM);
		pid = job->mPid;
	}
	LOG(LogInfo) << "CloudTransferJob: stopped for a game (" << (hard ? "SIGKILL" : "SIGTERM") << ", group " << pid
		<< (pid > 0 ? ")" : "; sent when the run says its pid)");
	return true;
}

bool CloudTransferJob::stopByPlayer()
{
	auto job = current();
	if (job == nullptr)
		return false;
	pid_t pid = 0;
	{
		std::unique_lock<std::mutex> lock(job->mMutex);
		if (job->mFinished)
			return false;
		if (testPauseInStop)
			testPauseInStop();
		job->mStoppedByPlayer = true;
		job->requestStop(SIGTERM);
		pid = job->mPid;
	}
	LOG(LogInfo) << "CloudTransferJob: cancelled by the player (SIGTERM, group " << pid
		<< (pid > 0 ? ")" : "; sent when the run says its pid)");
	return true;
}

// A stop is a request the run keeps, not a signal sent once (#308
// 5-cloud-sync-and-saves gpt F-CS-24, claude F-CS-26). The run's pid
// arrives on its first line; a stop that came before it used to set the
// flag and signal nothing, and the command ran to its end under a page
// that then called it stopped. Now the stop and the reader both call
// deliverStop, and whichever sees the request and the pid together sends
// it: the stop sets the request before it reads the pid, the reader the
// pid before it reads the request, so at least one of them sees both.
// A SIGKILL replaces a SIGTERM that has not got through; nothing replaces
// a SIGKILL.
void CloudTransferJob::requestStop(int sig)
{
	if (sig == SIGKILL)
		mStopSignal = SIGKILL;
	else
	{
		int none = 0;
		mStopSignal.compare_exchange_strong(none, sig);
	}
	deliverStop();
}

// Each signal once: the stop and the reader can both arrive here with the
// same request, and the second finds it sent.
void CloudTransferJob::deliverStop()
{
	const int sig = mStopSignal;
	const pid_t pid = mPid;
	if (sig == 0 || pid <= 0)
		return;
	if (mSignalSent.exchange(sig) == sig)
		return;
	::kill(-pid, sig);
}

bool CloudTransferJob::finished() const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mFinished;
}

time_t CloudTransferJob::finishedAt() const
{
	std::unique_lock<std::mutex> lock(mMutex);
	return mFinished ? mFinishedAt : 0;
}

int CloudTransferJob::elapsedMsLocked() const
{
	if (mFinished)
		return mElapsedMs;
	return (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mStarted).count();
}

// The unit's last "Transferred:" pair becomes the run's. Called with mMutex
// held: at the next ">>> unit", when the command exits, and when the byte
// counter drops -- rclone's never does within one invocation, so a drop
// means a second one started inside the unit, and what the first moved is
// banked before its numbers are replaced. A drop of the count line folds
// the files alone, in handleLine: the byte line of the same block comes
// first and has settled the bytes by then, and folding both again here
// banked the new rclone's first bytes twice whenever the old one had moved
// nothing but empty files (review, 2026-09-08).
void CloudTransferJob::foldUnit()
{
	mRunBytes += mUnitBytes;
	mRunFiles += mUnitFiles;
	mUnitBytes = 0;
	mUnitFiles = 0;
}

void CloudTransferJob::handleLine(const std::string& line)
{
	std::unique_lock<std::mutex> lock(mMutex);

	// "Transferred:   \t 1.4 GiB / 2.0 GiB, 70%, 2.5 MiB/s, ETA 3m2s"
	//
	// It appears twice per block: once for bytes, once for the file count
	// ("0 / 6, 0%"). The byte one is the one carrying a unit, which is also
	// the one somebody wants -- a count of files says nothing about how long
	// this will take when the files are a save game and a disc image.
	//
	// The scripts talk to this page through the ">>> " markers on stdout:
	//
	//   ">>> unit <label>|<i>|<n>" -- an item starts: a system ("nes|2|5")
	//     or a phase ("SAVES||", "SETTINGS||"). Everything per-block is reset
	//     with it; the label survives. The run numbers items across the
	//     whole of itself (the page's row 2, ITEM i OF n; D-UI-026), because one run
	//     chains several scripts and each counts only its own units: a label
	//     that differs from the current one is the next item, the same label
	//     again is a re-announcement and does not advance (ES announces
	//     SETTINGS before backuptool runs, then cloud_backup announces it
	//     again). The script's own i|n are read for one thing: an
	//     announcement carrying n says how many units that script has, so
	//     n = the items counted before that script's first announcement
	//     + its n + the single-item phases ES chained after it (mTrailing).
	//     Until a script says, n is ES's estimate from the constructor; 0 is
	//     unknown and row 2 reads ITEM i alone. i never exceeds n on the
	//     page. A script whose i starts over, or that carries a count after
	//     one that did not, is a new script.
	//   ">>> doing <keyword>" -- what the item is busy with while rclone is
	//     not running yet. "archive": backuptool is writing the settings
	//     archive (its own output is discarded), and row 3 says so until the
	//     next per-file, checks or totals line, or the next unit. "unpack":
	//     the same tool putting one back. Any other keyword is a newer
	//     script's and is ignored rather than shown raw.
	//   ">>> removed <files>|<bytes>|<per-system>" -- a match's summary, for
	//     the done page (below). A match cut off by the network prints it
	//     before exiting 69, so the page can say what had already gone.
	//   ">>> why <sentence>" -- a script saying, at the point of failure and
	//     in the player's words, what went wrong (D-UI-028). Attached to the
	//     unit it arrived under; before any unit, to the tier that reports
	//     next. The last one is the run's why for a command with no tiers.
	//   ">>> tier <label>|<rc>" -- GuiMenu's run composition reporting each
	//     of its parts as it ends (SAVES, ROMS AND BIOS, SETTINGS), so the
	//     done page can say which finished and which did not.
	//   ">>> offer <name>|<arg>|..." -- a question the script wants put to
	//     the player, and cannot put itself. Kept until the page is
	//     dismissed and raised then (input); the words are CloudOffer's,
	//     shared with the card.
	//
	// Which line is which is CloudText::classifyProtocolLine -- the one
	// parser both cloud surfaces read this protocol with -- and what each
	// kind does to this page stays here.
	//
	// It used to be a second parser living in this function, which knew
	// ">>> unit" and ">>> removed" and had never heard of ">>> offer": so
	// the empty-cloud question (#100, #127) was put to the player by the
	// card and not by this page, on the very route a freshly set-up
	// handheld takes (#145).
	//
	// Nothing carrying ">>> " is rclone's, so nothing carrying ">>> "
	// reaches the rclone parsers below -- a marker newer than this build
	// included. It is dropped deliberately here rather than left to fall
	// through to matchers that were never asked about it.
	const CloudText::ProtocolLine protocol = CloudText::classifyProtocolLine(line);
	if (protocol.kind != CloudText::ProtocolKind::NotProtocol)
	{
		switch (protocol.kind)
		{
		case CloudText::ProtocolKind::Pid:
			// run()'s own first line: the group a stop signals -- and a
			// stop that came before it is sent now (requestStop).
			mPid = protocol.number;
			if (mStopSignal != 0)
				LOG(LogInfo) << "CloudTransferJob: the run said its pid (" << protocol.number << "); sending the stop that came before it";
			deliverStop();
			break;
		case CloudText::ProtocolKind::Why:
		{
			// An empty why is a why line with nothing in it: the one this
			// page already had stands, and no item is blamed for it.
			if (protocol.text.empty())
				break;
			mWhy = protocol.text;
			mPendingWhys.push_back({ Utils::String::toUpper(mUnitLabel), protocol.text });
			break;
		}
		case CloudText::ProtocolKind::Tier:
		{
			Tier t;
			t.label = protocol.text;
			t.rc = protocol.number;
			t.unitsAnnounced = (int) mUnitsSinceTier.size();
			t.unitsFailed = 0;
			t.tierLevelFail = false;
			// A tier with no label is no tier: nothing is recorded, and the
			// whys waiting under it stay waiting for one that has a name.
			if (t.label.empty())
				break;
			const bool ok = t.rc == 0 || t.rc == 9;
			if (!ok)
			{
				std::vector<std::string> named;
				for (auto& w : mPendingWhys)
				{
					// A why printed before any unit is the tier's own; the tier
					// as a whole is the item that did not finish.
					const std::string label = w.label.empty() ? t.label : w.label;
					if (w.label.empty())
						t.tierLevelFail = true;
					else if (std::find(named.begin(), named.end(), w.label) == named.end())
						named.push_back(w.label);
					// The last why for an item wins; the scripts may say more
					// than one thing about the same unit as they give up on it.
					bool seen = false;
					for (auto& f : mFailed)
						if (f.label == label) { f.why = w.why; seen = true; }
					if (!seen)
						mFailed.push_back({ label, w.why });
				}
				t.unitsFailed = (int) named.size();
				// A part that failed without a word: the part is the item, and
				// the code supplies the why. Its units are not presumed to have
				// finished, because nothing said they did.
				if (mPendingWhys.empty())
				{
					t.tierLevelFail = true;
					mFailed.push_back({ t.label, ThreadedCloudSync::whyForCode(t.rc) });
				}
			}
			mTiers.push_back(t);
			mPendingWhys.clear();
			mUnitsSinceTier.clear();
			break;
		}
		case CloudText::ProtocolKind::Removed:
		{
			mRemovedFiles = protocol.files;
			mRemovedBytes = protocol.bytes;
			mRemovedDetail.clear();
			for (auto& sys : protocol.systems)
			{
				std::string d = sys.system + " " + sys.files + " " + std::string(sys.files == "1" ? _("FILE") : _("FILES"));
				if (sys.bytes > 0)
					d += " · " + CloudText::sizeLabel((unsigned long) sys.bytes);
				mRemovedDetail.push_back(d);
			}
			break;
		}
		case CloudText::ProtocolKind::Unit:
		{
			foldUnit();   // the unit that just ended: its last totals are the run's now
			const std::string label = protocol.text;
			const int scriptIndex   = protocol.number;
			const int scriptCount   = protocol.count;
			// The next item, unless it is the current one announced again. The
			// first announcement is an item whatever its label says.
			if (mItemIndex == 0 || label != mUnitLabel)
			{
				if (scriptCount > 0 && (mLastScriptIndex == 0 || scriptIndex <= mLastScriptIndex))
					mScriptBase = mItemIndex;   // a script's first announcement: what came before it is its base
				mItemIndex++;
				mUnitsSinceTier.push_back(Utils::String::toUpper(label));
			}
			mLastScriptIndex = scriptCount > 0 ? scriptIndex : 0;
			if (scriptCount > 0)
				mItemCount = mScriptBase + scriptCount + mTrailing;
			// Never ITEM 5 OF 4: a script that announced more than anybody
			// expected grows the count rather than overrun it.
			if (mItemCount > 0 && mItemIndex > mItemCount)
				mItemCount = mItemIndex;
			mUnitLabel = label;
			mDoing.clear();
			mCurrent.clear(); mFileProgress.clear(); mTotals.clear(); mFilesTotals.clear(); mChecking.clear();
			mFilesThisBlock = 0; mChecksThisBlock = 0; mSeenBlock = false;
			mChecksDone = 0; mChecksTotal = 0; mListed = 0;
			mBytePercent = -1; mFilePercent = -1; mCheckPercent = -1; mPercent = -1;
			break;
		}
		case CloudText::ProtocolKind::Doing:
			// Any keyword a newer script prints is ignored rather than shown
			// raw: row 3 says what the item is doing in the player's words,
			// and a word out of a script is not those.
			if (protocol.text == "archive" || protocol.text == "unpack")
				mDoing = protocol.text;
			break;
		case CloudText::ProtocolKind::Offer:
			// A question for the player, kept until the run is over and the
			// outcome has been read: while the page is running, the run is
			// all it has to say (#145, and input() raises it).
			mOffer = protocol.text;
			mOfferArgs = protocol.args;
			break;
		default:
			// Pid is the card's, for the process group it may have to
			// signal; a run of this kind is not cancelled for a launch (D-CLOUD-113).
			// Unknown is a marker newer than this build.
			break;
		}
		return;
	}

	// From here on the line is rclone's, so whatever the item was busy with
	// before rclone ran is over.
	if (line.rfind("Transferred:", 0) == 0)
	{
		std::string body = Utils::String::trim(line.substr(12));
		if (body.find('/') == std::string::npos)
			return;
		mDoing.clear();
		if (body.find("iB") == std::string::npos && body.find(" B") == std::string::npos)
		{
			mFilesTotals = body;   // the count line of the block: "12 / 45, 27%"
			mFilePercent = parsePercent(body);
			refreshPercent();
			// "12 / 45" -- twelve files done so far in this unit; it only
			// grows, so a smaller number is a new rclone inside the unit.
			// Files only -- the bytes were settled by this block's byte line,
			// which comes first (foldUnit).
			const long files = atol(body.c_str());
			if (files < mUnitFiles)
				mRunFiles += mUnitFiles;
			mUnitFiles = files;
			return;
		}

		mTotals = body;
		if (body.rfind("0 B /", 0) != 0)
			mAnyTransferred = true;
		// "80 KiB / 300 KiB" -- what this unit has moved so far, read from the
		// first field. Recorded only when it parsed; a byte line that was
		// seen at all is what lets the done page show any number.
		const long bytes = CloudText::parseBytes(body.substr(0, body.find('/')));
		if (bytes >= 0)
		{
			mRunSized = true;
			if (bytes < mUnitBytes)
				foldUnit();
			mUnitBytes = bytes;
		}
		// A block that carried no per-file line had nothing in flight -- the
		// unit's files are done or being checked -- so the name row does not
		// keep showing a file that finished a block ago. The same for a name
		// caught mid-comparison.
		if (mSeenBlock && mFilesThisBlock == 0)
		{
			mCurrent.clear();
			mFileProgress.clear();
		}
		if (mSeenBlock && mChecksThisBlock == 0)
			mChecking.clear();
		mSeenBlock = true;
		mFilesThisBlock = 0;    // a new block: the next " * " line is the head of it
		mChecksThisBlock = 0;

		// Each percentage is its own line's last word, and a "-" parses to
		// -1, so a line that prints no number clears its own value. The
		// other two are not reset here: rclone prints the Checks and the
		// count lines in every block once it has printed them at all (their
		// counters only grow), so a value left standing is one it is about
		// to overwrite a few microseconds on -- and resetting it made the
		// bar read the bytes alone for that instant, 100% over a run still
		// comparing. The one on screen is left as it is until this block
		// has produced a number: a bar that fell back to the spinner for
		// the instant between two lines would flicker once a second.
		mBytePercent = parsePercent(body);
		refreshPercent();
		return;
	}
	// "Checks:                12 / 45, 27%, Listed 300" -- what rclone has
	// compared rather than moved, and how much of both sides it has listed.
	// Before anything is queued to compare it reads "0 / 0, -, Listed 300"
	// (rclone 1.75 prints the line once checks, their total, or the listing
	// is non-zero), so the listing count is the first sign of life on a run
	// against a large remote. Older rclones end the line at the percentage.
	if (line.rfind("Checks:", 0) == 0)
	{
		std::string body = Utils::String::trim(line.substr(7));
		auto slash = body.find(" / ");
		if (slash == std::string::npos)
			return;
		mDoing.clear();
		mChecksDone  = atol(body.substr(0, slash).c_str());
		mChecksTotal = atol(body.substr(slash + 3).c_str());
		auto listed = body.find("Listed ");
		if (listed != std::string::npos)
			mListed = atol(body.substr(listed + 7).c_str());
		mCheckPercent = parsePercent(body);
		refreshPercent();
		return;
	}

	// " *   Some Game.zip: 45% /2.5Mi, 300Ki/s, 5s" -- one per parallel
	// transfer, four by default. The first names what to show; the rest are
	// counted, because "and 3 more" is the difference between a device that
	// looks stalled on one file and one that is saturating the link.
	if (!line.empty() && line[0] == '*')
	{
		mDoing.clear();
		std::string body = Utils::String::trim(line.substr(1));

		// " *   name: checking" -- a file rclone caught mid-comparison, under
		// its "Checking:" heading. Not a transfer: it is the name for the
		// CHECKING line, and counting it would promise a file that never
		// moves. " *   name: transferring" is one that is queued with no
		// bytes yet, so there is no percentage to split on; the name is
		// real and the progress is empty.
		static const std::string CHECKING = ": checking";
		static const std::string QUEUED   = ": transferring";
		if (body.size() > CHECKING.size() && body.compare(body.size() - CHECKING.size(), CHECKING.size(), CHECKING) == 0)
		{
			mChecking = Utils::FileSystem::getFileName(Utils::String::trim(body.substr(0, body.size() - CHECKING.size())));
			mChecksThisBlock++;
			return;
		}
		if (body.size() > QUEUED.size() && body.compare(body.size() - QUEUED.size(), QUEUED.size(), QUEUED) == 0)
			body = Utils::String::trim(body.substr(0, body.size() - QUEUED.size()));

		if (mFilesThisBlock == 0)
		{
			// "name.zip: 45% /2.5Mi, 300Ki/s, 5s" -- the name and its progress
			// arrive on one line; shown on two, so neither wraps.
			//
			// rclone prints the percentage as %3d, so the separator is ": " at
			// 45% and ":" at 100% ("name.zip:100% /40Mi, 1Mi/s, 0s"). Splitting
			// on ": " put the whole line on the name row at 100%, and the
			// basename of "1Mi/s, 0s" is "s" -- the file the maintainer saw
			// called "S". The separator is the last ':' followed by a
			// percentage; a name may contain ':' but not ':' + digits + '%'.
			size_t sep = std::string::npos;
			for (size_t i = body.size(); i-- > 0; )
			{
				if (body[i] != ':')
					continue;
				size_t j = i + 1;
				while (j < body.size() && body[j] == ' ')
					j++;
				size_t d = j;
				while (d < body.size() && isdigit((unsigned char) body[d]))
					d++;
				if (d > j && d < body.size() && body[d] == '%')
				{
					sep = i;
					break;
				}
			}
			std::string name = sep == std::string::npos ? body : body.substr(0, sep);
			mFileProgress = sep == std::string::npos ? "" : Utils::String::trim(body.substr(sep + 1));
			mCurrent = Utils::FileSystem::getFileName(Utils::String::trim(name));
		}
		mFilesThisBlock++;
		return;
	}
}

// The first percentage in an rclone stats field: "12 / 45, 27%, Listed 300"
// gives 27; "0 B / 0 B, -, 0 B/s, ETA -" gives -1. Only a number that was
// printed is ever returned -- the caller shows a spinner for -1 rather than
// a bar at a position nobody measured.
int CloudTransferJob::parsePercent(const std::string& body)
{
	auto pp = body.find('%');
	if (pp == std::string::npos || pp == 0)
		return -1;
	size_t st = pp;
	while (st > 0 && isdigit((unsigned char) body[st - 1]))
		st--;
	if (st == pp)
		return -1;
	const int v = atoi(body.substr(st, pp - st).c_str());
	return (v >= 0 && v <= 100) ? v : -1;
}

// The bar shows the work furthest from done, which is the lower of two
// percentages: the transfer's and the checks'. A run is both -- rclone
// compares as it lists and moves what differs -- and a saves backup with
// one changed save among hundreds moves its bytes in a second, then spends
// the run comparing. Bytes alone pinned the bar at 100% over a live
// CHECKING 120 OF 400 FILES for all of it (review, 2026-09-08); the lower
// of the two reads 27 -> 0 -> 89 -> 100 block by block on that run, every
// number rclone's.
//
// The transfer's percentage is bytes: they say how long this will take when
// the files are a save game and a disc image. The count of files
// transferred stands in only when the bytes have no number (nothing but
// empty files queued), because it counts completed files -- four disc
// images moving in parallel read 0 / 4 until the first lands, and a bar
// that took the lowest of all three would sit at zero for most of such a
// run. A run with nothing to move has neither, and then the count of files
// checked is the only measure there is -- a true one, which is more than
// the spinner it replaces could say. Never a number nobody printed: with
// none, mPercent keeps its last real value. Called with mMutex held.
void CloudTransferJob::refreshPercent()
{
	int p = mBytePercent >= 0 ? mBytePercent : mFilePercent;
	if (mCheckPercent >= 0 && (p < 0 || mCheckPercent < p))
		p = mCheckPercent;
	if (p >= 0)
		mPercent = p;
}

void CloudTransferJob::run()
{
	int ret = -1;
	// Braces around the whole command, not just " 2>&1" after it. The command
	// is a sequence, and a trailing redirection binds to its last element only
	// -- so everything the earlier tiers wrote to stderr went to the ES
	// process's own stderr and never reached the page.
	//
	// SIGPIPE ignored inside them, as the scan's job does (OfflineScanJob):
	// should this process end while the run is still going -- essway
	// restarting the interface under a transfer -- the scripts' own ">>> "
	// lines meet a closed pipe and fail without killing the shell, so a
	// part that can still end writes its stamp. rclone's
	// progress rides the same pipe, and rclone ends on a broken one, so the
	// run itself is not promised to outlive the interface: each file it
	// moves is renamed into place whole and the next run finishes what this
	// one did not (D-CLOUD-077). While this process lives, this thread reads
	// the pipe to the end whatever happens to the page.
	//
	// In a session of its own, saying so on its first line: setsid makes
	// the shell a process group leader and ">>> pid N" tells stopForLaunch
	// which group to signal, so the shell, the scripts and their rclone go
	// together -- the shape ThreadedCloudSync gives its commands, for the
	// same cancel (D-CLOUD-129). shellQuote, so a command with a quote in it
	// survives the trip.
	// The stamps before anything runs, on this thread rather than the
	// interface's (a stat and a short read a stamp): what a stop's restamp
	// compares against (G-E2-O2).
	mStampsBefore = ThreadedCloudSync::readStamps(mCommand);
	const std::string wrapped = "setsid sh -c "
		+ Utils::String::shellQuote("echo \">>> pid $$\"; { trap '' PIPE; " + mCommand + " ; }") + " 2>&1";
	FILE* pipe = popen(wrapped.c_str(), "r");
	if (pipe != nullptr)
	{
		// Read a character at a time, and treat three things as ending a line:
		// \n, \r, and the string "Transferred:" appearing mid-line.
		//
		// The third is not defensive programming, it is the observed format.
		// Piped (there is no terminal here), rclone ends each redraw after the
		// last " * file" line WITHOUT a newline, so the next block's
		// "Transferred:" is glued onto it:
		//
		//   * f4.bin: 26% / 3.8 MiB, 507 KiB/sTransferred: 6.1 MiB / 22.8 MiB...
		//
		// Splitting on newlines alone yields one line that is neither a file
		// line nor a totals line, and both halves are lost -- every block after
		// the first. \r is handled because a terminal-attached run does use it.
		std::string buf;
		int c;
		while ((c = fgetc(pipe)) != EOF)
		{
			if (c != '\n' && c != '\r')
			{
				if (buf.size() < 1024)
					buf += (char) c;

				static const std::string MARK = "Transferred:";
				if (buf.size() > MARK.size()
					&& buf.compare(buf.size() - MARK.size(), MARK.size(), MARK) == 0)
				{
					handleLine(cleanLine(buf.substr(0, buf.size() - MARK.size())));
					buf = MARK;
				}
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

	std::unique_lock<std::mutex> lock(mMutex);
	foldUnit();   // the last unit: no ">>> unit" follows it
	// A stop that came as the command was ending, and found it done: the
	// command's own 0 (or rclone's 9, nothing to move) says it completed,
	// and a completed run is not called stopped (#308 gpt F-CS-24). A run
	// the stop did end exits by the signal or by the scripts' trap, 130 --
	// never 0.
	if ((mStoppedForGame || mStoppedByPlayer) && (ret == 0 || ret == 9))
	{
		LOG(LogInfo) << "CloudTransferJob: the stop came after the command had finished; it completed";
		mStoppedForGame = false;
		mStoppedByPlayer = false;
	}
	// Stopped for a game, or by the player (D-UI-078): SIGTERM shows up as
	// a signal or as 143, and neither is what happened. CloudExit::Stopped
	// is the scripts' own word for being stopped, whoever did the stopping
	// (ThreadedCloudSync does the same for the card's cancel).
	const bool stopped = mStoppedForGame || mStoppedByPlayer;
	if (stopped)
	{
		ret = CloudExit::Stopped;
		// The scripts' INT/TERM trap stamped each part it was inside with 130
		// and no token, and the rows under MANAGE CLOUD STORAGE read that as
		// COULDN'T FINISH once this run was dismissed (#203). Say what
		// happened in their place: the token the card's cancel writes for a
		// game, the player's own for a cancel (CloudText's outcome tokens).
		restampStoppedParts(mStoppedByPlayer ? "player-cancelled" : "cancelled");
	}
	mExit = ret;
	// A command with no tier lines (the match; anything composed before
	// them) that did not complete: what it said is its failed item -- the
	// unit the why arrived under, or the why alone -- and with no why at
	// all, the code's phrase for the unit that was running, if one was.
	if (ret != 0 && ret != 9 && mFailed.empty() && !stopped)
	{
		for (auto& w : mPendingWhys)
			mFailed.push_back(w);
		if (mFailed.empty() && ret != CloudExit::LockHeld && ret != CloudExit::NoNetwork)
			mFailed.push_back({ Utils::String::toUpper(mUnitLabel), ThreadedCloudSync::whyForCode(ret) });
	}
	mPendingWhys.clear();
	mElapsedMs = elapsedMsLocked();
	mFinishedAt = time(nullptr);
	mFinished = true;
	LOG(LogInfo) << "CloudTransferJob: " << mTitle << " exited " << ret
		<< " (" << mRunFiles << " files, " << mRunBytes << " bytes, " << mTiers.size() << " tiers reported)";
}

// The part the stop interrupted, and only it: ThreadedCloudSync's rule,
// shared with the card (#308 claude F-CS-05, gpt F-CS-23). This page's
// table named cloud_backup's last-backup and cloud_restore's last-restore
// alone, so a stopped settings part kept its trap's 130 under the settings
// row, and restamped every stamp the run had written, so a saves part that
// finished before the stop read as stopped.
void CloudTransferJob::restampStoppedParts(const char* token)
{
	ThreadedCloudSync::restampStoppedParts(mCommand, mStampsBefore, mStartedAt, token);
}

// One line of a script's output: ANSI escapes, C0 controls and DEL dropped,
// every UTF-8 byte kept, then trimmed -- CloudText::cleanLine, the rule the
// sync card reads by since #308 F-CS-19. This page carried a copy of its
// own; the two gave the same line for every input, and one copy is one
// rule to change.
std::string CloudTransferJob::cleanLine(const std::string& raw)
{
	return CloudText::cleanLine(raw);
}
