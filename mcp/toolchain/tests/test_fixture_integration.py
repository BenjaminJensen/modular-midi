"""Integration tests that run the real pinned toolchain against small C++
fixtures under tests/fixtures/, to validate server.py's parsing against
genuine gcc/ctest output rather than hand-typed approximations.

Requires Docker (skipped automatically if unavailable). Runs inside the
same toolchain image server.py itself drives (server.run_in_container), so
the compiler/ctest versions exercised here are the ones actually pinned in
docker/Dockerfile.
"""
from __future__ import annotations

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
