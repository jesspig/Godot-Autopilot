# Godot Autopilot

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/jesspig/Godot-Autopilot)](https://github.com/jesspig/Godot-Autopilot/releases)
[![Build](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
[![Release workflow](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions/workflows/release.yml)
![Godot](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![Platform](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![CLI](https://img.shields.io/badge/CLI-gda-native-blue)](docs/wiki/modules/cli.md)
![Tools](https://img.shields.io/badge/tools-253-brightgreen)

> **Let an AI agent build, playtest, and debug your Godot game — from inside the editor.**

**English** | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md) | [한국어](README.ko.md)

Godot Autopilot (GDA) is a plugin that lives **inside the Godot editor** and exposes a running project through one local `gda` CLI. There is no protocol or endpoint to configure: your AI coding assistant (Claude Code, Cursor, Codex, OpenCode, …) operates the project the way you do by running CLI commands — builds scenes, tweaks properties, runs the game, presses buttons, takes screenshots, and reads logs — then iterates on what it sees. No bridge process and no copy-pasting errors into chat. A graphical editor session makes the project available when it opens and withdraws it when it closes.

![Demo 001](assets/demo-001.gif)

**253 tools** cover the editor-to-game loop: 247 domain tools, 5 meta tools, and `system_status` (the exact count grows with each version; ask the agent to check with `gda tool list`). 325 deprecated tool-name aliases stay resolvable with `gda tool describe` and callable with `gda tool call` until 0.6.0 (names still exposed as real tools win). Skill books (**1 entry card + 9 references**) teach the CLI calling protocol. References: [tool surface](docs/wiki/tool_surface_baseline.md), [skills](docs/wiki/modules/skills.md), [CLI](docs/wiki/modules/cli.md).

## Quick start

Requires **Godot 4.7+** and a graphical editor session.

1. **Install** — run the one-click script from your Godot project root, or follow [INSTALL.md](INSTALL.md) for the manual release and source-build paths.
   - macOS/Linux:

     ```bash
     curl -fsSL https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.sh \
       | bash -s -- --project "$PWD" --skills
     ```

   - Windows PowerShell:

     ```powershell
     & ([scriptblock]::Create((irm https://raw.githubusercontent.com/jesspig/Godot-Autopilot/master/scripts/install.ps1))) `
       -Project (Get-Location).Path -Skills
     ```

   The script installs the `gda` CLI, detects `project.godot`, installs the plugin, and optionally installs skill books.
2. **Open your project** — in a graphical editor session the plugin registers the project with the CLI automatically and binds a free loopback port on first launch (persisted per project, chosen from `20000–29999`). Headless/cmdline editor sessions do not make the project available.
3. **Connect through the CLI** — run `gda ping` and `gda status --format json`; the JSON connection record confirms the live project and its local endpoint. No client configuration is needed: every operation is a `gda` command that prints a JSON envelope. Claude Code, Codex, Cursor, OpenCode, and Copilot can all drive the CLI.
4. **Ask for something** — e.g. “create a player character that can run and jump, then playtest it and fix what looks wrong.”

## Safety notes

The CLI only reaches a loopback instance (a per-project port chosen by the plugin and addressed by the CLI automatically) and refuses non-local addresses. High-risk tools (`destructive`, `process`, `code_execute`, `game_runtime`) require `--confirm` — there is no global allow list. Details: [docs/wiki/security_contract.md](docs/wiki/security_contract.md).

## Documentation

- [INSTALL.md](INSTALL.md) — one-click script, manual release install, source build, and agent instructions.
- [docs/wiki/index.md](docs/wiki/index.md) — project knowledge base: architecture, modules, tests, and conventions.
- [docs/wiki/tool_surface_baseline.md](docs/wiki/tool_surface_baseline.md) — tool surface, aliases, schema and capability baselines.
- [docs/wiki/build.md](docs/wiki/build.md) — build, deploy, and package.
- [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md) — full CLI reference.
- [docs/wiki/modules/skills.md](docs/wiki/modules/skills.md) — skill books and `gda install skills`.
- [tests/README.md](tests/README.md) — test layout and commands.

## License

MIT
