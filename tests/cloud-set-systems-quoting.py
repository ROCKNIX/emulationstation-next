"""The content picker's selection reaches a shell as data (audit #307 PL-030).

The names on CONTENT TO RESTORE are folder names in the player's cloud, read
from `cloud_content_restore --scan`; the picker hands the ticked ones back
with `--set-systems` through executeScriptLegacy, which is a shell. This
compiles the shipped cloudSetSystemsCommand and cloudShellQuote out of
GuiMenu.cpp unchanged, runs the command they build through /bin/sh with the
script's path pointed at a stand-in that records its arguments, and checks
that a hostile name runs nothing and arrives as the one argument the script
has always taken.

    python3 tests/cloud-set-systems-quoting.py [path/to/GuiMenu.cpp]
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

source_path = (Path(sys.argv[1]) if len(sys.argv) > 1 else
               Path(__file__).resolve().parents[1] / "es-app/src/guis/GuiMenu.cpp")
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
#include <iostream>
#include <string>
#include <vector>
''' + definition('static std::string cloudShellQuote(') + '\n' \
    + definition('static std::string cloudSetSystemsCommand(') + r'''
int main(int argc, char** argv)
{
    std::vector<std::string> names;
    for (int i = 1; i < argc; i++)
        names.push_back(argv[i]);
    std::cout << cloudSetSystemsCommand(names);
    return 0;
}
'''

failures = []
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    cpp = tmp / "harness.cpp"
    exe = tmp / "harness"
    cpp.write_text(harness)
    subprocess.run(["g++", "-std=c++17", "-fsanitize=address,undefined", "-o", str(exe), str(cpp)], check=True)

    record = tmp / "args"
    stub = tmp / "cloud_content_restore"
    stub.write_text('#!/bin/sh\nfor a in "$@"; do printf "%s\\n" "$a"; done > "' + str(record) + '"\n')
    stub.chmod(0o755)

    marker = tmp / "ran"
    cases = [
        ["nes", "snes"],
        ["a$(touch " + str(marker) + ")b"],
        ["`touch " + str(marker) + "`"],
        ['nes"; touch ' + str(marker) + '; echo "'],
        ["it's"],
    ]
    for names in cases:
        if marker.exists():
            marker.unlink()
        if record.exists():
            record.unlink()
        command = subprocess.run([str(exe)] + names, check=True, capture_output=True, text=True).stdout
        command = command.replace("/usr/bin/cloud_content_restore", str(stub), 1)
        subprocess.run(["/bin/sh", "-c", command], check=False)
        got = record.read_text().splitlines() if record.exists() else None
        want = ["--set-systems", " ".join(names)]
        if marker.exists():
            failures.append(f"FAIL {names!r}: the name ran a command ({marker.name} was created); command: {command}")
        elif got != want:
            failures.append(f"FAIL {names!r}: the script received {got!r}, expected {want!r}; command: {command}")
        else:
            print(f"ok   {names!r} -> {got!r}")

for line in failures:
    print(line)
print(f"cloud-set-systems-quoting: {len(cases) - len(failures)} of {len(cases)} passed")
sys.exit(1 if failures else 0)
