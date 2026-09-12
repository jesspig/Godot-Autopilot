# Tool Gotchas

Catalogue of plugin-side behaviors that surprise callers: silent failures,
hard size limits, places where the built-in prompt text and the actual
implementation disagree, scene refresh timing, `code_execute` traps, and the
side-effect classes reported by `get_tool_detail`. Engine-side silent failures
(dropped node owners, cleared tile cells, audio bus fallbacks and friends)
live in the domain skills, not here.

## Silent failure: non-numeric strings into int properties become 0

`property_set` on an int property with a non-numeric string value (e.g. "abc")
returns ok and stores 0. The call does not error. After any property write
with a value you are unsure about, read it back with `property_get` and assert
the expected value.

Details: godot-autopilot-scene-system.

## Silent failure: unrecognized key names become KEY_NONE

The editor-side input injection tools (key pressing) resolve key names through
a parser that returns KEY_NONE for unrecognized names - without reporting an
error. Pressing KEY_NONE is a no-op, so a misspelled key name silently does
nothing. Prefer single letters and digits (mapped directly) or names from the
known set (SPACE, ENTER, ESCAPE, SHIFT, CONTROL, ALT, TAB, BACKSPACE, DELETE,
arrow keys).

Details: godot-autopilot-runtime (editor-side injection does not reach
the running game; use the game input channel there).

## Silent failure: schema-required parameters that have defaults

Three tools declare parameters as required in their schema but fall back to
defaults instead of erroring when you omit them:

- `create_scene_node` - name/type are marked required but default to NewNode
  and Node; an empty call creates a node with those defaults.
- `get_resource_extensions` - type is marked required; when omitted the
  handler passes an empty type and returns the extensions for ALL types
  instead of an error.
- `reimport_resource_files` - path is marked required; with no files and no
  path the call succeeds silently with a zero count.

Consequence: an omitted-argument bug can look like a successful call. Verify
the effect after calls to these three tools.

Details: godot-autopilot-scene-system and godot-autopilot-resources.

## Silent failure: property writes are read back, and rejections now error

`property_set` reads each written property back and no longer reports a plain
ok when the engine drops the value:

- A non-nil value read back as nil, an object-typed property whose readback is
  not an object, or an array-typed property read back empty when a non-empty
  array was written (or with an element read back as nil) is now an error:
  `value not applied: ...`, and the old value is restored. Previously such
  writes could return ok with no effect.
- A non-array JSON value for an array property is refused before writing - the
  error explains that a non-array value would silently clear the array. Pass
  `[]` to clear it explicitly.
- An array element that is a JSON object or array is refused when the declared
  element type cannot be determined. Declare the element type in the script
  (`Array[T]` / `[Export] T[]`) or build the array with `code_execute`.
- Values the engine merely adjusts still return ok, with a `warning` field
  describing the adjustment.

`set_resource_property` performs the same readback and errors when the
resource rejects the value (it does not restore the old value). Read-back
remains the authoritative check after every write you are unsure about:
`property_get` / `get_resource_property` and assert the expected value.

## Debugger reads without a debug session return empty + note

`get_debugger_errors`, `get_debugger_output` and `get_debugger_scene_tree`
fetch their data from the running game over the runtime channel. Without an
active debug session they return an empty result plus a `note` - there is no
editor-process fallback and no stale "last captured" tree. Use
`get_debugger_log` for editor-process script errors and output, and
`get_game_log_entries` for the game's on-disk log file. The same rule applies
to `get_game_input_status`: its responses never include
`recent_engine_errors`, so read errors through the debugger tools instead.

## Screenshots: image content through call_tool, raw base64 inside batch_execute

Through `call_tool`, `capture_editor_viewport`, `capture_game_viewport` and
`capture_display_screen` deliver the PNG as an MCP image content block: the
text JSON keeps `format`, `width` and `height`, while `data` becomes
`"<attached-as-image-content>"` and image_attached is `true`. Inside
`batch_execute` and `code_execute` no image block is attached - the JSON
`data` field keeps the full base64 and must be decoded by the caller.

## Size limits

These limits are enforced by the server. Truncation is always detectable -
responses carry fields such as truncated, scan_truncated or scan_limit rather
than silently dropping data.

| Limit | Value | Applies to |
|---|---|---|
| JSON response size | 4 MiB | every tool response |
| eval output / error text truncation | 8192 bytes | game eval output and captured error text |
| runtime error / output buffers | 200 / 500 entries | game runtime channel |
| tilemap cells per call | 64 | `set_tilemap_cells` (larger payloads may be truncated client-side; use script loops for bulk layouts) |
| batch operations / input sequence steps | 256 | `batch_execute` / input sequences |
| scene tree export | depth 64, 2000 nodes | scene tree reads (max depth reported in the response) |
| viewport capture | 4096 px per side, 8 MiB PNG | `capture_game_viewport`, `capture_editor_viewport`, `capture_display_screen` |
| Variant single string | 64 KiB | serialized Variant values |
| Variant array elements | 10000 | arrays and packed arrays |
| file scan | 10000 files, 2 MiB per file, 32 MiB total, depth 64 | resource/file scans |
| default / max timeout | 5000 / 30000 ms | operations taking `timeout_ms` |

For bulk work near any of these limits, prefer `code_execute` loops over
giant JSON payloads.

## Prompt-vs-implementation conflicts: the implementation wins

The plugin ships built-in prompt text that lags the implementation in a few
spots. Where they disagree, trust the implementation and `get_tool_detail`:

- The tool-usage prompt claims to describe 17 tools; the actual usage prompt
  body covers 19.
- KEY_ENTER: one built-in prompt says 4194310, the keycode reference table
  says 4194312 (4194310 is actually KEY_META). Prefer KEY_* name strings over
  hard-coded numbers.
- Serialization descriptions claiming that RID serializes to a plain number
  and PackedByteArray to a base64 string are outdated. The implementation
  serializes RID as an object like {"id": 42} and PackedByteArray as a plain
  JSON number array.

## Scene refresh timing

Right after `create_editor_scene` or `open_editor_scene`, an immediately
following `get_scene_tree` can still return the PREVIOUS scene's tree - the
editor applies the switch asynchronously relative to tool calls. Retry the
read (or check `get_editor_edited_scene_root` first) before concluding that
your change did not apply.

Details: godot-autopilot-scene-system.

## Error recovery

Two editor states block otherwise valid calls. Both have a defined recovery:

- **A dirty scene tab blocks open/create.** `open_editor_scene` refuses while
  any open scene has unsaved changes ("current scene has unsaved changes: ...
  - save first (save_editor_scene)"), and `create_editor_scene` with
  `close_current=true` refuses the same way. Recover by saving
  (`save_editor_scene` for the current scene, `save_editor_scenes` for every
  tab) or by discarding the edits - call `reload_editor_scene` with the
  listed path (the scene must be open) to reload it from disk - then retry
  the original call.
- **Importing or scanning blocks affected writes.** While the editor scans or
  imports, affected calls (for example `reimport_resource_files`, or
  `write_file` landing on an imported asset) fail soft with
  `retryable: true` and `retry_after_ms` (500) instead of erroring hard. Poll
  `get_editor_file_system_status` (its `result` carries `scanning` and
  `progress`) until `scanning` is false, wait at least `retry_after_ms`, then
  retry the same call. Do not fire-and-forget scans: `scan_editor_file_system`
  requested while a scan is already running is a no-op.

## code_execute traps

`code_execute` wraps your source in a generated @tool Node script. Four traps:

- Single-function mode is the default: source is inlined inside a generated
  function and top-level func definitions are rejected with an error. Inline
  statements only, or define named functions and call one via `function_name`
  (multi-function mode).
- Calling close_scene on the editor interface is refused outright with an
  error telling you to use the `close_editor_scene` tool instead - the call
  would destroy the executing node and crash the editor.
- Mixing tabs and spaces in the source is rejected with a mixed indentation
  error; reindent with one style throughout.
- The exposed SceneRoot variable IS the edited scene root node: node paths
  under it carry no root-name prefix (SceneRoot.get_node("Player"), not
  SceneRoot.get_node("Root/Player")).

Details: godot-autopilot-scripting.

## The GDA_FORCE_HEADLESS environment variable

The name is inverted. Setting GDA_FORCE_HEADLESS=1 does not force headless
operation - it DISABLES the plugin's cmdline (headless) mode detection, so an
editor launched without a window still builds its UI and starts the MCP server
as usual. If the server is unexpectedly absent in a headless CI editor, set
GDA_FORCE_HEADLESS=1; if you intended the lightweight cmdline mode, leave it
unset.

## Side effect classes

`get_tool_detail` reports a `side_effect` marker for every tool that can act
beyond reading editor state. Read it before calling anything you have not
used before. The classes, in increasing order of risk:

- `writes_file` - writes files under res:// or user://. Confirm the path
  stays inside the project before calling.
- `writes_config` - persists project or editor settings. Treat as a durable
  change the user will find on disk.
- `shows_alert` - opens a native dialog that blocks the editor until
  dismissed. Never fire one in a batch or without an explicit user request.
- `modifies_window` - moves, resizes, focuses or flashes windows; also
  clipboard, mouse cursor and screen state.
- `process` - spawns or kills OS processes, opens paths with the default
  application and edits the environment. The highest-risk class: commands
  run outside the project's scope, so double-check every argument before
  calling.

Two cases deserve the same caution as `process`: the `code_execute` meta tool
runs arbitrary GDScript in the editor process, and tools marked `game_runtime`
change the running game's state. Treat both at the highest risk level.

## See also

- godot-autopilot - usage overview, discovery protocol and the error watermark
- godot-autopilot-scripting - the script execution channels behind the code_execute traps
- godot-autopilot-scene-system - property JSON value shapes and the memory:// rule
