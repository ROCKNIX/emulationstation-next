#!/usr/bin/env python3
# Every command the interface builds from a credential a player typed hands
# it to the shell quoted (fork #198).
#
#   python3 tests/credential-quoting.py [es-source-root]
#
# setrootpass takes the device password, wifictl a network's name and key,
# and cloud_setup --set-syncpath the cloud folder; spliced in bare, a space
# cuts the value and $ ; & or a quote are the shell's, so the device stores
# something other than what was typed, or runs it. #198 named three
# setrootpass callers and its fix quoted two: the third, FINISH RESTORE
# PROCESS > DEVICE PASSWORD, stayed bare until 2026-09-25; the CLOUD FOLDER
# sat in double quotes, which leave $ and a quote to the shell, until the
# same day's trace (#273). So the check reads the source rather than a list
# of sites.
#
# A call site is a string literal that starts (after an optional
# "timeout N " and "/usr/bin/") with one of CREDENTIAL_COMMANDS and is
# followed by `+`. Every operand spliced into that statement must be a call
# (Utils::String::shellQuote(...), std::string(...)) or a literal: a bare
# identifier anywhere in the statement is a value handed to the shell as
# typed, whatever the first operand looks like. Exit 0 when every site
# quotes, 1 when one does not (printed as file:line with the bare names), 2
# when the scan finds no setrootpass site at all -- a scan that finds
# nothing to check has not passed -- or when the check's own cases fail
# (SELF_TEST): a classifier that passes an unquoted value has not passed
# anything either.

import os
import re
import sys

CREDENTIAL_COMMANDS = ("setrootpass", "wifictl connect", "wifictl enable", "wifictl join", "wifictl forget",
                       "cloud_setup --set-syncpath", "--connect", "--host --port")
# A site inside a WIN32-only region is code no ROCKNIX build compiles (fork
# #275: the netplay command has a Windows twin beside the ROCKNIX one); the
# check reads the region markers and leaves those sites alone.
WIN32_IF = re.compile(r'^\s*#\s*(?:if\s+(?:defined\s*\(\s*)?WIN32|ifdef\s+WIN32)\b')
PP_IF = re.compile(r'^\s*#\s*if(?:def|ndef)?\b')
PP_ELSE = re.compile(r'^\s*#\s*(?:else|elif)\b')
PP_ENDIF = re.compile(r'^\s*#\s*endif\b')


def win32_only_lines(text):
    """Line numbers (1-based) inside the taken branch of a #if WIN32 block."""
    out, stack = set(), []   # stack entries: True while inside a WIN32 branch
    for n, line in enumerate(text.split("\n"), 1):
        if PP_IF.match(line):
            stack.append(bool(WIN32_IF.match(line)))
        elif PP_ELSE.match(line) and stack:
            stack[-1] = False
        elif PP_ENDIF.match(line) and stack:
            stack.pop()
        elif any(stack):
            out.add(n)
    return out
SITE = re.compile(r'"((?:timeout \d+ )?(?:/usr/bin/)?(?:%s)) (?:\\")?"\s*\+\s*' % "|".join(re.escape(c) for c in CREDENTIAL_COMMANDS))
OPERAND = re.compile(r'\+\s*([A-Za-z_][\w:]*)\s*(\(?)')
# The calls that make an operand safe: everything else spliced in -- a bare
# name or any other call, SystemConf::getInstance()->get(...) included -- is a
# value handed to the shell as typed. The first cut accepted any call, which
# is how "--host --port " + SystemConf::getInstance()->get(...) read as quoted.
QUOTING_CALLS = ("Utils::String::shellQuote", "cloudShellQuote", "std::to_string")
# std::string(...) copies its argument and quotes nothing, so it is safe only
# around a literal or one of the calls above (#308 8b-es-core gpt F-ES-10:
# it was in QUOTING_CALLS, and "setrootpass " + std::string(pass) read as
# quoted).
WRAPPERS = ("std::string",)
QUOTED_ARG = re.compile(r'\s*(?:"|(?:%s)\s*\()' % "|".join(re.escape(c) for c in QUOTING_CALLS))


def call_argument(statement, open_paren):
    """The text between the '(' at open_paren and its matching ')'."""
    depth, i, in_str = 0, open_paren, False
    while i < len(statement):
        c = statement[i]
        if in_str:
            if c == "\\":
                i += 2; continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return statement[open_paren + 1:i]
        i += 1
    return statement[open_paren + 1:]


def statement_after(text, start):
    """The statement from start to its first semicolon outside a string
    literal -- a ';' inside a command string ("2>&1; echo") or after a
    trailing comment must not end it early or late."""
    i, n, in_str = start, len(text), False
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2; continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == ";":
            return text[start:i]
        i += 1
    return text[start:]


def bare_operands(statement):
    """Every operand spliced into a credential command that is not quoted."""
    bare = []
    for o in OPERAND.finditer(statement):
        name, call = o.group(1), o.group(2)
        if call and name in QUOTING_CALLS:
            continue
        if call and name in WRAPPERS:
            arg = call_argument(statement, o.end() - 1)
            if QUOTED_ARG.match(arg):
                continue
            bare.append("%s(%s)" % (name, arg.strip()))
            continue
        bare.append(name)
    return bare


# The check's own cases (#308 8b-es-core gpt F-ES-10): statements as a call
# site would read, and the operands the check must call bare. Run before the
# scan, every time, so a change to the classifier that lets a value through
# unquoted fails here rather than passing every site.
SELF_TEST = (
    ('"setrootpass " + Utils::String::shellQuote(pass)', []),
    ('"setrootpass " + cloudShellQuote(pass)', []),
    ('"setrootpass " + pass', ["pass"]),
    ('"setrootpass " + SystemConf::getInstance()->get("root.password")', ["SystemConf::getInstance"]),
    ('"--host --port " + std::to_string(port)', []),
    # A std::string built from a value quotes nothing: the constructor copies
    # the bytes, spaces, $ and quotes included.
    ('"setrootpass " + std::string(pass)', ["std::string(pass)"]),
    ('"setrootpass " + std::string(text.c_str())', ["std::string(text.c_str())"]),
    ('"wifictl join " + std::string(Utils::String::shellQuote(name))', []),
    ('"wifictl join " + std::string("--scan")', []),
)


def self_test():
    failed = []
    for statement, want in SELF_TEST:
        got = bare_operands(statement)
        if got != want:
            failed.append("%s: bare %s, expected %s" % (statement, got, want))
    return failed


def main():
    failed = self_test()
    for f in failed:
        print("SELF-TEST FAIL  " + f)
    if failed:
        print("credential-quoting: %d of %d of the check's own cases failed -- the check cannot be trusted" % (len(failed), len(SELF_TEST)))
        return 2
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sites, bare = 0, []
    for sub in ("es-app/src", "es-core/src"):
        for dp, _dirs, files in os.walk(os.path.join(root, sub)):
            for f in files:
                if not f.endswith((".cpp", ".h")):
                    continue
                path = os.path.join(dp, f)
                text = open(path, encoding="utf-8", errors="replace").read()
                skip = win32_only_lines(text)
                for m in SITE.finditer(text):
                    if text.count("\n", 0, m.start()) + 1 in skip:
                        continue
                    sites += 1
                    statement = statement_after(text, m.start())
                    line = text.count("\n", 0, m.start()) + 1
                    bad = bare_operands(statement)
                    if bad:
                        bare.append("%s:%d  %s (bare: %s)" % (os.path.relpath(path, root), line, m.group(1), ", ".join(bad)))
    setrootpass = sum(1 for _ in re.finditer(r'"setrootpass "', "\n".join(
        open(os.path.join(dp, f), encoding="utf-8", errors="replace").read()
        for sub in ("es-app/src", "es-core/src") for dp, _d, fs in os.walk(os.path.join(root, sub))
        for f in fs if f.endswith((".cpp", ".h")))))
    if setrootpass == 0:
        print("credential-quoting: no setrootpass call site found under %s -- the scan is broken, not clean" % root)
        return 2
    for b in bare:
        print("BARE  " + b)
    print("credential-quoting: %d credential call site(s), %d quoted, %d bare" % (sites, sites - len(bare), len(bare)))
    return 1 if bare else 0


if __name__ == "__main__":
    sys.exit(main())
