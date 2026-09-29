"""The content picker's selection is saved, and read back, before anything
moves on it; and nothing moves without the verb's press (audit of the fix
round PL-019).

CONTENT TO BACK UP / CONTENT TO RESTORE hand the ticked systems to
`cloud_content_restore --set-systems`, and the run that follows reads them
back with `--selected`. The script exits 0 over a rename that failed (a full
card, a folder gone read-only), and the file then still holds the last run's
picks -- systems the player did not tick, moved in either direction. And
when nothing but BIOS files was there to move, the picker wrote the
selection, threw the answer away and started the run with no press at all.

Two parts:

  1. The page's shape, read from GuiMenu.cpp: the continuation runs only
     after the verb's press (every `onDone()` in the picker is behind
     `*proceed`); every write of the selection goes through the checked
     writer; and every press that sets `*proceed` asked the checked writer
     first.
  2. The checked writer itself: the shipped cloudShellQuote,
     cloudSetSystemsCommand, cloudSelectionRead and cloudSaveSelection are
     compiled out of GuiMenu.cpp unchanged against a stand-in for
     ApiSystem that runs the command through /bin/sh with the script's path
     pointed at a stub, and each case says whether the picker would go on.

    python3 tests/cloud-content-selection.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/guis/GuiMenu.cpp")
source = source_path.read_text()

failures = []
passed = 0


def ok(label):
    global passed
    passed += 1
    print(f"ok   {label}")


def fail(label):
    failures.append(f"FAIL {label}")
    print(f"FAIL {label}")


def definition(signature, text=None):
    """The text of the first definition (not declaration) of `signature`."""
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


# ---------------------------------------------------------------- 1. shape

picker = definition('static void cloudContentSystemPicker(')
lines = picker.splitlines()

# (a) The continuation is a press on the verb: onDone() runs only where the
# press set *proceed, never straight from the scan's answer.
bare = [l.strip() for l in lines
        if re.search(r'\bonDone\s*\(\s*\)', l) and '*proceed' not in l]
if bare:
    fail(f"the picker continues without a press: {bare!r}")
else:
    ok("every onDone() in the picker is behind the verb's press (*proceed)")

# (b) Every write of the selection is the checked one.
writer_names = ('static std::string cloudSetSystemsCommand(', 'static bool cloudSaveSelection(')
rest = source
for name in writer_names:
    if name in rest:
        rest = rest.replace(definition(name, rest), '')
unchecked = [l.strip() for l in rest.splitlines()
             if 'cloudSetSystemsCommand(' in l and 'static std::string cloudSetSystemsCommand(' not in l]
if unchecked:
    fail(f"the selection is written unchecked in {len(unchecked)} place(s): {unchecked!r}")
else:
    ok("the selection is written only through cloudSaveSelection")

# (c) A press that sets *proceed asked the checked writer first.
unguarded = []
for i, l in enumerate(lines):
    if re.search(r'\*proceed\s*=\s*true', l):
        before = "\n".join(lines[max(0, i - 12):i])
        if 'cloudSaveSelection(' not in before:
            unguarded.append(l.strip())
if unguarded or not any(re.search(r'\*proceed\s*=\s*true', l) for l in lines):
    fail(f"a press sets *proceed without checking the selection was saved: {unguarded!r}")
else:
    ok("every press that sets *proceed saved and read back the selection first")

# (d) BIOS files alone get the page too: its branch builds a page with the
# verb and does not call the scripts itself.
m = re.search(r'if \(found\.empty\(\) && biosToMove\)', picker)
if not m:
    fail("the BIOS-alone branch is not where it was")
else:
    branch = definition('if (found.empty() && biosToMove)', picker)
    if 'new GuiSettings(' in branch and 'addButton(' in branch and 'executeScriptLegacy' not in branch:
        ok("BIOS files alone open the page, with the verb on it")
    else:
        fail("BIOS files alone do not open a page with the verb (or the branch runs a script itself)")

# ----------------------------------------------------------- 2. behaviour

try:
    pieces = [definition(s) for s in (
        'static std::string cloudShellQuote(',
        'static std::string cloudSetSystemsCommand(',
        'static bool cloudSelectionRead(',
        'static bool cloudSaveSelection(',
    )]
except ValueError as e:
    fail(f"the checked writer is not in GuiMenu.cpp ({e}): nothing reads the selection back")
    pieces = None

if pieces is not None:
    harness = r'''
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <utility>
#include <vector>

// The one file-system call the reader makes (gpt G3-E-03: a directory at
// the path is a failed read, never an empty selection), as the device's does.
namespace Utils { namespace FileSystem {
    static bool isRegularFile(const std::string& path)
    {
        struct stat st;
        return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
} }

enum { LogError, LogWarning, LogInfo };
#define LOG(level) std::cerr
static std::string gSelection;
#define CLOUD_CONTENT_SELECTION gSelection

// ApiSystem's pair form, as the device runs it: popen through /bin/sh, the
// exit status out of pclose. The script's path is pointed at the stub.
struct ApiSystem
{
    static std::pair<std::string, int> executeScriptLegacy(const std::string& command, const std::function<void(const std::string)>& func)
    {
        std::string cmd = command;
        const std::string script = "/usr/bin/cloud_content_restore";
        const size_t at = cmd.find(script);
        if (at != std::string::npos)
            cmd.replace(at, script.size(), std::getenv("STUB"));
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe == nullptr)
            return { "", -1 };
        char buff[1024];
        while (fgets(buff, sizeof(buff), pipe))
            if (func)
                func(buff);
        return { "", WEXITSTATUS(pclose(pipe)) };
    }
};

''' + "\n".join(pieces) + r'''

int main(int argc, char** argv)
{
    gSelection = argv[1];
    std::vector<std::string> names;
    for (int i = 2; i < argc; i++)
        names.push_back(argv[i]);
    std::cout << (cloudSaveSelection(names) ? "continue" : "refuse") << std::endl;
    return 0;
}
'''

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        cpp = tmp / "harness.cpp"
        exe = tmp / "harness"
        cpp.write_text(harness)
        build = subprocess.run(["g++", "-std=c++17", "-fsanitize=address,undefined", "-o", str(exe), str(cpp)],
                               capture_output=True, text=True)
        if build.returncode != 0:
            fail("the checked writer does not compile on its own:\n" + build.stderr[-2000:])
        else:
            selection = tmp / "content-systems"
            # The script's --set-systems, in the shape that matters here: the
            # names split on spaces, one a line, written whole or not at all.
            writes = tmp / "writes"
            writes.write_text(
                '#!/bin/bash\n'
                'shift\n'
                'set -f; read -r -a n <<< "$*"; set +f\n'
                '{ [ ${#n[@]} -eq 0 ] || printf "%s\\n" "${n[@]}"; } > "$SEL.tmp.$$"\n'
                'mv -f "$SEL.tmp.$$" "$SEL"\n'
                'echo "OK ${#n[@]} selected"\n')
            # A name the script refuses: exit 1, nothing changed.
            refuses = tmp / "refuses"
            refuses.write_text('#!/bin/sh\necho "Nothing was changed."\nexit 1\n')
            # The rename that failed: 0 and OK, and the old file stands.
            stale = tmp / "stale"
            stale.write_text('#!/bin/sh\necho "OK 2 selected"\nexit 0\n')
            for stub in (writes, refuses, stale):
                stub.chmod(0o755)
            missing = tmp / "not-installed"
            DIRECTORY = object()

            def run(stub, names, before, want, label):
                if selection.is_dir():
                    for child in selection.iterdir():
                        child.unlink()
                    selection.rmdir()
                if before is None:
                    if selection.exists():
                        selection.unlink()
                elif before is DIRECTORY:
                    # a directory where the file should be: the writer's rename
                    # lands inside it and says OK, and the read-back opens a
                    # directory -- a failed read, never an empty selection
                    # (the audit of the fixes, gpt G3-E-03)
                    if selection.exists():
                        selection.unlink()
                    selection.mkdir()
                else:
                    selection.write_text(before)
                got = subprocess.run([str(exe), str(selection)] + names, capture_output=True, text=True,
                                     env={"STUB": str(stub), "SEL": str(selection), "PATH": "/usr/bin:/bin"})
                answer = got.stdout.strip()
                if got.returncode != 0 or answer != want:
                    fail(f"{label}: the picker would {answer or 'crash'} (want {want}); stderr: {got.stderr.strip()[-400:]}")
                else:
                    ok(f"{label}: {answer}")

            run(writes, ["snes", "nes"], "gba\n", "continue", "a selection the script saved")
            run(writes, ["bios"], "snes\nnes\n", "continue", "BIOS alone, over an earlier selection")
            run(refuses, ["snes"], "gba\n", "refuse", "a selection the script refused (exit 1)")
            run(stale, ["bios"], "snes\nnes\n", "refuse", "exit 0 over a rename that failed: the last run's picks stand")
            run(missing, ["snes"], "gba\n", "refuse", "no script to save it")
            run(writes, [], "snes\n", "continue", "nothing ticked, saved as nothing")
            run(stale, [], None, "refuse", "nothing ticked, and no file was written")
            run(writes, ["my games"], None, "refuse", "a folder name the script splits in two")
            # nothing ticked, so the set comparison alone would say "continue"
            # (nothing read equals nothing asked): only a reader that reports
            # its own failure refuses here
            run(writes, [], DIRECTORY, "refuse", "a directory where the selection file should be, nothing ticked: a failed read refuses (gpt G3-E-03)")
            run(writes, ["x"], "snes\n", "continue", "a one-letter name (a two-byte file)")

print(f"cloud-content-selection: {passed} of {passed + len(failures)} passed")
sys.exit(1 if failures else 0)
