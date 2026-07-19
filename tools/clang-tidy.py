#!/usr/bin/env python3
"""Run clang-tidy on a single file, picking the right compile_commands.json
and, for firmware code, the right ARM cross-compile flags automatically --
same command on Windows and Linux.

Usage:
    python3 tools/clang-tidy.py <path> [-- <extra clang-tidy args>]

<path> is relative to the repo root (or absolute), and must be an actual
translation unit (a .c/.cpp with its own entry in a compile_commands.json) --
e.g.:
    python3 tools/clang-tidy.py src/projects/hub-master/main.cpp
    python3 tools/clang-tidy.py tests/shared/button_test.cpp

Requires build/ and build-tests/ to already be configured (run build.ps1 /
test.ps1, or the docker equivalents, first) -- that's what generates
compile_commands.json. Use --src-build-dir/--tests-build-dir to point at
build-docker/build-tests-docker instead.

Machine-specific tool paths (the ARM GNU Toolchain sysroot, and clang-tidy's
own fallback location on Windows) live in tools/paths.json, not in this
script -- see that file to update them for a different toolchain version or
install location.

clang-tidy itself is found via, in order: --clang-tidy if given, PATH
(shutil.which), then -- on Windows only -- the path in tools/paths.json.
"""
from __future__ import annotations

import argparse
import json
import logging
import platform
import shutil
import subprocess
import sys
from pathlib import Path
import re

REPO_ROOT = Path(__file__).resolve().parent.parent
PATHS_CONFIG = REPO_ROOT / "tools" / "paths.json"

logging.basicConfig(level=logging.WARNING, format="%(levelname)s: %(message)s", stream=sys.stderr)
logger = logging.getLogger("clang-tidy")


class ClangTidyToolError(Exception):
    """Raised for setup/usage failures (bad config, missing binary, file not in
    any compile database, ...) -- as opposed to clang-tidy itself running fine
    and reporting findings. Always caught in main() and turned into the same
    JSON error payload other outcomes are reported through, so stdout stays a
    single parseable JSON document no matter how the run ends."""


def load_paths_config() -> dict:
    return json.loads(PATHS_CONFIG.read_text())


def platform_key() -> str:
    return "windows" if platform.system() == "Windows" else "linux"


def find_clang_tidy(explicit: str | None, config: dict) -> str:
    if explicit:
        if Path(explicit).exists():
            return explicit
        raise ClangTidyToolError(f"--clang-tidy path {explicit} does not exist")
    which = shutil.which("clang-tidy")
    if which:
        return which
    if platform.system() == "Windows":
        fallback = config["windows"]["clang_tidy"]
        if Path(fallback).exists():
            return fallback
    raise ClangTidyToolError(
        "clang-tidy not found. Windows: install LLVM from "
        "https://github.com/llvm/llvm-project/releases (LLVM-*-win64.exe) to the "
        "default location (see tools/paths.json), or pass --clang-tidy. "
        "Linux: apt package clang-tidy."
    )


def arm_extra_args(config: dict) -> list[str]:
    platform_config = config[platform_key()]
    sysroot = Path(platform_config["arm_gcc_sysroot"])
    if not sysroot.is_dir():
        raise ClangTidyToolError(f"ARM toolchain sysroot not found at {sysroot} (see tools/paths.json / setup.md)")
    cxx_inc = sysroot / "include" / "c++" / config["arm_gcc_version"]
    cxx_target_inc = cxx_inc / "arm-none-eabi" / "thumb" / "v8-m.main+fp" / "hard"
    return [
        "--target=arm-none-eabi",
        f"--sysroot={sysroot}",
        f"-isystem{cxx_inc}",
        f"-isystem{cxx_target_inc}",
    ]


def db_files(build_dir: Path) -> set[Path]:
    """Every file with its own entry (translation unit) in build_dir's compile database."""
    db_path = build_dir / "compile_commands.json"
    if not db_path.exists():
        return set()
    entries = json.loads(db_path.read_text())
    files = set()
    for entry in entries:
        f = Path(entry["file"])
        if not f.is_absolute():
            f = (Path(entry["directory"]) / f).resolve()
        files.add(f)
    return files

def parse_clang_tidy_output(raw_stdout: str) -> str:
    """
    Parses raw clang-tidy terminal output, extracts core diagnostics,
    and returns a compact JSON string to minimize agent token usage.
    """
    # Regex breakdown:
    # 1. ^(.+): Capture file path, greedy so it also swallows a Windows drive
    #    letter's colon (e.g. "F:\...\main.cpp") -- backtracks to the last
    #    ":line:col:" in the line rather than stopping at the first colon.
    # 2. :(\d+):(\d+): Capture line and column numbers
    # 3. \s*(warning|error|note): Capture the severity level
    # 4. \s*(.*?): Capture the main descriptive message
    # 5. \s*\[([^\]]+)\]$: Capture the specific clang-tidy check name inside brackets
    pattern = re.compile(
        r'^(.+):(\d+):(\d+):\s*(warning|error|note):\s*(.*?)\s*\[([^\]]+)\]$'
    )
    
    findings = []
    total_warnings = 0
    total_errors = 0
    
    # Process line by line to extract matches
    for line in raw_stdout.splitlines():
        line = line.strip()
        match = pattern.match(line)
        
        if match:
            file_path, line_num, col_num, severity, message, check_name = match.groups()
            
            # Track metrics
            if severity == "error":
                total_errors += 1
            else:
                total_warnings += 1
                
            findings.append({
                "file": file_path,
                "line": int(line_num),
                "column": int(col_num),
                "severity": severity,
                "check": check_name,
                "message": message
            })
            
    # Package into a dense context payload
    payload = {
        "summary": {
            "status": "issues_found" if findings else "clean",
            "total_errors": total_errors,
            "total_warnings": total_warnings
        },
        "findings": findings
    }
    
    return json.dumps(payload, indent=2)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path", help="File to lint, relative to the repo root")
    parser.add_argument("--src-build-dir", default="build", help="Firmware build dir (default: build)")
    parser.add_argument("--tests-build-dir", default="build-tests", help="Host-test build dir (default: build-tests)")
    parser.add_argument("--clang-tidy", help="Path to clang-tidy binary (default: PATH, then tools/paths.json's Windows fallback)")
    parser.add_argument("clang_tidy_args", nargs=argparse.REMAINDER, help="Passed through to clang-tidy after --")
    args = parser.parse_args()

    passthrough = args.clang_tidy_args
    if passthrough and passthrough[0] == "--":
        passthrough = passthrough[1:]

    try:
        payload = run(args, passthrough)
        exit_code = 1 if payload["summary"]["status"] != "clean" else 0
    except ClangTidyToolError as e:
        logger.error(str(e))
        payload = {
            "summary": {"status": "error", "total_errors": 0, "total_warnings": 0},
            "findings": [],
            "error": str(e),
        }
        exit_code = 1

    print(json.dumps(payload, indent=2))
    sys.exit(exit_code)


def run(args: argparse.Namespace, passthrough: list[str]) -> dict:
    """Runs clang-tidy on args.path and returns the result payload dict.
    Raises ClangTidyToolError for any setup/usage failure; never calls
    sys.exit itself -- that's main()'s job, in exactly one place, so every
    outcome (success, findings, or a raised error) funnels through the same
    JSON-printing path."""
    target = (REPO_ROOT / args.path).resolve()
    if not target.exists():
        raise ClangTidyToolError(f"{target} does not exist")
    if not target.is_file():
        raise ClangTidyToolError(f"{target} is a directory -- this script only supports a single file for now")

    config = load_paths_config()
    clang_tidy = find_clang_tidy(args.clang_tidy, config)
    src_build_dir = REPO_ROOT / args.src_build_dir
    tests_build_dir = REPO_ROOT / args.tests_build_dir

    if target in db_files(src_build_dir):
        build_dir, extra_args = src_build_dir, arm_extra_args(config)
    elif target in db_files(tests_build_dir):
        build_dir, extra_args = tests_build_dir, []
    else:
        raise ClangTidyToolError(
            f"{target} not found in {src_build_dir}/compile_commands.json or "
            f"{tests_build_dir}/compile_commands.json.\n"
            "Configure the relevant build tree first (build.ps1 / test.ps1, or the "
            "docker equivalents). Note: headers have no compile-db entry of their "
            "own -- point this at the .cpp translation unit instead."
        )

    cmd = [clang_tidy, "-p", str(build_dir)]
    cmd += [f"-extra-arg={a}" for a in extra_args]
    cmd += passthrough
    cmd.append(str(target))

    logger.info(f"Running clang-tidy on {args.path}...")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.stderr.strip():
        logger.warning(proc.stderr.strip())

    payload = json.loads(parse_clang_tidy_output(proc.stdout))
    if proc.returncode != 0 and payload["summary"]["status"] == "clean":
        raise ClangTidyToolError(
            f"clang-tidy exited with code {proc.returncode} but reported no diagnostics -- "
            "likely a compile error clang-tidy couldn't recover from. See the logged stderr above."
        )
    return payload


if __name__ == "__main__":
    main()
