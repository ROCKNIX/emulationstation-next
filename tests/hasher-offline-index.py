"""An index that ran offline says only that it did (#308 1-raoffline claude
F-RA-05).

ThreadedHasher's constructor fetches RetroAchievements' hash library and,
when it does not come, empties its queue and has ProxyCards say the new
games wait for the link. It counted the games before emptying the queue,
so the count stayed above zero: a card and threads were made for an empty
queue, and the destructor toasted INDEXING COMPLETED. UPDATE GAMELISTS TO
APPLY CHANGES. beside the card that says nothing was indexed -- and the
threads, ending at once on the empty queue, could delete the hasher before
start() had read it back. This compiles the shipped constructor and
destructor out of ThreadedHasher.cpp against a stand-in class with the same
members (a member that drifts is a compile error here, which is the point)
and runs an offline index with games waiting.

    python3 tests/hasher-offline-index.py [path/to/ThreadedHasher.cpp]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/ThreadedHasher.cpp")
source = source_path.read_text()


def definition(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]


harness = r'''
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#define _(x) std::string(x)
#define _U(x) std::string(x)
enum LogLevel { LogError, LogWarning, LogInfo, LogDebug };
struct LogLine { template <class T> LogLine& operator<<(const T&) { return *this; } };
#define LOG(level) LogLine()

enum class MetaDataId { CheevosHash, CheevosId };
struct FileData { std::string getMetadata(MetaDataId) { return ""; } };
struct AsyncNotificationComponent { void updateTitle(const std::string&) {} void close() {} };
static int gCards = 0, gToasts = 0, gOfflineCards = 0, gTopUps = 0;
static std::string gLastToast;
struct Window {
    AsyncNotificationComponent card;
    AsyncNotificationComponent* createAsyncNotificationComponent(bool = false) { gCards++; return &card; }
    void displayNotificationMessage(const std::string& m, int = -1) { gToasts++; gLastToast = m; }
};
namespace Utils { namespace String { std::string toUpper(const std::string& s) { return s; } } }
namespace RetroAchievements { std::map<std::string, std::string> getCheevosHashes() { return {}; } }   // offline
namespace ProxyCards { void indexRanOffline(Window*, int) { gOfflineCards++; } }
namespace OfflineAchievements { void topUpAfterIndex(Window*) { gTopUps++; } }

class ThreadedHasher
{
public:
    enum HasherType : unsigned int { HASH_NETPLAY_CRC = 1, HASH_CHEEVOS_MD5 = 2, HASH_ALL = 3 };
    ThreadedHasher(Window* window, HasherType type, std::queue<FileData*> searchQueue, const std::vector<FileData*>& lookupOnly, bool forceAllGames = false);
    ~ThreadedHasher();
    int total() const { return mTotal; }
    size_t threads() const { return mThreads.size(); }
    void joinAll() { for (auto* t : mThreads) { t->join(); delete t; } mThreads.clear(); }
private:
    void run() {}
    std::queue<FileData*> mSearchQueue;
    std::unordered_set<FileData*> mLookupOnly;
    Window* mWindow;
    AsyncNotificationComponent* mWndNotification;
    std::map<std::string, std::string> mCheevosHashes;
    HasherType mType;
    std::vector<std::thread*> mThreads;
    int mThreadCount;
    int mTotal;
    bool mExit;
    bool mForce;
    bool mCheevosIndexed;
    static ThreadedHasher* mInstance;
    static bool sCheevosLibraryCame;
};
ThreadedHasher* ThreadedHasher::mInstance = nullptr;
bool ThreadedHasher::sCheevosLibraryCame = false;
#define ICONINDEX std::string("")
''' + definition('ThreadedHasher::ThreadedHasher(Window* window') + '\n' + definition('ThreadedHasher::~ThreadedHasher()') + r'''
int main()
{
    Window window;
    FileData a, b, c;
    std::queue<FileData*> queue;
    queue.push(&a); queue.push(&b); queue.push(&c);
    auto* hasher = new ThreadedHasher(&window, ThreadedHasher::HASH_CHEEVOS_MD5, queue, {}, false);
    int failures = 0;
    auto check = [&failures](bool ok, const std::string& what) { std::cout << (ok ? "ok   " : "FAIL ") << what << "\n"; if (!ok) failures++; };
    check(gOfflineCards == 1, "the offline card says the new games wait for the link");
    check(hasher->total() == 0, "an offline run counts nothing to index");
    check(gCards == 0 && hasher->threads() == 0, "no index card and no threads over an empty queue");
    hasher->joinAll();
    delete hasher;
    check(gToasts == 0, "no INDEXING COMPLETED toast beside the offline card" + (gToasts ? std::string(" -- got: ") + gLastToast : std::string()));
    check(gTopUps == 0, "no top-up after an index that identified nothing");
    std::cout << "hasher-offline-index: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "h.cpp").write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-pthread", "-o", str(tmp / "h"), str(tmp / "h.cpp")], check=True)
    sys.exit(subprocess.run([str(tmp / "h")]).returncode)
