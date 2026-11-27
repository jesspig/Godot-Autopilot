---
name: godot-autopilot
description: "CLI-only entry skill for Godot Autopilot (GDA): probe, discover tools, call with --confirm, follow change-observe-verify. Use when driving Godot via gda; load only the matching reference."
---

# Godot Autopilot

Godot Autopilot (GDA) exposes a running Godot editor through one local `gda` CLI. Every operation is a CLI command that prints a JSON envelope. Load only the reference matching the task; do not load them all.

## Start here

```bash
gda ping
gda status --format json
gda commands --json
```

`gda ping` proves the control plane is reachable; `gda status --format json` is the connection record; `gda commands --json` is the authoritative command and flag list. Never invent a command that is not in it.

## Discover tools, never guess

Tool names and counts change with the CLI. Discover first, then read the schema before the first call.

```bash
gda tool list
gda tool search <query>
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

1. `gda tool list` lists the canonical catalog; filter with `--category` or `--tags`. `gda tool categories` lists categories.
2. `gda tool search <query>` finds tools by object first ("tile cell", "audio track"); add a verb only when results are too broad, and try synonyms before concluding a tool is missing.
3. `gda tool describe <tool>` shows the schema, required `action` values and side effects. Read it before the first call; never infer parameter names.
4. `gda tool call <tool>` runs one tool. Use `--args '<json>'` for short input and `--args-file <path>` (or `-` for stdin) for long JSON.
5. A remembered name may be a legacy alias. Resolve it with `gda tool describe` or `gda tool search` and prefer the canonical name.

## Engine documentation

Never write an unfamiliar engine API from memory:

```bash
gda docs search <query>
gda docs class <name> --member <text>
```

`gda docs status` shows the resolved engine and cache; `gda docs sync` refreshes it. Pass `--godot` when the project pin and the binary differ.

## Confirmation and safety

High-risk effects return `confirmation_required` with a summary instead of executing. Review it, then re-run the same command with `--confirm`. A missing `--confirm` in a non-interactive session stops the command without sending anything.

```bash
gda tool call <tool> --args '<json>' --confirm
```

- `destructive`, `process`, `code_execute` and `game_runtime` annotations are stop-and-review signals.
- If a result is unknown, read the state back before resending a side-effecting call.
- Read back every file or scene write; confirm high-risk items individually in a batch. Overwrites need explicit confirmation.
- Details in `references/safety.md`.

## Watch the engine and recover

```bash
gda doctor
gda job
```

`gda doctor` diagnoses the environment; `gda doctor run --offline --ci` gives a stable machine-readable report. Long calls return a job id: track it with `gda job list`, `gda job wait <id>` and `gda job status <id>`; stop one with `gda job cancel <id> --confirm`. Read logs with `gda logs editor`, `gda logs game`, `gda logs plugin` or `gda logs cli`.

## Change / observe / verify

1. Change the project with a CLI command or tool call.
2. Observe with a read-back (`gda scene tree`, `gda resource get`), a capture (`gda game capture`) or logs (`gda logs editor`).
3. Diagnose from the observation, fix, and observe again until the result matches intent.
4. Validate with `gda test run`, `gda debug errors` or `gda scene verify` before declaring success.

## Task routing

| Task | Reference | First move |
|---|---|---|
| Install, connect, skills, plugin | `references/setup.md` | `gda setup status` |
| CLI syntax, envelope, args-file, jobs | `references/cli.md` | `gda commands --json` |
| Confirmation and error recovery | `references/safety.md` | `gda tool describe <tool>` |
| Offline and air-gapped use | `references/offline.md` | `gda doctor run --offline` |
| Tool discovery and aliases | `references/tools.md` | `gda tool search <query>` |
| Scenes, nodes, properties, signals | `references/scene.md` | `gda scene tree` |
| GDScript create, patch, run, reload | `references/scripting.md` | `gda script list` |
| Resources, import, rename, UID | `references/resources.md` | `gda resource inspect --path <path>` |
| Run game, input, capture, logs | `references/runtime.md` | `gda game status` |
| Debug, trace, report | `references/debugging.md` | `gda debug status` |
| Tests and runs | `references/testing.md` | `gda test list` |
| Workflows and long jobs | `references/workflows.md` | `gda workflow list` |
| TileMap, animation, audio, UI | `references/content.md` | `gda tool search <query>` |
| C# build loop | `references/csharp.md` | `gda doctor run --ci` |
| Servers, RIDs, physics, navigation | `references/servers.md` | `gda tool search <query>` |
