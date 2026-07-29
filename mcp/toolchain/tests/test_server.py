"""Mocked unit tests for server.py's parsing/branching logic.

Everything here mocks at the subprocess boundary (server.run_in_container /
server.ensure_image) — no Docker involved, sub-second. See
test_fixture_integration.py for tests that run the real toolchain.
"""
from __future__ import annotations

import subprocess

import server


def fake_proc(stdout: str = "", stderr: str = "", returncode: int = 0) -> subprocess.CompletedProcess:
    return subprocess.CompletedProcess(args=[], returncode=returncode, stdout=stdout, stderr=stderr)


# --- parse_diagnostics -------------------------------------------------

def test_parse_diagnostics_extracts_errors_and_warnings():
    output = (
        "src/shared/button.cpp:42:9: warning: unused variable 'x' [-Wunused-variable]\n"
        "src/shared/logger.h:10:3: error: 'foo' was not declared in this scope\n"
    )
    diagnostics = server.parse_diagnostics(output)
    assert diagnostics == [
        {
            "file": "src/shared/button.cpp",
            "line": 42,
            "column": 9,
            "severity": "warning",
            "message": "unused variable 'x' [-Wunused-variable]",
        },
        {
            "file": "src/shared/logger.h",
            "line": 10,
            "column": 3,
            "severity": "error",
            "message": "'foo' was not declared in this scope",
        },
    ]


def test_parse_diagnostics_no_matches_returns_empty_list():
    assert server.parse_diagnostics("Ninja is building your project...\ndone.\n") == []


# --- tail ----------------------------------------------------------------

def test_tail_truncates_to_last_n_lines():
    text = "\n".join(f"line {i}" for i in range(300))
    result = server.tail(text, n=200)
    assert result.splitlines() == [f"line {i}" for i in range(100, 300)]


def test_tail_passes_through_short_input():
    text = "line 1\nline 2"
    assert server.tail(text, n=200) == text


# --- ensure_image ----------------------------------------------------------

def test_ensure_image_builds_when_missing(monkeypatch):
    calls = []

    def fake_run(cmd, **kwargs):
        calls.append(cmd)
        if cmd[:3] == ["docker", "image", "inspect"]:
            return fake_proc(returncode=1)
        return fake_proc(returncode=0)

    monkeypatch.setattr(server.subprocess, "run", fake_run)
    server.ensure_image()

    assert calls[0] == ["docker", "image", "inspect", server.IMAGE]
    assert calls[1] == ["docker", "build", "-t", server.IMAGE, "-f", "docker/Dockerfile", "docker/"]


def test_ensure_image_skips_build_when_present(monkeypatch):
    calls = []

    def fake_run(cmd, **kwargs):
        calls.append(cmd)
        return fake_proc(returncode=0)

    monkeypatch.setattr(server.subprocess, "run", fake_run)
    server.ensure_image()

    assert len(calls) == 1
    assert calls[0] == ["docker", "image", "inspect", server.IMAGE]


# --- build() ---------------------------------------------------------------

def test_build_reports_failed_with_errors(monkeypatch, tmp_path):
    monkeypatch.setattr(server, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(
        server,
        "run_in_container",
        lambda cmd: fake_proc(
            stdout="",
            stderr="src/main.cpp:5:1: error: expected ';' before '}' token\n",
            returncode=1,
        ),
    )

    result = server.build()

    assert result["status"] == "failed"
    assert result["exit_code"] == 1
    assert len(result["errors"]) == 1
    assert result["errors"][0]["message"] == "expected ';' before '}' token"
    assert result["warnings"] == []
    assert result["elf_path"] is None


def test_build_reports_ok_with_elf_path(monkeypatch, tmp_path):
    monkeypatch.setattr(server, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(server, "run_in_container", lambda cmd: fake_proc(returncode=0))

    elf_dir = tmp_path / "build-docker" / "src" / "projects" / "hub-master"
    elf_dir.mkdir(parents=True)
    (elf_dir / "hub_master.elf").write_bytes(b"")

    result = server.build()

    assert result["status"] == "ok"
    assert result["errors"] == []
    assert result["elf_path"] == "build-docker/src/projects/hub-master/hub_master.elf"


def test_build_clean_removes_build_docker_first(monkeypatch, tmp_path):
    monkeypatch.setattr(server, "REPO_ROOT", tmp_path)
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(server, "run_in_container", lambda cmd: fake_proc(returncode=0))

    rmtree_calls = []
    monkeypatch.setattr(server.shutil, "rmtree", lambda path, **kw: rmtree_calls.append(path))

    server.build(clean=True)

    assert rmtree_calls == [tmp_path / "build-docker"]


# --- test() ------------------------------------------------------------

def test_test_reports_ok_when_all_pass(monkeypatch):
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(
        server,
        "run_in_container",
        lambda cmd: fake_proc(stdout="100% tests passed, 0 tests failed out of 3\n", returncode=0),
    )

    result = server.test()

    assert result["status"] == "ok"
    assert result["total"] == 3
    assert result["passed"] == 3
    assert result["failed"] == 0
    assert result["failures"] == []


def test_test_reports_failures(monkeypatch):
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    stdout = (
        "66% tests passed, 1 tests failed out of 3\n"
        "\n"
        "The following tests FAILED:\n"
        "\t  2 - button_double_tap (Failed)\n"
    )
    monkeypatch.setattr(server, "run_in_container", lambda cmd: fake_proc(stdout=stdout, returncode=1))

    result = server.test()

    assert result["status"] == "failed"
    assert result["total"] == 3
    assert result["passed"] == 2
    assert result["failed"] == 1
    assert result["failures"] == [{"name": "button_double_tap", "reason": "Failed"}]
    assert result["log_tail"] is not None


def test_test_reports_build_failed_when_no_summary(monkeypatch):
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(
        server,
        "run_in_container",
        lambda cmd: fake_proc(stderr="CMake Error: configure failed\n", returncode=1),
    )

    result = server.test()

    assert result["status"] == "build_failed"
    assert result["total"] is None
    assert result["failures"] == []


# --- lint_changed() ------------------------------------------------------

def test_lint_changed_passes_through_valid_json(monkeypatch):
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    payload = '{"summary": {"status": "ok", "total_errors": 0, "total_warnings": 0}, "findings": []}'
    monkeypatch.setattr(server, "run_in_container", lambda cmd: fake_proc(stdout=payload, returncode=0))

    result = server.lint_changed()

    assert result == {
        "summary": {"status": "ok", "total_errors": 0, "total_warnings": 0},
        "findings": [],
    }


def test_lint_changed_reports_error_on_malformed_json(monkeypatch):
    monkeypatch.setattr(server, "ensure_image", lambda: None)
    monkeypatch.setattr(
        server,
        "run_in_container",
        lambda cmd: fake_proc(stdout="not json", stderr="traceback...", returncode=1),
    )

    result = server.lint_changed()

    assert result["summary"]["status"] == "error"
    assert result["error"] == "tools/lint_changed.py did not print valid JSON"
    assert result["exit_code"] == 1
    assert "not json" in result["log_tail"]
