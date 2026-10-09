#!/usr/bin/env python3
"""Fail when a source file grows past the agreed size.

Keeps a codebase navigable: one responsibility per file, split before it becomes a monolith.
This tree is not going to repeat the oversized files of the macOS source it was ported from.

Usage:
    python tools/sizelint.py [--root .] [--soft 400] [--hard 600]

Exits non-zero when a file exceeds the hard limit and is not exempt in `.sizelint-allowlist`.
Files over the soft limit are reported but do not fail the run.
"""

from __future__ import annotations

import argparse
import fnmatch
import sys
from pathlib import Path

EXTENSIONS = {".cpp", ".cc", ".cxx", ".hpp", ".hxx", ".h"}

# Directories whose contents are vendored or generated and never linted.
SKIP_DIRS = {"build", ".git", ".vs", "third_party"}

# Per-prefix overrides of the hard limit (checked before the default).
HARD_OVERRIDES = {"app/win/": 500}


def load_allowlist(path: Path) -> list[str]:
    if not path.is_file():
        return []
    patterns: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("|", 1)[0].strip()
        if line and not line.startswith("#"):
            patterns.append(line)
    return patterns


def is_exempt(rel: str, patterns: list[str]) -> bool:
    return any(fnmatch.fnmatch(rel, pattern) for pattern in patterns)


def hard_limit_for(rel: str, default: int) -> int:
    for prefix, limit in HARD_OVERRIDES.items():
        if rel.startswith(prefix):
            return limit
    return default


def source_files(root: Path):
    for path in sorted(root.rglob("*")):
        if path.suffix.lower() not in EXTENSIONS or not path.is_file():
            continue
        parts = set(path.relative_to(root).parts)
        if parts & SKIP_DIRS:
            continue
        yield path


def count_lines(path: Path) -> int:
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        return sum(1 for _ in handle)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="tree to scan, relative to the repo root")
    parser.add_argument("--soft", type=int, default=400, help="warn above this many lines")
    parser.add_argument("--hard", type=int, default=600, help="fail above this many lines")
    parser.add_argument("--config", default=".sizelint-allowlist", help="exemption file, relative to --root")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    root = (repo_root / args.root).resolve() if not Path(args.root).is_absolute() else Path(args.root)
    if not root.is_dir():
        print(f"sizelint: root not found: {root}", file=sys.stderr)
        return 2

    allow_patterns = load_allowlist((root / args.config) if not Path(args.config).is_absolute() else Path(args.config))

    warnings: list[tuple[Path, int]] = []
    failures: list[tuple[Path, int, int]] = []

    for path in source_files(root):
        rel = path.relative_to(root).as_posix()
        if is_exempt(rel, allow_patterns):
            continue
        lines = count_lines(path)
        hard = hard_limit_for(rel, args.hard)
        if lines > hard:
            failures.append((path, lines, hard))
        elif lines > args.soft:
            warnings.append((path, lines))

    for path, lines in warnings:
        print(f"WARN  {lines:>5} lines  {path.relative_to(root).as_posix()}  (soft limit {args.soft})")

    for path, lines, hard in failures:
        print(f"FAIL  {lines:>5} lines  {path.relative_to(root).as_posix()}  (hard limit {hard})")

    if failures:
        print(f"\nsizelint: {len(failures)} file(s) over the hard limit. Split them or add an exempt entry with a reason.")
        return 1

    print(f"\nsizelint: clean ({len(warnings)} warning(s)).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
