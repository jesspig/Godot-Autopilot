# Script Access to the Godot Autopilot MCP Server

Companion reference of the godot-autopilot-tools volume: how to call the server through the bundled script when the native MCP channel is broken. The server itself is almost always fine — typically the client harness lost its tool list after an editor restart.

Hard rule: never send hand-written HTTP requests with command-line HTTP utilities or raw request code. Every call goes through the harness MCP tools or this script. This reference documents the only sanctioned fallback.

## When to use this

- Your MCP client shows an empty tool list or calls fail right after the editor restarted, and reconnecting did not help.
- A shell with Node 18+ or Bun is available and the project checkout contains the script.
- If no script runtime exists either, stop and switch to the manual guide in references/manual-fallback.md instead.

## Subcommands

The script exposes six subcommands. Meta tools are called directly; domain tools go through `call_tool`, exactly as on the MCP channel.

| Subcommand | Meaning | Notes |
|---|---|---|
| `ping` | Health check | Safest first call after a restart |
| `status` | Server status | Resolved through `call_tool` to `system_status` |
| `categories` | List all tool categories | Mirrors `list_categories` |
| `search` | Search tools by keyword | Mirrors `search_tools`; takes the query text, with category and tag filtering |
| `describe` | Full schema and side-effect marker for one tool | Mirrors `get_tool_detail`; takes the exact tool name |
| `call` | Execute one tool | Meta tools by direct name; every domain tool via `call_tool`; takes the tool name plus `--args JSON` or `--args-file FILE` for the arguments object |

Follow the same discovery order as the native channel: `ping`, then `search` or `categories`, then `describe`, then `call`. Calling with an unknown name fails with a message telling you to search — never guess names.

## Global options

| Option | Meaning | Default |
|---|---|---|
| `--port` | Server port, highest priority | See Port resolution |
| `--project-dir` | Project directory used to locate project.godot for port discovery | Current directory |
| `--timeout` | Script-side wait budget in seconds | 60 |
| `--save-images DIR` | Decode image payloads into DIR | Off — images report metadata only |
| `--raw` | Print the unprocessed response envelope | Off |
| `--quiet` | Minimal output | Off |

There is no URL option: the script always targets the loopback interface and only the port varies. `--timeout` raises how long the script waits; the server-side operation caps from the godot-autopilot-tools volume still apply.

## Port resolution

Never assume the default. The script resolves the port from the first available source in this order:

| Priority | Source | What to read |
|---|---|---|
| 1 | `--port` flag | The value you pass |
| 2 | `GODOT_AUTOPILOT_PORT` environment variable | The value it holds |
| 3 | Trace output | The port on the latest server-ready line |
| 4 | Plugin config file | The port key in config.json under the Godot user data folder |
| 5 | Fallback | 9527 |

Pass `--port` explicitly whenever you already know the editor was configured with a custom port.

## Calling convention

Domain calls carry the domain tool name plus its arguments object. The arguments travel in a JSON file:

```
node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs call <tool> --args-file args.json
```

with args.json holding the arguments object, for example:

```json
{"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}
```

Further examples:

```
node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs ping
node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs status
node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs search "tilemap cell"
node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs describe create_scene_node
```

`status` is shorthand for a `call_tool` round trip to `system_status`. `search` mirrors `search_tools` ranking (object-first queries, synonym retries, `--category` and `--tags` filters) and prints a compact projection of the matches. `describe` mirrors `get_tool_detail` — read it before the first write, same as on the native channel.

## Handshake behavior

The script performs a lazy handshake: if the server answers 400 with an uninitialized-session code, the script sends the initialization notification once and retries the call automatically. You only need to act when the retry also fails — typically the editor restarted mid-session, in which case just run the command again.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success — the tool call succeeded |
| 1 | Tool failure — the server ran the tool and the tool reported an error; read the error object, not the transport |
| 2 | Transport failure — connection refused, timeout, or the server is unreachable |
| 3 | Usage error — unknown subcommand, missing tool name, unreadable arguments file |

Exit code 1 carries the same business payload as the MCP channel, including the one-shot watermark field: read `new_errors_since_last_call` after every call, even on success, exactly as documented in the godot-autopilot-tools volume. Retryable import errors carry the suggested wait interval — wait it out, then retry the same command.

## Image rules

Screenshot tools (`capture_editor_viewport`, `capture_game_viewport`) return image payloads. By default the script reports image metadata only (dimensions, format, where it would land) without writing files. Pass `--save-images DIR` to decode the payloads into DIR under self-generated file names. Inspect the saved files the same way you would inspect image content on the native channel before trusting an edit.

## Troubleshooting

| Symptom | Meaning | Fix |
|---|---|---|
| Connection refused | Editor closed, plugin disabled, or wrong port | Open the editor with the plugin enabled; walk the Port resolution table top down; pass `--port` explicitly |
| Timeout (exit 2) | Work unit too large for the wait budget | Split into smaller calls; `--timeout` only extends the script wait, server caps still apply |
| 400 uninitialized even after retry | Session reset between handshake and call | Editor restarted mid-session — run the command again |
| Unknown tool name (exit 1) | Guessed or stale tool name | Discover exact names with `search` and confirm with `describe`; never invent names |
| Authorization denied (exit 1) | `code_execute`, `game_runtime` or `user_tools` gate | Enable via the `GODOT_AUTOPILOT_ALLOW` environment variable plus engine restart, or the matching MCP Config dock checkbox effective on the next call |
| Empty or missing image output | Metadata-only default | Re-run with `--save-images DIR` and inspect the files on disk |

Tool-level failures (bad arguments, missing nodes) are not transport errors — they arrive as exit code 1 with an error key in the business payload. Handle them with the same error protocol as the native channel: read the watermark, diagnose from real output, confirm the fix by driving the watermark back to 0.

## Constraints

- Loopback only, no authentication: never expose the port and never target a remote machine.
- At most 8 concurrent in-flight requests; exceeding the limit fails fast.
- The script honors the same limits as the native channel: operation timeouts, the 4 MiB response cap and at most 256 operations per `batch_execute`.

## See also

- godot-autopilot-tools — channel ladder, discovery protocol, error protocol and limits
- references/manual-fallback.md — what to do when no script runtime exists either
