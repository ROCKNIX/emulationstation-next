"""The sign-in wait trusts only the listener's own word (#308 8a-es-app gpt
F-ES-18).

cloudOAuthAwaitSession polls `cloud_oauth info` for STATUS=waiting with an
address, and falls back to `cloud_oauth url` for an image whose script
predates `info`. This compiles the shipped function out of GuiMenu.cpp with
a scripted cloud_oauth (the poll's sleep taken out of the extracted text, so
the case runs in milliseconds) and checks: a listener that answers and says
it failed is not taken for a started sign-in on the strength of an address
`url` still holds; an older script with no `info` still gets its address;
a waiting listener is found.

    python3 tests/cloud-oauth-await.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import re
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


body = definition('static bool cloudOAuthAwaitSession(')
# The wait between polls, whichever way it is written, is not what is tested.
body = re.sub(r'std::this_thread::sleep_for\([^;]*\);', 'pollPause();', body)
body = re.sub(r'Utils::Platform::runSystemCommand\("sleep 0\.5"[^;]*\);', 'pollPause();', body)

harness = r'''
#include <functional>
#include <iostream>
#include <string>
#include <vector>
static int gPauses = 0;
static void pollPause() { gPauses++; }
static std::function<std::vector<std::string>(const std::string&)> gScript;
namespace Utils {
namespace String {
std::string trim(const std::string& s) { size_t a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) return ""; size_t b = s.find_last_not_of(" \t\r\n"); return s.substr(a, b - a + 1); }
bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
}
namespace Platform {
std::vector<std::string> GetShOutputLines(const std::string& cmd) { return gScript(cmd); }
int runSystemCommand(const std::string&, const std::string&, void*) { return 0; }
}
}
''' + body + r'''
int main()
{
    int failures = 0;
    auto check = [&failures](bool ok, const std::string& what) { std::cout << (ok ? "ok   " : "FAIL ") << what << "\n"; if (!ok) failures++; };
    std::string url, hint; bool onDevice = false;

    gScript = [](const std::string& cmd) -> std::vector<std::string> {
        if (cmd.find(" info") != std::string::npos) return { "STATUS=failed", "URL=http://192.168.1.2:53682/old-pin" };
        if (cmd.find(" url") != std::string::npos) return { "http://192.168.1.2:53682/old-pin" };
        return {};
    };
    check(!cloudOAuthAwaitSession(url, onDevice, hint), "a listener that says it failed is not a started sign-in");

    gScript = [](const std::string& cmd) -> std::vector<std::string> {
        if (cmd.find(" info") != std::string::npos) return {};
        if (cmd.find(" url") != std::string::npos) return { "http://192.168.1.2:53682/pin" };
        return {};
    };
    check(cloudOAuthAwaitSession(url, onDevice, hint) && url == "http://192.168.1.2:53682/pin", "an older script without info still gets its address");

    gScript = [](const std::string& cmd) -> std::vector<std::string> {
        if (cmd.find(" info") != std::string::npos) return { "STATUS=waiting", "URL=http://192.168.1.2:53682/new", "ON_DEVICE=yes" };
        return {};
    };
    check(cloudOAuthAwaitSession(url, onDevice, hint) && url == "http://192.168.1.2:53682/new" && onDevice, "a waiting listener is found");

    std::cout << "cloud-oauth-await: " << (failures ? "FAIL" : "PASS") << "\n";
    return failures ? 1 : 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "h.cpp").write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-o", str(tmp / "h"), str(tmp / "h.cpp")], check=True)
    sys.exit(subprocess.run([str(tmp / "h")]).returncode)
