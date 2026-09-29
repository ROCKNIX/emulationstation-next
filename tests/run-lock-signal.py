"""STOP IT AND PLAY signals the offline achievements' run, and nothing else
(audit of the fix round, G2-E-app-05 gpt / G2-E-app-06 claude).

OfflineAchievements::stopRun sends SIGTERM to the pid in the ctl's run lock
(RunLock::holder: the lock is held, and the pid's command line mentions
raofflineproxy-ctl). The ctl writes its pid just after it takes the lock,
so for that moment -- and after a run killed -9 -- the file names an
earlier run's pid, which may be anything by now: a mention in an argument
(a tail of the ctl's log, a grep) passed the test, and that process was
the one signalled.

This compiles the shipped stopRun, and whatever helpers it calls from
OfflineAchievements.cpp's own namespace, against the real RunLock.h, points
the lock at a file of its own, and runs it against real processes:

  1. the lock held by a process that has not written its pid, the file
     naming a live impostor whose argv says raofflineproxy-ctl: nothing
     is signalled, and stopRun says so;
  2. a ctl-shaped holder that wrote its own pid: it is signalled;
  3. the holder writes its pid a moment after stopRun is asked: the
     holder is signalled, not the impostor the file named first.

    python3 tests/run-lock-signal.py [path/to/OfflineAchievements.cpp]
"""
from pathlib import Path
import os
import re
import signal
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[1]
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "es-app/src/OfflineAchievements.cpp"
source = source_path.read_text()

failures = []
passed = 0


def ok(label):
    global passed
    passed += 1
    print(f"ok   {label}")


def fail(label):
    failures.append(label)
    print(f"FAIL {label}")


def definition(signature, text=None):
    text = source if text is None else text
    start = text.index(signature)
    while text.index(';', start) < text.index('{', start):
        start = text.index(signature, start + 1)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end]


stop_run = definition('bool OfflineAchievements::stopRun()')
# The helpers stopRun calls that live beside it (none before this fix).
helpers = []
for name in re.findall(r'\b([a-z][A-Za-z]+)\s*\(', stop_run):
    m = re.search(r'\n\t(?:static )?(?:bool|long|int) ' + name + r'\(', source)
    if m and name not in ('stopRun',):
        text = definition(source[m.start() + 2:source.index('(', m.start()) + 1])
        if text not in helpers:
            helpers.append(text)

harness = r'''
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include "RunLock.h"

enum { LogError, LogWarning, LogInfo };
#define LOG(level) std::cerr
static std::string gLock;
#define SCAN_LOCK gLock.c_str()

struct OfflineAchievements { static bool stopRun(); };

namespace {
''' + "\n".join(helpers) + r'''
}

''' + stop_run + r'''

int main(int argc, char** argv)
{
    gLock = argv[1];
    std::cout << (OfflineAchievements::stopRun() ? "signalled" : "nothing") << std::endl;
    return 0;
}
'''


def alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    # A zombie is a process that has died; reap-less children read as alive.
    try:
        with open(f"/proc/{pid}/stat") as f:
            return f.read().split()[2] != 'Z'
    except FileNotFoundError:
        return False


with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    cpp = tmp / "harness.cpp"
    exe = tmp / "harness"
    cpp.write_text(harness)
    build = subprocess.run(["g++", "-std=c++17", "-fsanitize=address,undefined", "-I", str(root / "es-app/src"),
                            "-o", str(exe), str(cpp)], capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stderr[-3000:])
        fail("the shipped stopRun does not compile on its own")
    else:
        lock = None

        def impostor():
            # A process whose command line mentions the ctl and holds nothing:
            # argv[0] set as a tail of the ctl's log would carry it.
            return subprocess.Popen(["bash", "-c", "exec -a raofflineproxy-ctl.log-tail sleep 30"], start_new_session=True)

        def holder(write_pid_after):
            # The ctl's take_lock: the file opened without truncating, the
            # lock, then its pid on the first line -- here after a delay (or
            # never, with -1), which is the window the finding names. It
            # stays bash, as the ctl does, so its argv keeps naming the ctl:
            # a last simple command in bash -c is exec'd in its place, so the
            # sleep is waited for rather than run last.
            # Blocking: the probe below takes the lock for an instant, and the
            # ctl retries a busy lock rather than give up at once.
            script = ('exec 9<>"$0"; flock 9 || exit 3; '
                      + ('' if write_pid_after < 0 else f'sleep {write_pid_after}; printf "%s\\n" $$ >&9; ')
                      + 'sleep 30 & wait $!')
            return subprocess.Popen(["bash", "-c", script, str(lock), "raofflineproxy-ctl"], start_new_session=True)

        def wait_locked():
            for _ in range(200):
                r = subprocess.run(["flock", "-n", str(lock), "true"])
                if r.returncode != 0:
                    return True
                time.sleep(0.01)
            return False

        def stop_run():
            got = subprocess.run([str(exe), str(lock)], capture_output=True, text=True)
            return got.stdout.strip() or "crash: " + got.stderr[-400:]

        def cleanup(*procs):
            # The session: a holder signalled mid-sleep leaves its sleep,
            # which holds the lock, behind.
            for p in procs:
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                p.wait()

        # 1. The pid is not yet written; the file still names an impostor.
        lock = tmp / "one.lock"
        imp = impostor()
        time.sleep(0.2)
        lock.write_text(f"{imp.pid}\n")
        h = holder(-1)
        if not wait_locked():
            fail("case 1: the holder never took the lock")
        else:
            answer = stop_run()
            time.sleep(0.2)
            if not alive(imp.pid):
                fail(f"a pid that does not hold the lock was signalled (stopRun said {answer}): the impostor {imp.pid} died")
            elif answer != "nothing":
                fail(f"case 1: stopRun said {answer} and signalled nobody")
            else:
                ok("the file names a process that does not hold the lock: nothing is signalled, and stopRun says so")
        cleanup(imp, h)

        # 2. A ctl-shaped holder that wrote its own pid.
        lock = tmp / "two.lock"
        lock.write_text("")
        h = holder(0)
        time.sleep(0.3)
        answer = stop_run()
        try:
            h.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        if answer == "signalled" and h.returncode == -signal.SIGTERM:
            ok("the holder that wrote its pid is signalled")
        else:
            fail(f"case 2: the holder was not stopped (stopRun said {answer}, holder rc {h.returncode})")
        cleanup(h)

        # 3. The holder writes its pid a moment after stopRun is asked.
        lock = tmp / "three.lock"
        imp = impostor()
        time.sleep(0.2)
        lock.write_text(f"{imp.pid}\n")
        h = holder(0.2)
        if not wait_locked():
            fail("case 3: the holder never took the lock")
        else:
            answer = stop_run()
            try:
                h.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            time.sleep(0.2)
            if not alive(imp.pid):
                fail(f"case 3: the impostor {imp.pid} was signalled while the holder wrote its pid (stopRun said {answer})")
            elif answer == "signalled" and h.returncode == -signal.SIGTERM:
                ok("a pid written a moment late: the holder is signalled, not the pid the file named first")
            else:
                fail(f"case 3: the holder was not stopped (stopRun said {answer}, holder rc {h.returncode})")
        cleanup(imp, h)

print(f"run-lock-signal: {passed} of {passed + len(failures)} passed")
sys.exit(1 if failures else 0)
