# Godot-Autopilot

[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build](https://github.com/jesspig/Godot-Autopilot/actions/workflows/ci.yml/badge.svg)](https://github.com/jesspig/Godot-Autopilot/actions)
![Godot](https://img.shields.io/badge/Godot-4.7%2B-478CBF?logo=godotengine&logoColor=white)
![Platform](https://img.shields.io/badge/platform-Windows%7CLinux%7CmacOS-lightgrey)
[![Protocol](https://img.shields.io/badge/MCP-Streamable_HTTP-blueviolet)](https://modelcontextprotocol.io/)
![Tools](https://img.shields.io/badge/tools-443%2B-brightgreen)

> **Let an AI agent build, playtest, and debug your Godot game — from inside the editor.**

**English** | [简体中文](README.zh-CN.md) | [繁體中文](README.zh-TW.md) | [日本語](README.ja.md) | [한국어](README.ko.md)

Godot-Autopilot is an [MCP](https://modelcontextprotocol.io/) server that lives **inside the Godot editor** as a plugin. Once installed, your AI coding assistant (Claude Code, Cursor, Codex, OpenCode, …) can operate your project the way you do: build scenes, tweak properties, run the game, press buttons, take screenshots, and read the logs — then iterate on what it sees.

No bridge process, no copy-pasting errors into a chat window. The server starts when your project opens and stops when it closes.

## End-to-end demo

AI controlling a game character in real time during end-to-end tests:

![Demo 001](assets/demo-001.gif)

## What it can do

Over **443 tools** covering the whole editor-to-game loop (the exact count grows with each version — ask the agent to check via `search_tools`). Highlights:

- **Build scenes** — create, rename, move, and delete nodes; read and write properties; connect signals; build 3D block levels with GridMap (e.g. `create_scene_node`, `property_set`, `signal_connect`, `scene_tree_items`; GridMap: `create_gridmap`, `set_gridmap_cell`, `fill_gridmap_box`).
- **Look, sound, and feel** — meshes and materials, lights and environments, audio playback, animations and tweens, UI themes, tilemaps (e.g. `create_animation`, `tween_editor_property`, `create_game_tween`, `play_audio_player`, `fill_tilemap_rect`).
- **Physics and navigation** — ray/shape casts, rigid bodies, navigation maps and path queries (e.g. `intersect_physics_3d_ray`, `create_nav_3d_map`, `get_nav_3d_map_path`).
- **Hands: input and UI automation** — simulate keys, mouse, and gamepad; click editor controls by name; run editor shortcuts; click buttons in the running game (e.g. `click_input_mouse`, `get_editor_ui_elements`, `click_editor_element`, `run_editor_shortcut`, `click_game_ui_element`).
- **Scripts and debugging** — run GDScript in the editor or in the game, launch long game scripts as background jobs, read plugin/game/script-error logs (e.g. `execute_script`, `execute_game_script`, `start_game_job`, `get_plugin_log`).
- **Project and knowledge** — read, search, and copy project files and directories, inspect project settings, query the engine's built-in API docs offline (e.g. `read_file`, `find_in_files`, `list_directory`, `copy_file`, `get_project_settings`, `get_docs_class`).
- **Play and verify visually** — launch the current scene, inject timed input sequences, screenshot the editor and the game, and diff before/after (e.g. `play_editor_current_scene`, `queue_game_input`, `capture_editor_viewport`, `capture_game_viewport`, `review_scene_visually`).

## How the agent works with it

You don't need to memorize tool names. The agent follows a simple loop:

1. **Discover** — `search_tools` to find candidates, `get_tool_detail` to confirm parameters (never guess names).
2. **Act** — `call_tool` for one step, `batch_execute` for a batch.
3. **Verify** — screenshot the result, compare with the previous capture, fix, and repeat.

To make agents good at this out of the box, the plugin ships **9 skill books** (an overview, a calling guide, plus scenes, resources, scripting, runtime, servers, content, and C# volumes) and **7 starter guides** (3D scene, character controller, physics debugging, input map, GUI, …). One click in the **MCP Config** panel renders the skills into your project's `.agents/skills/`. Positioning: MCP is the primary product channel and skills/scripts are the experience layer — the calling guide falls back to a script bridge when MCP is down (needs Node 18+ or Bun) and to a manual guide with no script runtime.

Two editor panels come with the plugin:

- **MCP Config** — server status and port, one-click client configuration, capability switches, skill generation.
- **Log** — plugin-only log with level/category filters and search, so misbehaving calls are easy to trace.

## Getting started

Requires **Godot 4.7+**.

1. **Get the plugin** — download `addons.zip` from the project's GitHub Releases page ([jesspig/Godot-Autopilot](https://github.com/jesspig/Godot-Autopilot)) and copy the `godot-autopilot/` folder into your project's `addons/` directory:
   ```
   your-project/
   └── addons/
       └── godot-autopilot/
           ├── godot-autopilot.dll      (Windows)
           ├── libgodot-autopilot.so    (Linux)
           ├── libgodot-autopilot.dylib (macOS)
           └── godot-autopilot.gdextension
   ```
2. **Open your project** — the server starts automatically on the port from the plugin settings (default `http://127.0.0.1:9527/mcp`). No enable toggle needed.
3. **Connect your AI client** — in the editor's **MCP Config** panel, pick your client and click **Generate**. 20 clients are supported, including Claude Code, Codex, Cursor, OpenCode, and Copilot. Approve the generated config file in the client if it asks.
4. **Unlock powerful capabilities (optional)** — script execution (`code_execute`) and game control (`game_runtime`) are off by default; tick them in the panel when you need them.
5. **Ask for something** — e.g. “create a player character that can run and jump, then playtest it and fix what looks wrong.”

Want to build from source or contribute? See [docs/wiki/](docs/wiki/) (start at [index.md](docs/wiki/index.md), building at [build.md](docs/wiki/build.md)) and [tests/README.md](tests/README.md).

## Command-line access

Prefer a terminal to a chat client? The GDA CLI drives a running GDA instance from the shell — no MCP client integration needed. It is Bun + TypeScript, runs from source, and has **zero runtime dependencies** and no build step.

Run it one-off with `bunx` (quoted on PowerShell, because `#` starts a comment):

```
bunx "github:jesspig/Godot-Autopilot#0.2.7" ping
```

Or install it globally once and use the `gda` command:

```
bun install -g github:jesspig/Godot-Autopilot#0.2.7
gda ping
```

Command families: **calling** — `ping` / `status` / `categories` / `search` / `describe` / `call`; **lifecycle** — `list` / `start` / `stop` / `logs`.

Requires **Bun**; on a machine without Bun or Node, use the standalone `gda` binary from the Releases page instead (see [docs/wiki/modules/cli.md](docs/wiki/modules/cli.md) for the full reference).

## Let an AI set it up for you

Rather than following the steps above by hand, hand them to a coding agent that has shell access on the same machine. Copy the block below, fill in the absolute path of your project, and send it:

```text
Set up Godot Autopilot (GDA) on this machine end to end, then prove it works.

Project: <ABSOLUTE PATH TO YOUR GODOT PROJECT> (Godot 4.7+).

1. Install the plugin: download addons.zip from
   https://github.com/jesspig/Godot-Autopilot/releases/latest and extract it so that
   <project>/addons/godot-autopilot/godot-autopilot.gdextension exists.
2. Start the editor: run `godot --editor --path <project>` (ask me for the engine path if it is
   not on PATH) and wait until http://127.0.0.1:9527/mcp answers.
3. Verify the server from a shell. Try in this order, stop at the first that works, and tell me
   which one it was:
   - bunx "github:jesspig/Godot-Autopilot#0.2.7" ping
   - download the gda release asset for this OS and CPU from the same Releases page, verify it
     against sha256sums.txt, then run ./gda ping
4. Connect my AI client: add
   {"mcpServers":{"godot-autopilot":{"url":"http://127.0.0.1:9527/mcp"}}}
   to its MCP configuration, or tell me to use the plugin's MCP Config panel instead.
5. Summarize: which channel worked, every command you ran, and anything that failed.

Rules: never hand-write HTTP requests against the port; never expose 9527 beyond loopback;
leave code_execute and game_runtime disabled unless I ask for them.
```

If your client supports skills (Claude Code, Codex, Cursor and others do), also point it at this repository's `skills/` directory so the agent gets the full calling protocol instead of improvising.

## Safety notes

The server only listens on your machine's loopback address (port from the plugin settings, 9527 by default, fixed path `/mcp`) and refuses non-local addresses. Script execution, game control, and OS process tools are **denied by default** — enable them only in a local project you trust. Details: [docs/wiki/security_contract.md](docs/wiki/security_contract.md).

## Asset attribution

The 5 sample projects under `demo/` (2D/3D beginner tutorials, UI control gallery, viewport composition, networked Pong) come from the official [Godot demo projects](https://github.com/godotengine/godot-demo-projects); only their assets and clean project configurations are kept for testing. Copyright and licensing details live in each project's README and LICENSE files.

## License

MIT
