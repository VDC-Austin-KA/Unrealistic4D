#!/usr/bin/env python3
"""Fast static validator for the Earth4D command layer, run as a PostToolUse hook.

Reads the hook's JSON event on stdin, and ONLY acts when the edited file is one of
the command-layer files. It catches the recurring, compile-only bug classes cheaply
(in milliseconds) so they don't survive to a multi-minute UE build:

  * Earth4DTools.cpp  — the tool schema must be valid JSON, and every advertised
                        tool must have a dispatch handler (and vice-versa).
  * Earth4DSubsystem  — every BlueprintCallable verb declared in the header must
                        have a definition in the .cpp (catches decl/def drift).

A non-zero exit with stderr is surfaced to the model as feedback. Anything else
(other files, parse hiccups) is a silent no-op so the hook never gets in the way.
"""
import json
import re
import sys
from pathlib import Path


def _read_event():
    try:
        return json.load(sys.stdin)
    except Exception:
        return {}


def _edited_path(event):
    ti = event.get("tool_input") or {}
    return ti.get("file_path") or ti.get("path") or ""


def check_tools(repo: Path):
    f = repo / "Earth4D/Source/Earth4DRuntime/Private/Earth4DTools.cpp"
    if not f.exists():
        return []
    src = f.read_text(encoding="utf-8", errors="ignore")
    problems = []
    blocks = re.findall(r'R"JSON\((.*?)\)JSON"', src, re.S)
    try:
        data = json.loads("".join(blocks))
        names = {t["name"] for t in data}
    except Exception as e:
        return [f"Earth4DTools.cpp: tool schema is not valid JSON ({e}). "
                f"Check raw-string blocks / trailing commas."]
    handled = set(re.findall(r'ToolName == TEXT\("([a-z_]+)"\)', src))
    missing = sorted(names - handled)
    orphan = sorted(handled - names)
    if missing:
        problems.append(f"Earth4DTools.cpp: advertised but NOT handled in Dispatch: {missing}")
    if orphan:
        problems.append(f"Earth4DTools.cpp: handled but NOT advertised in the schema: {orphan}")
    return problems


def check_subsystem(repo: Path):
    h = repo / "Earth4D/Source/Earth4DRuntime/Public/Earth4DSubsystem.h"
    c = repo / "Earth4D/Source/Earth4DRuntime/Private/Earth4DSubsystem.cpp"
    if not (h.exists() and c.exists()):
        return []
    htext = h.read_text(encoding="utf-8", errors="ignore")
    ctext = c.read_text(encoding="utf-8", errors="ignore")
    # Verbs returning FEarth4DResult, declared on one line in the header.
    decls = set(re.findall(r'FEarth4DResult\s+([A-Za-z0-9_]+)\s*\(', htext))
    problems = []
    for v in sorted(decls):
        if f"::{v}(" not in ctext:
            problems.append(f"Earth4DSubsystem: verb '{v}' is declared but has no definition in the .cpp")
    return problems


def main():
    event = _read_event()
    path = _edited_path(event).replace("\\", "/")
    repo = Path(__file__).resolve().parent.parent

    problems = []
    if path.endswith("Earth4DTools.cpp"):
        problems += check_tools(repo)
    if "Earth4DSubsystem" in path:
        problems += check_subsystem(repo)

    if problems:
        sys.stderr.write("Earth4D command-layer validation found issues:\n  - " +
                         "\n  - ".join(problems) + "\n")
        sys.exit(2)  # exit 2 → surfaced to the model as actionable feedback
    sys.exit(0)


if __name__ == "__main__":
    main()
