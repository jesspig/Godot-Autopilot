# Godot Autopilot Usage

Entry point for working on a Godot project through the godot-autopilot MCP server. Read this first: it explains how to connect, how to discover tools (never guess names), and how to read the error protocol every response follows.

## Prerequisites

- The Godot editor must be open with the godot-autopilot plugin enabled. The MCP server starts automatically with the plugin and stops when the plugin is disabled or the editor closes.
- The MCP endpoint is `http://127.0.0.1:9527/mcp` by default, but the port is configurable. Never assume 9527; see godot-autopilot-direct-http for the four-level port discovery order.
- The server listens on the loopback interface only, with no authentication. It is meant for trusted local MCP clients on the same machine.

## What the server exposes

The MCP layer intentionally exposes only seven meta tools:

- `ping` - health check
- `search_tools` - search tools by keyword, category or tags
- `list_categories` - list all tool categories with counts
- `get_tool_detail` - full schema and side-effect marker for one tool
- `call_tool` - execute any domain tool by name
- `batch_execute` - run several tool calls in sequence
- `code_execute` - run GDScript in the editor

The 363 domain tools (scene, property, resource, script, physics, render, audio, and so on) plus `system_status` are not registered as MCP tools directly. Call every domain tool through `call_tool`, passing the domain tool name and its arguments object.

## Discovery protocol - search first, never guess

Tool names follow the pattern verb_category_dimension_object_modifier in snake_case with the verb first, but segment count varies (2-6 segments), so guessing is unreliable. Always discover:

1. `ping` to confirm the server responds.
2. `search_tools` with a `query` (optionally `category` or `tags` filters), or `list_categories` to browse by domain.
3. `get_tool_detail` with the exact `name` to read its input schema, description and side-effect marker before calling anything that writes files or config.
4. `call_tool` with `name` and `arguments` to execute.

Calling `call_tool` with an unknown name fails with a message telling you to use `search_tools`. Use it instead of improvising similar names.

Example domain call through `call_tool`:

```json
{"name": "create_scene_node", "arguments": {"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}}
```

## Batching with batch_execute

`batch_execute` orchestrates tools that already exist, in order:

- `operations` is an ordered array of `{"tool": ..., "args": {...}}` items, at most 256 per call.
- `stop_on_error` defaults to true: the first failing operation skips the rest.
- `rollback_on_error` (default false) undoes the editor global undo history changes made by the batch on failure; the response then reports how many operations were rolled back and whether that rollback was only partial.
- The response lists per-operation `results` plus `succeeded` and `failed` counts.

Use `batch_execute` for deterministic sequences of existing tools. Use `code_execute` when you need loops or computation (see godot-autopilot-tool-map for the decision guide).

## Error protocol

Every tool failure is returned as a JSON object with an `error` key. For `call_tool` and `code_execute`, a top-level `error` also sets the MCP-level error flag on the response.

In addition, every meta-tool response carries a top-level `new_errors_since_last_call` integer - the error watermark:

- It counts new errors recorded since your previous call, including errors the call itself just produced. A failed call reports at least its own error.
- It is one-shot: the value is consumed when you read it and the counter resets. Never skip reading it, even when the call succeeded.
- Errors printed by a running game are recorded too, so after fixing a problem, make any meta-tool call and confirm the watermark comes back 0.

Soft (retryable) errors: while the editor is importing or scanning resources, affected calls return a structured error with `retryable` set to true and a suggested `retry_after_ms`:

```json
{"error": "editor is currently importing/scanning resources; retry shortly", "retryable": true, "retry_after_ms": 500}
```

Wait at least `retry_after_ms`, then retry the same call.

Export lockout: while the editor is exporting the project, all tool calls are rejected with:

```json
{"error": "editor is exporting; retry after export completes"}
```

Wait for the export to finish instead of retrying in a tight loop.

## Limits

- Default operation timeout is 5000 ms; the maximum accepted timeout is 30000 ms.
- JSON responses are capped at 4 MiB. Large results are truncated with detectable fields (such as a `truncated` flag) rather than silently dropped - check them before trusting completeness.
- `batch_execute` accepts at most 256 operations per call.

## If MCP tools are missing

If your MCP client shows an empty tool list or calls fail right after the editor restarted, the server is usually fine and the client harness simply did not reconnect. See godot-autopilot-direct-http to keep working by sending JSON-RPC over HTTP directly.

## See also

- godot-autopilot-direct-http
- godot-autopilot-tool-map
