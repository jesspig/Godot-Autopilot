# playtest-debug

## 1. Trigger and inputs
Use this recipe when you need to run the game, drive input, capture evidence, and collect debugger errors for an incident. Inputs: a scene path, an input type and mode with an event payload, a capture target and directory, and a report label and file path.

## 2. Preconditions
The editor is running and the scene to play is known. You accept that starting and stopping the game are runtime changes that require confirmation. A writable directory exists for captures and reports.

## 3. Copyable CLI commands
```bash
gda game status
gda game play --path res://scenes/main.tscn --confirm
gda game input --type action --mode api --event '{"action": "jump"}' --confirm
gda game capture --target game --save --dir captures
gda debug status
gda debug errors --limit 50 --level warning
gda debug repro --path res://scenes/main.tscn --confirm
gda game stop --confirm
gda logs game --tail 100 --level error
gda debug report --label smoke --report-file reports/smoke.json
```

## 4. Fields to observe at each step
Before play, read the game status to avoid double-starting. After play, confirm the running scene and instance binding. After input, check that the event was accepted. For capture, verify the saved image path and target. From the incident snapshot read editor, game, and trace state together. From debugger errors read `code`, `message`, `retryable`, and `request_sent`; correlate with the on-disk game log. After stopping, confirm the game is no longer running. The report should reference the label, image path, and report file.

## 5. Verification and exit criteria
Exit when the intended scene ran, the input was accepted, the capture exists, debugger errors are collected with their request state, the game is stopped, and the report file was written.

## 6. Failure branches
If play returns confirmation_required, reissue with --confirm after reviewing the target. If the game did not start, read the status and the editor log. If no errors appear in the debugger stream, widen the level or limit and read the game log. If a capture fails, check the target and directory. Never resend an input or runtime call whose result is unknown; read status first.

## 7. Safety and confirmation points
Game play, input, eval, scenario, and stop are runtime changes. When one returns confirmation_required, reissue the same call with --confirm after reviewing tool, target, risk, and arguments. In non-interactive sessions, a missing confirmation means stop. Never resend a call whose outcome is unknown. Treat destructive, process, code-execute, or game-runtime signals as a stop-and-review point, especially before bulk input timelines.

## 8. Explicit non-goals
Do not leave the game running after the incident is captured. Do not guess an input type, event shape, or capture target. Do not hardcode tool counts or version windows.
