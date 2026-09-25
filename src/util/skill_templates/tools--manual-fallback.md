# Manual Fallback for Godot Autopilot Calls

Companion reference of the godot-autopilot-tools volume: what to do when no automated channel exists. Native MCP tools are down, and the script bridge is unavailable too — hand the work to the user with precise editor instructions instead of reaching for workarounds.

## Trigger criteria

Enter this mode only when at least one holds:

- No working MCP client and no Node 18+ / Bun runtime to run the script bridge.
- The harness has no shell at all, so neither the native channel nor the script can run.
- The user explicitly asks for manual steps.

If any automated channel works, leave this mode immediately and use it — manual steps are a last resort, not a preference.

## Hard rules

- Stop all automation. Issue no tool calls, run no commands, touch nothing in the project.
- Do not operate the scene on the user's behalf and do not claim an edit was made.
- Do not fabricate call results, log lines, screenshots or watermark values. Anything you did not observe through a real channel does not exist.
- Do not hand-write raw requests against the server endpoint from any tool, however urgent the task looks. The ban from the godot-autopilot-tools volume holds here as well.
- Keep every instruction inside the editor UI the user already has: menus, docks, inspectors, dialogs. No new software, no config file surgery beyond what the recovery section describes.

## Handoff template

Produce one block per requested change, following this shape:

```
Goal: <one sentence stating the intended outcome>
Why manual: <which trigger criterion applies>

Steps:
1. <menu / dock / button path with exact labels>
2. <value to enter or option to pick, with the field name>
3. <repeat until the change is fully described>

Expected result: <what the user should see when the step worked>
How to verify: <where to look — panel, inspector value, log line, viewport>
If it fails: <the most likely cause and the corrected step>

Information needed back:
- <answer, screenshot, or log excerpt required to continue>
```

Fill every section. A template with an empty verification or an empty information-needed-back section is incomplete — the round trip stalls without them.

## Limits you must declare

State these up front in the handoff so the user can calibrate expectations:

- No live scene access: you cannot read the current scene tree, selection or inspector values. Ask the user to report or screenshot anything you would normally read with `search_tools` discovery, `property_get` or `get_scene_tree`.
- No screenshots: you cannot capture the editor or game viewport. Ask the user to attach one when appearance matters.
- No logs: you cannot read the debugger, engine or plugin output. Ask the user to paste the relevant lines when diagnosis depends on them.
- No watermark confirmation: the error-watermark loop from the godot-autopilot-tools volume cannot run. The user confirms each step against the Expected result line instead.
- No batching across steps: each manual step is confirmed by the user before you describe the next dependent one. Never chain five unverified edits in a single message.

## Recovery — restoring automation

Close every handoff with the shortest applicable path back to an automated channel:

1. Restore the native channel. In the editor, open the plugin MCP Config dock and use the Generate button: it writes a project-level MCP configuration for the client in use (twenty clients covered). Restart or reconnect the client harness afterwards — a lost tool list right after an editor restart is a harness reconnect problem, not a server problem.
2. Restore the script bridge. Install Node 18+ or Bun, then use the bundled script per references/script-access.md. No client reconfiguration needed.
3. Clear authorization denials on the way back. Script execution needs the `code_execute` capability, game-runtime tools need `game_runtime`, and user-registered dynamic tools need `user_tools`. Each can be enabled via the `GODOT_AUTOPILOT_ALLOW` environment variable plus an engine restart, or via the matching MCP Config dock checkbox ("Allow code_execute", "Allow game_runtime", "Allow user tools"), which takes effect on the next tool call. When the environment variable is set it wins over the dock setting.

## See also

- godot-autopilot-tools — channel ladder, discovery protocol, error protocol and limits
- references/script-access.md — the script bridge to return to first
