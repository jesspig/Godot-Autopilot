# Calling Protocol and Docs CLI

This reference is the cross-domain calling knowledge: channels, the CLI
surface, engine docs, confirmation, errors, and limits. Domain behavior lives in
the other references.

## Channels

- **GDA CLI.** Use `gda` whenever a shell works. It discovers the plugin port,
  prefers IPC, falls back to HTTP, and prints JSON.
- **Native MCP tools.** When the harness lists GDA tools, use them directly.
  Semantics match the CLI; only the transport differs.
- **Manual editor steps.** When neither is available, give the user a short,
  checkable sequence and ask for one observation. Never invent tool output.

## Core CLI flow

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"path": "Level", "cells": []}'
gda tools list
```

- `gda search <query>` searches the live server by name, description, category,
  and tags; `--category` and `--tags` narrow it. It returns at most 20 results.
- `gda describe <name>` prints the schema and `side_effect` marker. Read it
  before the first write; never infer parameter names.
- `gda call <name> --args '<json>'` or `--args-file <path>` executes one call.
- `gda tools list` prints every directly callable tool. Discovery is live-only;
  start an editor with `gda start --project <dir>` when no instance is running.

Target resolution: explicit `--socket` or `--port`, then `--id`, then the live
instance matching `--project` or the current directory. The plugin owns the
port; `--port` is a connection override only. `gda status --json` reports the
resolved `mcp_url` and transport. If resolution fails, `gda list --all` shows
known projects.

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | The tool ran and returned a failure envelope |
| 2 | Transport or environment failure: no instance, endpoint down, timeout |
| 3 | Usage error: unknown command, flag, or malformed arguments |

Stdout is JSON; diagnostics go to stderr. Exit 2 means the channel is down;
never report a fabricated result after it.

## Engine documentation CLI

`gda docs` is the offline engine reference. It reads the local engine dump,
caches it by engine minor, and searches classes, members, and globals.

```bash
gda docs search "tween"
gda docs class Node2D --member position
gda docs member Node2D look_at
gda docs sync
```

- `status` is read-only: project pin, resolved engine, cache path, staleness.
- `search` defaults to 20 results (`--limit` up to 200) and accepts `--kind`
  (`class`, `method`, `property`, `signal`, `enum`, `constant`, `utility`) and
  `--class`. `class` returns brief, description, and counts by default; use
  `--section`, `--member`, or `--full` to expand. `member` auto-detects the
  kind, and an ambiguous name returns all matches.
- `sync` always exports. A missing cache or a version mismatch exports on the
  next query. A failed export is an error; it never falls back to another
  version.
- `--docs-file <path>` reads an external `extension_api.json` and bypasses the
  cache. If the project pin and the available binary disagree, pass a matching
  `--godot <path>`; the command refuses to mix minors.
- `clear --engine <x.y>` removes one bucket; `--all` requires `--force`.

## Confirmation

High-risk side effects return a structured `confirmation_required` error before
executing. Review the description and target, then retry with `--confirm` (CLI)
or `confirm: true` (MCP). The classes are `destructive` (removes an existing
object), `process` (controls an OS process), `code_execute` (runs arbitrary
GDScript), and `game_runtime` (reaches into the running game).

File, config, and scene writes do not require confirmation, but they change
content: read the target back. Never switch to another tool with the same effect
to bypass confirmation, and never hide a high-risk call inside a batch.

## Batch and code execution

- `batch_execute` runs an ordered `operations` list (at most 256). Each operation
  has `tool` and optional `args`; `stop_on_error` defaults to true and
  `rollback_on_error` is optional. Use it for deterministic sequences.
- `code_execute` runs GDScript in the editor for loops, math, and computed
  values. Without a line-start `func ` it wraps the source in one entry
  function; with `func ` it preserves the source and uses `function_name`.
  `timeout_ms` defaults to 5000 and is capped by the schema.
- Decision rule: a handful of existing calls → `batch_execute`; loops, math,
  conditions, or very large payloads → `code_execute`.

## Errors and the watermark

Every tool failure is a JSON object with an `error` key. Some carry a stable
`code`; branch on the code, not the prose. Every meta-tool response also carries
`new_errors_since_last_call`, which counts new errors since your previous call,
including errors the call itself produced. It is one-shot: reading resets it.

| Code | Raised when | Retry? |
|---|---|---|
| `exec_timeout` | A `code_execute` script outran `timeout_ms` | Split the work or raise the timeout within the cap |
| `eval_timeout` | A game-side eval outlived the host wait | Not immediately; the game may still be running it |
| `main_thread_timeout` | The editor main thread missed its budget | Read state first; the side effect may still land |
| `queue_unavailable` | The main-thread queue was full or closing | Yes, later |
| `confirmation_required` | A high-risk tool ran without confirmation | Review the target, then retry with `--confirm` |

Soft, retryable errors carry `retryable: true` and `retry_after_ms` while the
editor imports or scans resources. Wait at least the suggested interval and
retry. During project export, all tool calls are rejected; wait for the export
instead of retrying in a tight loop.

Confirm a fix with the watermark: reproduce, diagnose, fix, rerun the failing
call, and make meta calls until the watermark returns 0. Zero after a successful
call is closing evidence that the fix produced no new errors.

## Resources, prompts, and tasks

`gda resources read <uri>` reads editor and debugger state; `gda resources list`
is the URI authority. `gda prompts list|get` renders registered prompts.
`gda call --async` plus `gda tasks list|get|wait|cancel|clean` manages locally
recorded tasks.
## Limits

- Default operation timeout is 5000 ms; the schema reports the current cap.
  Game operations cap `timeout_ms` at 25000 ms.
- JSON responses are capped at 4 MiB with truncation fields; check them before
  trusting completeness. `batch_execute` accepts at most 256 operations, and at
  most 8 requests may be in flight concurrently.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| Exit 2 with no JSON | No live instance or endpoint down | `gda list --all`, then `gda start --project .` |
| Exit 3 | Unknown command, flag, or malformed JSON | `gda --help`; validate the `--args` JSON |
| `confirmation_required` | High-risk call without confirmation | Review the target, then `--confirm` |
| `main_thread_timeout` | Editor main thread missed its budget | Read state before retrying |
