#!/usr/bin/env python3
"""Run clang-tidy over every file in a directory (or a single file), picking
the right compile_commands.json and, for firmware code, the right ARM
cross-compile flags automatically -- same command on Windows and Linux.

Usage:
    python3 tools/clang-tidy-dir.py <path> [-- <extra clang-tidy args>]

<path> is relative to the repo root (or absolute), e.g.:
    python3 tools/clang-tidy-dir.py src/shared/hal/rp2350
    python3 tools/clang-tidy-dir.py tests/shared
    python3 tools/clang-tidy-dir.py src/projects/hub-master/main.cpp

Pointing this at a single header (e.g. src/shared/button.h) works too:
headers aren't their own compiled translation unit, so this searches src/
and tests/ for a .cpp that #includes it (transitively, following the
#include chain if needed), lints through that .cpp instead, and adds
-header-filter to scope the output down to just the header you asked for
(unless you already passed your own -header-filter after --). If no such
.cpp can be found, point this at the containing directory or a specific
.cpp instead.

Requires build/ and build-tests/ to already be configured (run build.ps1 /
test.ps1, or the docker equivalents, first) -- that's what generates
compile_commands.json. Use --src-build-dir/--tests-build-dir to point at
build-docker/build-tests-docker instead.

clang-tidy itself is found via, in order: --clang-tidy if given, PATH
(shutil.which), then -- on Windows only -- the default install location of
the standalone LLVM installer (https://github.com/llvm/llvm-project/releases,
"LLVM-*-win64.exe"), C:\\Program Files\\LLVM\\bin\\clang-tidy.exe. That's the
one tool install this project expects locally now; it replaces the older
MSYS2 UCRT64 clang-tidy package.
"""
from __future__ import annotations

import argparse
import json
import platform
import re
import shutil
import subprocess
import sys
from collections import deque
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# ARM GNU Toolchain release this repo is pinned to (see CLAUDE.md / docker/Dockerfile).
ARM_GCC_VERSION = "14.3.1"

HEADER_EXTS = {".h", ".hpp", ".hh", ".hxx"}
SOURCE_EXTS = {".c", ".cpp"} | HEADER_EXTS
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')


def find_clang_tidy(explicit: str | None) -> str:
    if explicit:
        if Path(explicit).exists():
            return explicit
        sys.exit(f"--clang-tidy path {explicit} does not exist")
    candidates = [shutil.which("clang-tidy")]
    if platform.system() == "Windows":
        candidates.append(r"C:\Program Files\LLVM\bin\clang-tidy.exe")
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return candidate
    sys.exit(
        "clang-tidy not found. Windows: install LLVM from "
        "https://github.com/llvm/llvm-project/releases (LLVM-*-win64.exe) to the "
        "default location, or pass --clang-tidy. Linux: apt package clang-tidy."
    )


def arm_extra_args() -> list[str]:
    if platform.system() == "Windows":
        sysroot = Path("C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.3 rel1/arm-none-eabi")
    else:
        sysroot = Path("/opt/arm-gnu-toolchain/arm-none-eabi")
    if not sysroot.is_dir():
        sys.exit(f"ARM toolchain sysroot not found at {sysroot} (see setup.md / docker/Dockerfile)")
    cxx_inc = sysroot / "include" / "c++" / ARM_GCC_VERSION
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


def files_under(build_dir: Path, target: Path) -> list[Path]:
    return [f for f in db_files(build_dir) if f == target or target in f.parents]


def build_includers_map() -> dict[str, list[Path]]:
    """Maps an included file's basename (e.g. "button.h") to every .c/.cpp/.h file
    under src/ or tests/ whose text contains a #include line naming it.

    This is a textual, one-hop-per-lookup edge list (not a real preprocessor
    include graph) -- good enough to walk transitively via BFS in
    find_header_includer, without needing to invoke a compiler.
    """
    includers: dict[str, list[Path]] = {}
    for base in (REPO_ROOT / "src", REPO_ROOT / "tests"):
        if not base.is_dir():
            continue
        for f in base.rglob("*"):
            if f.suffix not in SOURCE_EXTS:
                continue
            try:
                text = f.read_text(errors="ignore")
            except OSError:
                continue
            for line in text.splitlines():
                m = INCLUDE_RE.match(line)
                if m:
                    includers.setdefault(Path(m.group(1)).name, []).append(f)
    return includers


def find_header_includer(build_dir: Path, includers: dict[str, list[Path]], header: Path) -> Path | None:
    """BFS outward from `header` over the #include edges in `includers`, looking for
    a file that's an actual translation unit in build_dir's compile database."""
    files = db_files(build_dir)
    if not files:
        return None
    frontier = deque([header.name])
    visited = {header.name}
    while frontier:
        name = frontier.popleft()
        for includer in includers.get(name, []):
            resolved = includer.resolve()
            if resolved in files:
                return resolved
            if includer.name not in visited:
                visited.add(includer.name)
                frontier.append(includer.name)
    return None


DIAG_START_RE = re.compile(r"^(?P<file>.+):\d+:\d+: (?:warning|error): ")
DIAG_FILE_RE = re.compile(r"^(?P<file>.+):\d+:\d+: (?:warning|error|note): ")


def filter_diagnostics(text: str, keep: Path) -> str:
    """Keeps only whole diagnostic blocks (warning/error + its note/context lines)
    located in `keep`. Used to scope a header-filtered run down to only the
    diagnostics actually inside the header the caller asked about -- -header-filter
    alone can't do this, since clang-tidy always also reports the main TU's own
    diagnostics regardless of that filter.

    Diagnostics with no location at all (e.g. a macro defined via a command-line
    -D rather than in a source file) can't be attributed to `keep` either way, so
    they're dropped along with everything else that isn't in `keep`.
    """
    lines = text.splitlines(keepends=True)
    blocks: list[list[str]] = []
    current: list[str] | None = None
    for line in lines:
        if DIAG_START_RE.match(line):
            current = [line]
            blocks.append(current)
        elif current is not None:
            current.append(line)

    kept: list[str] = []
    for block in blocks:
        m = DIAG_FILE_RE.match(block[0])
        try:
            same = m and Path(m.group("file")).resolve() == keep
        except OSError:
            same = False
        if same:
            kept.extend(block)
    return "".join(kept)


def run_one(
    clang_tidy: str,
    build_dir: Path,
    extra_args: list[str],
    passthrough: list[str],
    file: Path,
    keep_file: Path | None = None,
) -> bool:
    """Runs clang-tidy on one file. Returns True if it reported anything.

    clang-tidy's own exit code is 0 even when it reports warnings (only
    nonzero on a hard failure like a compile error it can't recover from),
    so -- like tools/lint-diff.sh -- this checks the output text instead.

    `keep_file`, if given, restricts both the printed output and the returned
    bool to diagnostics located in that file (see filter_diagnostics) --
    used when `file` is only the entry point TU for linting some header.
    """
    cmd = [clang_tidy, "-p", str(build_dir)]
    cmd += [f"-extra-arg={a}" for a in extra_args]
    cmd += passthrough
    cmd.append(str(file))
    proc = subprocess.run(cmd, capture_output=True, text=True)
    stdout = filter_diagnostics(proc.stdout, keep_file) if keep_file else proc.stdout
    if stdout.strip():
        print(stdout, end="")
    if proc.stderr.strip():
        print(proc.stderr, end="", file=sys.stderr)
    return proc.returncode != 0 or "warning:" in stdout or "error:" in stdout


def run_single_file(
    clang_tidy: str, src_build_dir: Path, tests_build_dir: Path, target: Path, passthrough: list[str]
) -> None:
    """Fast path for a single explicit file -- no thread pool.

    If `target` is itself a translation unit, runs one clang-tidy invocation
    directly. If it's a header with no compile-db entry of its own, finds a
    .cpp that (transitively) #includes it and lints through that instead,
    scoped down to just this header via -header-filter.
    """
    if files_under(src_build_dir, target):
        build_dir, extra_args = src_build_dir, arm_extra_args()
    elif files_under(tests_build_dir, target):
        build_dir, extra_args = tests_build_dir, []
    else:
        run_header(clang_tidy, src_build_dir, tests_build_dir, target, passthrough)
        return

    print(f"Running clang-tidy on {target}...")
    findings = run_one(clang_tidy, build_dir, extra_args, passthrough, target)
    print(f"\n{'warnings/errors found' if findings else 'clean'}.")
    sys.exit(1 if findings else 0)


def run_header(
    clang_tidy: str, src_build_dir: Path, tests_build_dir: Path, target: Path, passthrough: list[str]
) -> None:
    if target.suffix not in HEADER_EXTS:
        sys.exit(
            f"{target} not found in {src_build_dir}/compile_commands.json or "
            f"{tests_build_dir}/compile_commands.json.\n"
            "Configure the relevant build tree first (build.ps1 / test.ps1, or the "
            "docker equivalents)."
        )

    includers = build_includers_map()
    jobs: list[tuple[Path, Path, list[str]]] = []
    if src_tu := find_header_includer(src_build_dir, includers, target):
        jobs.append((src_tu, src_build_dir, arm_extra_args()))
    if tests_tu := find_header_includer(tests_build_dir, includers, target):
        jobs.append((tests_tu, tests_build_dir, []))

    if not jobs:
        sys.exit(
            f"{target} is a header with no compile-db entry of its own, and no .cpp "
            "under src/ or tests/ was found (even transitively via #include) that "
            "pulls it in. Point clang-tidy at that .cpp directly instead."
        )

    if not any(a.startswith("-header-filter") for a in passthrough):
        passthrough = [*passthrough, f"-header-filter={re.escape(target.name)}$"]

    findings = False
    for tu, build_dir, extra_args in jobs:
        print(f"Running clang-tidy on {target} via {tu.relative_to(REPO_ROOT)} ({build_dir.name})...")
        findings |= run_one(clang_tidy, build_dir, extra_args, passthrough, tu, keep_file=target)

    print(f"\n{'warnings/errors found' if findings else 'clean'}.")
    sys.exit(1 if findings else 0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path", help="File or directory to lint, relative to the repo root")
    parser.add_argument("--src-build-dir", default="build", help="Firmware build dir (default: build)")
    parser.add_argument("--tests-build-dir", default="build-tests", help="Host-test build dir (default: build-tests)")
    parser.add_argument("--clang-tidy", help="Path to clang-tidy binary (default: PATH, then the LLVM installer's default location on Windows)")
    parser.add_argument("clang_tidy_args", nargs=argparse.REMAINDER, help="Passed through to clang-tidy after --")
    args = parser.parse_args()

    passthrough = args.clang_tidy_args
    if passthrough and passthrough[0] == "--":
        passthrough = passthrough[1:]

    target = (REPO_ROOT / args.path).resolve()
    if not target.exists():
        sys.exit(f"{target} does not exist")

    clang_tidy = find_clang_tidy(args.clang_tidy)
    src_build_dir = REPO_ROOT / args.src_build_dir
    tests_build_dir = REPO_ROOT / args.tests_build_dir

    if target.is_file():
        run_single_file(clang_tidy, src_build_dir, tests_build_dir, target, passthrough)
        return

    jobs: list[tuple[Path, Path, list[str]]] = []
    jobs += [(f, src_build_dir, arm_extra_args()) for f in files_under(src_build_dir, target)]
    jobs += [(f, tests_build_dir, []) for f in files_under(tests_build_dir, target)]

    if not jobs:
        sys.exit(
            f"No files under {target} found in "
            f"{src_build_dir}/compile_commands.json or {tests_build_dir}/compile_commands.json.\n"
            "Configure both build trees first (build.ps1 / test.ps1, or the docker "
            "equivalents), and if you're targeting a header, point this at the "
            "directory/.cpp that includes it instead -- clang-tidy needs a "
            "translation unit to enter through."
        )

    def run_job(job: tuple[Path, Path, list[str]]) -> bool:
        file, build_dir, extra_args = job
        return run_one(clang_tidy, build_dir, extra_args, passthrough, file)

    print(f"Running clang-tidy on {len(jobs)} file(s)...")
    with ThreadPoolExecutor() as pool:
        results = list(pool.map(run_job, jobs))

    findings = sum(results)
    print(f"\n{len(jobs)} file(s) checked, {findings} with warnings/errors.")
    sys.exit(1 if findings else 0)


if __name__ == "__main__":
    main()
