"""Integration tests that run the real pinned toolchain against small C++
fixtures under tests/fixtures/, to validate server.py's parsing against
genuine gcc/ctest output rather than hand-typed approximations.

Requires Docker (skipped automatically if unavailable). Runs inside the
same toolchain image server.py itself drives (server.run_in_container), so
the compiler/ctest versions exercised here are the ones actually pinned in
docker/Dockerfile.
"""
from __future__ import annotations

import json
import shutil

import pytest

import server

pytestmark = [
    pytest.mark.integration,
    pytest.mark.skipif(shutil.which("docker") is None, reason="Docker is required for fixture integration tests"),
]

FIXTURES_DIR = "mcp/toolchain/tests/fixtures"


def run(cmd: str) -> str:
    server.ensure_image()
    proc = server.run_in_container(cmd)
    return proc.stdout + "\n" + proc.stderr


def test_build_warning_fixture_produces_real_parseable_warning():
    output = run(
        f"arm-none-eabi-g++ -mcpu=cortex-m33 -mthumb -Wall -c "
        f"{FIXTURES_DIR}/build_warning.cpp -o /tmp/toolchain_mcp_warn.o"
    )
    diagnostics = server.parse_diagnostics(output)

    assert len(diagnostics) == 1
    diagnostic = diagnostics[0]
    assert diagnostic["severity"] == "warning"
    assert diagnostic["file"] == f"{FIXTURES_DIR}/build_warning.cpp"
    assert diagnostic["line"] == 2
    assert "unused" in diagnostic["message"]


def test_build_error_fixture_produces_real_parseable_error():
    output = run(
        f"arm-none-eabi-g++ -mcpu=cortex-m33 -mthumb -Wall -c "
        f"{FIXTURES_DIR}/build_error.cpp -o /tmp/toolchain_mcp_err.o"
    )
    diagnostics = server.parse_diagnostics(output)

    assert len(diagnostics) == 1
    diagnostic = diagnostics[0]
    assert diagnostic["severity"] == "error"
    assert diagnostic["file"] == f"{FIXTURES_DIR}/build_error.cpp"
    assert diagnostic["line"] == 2
    assert "undeclared_identifier" in diagnostic["message"]


def test_ctest_fixture_produces_real_parseable_summary_and_failure():
    build_dir = "/tmp/toolchain_mcp_ctest_fixture_build"
    output = run(
        f"cmake -G Ninja -S {FIXTURES_DIR}/ctest_project -B {build_dir} "
        f"-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && "
        f"cmake --build {build_dir} && "
        f"ctest --test-dir {build_dir} --output-on-failure"
    )

    summary_match = server.CTEST_SUMMARY_RE.search(output)
    assert summary_match is not None
    assert int(summary_match["total"]) == 2
    assert int(summary_match["failed"]) == 1

    failures = [m["name"] for m in server.CTEST_FAILED_LINE_RE.finditer(output)]
    assert failures == ["failing_test"]


def test_clang_tidy_fixture_produces_real_parseable_finding():
    # tools/clang-tidy.py itself does all the parsing lint_file() passes
    # through unmodified, so this drives it directly (like build_warning.cpp/
    # build_error.cpp drive the compiler directly) rather than through
    # server.lint_file(), which hardcodes build-docker/build-tests-docker
    # rather than a throwaway fixture build dir.
    #
    # cmake's own configure-step stdout is redirected to /dev/null so that
    # proc.stdout below is exactly tools/clang-tidy.py's JSON output and
    # nothing else -- both commands have to run in the same `docker run`
    # invocation (chained with &&) since build_dir lives under /tmp, which
    # isn't shared across separate --rm container runs.
    server.ensure_image()
    build_dir = "/tmp/toolchain_mcp_clang_tidy_fixture_build"
    fixture_project = f"{FIXTURES_DIR}/clang_tidy_project"
    fixture_file = f"{fixture_project}/nullptr_finding.cpp"

    proc = server.run_in_container(
        f"cmake -G Ninja -S {fixture_project} -B {build_dir} "
        f"-DCMAKE_CXX_COMPILER=arm-none-eabi-g++ "
        f'-DCMAKE_CXX_FLAGS="-mcpu=cortex-m33 -mthumb" '
        f"-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY > /dev/null && "
        # --src-build-dir must precede the positional path: clang-tidy.py's
        # trailing REMAINDER positional (passthrough clang-tidy args) would
        # otherwise swallow a --src-build-dir that comes after the path.
        f"python3 tools/clang-tidy.py --src-build-dir {build_dir} {fixture_file}"
    )

    payload = json.loads(proc.stdout)
    assert payload["summary"]["status"] == "issues_found"

    nullptr_findings = [f for f in payload["findings"] if f["check"] == "modernize-use-nullptr"]
    assert len(nullptr_findings) == 1
    finding = nullptr_findings[0]
    assert finding["file"] == f"/workspace/{fixture_file}"
    assert finding["line"] == 4
    assert finding["severity"] == "warning"
    assert "nullptr" in finding["message"].lower()
