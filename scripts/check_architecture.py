#!/usr/bin/env python3
"""Local architecture guardrails for the Realm refactor.

This intentionally enforces only rules that have already been migrated, so it
can run in the current transitional codebase without blocking planned work.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


RULES: list[tuple[str, list[str], re.Pattern[str]]] = [
    (
        "command/domain code must emit events instead of calling UI helpers directly",
        [
            "src/core/*_service.cpp",
            "src/commands/orders.cpp",
            "src/commands/command_resolver.cpp",
            "src/commands/command_dispatcher.cpp",
        ],
        re.compile(r"\b(setStatus|addActionMarker)\s*\("),
    ),
    (
        "commands must not pack coordinates into integer payload fields",
        [
            "src/commands/*.h",
            "src/commands/*.cpp",
            "src/render/sdl/gfx_renderer.cpp",
        ],
        re.compile(r"\bgroupIndex\b|<<\s*16|&\s*0xffff"),
    ),
]


def iter_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        files.extend(ROOT.glob(pattern))
    return sorted({path for path in files if path.is_file()})


def main() -> int:
    failures: list[str] = []
    for description, patterns, regex in RULES:
        for path in iter_files(patterns):
            rel = path.relative_to(ROOT)
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                if regex.search(line):
                    failures.append(f"{rel}:{line_no}: {description}: {line.strip()}")

    if failures:
        print("Architecture check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print("Architecture check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
