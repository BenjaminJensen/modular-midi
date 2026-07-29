#!/usr/bin/env python3
"""List files changed relative to a base branch, for feeding into per-file
tools like tools/clang-tidy.py instead of diff-hunk-based linting.

"Changed" is the union of two sets, since either one alone misses a real
case:
  - every file touched by a commit already on this branch since it diverged
    from --base (what CI sees: a clean checkout with nothing uncommitted)
  - every file with uncommitted changes right now, staged or not, including
    untracked files (what a developer or agent mid-edit sees, before
    they've committed anything)
Deleted files (in either set) are dropped by simply checking the path still
exists on disk -- cheaper and more robust than interpreting git's status
letters (M/A/D/R/C) ourselves.

Usage:
    python3 tools/changed_files.py [--base main] [--ext .cpp,.h] [--exclude mcp/,other/prefix/]

Prints a single JSON document to stdout: {"base_ref": ..., "files": [...]}.
No filtering is applied by default -- pass --ext to scope to specific
extensions (e.g. the source-file extensions clang-tidy.py can act on), and
--exclude to drop any changed file whose repo-relative path starts with one
of the given prefixes.
"""
from __future__ import annotations

import argparse
import json
import logging
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s", stream=sys.stderr)
logger = logging.getLogger("changed-files")


class ChangedFilesError(Exception):
    """Raised for setup/usage failures (bad base ref, git itself failing, ...).
    Always caught in main() and turned into a JSON error payload, same
    convention as tools/clang-tidy.py."""


def run_git(args: list[str]) -> str:
    proc = subprocess.run(["git", *args], cwd=REPO_ROOT, capture_output=True, text=False)
    if proc.returncode != 0:
        raise ChangedFilesError(f"git {' '.join(args)} failed: {proc.stderr.decode(errors='replace').strip()}")
    return proc.stdout.decode(errors="replace")


def resolve_base_ref(explicit: str | None) -> str:
    if explicit:
        return explicit
    for candidate in ("main", "origin/main"):
        if subprocess.run(
            ["git", "rev-parse", "--verify", "--quiet", candidate],
            cwd=REPO_ROOT, capture_output=True,
        ).returncode == 0:
            return candidate
    raise ChangedFilesError(
        "Neither local 'main' nor 'origin/main' could be resolved -- pass --base explicitly."
    )


def committed_diff_paths(base_ref: str) -> set[Path]:
    """Every path touched by a commit on this branch since it diverged from
    base_ref (git diff --name-status, not just --name-only, since renames
    emit an extra old-path token that --name-status lets us recognize)."""
    merge_base_proc = subprocess.run(
        ["git", "merge-base", base_ref, "HEAD"], cwd=REPO_ROOT, capture_output=True, text=True,
    )
    if merge_base_proc.returncode != 0:
        raise ChangedFilesError(
            f"Could not find a merge base between {base_ref!r} and HEAD: {merge_base_proc.stderr.strip()}"
        )
    merge_base = merge_base_proc.stdout.strip()

    output = run_git(["diff", "--name-status", "-z", f"{merge_base}...HEAD"])
    tokens = output.split("\0")[:-1]  # trailing element after the last NUL is empty

    paths: set[Path] = set()
    i = 0
    while i < len(tokens):
        status = tokens[i]
        i += 1
        if status[0] in ("R", "C"):
            paths.add(REPO_ROOT / tokens[i])
            paths.add(REPO_ROOT / tokens[i + 1])
            i += 2
        else:
            paths.add(REPO_ROOT / tokens[i])
            i += 1
    return paths


def working_tree_paths() -> set[Path]:
    """Every path with an uncommitted change right now -- staged, unstaged,
    or untracked (git status --porcelain=v1 -z). --untracked-files=all makes
    git list individual files inside a new untracked directory instead of
    collapsing it to one "dir/" entry, which would otherwise hide new files
    inside a directory that isn't tracked yet."""
    output = run_git(["status", "--porcelain=v1", "-z", "--untracked-files=all"])
    tokens = output.split("\0")[:-1]

    paths: set[Path] = set()
    i = 0
    while i < len(tokens):
        entry = tokens[i]
        i += 1
        status, path = entry[:2], entry[3:]
        paths.add(REPO_ROOT / path)
        if "R" in status or "C" in status:
            paths.add(REPO_ROOT / tokens[i])  # old path, pre-rename
            i += 1
    return paths


def changed_files(base_ref: str, extensions: list[str] | None, exclude_prefixes: list[str] | None = None) -> list[str]:
    all_paths = committed_diff_paths(base_ref) | working_tree_paths()
    existing = (p for p in all_paths if p.is_file())
    if extensions:
        existing = (p for p in existing if p.suffix in extensions)
    rel_paths = sorted(p.relative_to(REPO_ROOT).as_posix() for p in existing)
    if exclude_prefixes:
        rel_paths = [p for p in rel_paths if not any(p.startswith(prefix) for prefix in exclude_prefixes)]
    return rel_paths


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", help="Base ref to diff committed history against (default: main, falling back to origin/main)")
    parser.add_argument("--ext", help="Comma-separated list of extensions to keep, e.g. .cpp,.h (default: no filtering)")
    parser.add_argument("--exclude", help="Comma-separated repo-relative path prefixes to drop, e.g. mcp/ (default: no exclusion)")
    args = parser.parse_args()

    extensions = [e if e.startswith(".") else f".{e}" for e in args.ext.split(",")] if args.ext else None
    exclude_prefixes = args.exclude.split(",") if args.exclude else None

    try:
        base_ref = resolve_base_ref(args.base)
        files = changed_files(base_ref, extensions, exclude_prefixes)
        payload = {"base_ref": base_ref, "files": files}
        exit_code = 0
    except ChangedFilesError as e:
        logger.error(str(e))
        payload = {"base_ref": None, "files": [], "error": str(e)}
        exit_code = 1

    print(json.dumps(payload, indent=2))
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
