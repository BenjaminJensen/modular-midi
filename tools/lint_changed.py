#!/usr/bin/env python3
"""Lint every changed file (see tools/changed_files.py) with tools/clang-tidy.py,
one file at a time, and report one aggregated JSON payload. This is what backs
the CI lint step -- see .github/workflows/ci.yml.

Usage:
    python3 tools/lint_changed.py [--base main] [--src-build-dir build] [--tests-build-dir build-tests]

Composes the two other tools as subprocesses rather than importing them --
clang-tidy.py's hyphenated filename isn't importable as a plain Python
module, and keeping each tool an independently invocable CLI keeps them
decoupled from each other.
"""
from __future__ import annotations

import argparse
import json
import logging
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CHANGED_FILES_SCRIPT = REPO_ROOT / "tools" / "changed_files.py"
CLANG_TIDY_SCRIPT = REPO_ROOT / "tools" / "clang-tidy.py"

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s", stream=sys.stderr)
logger = logging.getLogger("lint-changed")

DEFAULT_EXTENSIONS = [".c", ".cpp"]
# mcp/ holds MCP server code (Python) and, under tests/fixtures/, small C++
# fixtures deliberately compiled ad hoc (see mcp/toolchain/tests/) rather
# than through build/build-tests -- they have no compile_commands.json
# entry by design and would only ever show up here as spurious file_errors.
DEFAULT_EXCLUDE_PREFIXES = ["mcp/"]


class LintChangedError(Exception):
    """Raised when tools/changed_files.py itself fails (bad base ref, git
    failure, ...) -- as opposed to a single file failing to lint, which is
    recorded per-file in the output instead of aborting the whole run."""


def run_json(cmd: list[str]) -> dict:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError as e:
        raise LintChangedError(f"{cmd[1]} did not print valid JSON: {e}\nstdout: {proc.stdout}\nstderr: {proc.stderr}")


def get_changed_files(base_ref: str | None, extensions: list[str], exclude_prefixes: list[str]) -> list[str]:
    cmd = [sys.executable, str(CHANGED_FILES_SCRIPT), "--ext", ",".join(extensions)]
    if base_ref:
        cmd += ["--base", base_ref]
    if exclude_prefixes:
        cmd += ["--exclude", ",".join(exclude_prefixes)]
    payload = run_json(cmd)
    if "error" in payload:
        raise LintChangedError(f"tools/changed_files.py failed: {payload['error']}")
    return payload["files"]


def lint_file(path: str, src_build_dir: str, tests_build_dir: str) -> dict:
    cmd = [
        sys.executable, str(CLANG_TIDY_SCRIPT),
        "--src-build-dir", src_build_dir,
        "--tests-build-dir", tests_build_dir,
        path,
    ]
    return run_json(cmd)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", help="Base ref, passed through to tools/changed_files.py (default: main, falling back to origin/main)")
    parser.add_argument("--src-build-dir", default="build", help="Firmware build dir (default: build)")
    parser.add_argument("--tests-build-dir", default="build-tests", help="Host-test build dir (default: build-tests)")
    parser.add_argument("--ext", help="Comma-separated extensions to lint (default: .c,.cpp)")
    parser.add_argument(
        "--exclude",
        help=f"Comma-separated repo-relative path prefixes to skip (default: {','.join(DEFAULT_EXCLUDE_PREFIXES)})",
    )
    args = parser.parse_args()

    extensions = [e if e.startswith(".") else f".{e}" for e in args.ext.split(",")] if args.ext else DEFAULT_EXTENSIONS
    exclude_prefixes = args.exclude.split(",") if args.exclude else DEFAULT_EXCLUDE_PREFIXES

    try:
        files = get_changed_files(args.base, extensions, exclude_prefixes)
    except LintChangedError as e:
        logger.error(str(e))
        print(json.dumps({
            "files_checked": [],
            "summary": {"status": "error", "total_errors": 0, "total_warnings": 0},
            "findings": [],
            "file_errors": [],
            "error": str(e),
        }, indent=2))
        sys.exit(1)

    logger.info(f"Linting {len(files)} changed file(s): {', '.join(files) if files else '(none)'}")

    all_findings = []
    file_errors = []
    for path in files:
        logger.info(f"-- {path} --")
        result = lint_file(path, args.src_build_dir, args.tests_build_dir)
        if result["summary"]["status"] == "error":
            file_errors.append({"file": path, "error": result.get("error", "unknown error")})
        else:
            all_findings.extend(result["findings"])

    total_errors = sum(1 for f in all_findings if f["severity"] == "error")
    total_warnings = len(all_findings) - total_errors
    status = "clean"
    if file_errors:
        status = "error"
    elif all_findings:
        status = "issues_found"

    payload = {
        "files_checked": files,
        "summary": {
            "status": status,
            "total_errors": total_errors,
            "total_warnings": total_warnings,
        },
        "findings": all_findings,
        "file_errors": file_errors,
    }
    print(json.dumps(payload, indent=2))
    sys.exit(0 if status == "clean" else 1)


if __name__ == "__main__":
    main()
