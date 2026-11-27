---
name: godot-autopilot
description: "CLI-only entry skill for Godot Autopilot (GDA): probe with gda ping, discover commands with gda commands, find tools with gda tool list/search/describe, call them with gda tool call --confirm, and follow change-observe-verify. Read only the reference matching the task."
---

# Godot Autopilot

Godot Autopilot (GDA) exposes a running Godot editor through one local `gda`
CLI. There is no second protocol and no endpoint to configure: every operation
is a CLI command that prints a JSON envelope. Read only the one reference that
matches the task; do not load them all.

## Start here

```bash
gda ping
gda status --format json
gda commands --json
```

`gda ping` is a health probe, not a handshake; it only proves the control plane
is reachable. `gda status --format json` is the connection record.
`gda commands --json` is the authoritative list of implemented commands and
flags; never invent a command that is not in it.

## Discover tools, never guess

The tool catalog changes with the CLI; names and counts are not fixed. Use the
CLI to discover, then read the schema before the first call.

```bash
gda tool list
gda tool search <query>
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

1. `gda tool list` lists the current canonical catalog; filter with `--category`
   or `--tags`. `gda tool categories` lists categories.
2. `gda tool search <query>` finds tools by object first ("tile cell", "audio
   track"), adding a verb only when results are too broad; try synonyms before
   concluding a tool is missing.
3. `gda tool describe <tool>` shows the schema, required `action` values and
   side effects. Read it before the first call; never infer parameter names.
4. `gda tool call <tool>` runs one tool. Use `--args '<json>'` for short input
   and `--args-file <path>` (or `-` for stdin) for long JSON.
5. A remembered tool name may be a legacy alias. Resolve it with
   `gda tool describe` or `gda tool search` and prefer the canonical name and
   action; do not rely on a remembered migration list.

## Engine documentation

Never write an unfamiliar engine API from memory. Look it up first:

```bash
gda docs search <query>
gda docs class <name> --member <text>
```

`gda docs status` shows the resolved engine and cache; `gda docs sync` refreshes
the cache. Pass `--godot` when the project pin and the available binary differ.

## Confirmation and safety

High-risk effects are not executed. They return `confirmation_required` with a
summary of tool, target, risk and arguments; re-run the same command with
`--confirm` after reviewing that summary. In a non-interactive session a missing
`--confirm` stops the command and sends nothing.

```bash
gda tool call <tool> --args '<json>' --confirm
```

- `destructive`, `process`, `code_execute` and `game_runtime` annotations are
  stop-and-review signals, not formalities.
- If a result is unknown, do not resend a side-effecting call; read the state
  back first.
- Read back every file or scene write; confirm each high-risk item separately
  in a batch. Overwrites need explicit confirmation.
- Details in `references/safety.md`.

## Watch the engine and recover

```bash
gda doctor
gda job
```

`gda doctor` diagnoses the environment; `gda doctor run --offline --ci` gives a
stable machine-readable report. Long-running calls return a job id: track them
with `gda job list`, `gda job wait <id>` and `gda job status <id>`; stop one with
`gda job cancel <id> --confirm`. Read logs with `gda logs editor`,
`gda logs game`, `gda logs plugin` or `gda logs cli`.

## Change / observe / verify

1. Change the project with a CLI command or tool call.
2. Observe with a read-back (`gda scene tree`, `gda resource get`), a capture
   (`gda game capture`) or logs (`gda logs editor`).
3. Diagnose from the observation, fix, and observe again until the result
   matches intent.
4. Validate with `gda test run`, `gda debug errors` or `gda scene verify` before
   declaring success.

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
