# MCP servers

Each MCP server exposed to agents working in this repo lives in its own
subdirectory here, with its own `server.py`, `pyproject.toml`, `.venv`, and
`tests/`.

- [`toolchain/`](toolchain/) — wraps the Docker build/test/lint workflow
  (see `CLAUDE.md`'s "Agent workflow: build/test/lint via MCP" section).

Adding a new server: create a new subdirectory here with the same shape
(`server.py` + `pyproject.toml` + `tests/`), and it'll pick up the
`.gitignore` patterns (`mcp/*/.venv/`, `mcp/*/*.egg-info/`,
`mcp/*/.pytest_cache/`) automatically.
