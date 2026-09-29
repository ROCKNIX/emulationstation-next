"""What a failed maintenance run says (#308 8-es-menus-and-core claude
F-ES-13).

runMaintenanceCommand (SYSTEM MANAGEMENT AND RESET: back up, restore, the
factory resets) shows COULDN'T FINISH - <text> when a script fails. The
scripts print a `>>> why <sentence>` line at the point of failure
(es-player-text.md, Outcome vocabulary), and backuptool's fail() then
prints the fuller sentence, recovery included, as its last line. This
compiles the shipped maintenanceWhy out of GuiMenu.cpp and checks the
choice: the fail sentence that carries the why, the why over a stray tool
line printed after it, and the last line only for a script that printed
no why.

    python3 tests/maintenance-why.py [path/to/GuiMenu.cpp]
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
#include <iostream>
#include <string>
namespace Utils { namespace String {
bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
}}
''' + definition('static std::string maintenanceWhy(') + r'''
int main()
{
    int failures = 0;
    auto check = [&failures](const std::string& got, const std::string& want, const std::string& what) {
        const bool ok = got == want;
        std::cout << (ok ? "ok   " : "FAIL ") << what << (ok ? "" : " -- got \"" + got + "\"") << "\n";
        if (!ok) failures++;
    };
    const std::string why = "THERE'S NO SETTINGS BACKUP ON THIS DEVICE YET";
    const std::string sentence = why + ". NOTHING WAS CHANGED. BACK UP SETTINGS FIRST, OR RESTORE THEM FROM THE CLOUD.";
    check(maintenanceWhy(sentence, why), sentence, "the fail sentence that carries the why, recovery and all");
    check(maintenanceWhy("tar: short read", why), why, "the why over a stray tool line printed after it");
    check(maintenanceWhy("Resetting RetroArch failed", ""), "Resetting RetroArch failed", "the last line when the script printed no why");
    check(maintenanceWhy("", why), why, "the why alone");
    std::cout << "maintenance-why: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "h.cpp").write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-o", str(tmp / "h"), str(tmp / "h.cpp")], check=True)
    sys.exit(subprocess.run([str(tmp / "h")]).returncode)
