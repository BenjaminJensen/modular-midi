# toolchain MCP server

Wraps the Docker build/test/lint workflow (see the repo root `CLAUDE.md`'s
"Agent workflow: build/test/lint via MCP" section) as four MCP tools —
`build`, `test`, `lint_changed`, `lint_file` — for agents to call instead
of shelling out to `build-docker.ps1`/`.sh`, `docker run ... ctest`,
`tools/lint_changed.py`, or `tools/clang-tidy.py` directly.

Registers as `toolchain` in `.mcp.json` / `/mcp`. Everything here shells
out to the `modular-midi-build` Docker image (see `docker/Dockerfile`) —
override the image it targets with the `TOOLCHAIN_DOCKER_IMAGE`
environment variable (used by CI to point at the image the
`build-test-lint` job already built, instead of triggering a second
`docker build`).

## Setup

```sh
python3 -m venv mcp/toolchain/.venv
mcp/toolchain/.venv/bin/pip install -e "mcp/toolchain[dev]"
```

`.mcp.json` expects the venv at exactly that path.

## Tests

```sh
# Fast, mocked unit tests — no Docker needed
mcp/toolchain/.venv/bin/pytest mcp/toolchain/tests -m "not integration"

# Integration tests — build real C++ fixtures (tests/fixtures/) through
# the pinned Docker toolchain, to validate build()/test()'s regex parsing
# and lint_file()'s underlying tools/clang-tidy.py output against genuine
# arm-none-eabi-gcc/ctest/clang-tidy output instead of hand-typed
# approximations. Requires Docker; self-skips if it's unavailable.
mcp/toolchain/.venv/bin/pytest mcp/toolchain/tests -m integration

# Everything
mcp/toolchain/.venv/bin/pytest mcp/toolchain/tests
```

CI runs the mocked suite as its own fast native job (`mcp-tests`), and the
integration suite as an added step inside the existing Docker-based
`build-test-lint` job (reusing the image that job already builds).
