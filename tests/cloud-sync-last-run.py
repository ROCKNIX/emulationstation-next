"""The sync row and its confirmation speak of the same run (#308 8a-es-app
gpt F-ES-16).

SYNC SAVES WITH THE CLOUD's line reports the newest sync by any route --
the one the player asked for, the exit's, the startup's (#112) -- and its
confirmation adds LAST TIME IT COULDN'T FINISH: <why> for the last run.
This compiles the shipped cloudLastRunWhy (and cloudLatestRun, where the
source has it) out of GuiMenu.cpp with a table of stamps, and checks the
confirmation after a manual sync that failed and an exit sync that then
completed: nothing about a failure the row no longer shows.

    python3 tests/cloud-sync-last-run.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/guis/GuiMenu.cpp")
source = source_path.read_text()


def definition(signature):
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
#include <ctime>
#include <iostream>
#include <map>
#include <string>
#define _(x) std::string(x)
''' + definition('struct CloudLastRun').rstrip() + ''';
static std::map<std::string, CloudLastRun> gStamps;
static CloudLastRun cloudReadLastRun(const std::string& name) { auto it = gStamps.find(name); return it == gStamps.end() ? CloudLastRun() : it->second; }
''' + (definition('static CloudLastRun cloudLatestRun(') + '\n' if 'static CloudLastRun cloudLatestRun(' in source else '') \
    + definition('static std::string cloudLastRunWhy(') + r'''
int main()
{
    CloudLastRun manual; manual.ran = true; manual.when = 1000; manual.outcome = "COULDN'T FINISH"; manual.why = "YOUR CLOUD STOPPED ANSWERING";
    CloudLastRun exitRun; exitRun.ran = true; exitRun.when = 2000; exitRun.outcome = "COMPLETED";
    gStamps["sync-manual"] = manual;
    gStamps["sync-exit"] = exitRun;
    const std::string why = cloudLastRunWhy("sync-manual");
    int failures = 0;
    if (why.empty()) std::cout << "ok   the confirmation says nothing of a failure the row no longer shows\n";
    else { std::cout << "FAIL the confirmation explains an older run than the row shows:" << why << "\n"; failures++; }

    gStamps["sync-exit"].when = 500;   // the manual run is the newest again
    if (cloudLastRunWhy("sync-manual").find("YOUR CLOUD STOPPED ANSWERING") != std::string::npos)
        std::cout << "ok   the newest run's failure is still said\n";
    else { std::cout << "FAIL the newest run's failure was not said\n"; failures++; }

    gStamps.clear();
    gStamps["backup"] = manual;
    if (cloudLastRunWhy("backup").find("YOUR CLOUD STOPPED ANSWERING") != std::string::npos)
        std::cout << "ok   a row with one route reads its own stamp\n";
    else { std::cout << "FAIL a row with one route lost its why\n"; failures++; }

    std::cout << "cloud-sync-last-run: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "h.cpp").write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-o", str(tmp / "h"), str(tmp / "h.cpp")], check=True)
    sys.exit(subprocess.run([str(tmp / "h")]).returncode)
