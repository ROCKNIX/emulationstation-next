"""A changed cloud folder brings the player back to the row they pressed
(audit of the fixes, orchestrator finding G-E2-O1; D-UI-042).

e98bfda4b made a changed folder reopen the CLOUD hub so CHANGE CLOUD
FOLDER's line names the new folder. The reopened page opened on its first
row, BACK UP TO THE CLOUD, with CHANGE CLOUD FOLDER scrolled off the foot
of the page: the next A opened the back-up page instead of the editor
(vm-qa run 68, walk confirm-cloud-folder, frames 04 and 05).

The page is built from the menus and the window, which nothing compiles
without SDL, so this reads the shipped source for the two facts the fix
is: the reopen from the editor's onDone asks for the folder row, and the
row is added with the cursor there when asked. The frame is the walk's.

    python3 tests/cloud-folder-reopen.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import re
import sys

src = (Path(sys.argv[1]) if len(sys.argv) > 1 else
       Path(__file__).resolve().parents[1] / "es-app/src/guis/GuiMenu.cpp").read_text()

failures = 0


def check(ok, what):
    global failures
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failures += 1


start = src.index("void GuiMenu::openCloud(Window* window")
body = src[start:src.index("\n}\n", start)]
signature = body[:body.index(")") + 1]
m = re.search(r"void GuiMenu::openCloud\(Window\* window, bool (\w+)\)", signature)
check(m is not None, "openCloud takes whether to open on the folder row")
flag = m.group(1) if m else None

row = body.index('_("CHANGE CLOUD FOLDER")')
call = body[row:body.index(";", body.index("cloudSetupOpenSyncPathEditor", row))]
call = call + body[body.index(";", body.index("cloudSetupOpenSyncPathEditor", row)):][:200]
check(re.search(r"GuiMenu::openCloud\(window, true\)", call) is not None,
      "the editor's onDone reopens the hub on the folder row")
tail = re.search(r'"",\s*(\w+),\s*true\);', call)
check(flag is not None and tail is not None and tail.group(1) == flag,
      "the folder row is added with the cursor there when asked")

print("cloud-folder-reopen: " + ("FAIL" if failures else "PASS"))
sys.exit(1 if failures else 0)
