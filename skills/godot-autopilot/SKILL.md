---
name: godot-autopilot
description: "Single entry skill for Godot Autopilot (GDA): connect through gda status --json and mcp_url, look up engine APIs with gda docs, discover live tools with gda search/describe, call them with gda call, respect confirmation and the error watermark, and follow the change-observe-verify loop. Read the matching reference for setup, tools, scenes, scripting, resources, runtime, content, C#, or servers."
---

# Godot Autopilot

Godot Autopilot (GDA) is an in-process GDExtension that exposes a running Godot
editor through a local `gda` CLI and an MCP endpoint. This card is the only entry
point. Read the reference that matches the task; do not load all of them.

## Connection model

- The `gda` CLI is the first-class interface. It discovers the plugin's random
  port, prefers IPC when available, falls back to HTTP, and prints JSON.
- The plugin owns the port and persists it. Never choose a listen port, assume a
  default, or reuse a port across editor restarts.
- `gda status --json` is the connection record. `mcp_url` is the only
  authoritative endpoint for an MCP client; `transport` and `ipc_kind` describe
  the local channel the CLI used.
- The server is loopback-only. Never forward, proxy, or publish its port.
- If no instance is visible, `gda list --all` shows known projects and
  `gda start --project <dir>` launches the editor. Re-read `mcp_url` after every
  restart.
- The MCP face is the compatibility channel for clients that already list native
  tools: `search_tools`, `get_tool_detail`, `call_tool`, `batch_execute`, and
  `code_execute`. The same rules apply; the CLI is the default when the client
  has no native tools.

## Engine documentation

Never write an unfamiliar engine API from memory. Property names, method
signatures, enum values, and defaults change between Godot versions, and a
recalled signature is a common source of silent failures.

- Before writing or patching a script that touches an unfamiliar API, setting a
  property whose exact name or accepted values are unclear, or chasing an
  "unknown property" or parse error, run `gda docs search <query>`.
- Then narrow with `gda docs class <name> --member <text>` or
  `gda docs member <class> <name>`.
- Docs come from the local engine binary and are cached by engine minor under
  the GDA home. The first query may export the cache; later queries are fast.
- If the project pin and the available binary disagree, the command fails and
  suggests `--godot`. Pass a matching Godot binary instead of guessing.
- `gda docs status` shows the project pin, resolved engine, and cache state.
  `gda docs sync` forces an export; `--docs-file <path>` reads an external dump.

## Discover tools, never guess names

Tool names are snake_case and usually verb-first, but segment counts vary and
names change as the toolset grows. Discovery is mandatory.

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"path": "Level", "cells": []}'
```

1. `gda search <query>` searches the live server by name, description, category,
   and tags. It returns at most 20 results by default.
2. `gda describe <name>` prints the schema and the `side_effect` marker. Read it
   before the first call; never infer parameter names.
3. `gda call <name> --args '<json>'` or `--args-file <path>` executes one tool
   against the live editor.
4. `gda tools list` prints every directly callable tool when you need the full
   inventory.

Discovery is live-only; there is no offline catalog. Start an editor with
`gda start --project <dir>` when no instance is running. Search by the object
first ("tilemap cell", "animation track", "import sidecar"), then add a verb only
when the result is too broad. Retry with synonyms before concluding a tool is
missing: tile/cell, sprite/texture, log/output, delete/remove/erase,
create/add/new.

## Task routing

Reduce the task to its object and intent, then read the matching reference.
Confirm names and schemas with `gda search` and `gda describe` before the first
call.

| Task | Reference | First move |
|---|---|---|
| Install, start, connect, install skills | `references/setup.md` | `gda status --json`; if missing, install the plugin |
| Calling protocol, confirmation, errors, docs CLI | `references/tools.md` | `gda describe <tool>` before the first call |
| Scenes, nodes, properties, signals, undo | `references/scene-system.md` | `get_scene_tree` and `property_get` |
| GDScript lifecycle, execution channels, reload | `references/scripting.md` | Check `@tool` and pick the channel |
| Resources, imports, rename/move, UID | `references/resources.md` | Inspect dependencies before moving |
| Run the game, inject input, debug, logs | `references/runtime.md` | `get_game_status` and the error watermark |
| TileMap, animation, audio, UI theming | `references/content.md` | Inspect the existing resource before editing |
| C#, csproj, build loop, reflection bridge | `references/csharp.md` | Probe `dotnet_environment`, then build |
| Servers, RIDs, physics, navigation | `references/servers.md` | Verify the RID lifecycle and thread |

## Symptom routing

| Symptom | Reference | First move |
|---|---|---|
| A property write has no effect | `references/scene-system.md` | Read it back; check owner and value shape |
| A saved scene is missing a node | `references/scene-system.md` | Check parent, owner, and save target |
| The game ignores injected input | `references/runtime.md` | `get_game_status`; align input to frames |
| A script does not run in the editor | `references/scripting.md` | Add `@tool` or use an editor channel |
| A C# change is not applied | `references/csharp.md` | Rebuild and read `diagnostics.instantiable` |
| A resource path breaks after a rename | `references/resources.md` | Check UID, dependencies, and references |
| Tiles or animations look wrong | `references/content.md` | Inspect TileSet, tracks, and SpriteFrames |
| A server call fails silently | `references/servers.md` | Verify RID validity and main-thread execution |

## Confirmation and safety

High-risk side effects return a structured `confirmation_required` error instead
of executing. The classes are `destructive`, `process`, `code_execute`, and
`game_runtime`. Review the tool description and the exact target, then retry:

```bash
gda call code_execute --args '{"source_code": "return 1"}' --confirm
```

With native MCP tools, pass `confirm: true` in the arguments object. File and
scene writes (`writes_file`, `writes_config`, `writes_scene`) do not require
confirmation, but they still change project content: read the target back after
every write. Never work around a confirmation error by switching to a different
tool with the same effect. Treat `destructive` and `game_runtime` annotations as
a stop-and-review signal, not a formality.

## Watch the engine state

Never chain writes blind. Every change lands in a live editor with an import
queue, a debugger, and an engine log; state you did not observe is state you do
not know.

- Read `new_errors_since_last_call` after every call. A non-zero value means new
  errors were recorded; pull the real log lines, fix or explain them, then
  continue. The counter is one-shot and resets when read.
- While a game runs, `get_debugger_errors` and `get_debugger_output` carry its
  errors and prints. Without a running game, `get_debugger_log` reads the editor
  output and engine log, and `get_game_log_entries` reads the on-disk log after
  the session ends.
- When the problem is inside the plugin itself (confirmation denials, timeouts,
  dropped responses), read `get_plugin_log`.
- Prefer observed output — a log line, a screenshot, a read-back — over assuming
  a call did what its name suggests. `ok` only means the call went through.

## The change/observe/verify loop

1. Change the project through tools, or orchestrate a deterministic sequence
   with `batch_execute` or `code_execute`.
2. Observe with a screenshot (`capture_editor_viewport`,
   `capture_game_viewport`), a read-back (`property_get`, `get_scene_tree`,
   `get_resource_property`), or output (`get_debugger_log`, and while a game
   runs `get_debugger_errors` / `get_debugger_output`).
3. Diagnose from the observation, fix, and observe again until the result
   matches the intent.
4. Confirm the fix with the error watermark at zero, then run the relevant
   validation tool (`validate_scene_file`, inline GDScript tests, or a game
   probe) before declaring success.

## References

- `references/setup.md` — install, start, connect, skills, `--prune`.
- `references/tools.md` — CLI protocol, confirmation, errors, limits, docs CLI.
- `references/scene-system.md` — scenes, nodes, properties, signals, undo.
- `references/scripting.md` — GDScript lifecycle and execution channels.
- `references/resources.md` — resources, imports, rename/move, UID.
- `references/runtime.md` — running the game, input, logs, debugging.
- `references/content.md` — tilemap, animation, audio, UI theming.
- `references/csharp.md` — C# build loop and reflection bridge.
- `references/servers.md` — RIDs, physics, navigation, rendering.
