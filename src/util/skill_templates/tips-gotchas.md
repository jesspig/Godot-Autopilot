# Tips, Traps and Known Pitfalls

Cross-domain catalogue of behaviors that surprise callers: silent failures,
hard size limits, places where the built-in prompt text and the actual
implementation disagree, JSON value shapes, and Godot 4.7 API differences.
Each section names the skill that documents the tool family in depth.

## Silent failure 1: non-numeric strings into int properties become 0

`property_set` on an int property with a non-numeric string value (e.g. "abc")
returns ok and stores 0. The call does not error. After any property write
with a value you are unsure about, read it back with `property_get` and assert
the expected value.

Details: godot-autopilot-properties-signals.

## Silent failure 2: unrecognized key names become KEY_NONE

The editor-side input injection tools (key pressing) resolve key names through
a parser that returns KEY_NONE for unrecognized names - without reporting an
error. Pressing KEY_NONE is a no-op, so a misspelled key name silently does
nothing. Prefer single letters and digits (mapped directly) or names from the
known set (SPACE, ENTER, ESCAPE, SHIFT, CONTROL, ALT, TAB, BACKSPACE, DELETE,
arrow keys).

Details: godot-autopilot-running-games (editor-side injection does not reach
the running game; use the game input channel there).

## Silent failure 3: schema-required parameters that have defaults

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

Details: godot-autopilot-scene-building and godot-autopilot-resources-files.

## Size limits

These limits are enforced by the server. Truncation is always detectable -
responses carry fields such as truncated, scan_truncated or scan_limit rather
than silently dropping data.

| Limit | Value | Applies to |
|---|---|---|
| JSON response size | 4 MiB | every tool response |
| eval output / error text truncation | 8192 bytes | game eval output and captured error text |
| runtime error / output buffers | 200 / 500 entries | game runtime channel |
| tilemap cells per call | about 64 | `set_tilemap_cells` (larger payloads may be truncated client-side; use script loops for bulk layouts) |
| batch operations / input sequence steps | 256 | `batch_execute` / input sequences |
| scene tree export | depth 64, 2000 nodes | scene tree reads (max depth reported in the response) |
| viewport capture | 4096 px per side, 8 MiB PNG | `capture_game_viewport`, `capture_editor_viewport` |
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

## JSON value shape quick reference

Property values and method arguments use these shapes:

```json
{"x": 1.5, "y": -2.0}                                       Vector2
{"x": 1.0, "y": 2.0, "z": 3.0}                              Vector3
{"r": 1.0, "g": 0.5, "b": 0.25, "a": 1.0}                   Color
{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}  Rect2
{"path": "res://icon.svg"}                                  resource reference
{"id": 42}                                                  RID handle
```

Details: godot-autopilot-properties-signals.

## Scene refresh timing

Right after `create_editor_scene` or `open_editor_scene`, an immediately
following `get_scene_tree` can still return the PREVIOUS scene's tree - the
editor applies the switch asynchronously relative to tool calls. Retry the
read (or check `get_editor_edited_scene_root` first) before concluding that
your change did not apply.

Details: godot-autopilot-scene-building.

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

## Godot 4.7 API differences

Three differences that bite most often in this plugin's workflows:

- TileSetAtlasSource.get_tile_data takes atlas coordinates plus the
  alternative id (the old source-id-first call order is gone). Relevant when
  scripting tile data access via `code_execute`; see godot-autopilot-tilemap.
- AnimatedSprite2D exposes sprite_frames - the Godot 3 name frames does not
  exist. See godot-autopilot-animation.
- CharacterBody2D motion_mode: GROUNDED is 0 and FLOATING is 1 when set
  numerically.

For the ten most common Godot 3 to 4 renamed properties (frames to
sprite_frames, cast_to to target_position, rect_position to position,
translation to position, and friends), see the renamed-properties table in
godot-autopilot-properties-signals. `property_set` failures automatically
attach Levenshtein candidates and that rename table.

## See also

- godot-autopilot-os-display - OS processes, files, dialogs and sub-windows
  (side-effect classes explained)
- godot-autopilot-project-config - the set-then-save flow for project.godot,
  editor settings and InputMap
- godot-autopilot-usage - connection prerequisites, tool discovery and the
  error protocol including new_errors_since_last_call
