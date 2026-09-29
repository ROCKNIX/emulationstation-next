"""A launch waits for the last game's capture (audit #307 PL-061).

After a game exits, cloud_capture records its saves on a thread of its own
(fork #290). The launch's gates covered the sync, the transfer, the send and
the top-up, not the capture, so a game started inside the capture's run
wrote a save the capture was hashing. This compiles the shipped captureGate
out of FileData.cpp with doubles for the window and the spinner page, and
checks: no capture, the launch goes on; a quick capture, the launch goes on
once it is done, with no spinner; a slow one, the launch waits behind the
spinner, owns the exit sync (the generation moves), and starts once. At the
bound the launch is refused and says why, and the exit sync is left to the
capture (audit of the fixes, E2 gpt G-E2-03, claude G-E2-06): the next
launch waits again, and only a capture old enough to be hung, not slow,
stops holding the device.

    python3 tests/launch-capture-gate.py [path/to/FileData.cpp]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/FileData.cpp")
source = source_path.read_text()


def definition(signature):
    """The body of the first definition (not declaration) of `signature`."""
    start = source.index(signature)
    while source.index(';', start) < source.index('{', start):
        start = source.index(signature, start + 1)
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
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#define _(value) std::string(value)
enum LogLevel { LogError, LogWarning, LogInfo };
struct LogLine { std::ostringstream o; template <class T> LogLine& operator<<(const T& v) { o << v; return *this; } ~LogLine() { std::cout << "  log: " << o.str() << "\n"; } };
#define LOG(level) LogLine()

struct Gui { virtual ~Gui() {} };
struct IGuiLoadingHandler {};
template <class T> struct GuiLoading : Gui {
    std::thread worker;
    std::atomic<bool> done{ false };
    GuiLoading(void*, const std::string&, const std::function<T(IGuiLoadingHandler*)>& work, const std::function<void(T)>& then) {
        worker = std::thread([this, work, then] { T r = work(nullptr); then(r); done = true; });
    }
    ~GuiLoading() { if (worker.joinable()) worker.join(); }
};
struct Window { std::mutex m; std::vector<std::unique_ptr<Gui>> stack; void pushGui(Gui* g) { std::lock_guard<std::mutex> l(m); stack.emplace_back(g); } };
static std::atomic<int> gRefused{ 0 };
static std::string gRefusal;
struct GuiMsgBox : Gui { GuiMsgBox(Window*, const std::string& text) { gRefusal = text; gRefused++; } };
struct FileData {};
struct LaunchGameOptions {};

static std::atomic<unsigned> sExitGeneration{ 0 };
static std::atomic<unsigned> sCaptureInFlight{ 0 };
static std::atomic<unsigned> sCaptureGivenUp{ 0 };
static std::atomic<unsigned> sCaptureWaitedOn{ 0 };
static std::atomic<long> gAgeSeconds{ 0 };
static long captureAgeSeconds() { return gAgeSeconds.load(); }
static const long CaptureHungSeconds = 120;
static std::atomic<int> gLaunched{ 0 };
static void launchNow(Window*, FileData*, const LaunchGameOptions&) { gLaunched++; }
''' + definition('static bool captureGate(') + r'''

static void finishIn(unsigned gen, int ms) {
    std::thread([gen, ms] { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); unsigned g = gen; sCaptureInFlight.compare_exchange_strong(g, 0); }).detach();
}

int main()
{
    std::cout << std::unitbuf;
    int failures = 0;
    auto check = [&failures](bool ok, const std::string& what) { std::cout << (ok ? "ok   " : "FAIL ") << what << "\n"; if (!ok) failures++; };
    FileData game; LaunchGameOptions options;

    { // no capture
        Window w;
        check(captureGate(&w, &game, options) == true && w.stack.empty(), "no capture: the launch goes on");
    }
    { // a quick capture: done inside the moment the gate waits on its own thread
        Window w; sCaptureInFlight = 7; finishIn(7, 80);
        const unsigned gen = sExitGeneration;
        const bool go = captureGate(&w, &game, options);
        check(go && w.stack.empty() && sCaptureInFlight == 0, "a quick capture: the launch goes on after it, no spinner");
        check(sExitGeneration == gen, "a quick capture: the exit sync is left as it was");
    }
    { // a slow capture: the spinner, then one launch
        Window w; sCaptureInFlight = 8; finishIn(8, 1500); gLaunched = 0;
        const unsigned gen = sExitGeneration;
        const bool go = captureGate(&w, &game, options);
        check(!go && w.stack.size() == 1, "a slow capture: the launch waits behind the spinner");
        check(sExitGeneration == gen, "a slow capture: the exit sync is not taken while the launch is not certain");
        check(gLaunched == 0, "a slow capture: nothing launches while it records");
        auto* page = dynamic_cast<GuiLoading<bool>*>(w.stack.empty() ? nullptr : w.stack.back().get());
        const auto t0 = std::chrono::steady_clock::now();
        while (page && !page->done && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(8))
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        check(gLaunched == 1 && sCaptureInFlight == 0, "a slow capture: the game starts once, after it");
        check(sExitGeneration == gen + 1, "a slow capture: the launch that goes owns the exit sync");
    }
    { // still recording at the bound: nothing launches under it, and it says why
        Window w; sCaptureInFlight = 9; gLaunched = 0; gRefused = 0; gAgeSeconds = 12;
        const unsigned gen = sExitGeneration;
        const bool go = captureGate(&w, &game, options);
        GuiLoading<bool>* page = nullptr;
        { std::lock_guard<std::mutex> l(w.m); page = w.stack.empty() ? nullptr : dynamic_cast<GuiLoading<bool>*>(w.stack.front().get()); }
        const auto t0 = std::chrono::steady_clock::now();
        while (page && !page->done && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(15))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        check(!go && gLaunched == 0, "still recording at the bound: the game does not start under it");
        check(gRefused == 1 && gRefusal.find("STILL BEING RECORDED") != std::string::npos, "still recording at the bound: the launch says why");
        check(sExitGeneration == gen, "still recording at the bound: the exit sync stays the capture's");
        check(sCaptureWaitedOn == 0, "still recording at the bound: no launch is left waiting on it");
        // The next launch waits again, rather than going under it.
        Window w2; finishIn(9, 1200);
        const bool again = captureGate(&w2, &game, options);
        check(!again && !w2.stack.empty(), "the next launch waits for it again");
        GuiLoading<bool>* page2 = nullptr;
        { std::lock_guard<std::mutex> l(w2.m); page2 = w2.stack.empty() ? nullptr : dynamic_cast<GuiLoading<bool>*>(w2.stack.front().get()); }
        const auto t1 = std::chrono::steady_clock::now();
        while (page2 && !page2->done && std::chrono::steady_clock::now() - t1 < std::chrono::seconds(8))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        check(gLaunched == 1, "... and starts once it is done");
    }
    { // a capture old enough to be hung: the device is not held for it
        Window w; sCaptureInFlight = 10; gLaunched = 0; gAgeSeconds = 200;
        check(captureGate(&w, &game, options) && w.stack.empty(), "a hung capture: the launch goes on");
        sCaptureInFlight = 0; gAgeSeconds = 0;
    }
    std::cout << "launch-capture-gate: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    cpp = tmp / "harness.cpp"
    exe = tmp / "harness"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-pthread", "-fsanitize=address,undefined", "-o", str(exe), str(cpp)], check=True)
    sys.exit(subprocess.run([str(exe)]).returncode)
