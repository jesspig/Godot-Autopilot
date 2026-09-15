# Development Workflow: Change, Observe, Verify

The routing tables in `godot-autopilot` pick the right tool for one call; this
reference is about the loop that surrounds them. A tool returning `ok` (or a
`saved` / `created` result) only reports that the call was executed - not that
the project now looks or behaves the way you intended. Close the loop every
time: change, observe, diagnose, fix, verify.

## The loop

1. **Change** - apply one coherent edit through tools (`create_scene_node`,
   `property_set`, `create_script`, `write_file`, `create_resource`,
   `apply_theme_to_control`, and so on), or orchestrate several through
   `batch_execute` / `code_execute`.
2. **Observe** - look at the actual result with at least one real observation
   (see below): a screenshot, a read-back, or log / debugger output.
3. **Diagnose** - compare what you observed against what you intended; find
   the cause in the observation, not in your assumptions.
4. **Fix** - apply the correction.
5. **Verify** - observe again until the observation matches, then confirm no
   new errors through the one-shot `new_errors_since_last_call` watermark and
   the loop described in `godot-autopilot` (sections `Error protocol` and
   `Confirming a fix with the error watermark`). For testable logic, close the
   loop with `run_gdscript_tests`.

## Three rules that keep the loop honest

1. **Tool success is not effect success.** `ok` means the call was accepted;
   the engine may clamp, convert or drop the value. Confirm with a read-back
   (`property_get`, `get_scene_tree`, `get_resource_property`), a screenshot,
   or a log line before building anything on top of it.
2. **Read the output before changing code again.** After editing a `.gd`,
   `.tscn`, `.tres` or `.cs` file, read `get_debugger_log` first - script
   parse errors and `print` output land there without running the game. While
   a game runs, read `get_debugger_errors`, and fall back to
   `get_game_log_entries` when the runtime channel may have dropped messages.
   Never edit blindly in a loop.
3. **Every observation must land somewhere.** A claim that a change worked is
   only as good as the screenshot you looked at, the value you read back, or
   the log you read. If none of the three exists, you have not observed yet -
   keep working.

## Observation sources

- **Screenshots.** `capture_editor_viewport` (default `target` `editor`)
  captures the editor viewport while you build a scene;
  `capture_editor_viewport` with `target` `game` and `capture_game_viewport`
  capture the running game; `capture_display_screen` captures a whole
  physical screen. Through `call_tool` the base64 PNG arrives as an MCP image
  content block: the text JSON keeps `format`, `width` and `height`, and its
  `data` field reads `"<attached-as-image-content>"` with
  image_attached: true, so a multimodal model sees the picture directly.
  Inside `batch_execute` and `code_execute` no image block is attached - the
  JSON keeps the full base64 and must be decoded. Captures are limited to
  4096 pixels per side and an 8 MiB PNG; the JSON response cap is 4 MiB and
  base64 inflates the payload by about one third, so keep the captured
  resolution modest.
  `capture_editor_viewport` also offers two low-cost verification options:
  `annotate` draws numbered boxes over the elements on the image and adds
  an element table to the result, and `diff_against_last` reports the
  changed ratio and bounding box against the previous capture of the same
  target. A diff of zero after an action that should have changed the UI
  means the action did not land - check it before studying the pixels.
- **Read-backs.** `property_get` / `property_get_list`, `get_scene_tree`,
  `get_editor_edited_scene_root` and `get_resource_property` show what the
  engine actually stored - the reliable check after `property_set`,
  `set_resource_property`, `execute_game_script` or `code_execute`.
- **Logs and debugger output.** After file edits read `get_debugger_log`
  (editor process, always available). With a running game read
  `get_debugger_errors` / `get_debugger_output`; `get_game_log_entries` is
  the on-disk fallback that passes none of the debug transport's drop gates;
  `get_plugin_log` reads the plugin's own in-process diagnostics
  (authorization denials, timeout bookkeeping, dropped late game responses).
  The four paths are documented in `godot-autopilot-runtime`.

## Loop examples

### UI and scene appearance

1. Change: `create_scene_node`, `property_set`, `create_theme_resource` plus
   `apply_theme_to_control`, or a `code_execute` layout pass.
2. Observe: read the values back (`property_get`) and capture the editor
   viewport with `capture_editor_viewport`.
3. Run the scene (`play_editor_current_scene`) and capture the in-game result
   with `capture_game_viewport`; the running UI is also enumerable with
   `get_game_ui_elements`.
4. Fix what the image shows (spacing, anchors, colors), then capture again -
   do not trust the set values alone.

### Script logic

1. Change: `create_script` (compile-checked before writing) or `write_file`
   for an existing script. `write_file` on a script (`.gd`, `.cs`, shader)
   returns a `diagnostics` object; when its `ok` is false the script failed
   to load - read `get_debugger_log` for line-level detail instead of
   guessing.
2. Run: `play_editor_current_scene`, then exercise the code path via
   `execute_game_script` or injected input.
3. Observe: `get_debugger_errors` while the session is active, and the
   `new_errors_since_last_call` watermark on your next call; cross-check
   `get_game_log_entries` when an error storm may have hit the drop gates.
4. Fix, then `reload_game_scripts` for a quick re-check or restart the game;
   `run_gdscript_tests` verifies pure logic without running a game.

### Resources and imports

1. Change: `create_resource` + `set_resource_property` + `save_resource`, or
   `write_file` / `copy_resource_file` / `rename_resource_file`.
2. Observe: `has_resource`, `get_resource_type`, `get_resource_dependencies`
   and `get_resource_property` show what is actually on disk and in memory.
3. If the editor is scanning or importing, affected calls fail soft with
   `retryable` / `retry_after_ms`; poll `get_editor_file_system_status` until
   `scanning` is false, wait `retry_after_ms`, then retry. For imported assets
   follow up with `reimport_resource_files` and poll again.
4. Verify the on-disk result (`get_editor_file_system_tree`, `reload_resource`)
   before referencing the asset elsewhere.

### Runtime debugging and input injection

1. Run: `play_editor_current_scene`, then gate on `get_game_status`
   (`healthy`); a half-dead process is stopped and restarted, not debugged.
2. Reconnoiter: `get_game_ui_elements` locates the controls and their
   `global_rect` coordinates.
3. Drive: `queue_game_input` / `sequence_game_inputs` (frame-accurate), then
   observe with `get_game_input_status`, a fresh `capture_game_viewport`, and
   `get_debugger_errors`.
4. Fix and verify: `reload_game_scripts` or restart, repeat the injection,
   and let the watermark confirm the fix produced no new errors.

### Editor UI automation

1. Enumerate: `get_editor_ui_elements` lists the editor's UI elements;
   when a coordinate is already known, `hit_test_editor_point` previews
   which element it would hit.
2. Act: `click_editor_element` with the element `path`, or
   `type_editor_element_text` / `run_editor_shortcut` for text entry and
   shortcuts. Fall back to `click_input_mouse`, `scroll_input_mouse`,
   `drag_input_mouse` or `type_input_text` only when no element path
   exists (2D canvas, 3D viewport, custom-drawn controls).
3. Verify: capture with `capture_editor_viewport`; `annotate` numbers the
   elements it sees and `diff_against_last` measures what changed against
   the previous capture. Re-enumerate before the next click when the
   action may have rearranged the layout.

## See also

- `godot-autopilot` - discovery protocol, the error watermark and retryable
  soft errors.
- `tool-gotchas.md` - silent failures and limits that break naive loops.
- `godot-autopilot-runtime` - capture, log and injection semantics.
- `godot-autopilot-scene-system` - property JSON shapes and read-back
  behavior.
