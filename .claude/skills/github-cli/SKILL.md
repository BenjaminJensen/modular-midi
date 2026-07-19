---
name: github-cli
description: Interact with github via the github cli "gh" command.
---

## Core Rules
1. ALWAYS use the `--json` flag when querying data (e.g., `gh pr list`, `gh issue view`) to ensure the output is cleanly structured for your internal parser.
2. ALWAYS use non-interactive flags like `-y` or `--yes` (e.g., `gh pr create --yes`) to prevent the terminal from hanging on confirmation prompts.
3. NEVER execute destructive commands such as `gh repo delete` or modifying branch protection rules without explicit user approval.
4. If a command fails due to a missing permission, output the exact error and stop. Do not attempt to bypass it.

## Output Formatting
When fetching data from GitHub, always select only the specific fields you need to minimize token waste.
Example: `gh pr list --json number,title,headRefName`
