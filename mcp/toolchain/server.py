#!/usr/bin/env python3
"""MCP server wrapping the modular-midi Docker build/test/lint workflow.

Everything here shells out to the same `modular-midi-build` Docker image used
by build-docker.sh/.ps1 (see docker/Dockerfile) — there is no native ARM
toolchain or MSYS2 available under WSL, so the container path is the only
path, not an alternative to the *.ps1 scripts.

Run with: python3 mcp/toolchain/server.py  (stdio transport)
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Any

from fastmcp import FastMCP

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
IMAGE = os.environ.get("TOOLCHAIN_DOCKER_IMAGE", "modular-midi-build")

DIAGNOSTIC_RE = re.compile(
    r"^(?P<file>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+):\s*"
    r"(?P<severity>error|warning):\s*(?P<message>.*)$",
    re.MULTILINE,
)
CTEST_SUMMARY_RE = re.compile(
    r"(?P<pct>\d+)% tests passed, (?P<failed>\d+) tests failed out of (?P<total>\d+)"
)
CTEST_FAILED_LINE_RE = re.compile(r"^\s*\d+\s*-\s*(?P<name>\S+)\s*\((?P<why>\w+)\)\s*$", re.MULTILINE)

LOG_TAIL_LINES = 200

mcp = FastMCP("toolchain")


def ensure_image() -> None:
    inspect = subprocess.run(
        ["docker", "image", "inspect", IMAGE],
        capture_output=True,
    )
    if inspect.returncode != 0:
        subprocess.run(
            ["docker", "build", "-t", IMAGE, "-f", "docker/Dockerfile", "docker/"],
            cwd=REPO_ROOT,
            check=True,
        )


def run_in_container(cmd: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["docker", "run", "--rm", "-v", f"{REPO_ROOT}:/workspace", "-w", "/workspace", IMAGE, "bash", "-c", cmd],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )


def tail(text: str, n: int = LOG_TAIL_LINES) -> str:
    lines = text.splitlines()
    return "\n".join(lines[-n:])


def parse_diagnostics(output: str) -> list[dict[str, Any]]:
    return [
        {
            "file": m["file"],
            "line": int(m["line"]),
            "column": int(m["col"]),
            "severity": m["severity"],
            "message": m["message"].strip(),
        }
        for m in DIAGNOSTIC_RE.finditer(output)
    ]


@mcp.tool()
def build(clean: bool = False) -> dict[str, Any]:
    """Build the hub-master firmware inside the Docker toolchain image.

    Configures + builds into build-docker/ (mirrors build-docker.sh). Set
    clean=True to wipe build-docker/ first and reconfigure from scratch.
    """
    if clean:
        shutil.rmtree(REPO_ROOT / "build-docker", ignore_errors=True)

    ensure_image()

    proc = run_in_container(
        "cmake -G Ninja -B build-docker -DPICO_BOARD=pico2 -DPICO_NO_PICOTOOL=1 "
        "-DPICO_PLATFORM=rp2350 -DCMAKE_BUILD_TYPE=Debug && cmake --build build-docker"
    )
    combined = proc.stdout + "\n" + proc.stderr
    diagnostics = parse_diagnostics(combined)
    errors = [d for d in diagnostics if d["severity"] == "error"]
    warnings = [d for d in diagnostics if d["severity"] == "warning"]

    # PICO_NO_PICOTOOL=1 (set above) disables pico-sdk's UF2 output entirely,
    # not just when picotool is unavailable -- see on_device.cmake's
    # pico_add_extra_outputs. The .elf is what tools/flash.jlink loads
    # anyway, so that's the only artifact worth reporting.
    elf_path = REPO_ROOT / "build-docker" / "src" / "projects" / "hub-master" / "hub_master.elf"

    result: dict[str, Any] = {
        "status": "ok" if proc.returncode == 0 else "failed",
        "exit_code": proc.returncode,
        "errors": errors,
        "warnings": warnings,
        "elf_path": str(elf_path.relative_to(REPO_ROOT)) if elf_path.exists() else None,
        "log_tail": tail(combined),
    }
    return result


@mcp.tool()
def test(filter: str | None = None) -> dict[str, Any]:
    """Run the host-side doctest suite (tests/) inside the Docker toolchain image.

    Configures + builds into build-tests-docker/, then runs ctest. `filter`
    is passed through to ctest as -R <regex> to run a subset of tests.
    """
    ensure_image()

    cmd = (
        "cmake -G Ninja -S tests -B build-tests-docker -DCMAKE_C_COMPILER=gcc "
        "-DCMAKE_CXX_COMPILER=g++ && cmake --build build-tests-docker && "
        "ctest --test-dir build-tests-docker --output-on-failure"
    )
    if filter:
        cmd += f" -R {filter}"

    proc = run_in_container(cmd)
    combined = proc.stdout + "\n" + proc.stderr

    summary_match = CTEST_SUMMARY_RE.search(combined)
    if summary_match is None:
        # ctest never ran (configure/build step failed) or produced no summary.
        return {
            "status": "build_failed" if proc.returncode != 0 else "unknown",
            "exit_code": proc.returncode,
            "total": None,
            "passed": None,
            "failed": None,
            "failures": [],
            "log_tail": tail(combined),
        }

    total = int(summary_match["total"])
    failed_count = int(summary_match["failed"])
    failures = [
        {"name": m["name"], "reason": m["why"]}
        for m in CTEST_FAILED_LINE_RE.finditer(combined)
    ]

    return {
        "status": "ok" if failed_count == 0 and proc.returncode == 0 else "failed",
        "exit_code": proc.returncode,
        "total": total,
        "passed": total - failed_count,
        "failed": failed_count,
        "failures": failures,
        "log_tail": tail(combined) if failed_count > 0 else None,
    }


@mcp.tool()
def lint_changed(base: str = "origin/main") -> dict[str, Any]:
    """Lint every file changed relative to `base` with clang-tidy, inside Docker.

    Wraps tools/lint_changed.py, pointed at build-docker/build-tests-docker
    (the compile databases produced by the `build`/`test` tools above — run
    those first if this reports missing-compile-command errors).
    """
    ensure_image()

    proc = run_in_container(
        f"python3 tools/lint_changed.py --base {base} "
        f"--src-build-dir build-docker --tests-build-dir build-tests-docker"
    )
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {
            "summary": {"status": "error", "total_errors": 0, "total_warnings": 0},
            "findings": [],
            "file_errors": [],
            "error": "tools/lint_changed.py did not print valid JSON",
            "exit_code": proc.returncode,
            "log_tail": tail(proc.stdout + "\n" + proc.stderr),
        }


@mcp.tool()
def lint_file(path: str) -> dict[str, Any]:
    """Lint a single file with clang-tidy, inside Docker.

    Wraps tools/clang-tidy.py, pointed at build-docker/build-tests-docker
    (the compile databases produced by the `build`/`test` tools above — run
    those first if this reports a missing-compile-command error). `path`
    must be an actual translation unit (a .c/.cpp), not a header.
    """
    ensure_image()

    # --src-build-dir/--tests-build-dir must precede the positional path:
    # clang-tidy.py's trailing REMAINDER positional (passthrough clang-tidy
    # args) would otherwise swallow them if they came after the path.
    proc = run_in_container(
        f"python3 tools/clang-tidy.py --src-build-dir build-docker "
        f"--tests-build-dir build-tests-docker {path}"
    )
    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {
            "summary": {"status": "error", "total_errors": 0, "total_warnings": 0},
            "findings": [],
            "error": "tools/clang-tidy.py did not print valid JSON",
            "exit_code": proc.returncode,
            "log_tail": tail(proc.stdout + "\n" + proc.stderr),
        }


if __name__ == "__main__":
    mcp.run()
