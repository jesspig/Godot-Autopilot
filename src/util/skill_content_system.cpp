#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kOsDisplayDescription =
    R"gda_skill(OS and display integration via godot-autopilot: background or synchronous processes, environment variables, system info, dialogs, clipboard, text-to-speech and editor sub-windows. Most of these are side-effect tools - confirm before use. Use when shell commands, OS information or window management is needed.)gda_skill";

const char *kProjectConfigDescription =
    R"gda_skill(Project configuration via godot-autopilot: read and change project.godot settings with the two-step set-then-save flow, editor settings, InputMap action management, and C# assembly builds. Use when changing project settings or input actions.)gda_skill";

const char *kTipsGotchasDescription =
    R"gda_skill(Cross-domain tips, traps and known pitfalls for godot-autopilot tools: silent failures, JSON value shapes, size limits, prompt-vs-implementation conflicts, and Godot 4.7 API differences. Read when a tool call fails unexpectedly or before complex multi-tool operations.)gda_skill";

const char *kOsDisplayBody = R"gda_skill(# OS and Display Integration

Tools in this skill talk to the operating system and the desktop around the
editor: processes, environment, files, dialogs, clipboard, mouse, screens,
text-to-speech and editor sub-windows. These are registered domain tools; call
them through `call_tool` unless your MCP client exposes them directly.

## Side-effect warning - read before calling anything

Most of these tools change state outside the editor. The risk class of every
tool is machine-readable: the `get_tool_detail` response carries a side_effect
field. Check it before calling, and especially before batching.

- process - highest risk class. Covers `create_os_process`,
  `execute_os_process`, `kill_os_process`, `open_os_path`,
  `set_os_environment` and `build_csharp_assembly`. These start, stop or
  influence processes and the editor environment. Confirm intent before
  calling; only kill PIDs that came from `create_os_process`.
- shows_alert - user-visible popups and speech: `show_os_alert`,
  `show_display_dialog`, `speak_display_tts`, `stop_display_tts`. Never
  batch-trigger these without an explicit user request. `show_os_alert` blocks
  the editor UI until a human dismisses the dialog - in an unattended or
  automated run that is a hang, not a notification.
- modifies_window - changes desktop or window state: `set_display_clipboard`,
  `set_display_mouse_mode`, `warp_display_mouse` and every sub-window tool.
- writes_file - `write_file` and `move_os_file_to_trash`. Confirm the target
  path stays inside the project before writing.

Read-only queries (`get_os_datetime`, `get_os_system_info`, `read_file`,
`find_in_files`, the screen getters and similar) are safe to call freely.

## Processes: create, execute, kill

- `create_os_process` starts an executable as a detached background process and
  returns immediately with the PID. Use it for long-running or fire-and-forget
  programs. Keep the PID to stop it later with `kill_os_process`.
- `execute_os_process` runs a command synchronously and blocks until it exits.
  Pass `output` as true to capture stdout and stderr into the response (both
  are empty strings when `output` is false or omitted, which is the default).
  Use it for short commands whose result you need immediately.
- `kill_os_process` forcefully kills a PID. Killing is immediate and cannot be
  undone; unsaved data in the target process is lost.

```json
{"tool": "execute_os_process", "args": {"path": "git", "arguments": ["status", "--short"], "output": true}}
```

The response carries the exit code plus the captured output streams. Output is
not truncated per stream, but the whole tool response is bounded by the
server-wide JSON response size limit (see godot-autopilot-tips-gotchas).

Related process-class tools: `open_os_path` opens a URL or file path with the
default application (browser, file manager); `set_os_environment` sets an
environment variable in the editor process for this session only - it is not
persisted and only affects processes started afterwards.

## System information (read-only)

- `get_os_datetime` - calendar fields (year, month, day, weekday, hour,
  minute, second, dst); optional `utc` selects UTC over local time.
- `get_os_unix_time` - fractional Unix seconds, for elapsed-time measurement.
- `get_os_locale` - BCP 47 language code such as en or zh_CN.
- `get_os_system_fonts` - installed font names, useful before building font
  resources or theme font entries.
- `get_os_system_info` - OS name, version, processor count and processor name.
- `get_os_unique_id` - stable machine identifier (not a secret).
- `get_os_user_data_dir` - the project user data directory, e.g. where the
  on-disk game log lives.
- `get_os_environment` - one environment variable by `variable` name; returns
  an empty string when unset.

## File tools and the trash

- `write_file` writes or appends text to a file: `path`, `content`, and
  optional `mode` (WRITE overwrites by default, APPEND appends). Paths under
  res:// are engine-managed: imported asset types are queued for reimport,
  plain text files sync into the editor file system, and script files (.gd,
  .gdshader, .gdshaderinc, .cs) additionally return a diagnostics object
  reporting whether the script still loads. user:// and absolute paths are
  plain I/O outside editor tracking.
- `read_file` returns the full text of a file (absolute path, or res:// /
  user:// relative). No editor resource tracking.
- `find_in_files` searches text files recursively: `query` plus optional `dir`
  (default res://), `extensions` filter, `case_sensitive` and `max_results`
  (default 500). It stops early once the limit is reached and reports a
  truncated flag so results are never silently cut.
- `move_os_file_to_trash` moves a file or folder to the system trash
  (res:// and user:// paths are auto-globalized). This is the safe delete:
  removal is recoverable. On systems without a trash the deletion may fail.

## Clipboard, dialogs and mouse

- `get_display_clipboard` / `set_display_clipboard` - read and replace the
  system clipboard text. The clipboard is global to the OS and persists after
  the call.
- `show_display_dialog` - native modal dialog with `title`, `description` and
  a `buttons` array of label strings. It is fire-and-forget: the clicked
  button is NOT reported back, so never rely on the user's choice.
- `get_display_mouse_position` - current pointer position in screen
  coordinates.
- `warp_display_mouse` - move the pointer instantly to `x` / `y` screen
  coordinates, e.g. before simulating a click with the input tools.
- `set_display_mouse_mode` - cursor behavior: 0=visible, 1=hidden, 2=captured
  (pointer locked to the window, for FPS-style control), 3=confined,
  4=confined_hidden. Values outside 0-4 return an error.

## Screens and screen capture

- `get_display_screen_count` - number of monitors; valid indices are 0 to
  count-1 for every screen-indexed tool below.
- `get_display_screen_size` / `get_display_screen_position` - pixel size and
  desktop-space top-left corner of a screen.
- `get_display_screen_dpi` / `get_display_screen_refresh_rate` - density and
  refresh rate; some platforms report 0 when the value is unavailable.
- `capture_display_screen` - capture a whole physical display as a base64
  PNG (`screen` index, default 0). This is different from
  `capture_editor_viewport` (editor viewport) and `capture_game_viewport`
  (running game).

## Text-to-speech

- `get_display_tts_voices` - list available voices; each voice has an id that
  can be passed to `speak_display_tts` as `voice`.
- `speak_display_tts` - speak `text` aloud (optional `voice`, `volume` 0-100,
  `pitch`, `rate`). Speech is asynchronous.
- `stop_display_tts` - interrupt current speech; safe to call unconditionally.

These are user-visible side-effect tools: only speak when the user asked for
audible feedback, and never in batch loops.

## Editor sub-windows

Sub-windows live in the editor process. `create_display_window` returns a
window_id that every other window tool accepts; it takes an optional `mode`
(a Window.Mode enum value) and `rect` object with x, y, w, h fields (default
800x600 at the origin). Clean up with `delete_display_window`.

- `set_display_window_title` - set the title bar text, ideally right after
  creating the window so the user can identify it.
- `set_display_window_size` / `set_display_window_position` - resize to
  `width` / `height`, or move to `x` / `y` screen coordinates.
- `set_display_window_mode` - window mode as a DisplayServer.WindowMode enum
  value (e.g. fullscreen, minimized, maximized). Mind the trap: this enum is
  NOT the same as the Window.Mode enum accepted by `create_display_window`.
- `set_display_window_flag` - toggle a DisplayServer.WindowFlags value
  (always-on-top, borderless, resizable, ...) with `enabled`.
- `move_display_window_to_foreground` - raise and focus; useful right after
  creation.
- `request_display_window_attention` - flash the taskbar entry without
  stealing focus, e.g. after a long-running operation finishes.

All window tools default to the main window when `window_id` is omitted.

## Boundary: editor desktop vs the running game

Every tool in this skill acts on the editor process and the desktop around it.
None of it reaches a game launched with `play_editor_current_scene`: the game
is a separate process with its own display server. To run shell-like logic
inside the game or touch its OS integration, use `execute_game_script` (see
godot-autopilot-scripting) or the game channel tools (see
godot-autopilot-running-games).

## See also

- godot-autopilot-project-config - project.godot settings, editor settings,
  InputMap and C# builds
- godot-autopilot-tips-gotchas - size limits, silent failures and
  prompt-vs-implementation conflicts
- godot-autopilot-usage - connection prerequisites, tool discovery and the
  error protocol
)gda_skill";

const char *kProjectConfigBody = R"gda_skill(# Project Configuration

This skill covers project-wide configuration: project.godot settings, editor
preferences, InputMap actions, the C# build trigger and the main scene. All
tools are called through `call_tool` unless your MCP client exposes them
directly.

## Project settings: the two-step set-then-save flow

Read side:

- `get_project_settings` reads one setting by `name` (e.g.
  application/config/name), with an optional `default` returned when the
  setting does not exist.
- `has_project_settings` distinguishes a missing setting from a stored value.

Write side is a two-step flow - this is the single most important rule in this
skill:

1. `set_project_settings` changes the setting IN MEMORY ONLY. The tool
   description says it explicitly: the change is NOT written to
   project.godot automatically.
2. `save_project_settings` persists all project settings to project.godot on
   disk. Without this second call the change is lost when the editor closes.
   It is the only config tool that writes project.godot.

```json
{"tool": "set_project_settings", "args": {"name": "application/config/name", "value": "My Game"}}
{"tool": "save_project_settings", "args": {}}
```

Treat set-then-save as one atomic intention. When the new value is a string,
the engine keeps the existing type of a known setting (e.g. storing 1920 for
an int setting stays an int).

## Editor settings persist automatically

`get_editor_settings`, `set_editor_settings` and `has_editor_settings` read
and write the editor's own preference store (theme, docks, interface options),
which is separate from project.godot. Unlike project settings, editor settings
persist automatically - no explicit save step is required, and values survive
editor restarts. Verify a write with `get_editor_settings`.

## InputMap: adding an action end to end

The InputMap tools edit the editor project's InputMap. A running game loads
its InputMap at startup, so it never sees these changes until it is restarted.

1. `add_input_map_action` creates an empty action by `action` name; `deadzone`
   defaults to 0.5 (the analog threshold, range 0.0 to 1.0). No events are
   bound yet.
2. `add_input_map_action_event` binds an event: `event` must be an object with
   a concrete class field, e.g. an InputEventKey with keycode 65, and
   keycode/physical_keycode accept either numeric codes or KEY_* name strings
   such as KEY_A.
3. `save_input_map` writes the current InputMap into ProjectSettings and saves
   it to disk. Optional `actions` restricts persistence to the listed names;
   when omitted, actions prefixed with ui_ or containing a slash are skipped.
   The response reports the outcome: persisted count, readback verification,
   skipped actions and any save error.

```json
{"tool": "add_input_map_action", "args": {"action": "jump"}}
{"tool": "add_input_map_action_event", "args": {"action": "jump", "event": {"class": "InputEventKey", "keycode": "KEY_A"}}}
{"tool": "save_input_map", "args": {}}
```

Queries: `get_input_map_actions` lists all action names (including built-in
ui_* actions), `has_input_map_action` checks one.

Keycodes: trust the implementation, not remembered tables. The plugin's
built-in keycode prompt has known errors (two built-in prompts disagree on the
KEY_ENTER value - see godot-autopilot-tips-gotchas), so prefer KEY_* name
strings inside `event` objects; they resolve through the implementation's own
name-to-code table. Note that the separate input-injection tools use a
different parser that silently maps unrecognized key names to KEY_NONE instead
of erroring.

Removal flow: `erase_input_map_action_event` removes one event by zero-based
`event_index` (out-of-range returns an error); `erase_input_map_action`
removes the whole action and clears its persisted project setting;
`set_input_map_action_deadzone` adjusts the threshold in the 0.0 to 1.0 range.

## C# assembly builds

`build_csharp_assembly` triggers a C# project build by launching
`dotnet build --nologo` as an async OS process on the res:// root
.csproj/.sln. Limits to know before relying on it:

- It is intended for CI / command-line compile verification. It does NOT
  rewind to an in-editor Build and does NOT hot-reload the loaded .NET
  assembly.
- After changing C# class or method signatures, the editor-side view (script
  load, validation, GDScript interop) still requires the user to press Build
  in the editor or restart it; the plugin has no API to trigger the in-editor
  .NET assembly reload.
- GDScript and `code_execute` workflows are unaffected by this limitation.

The result reports the project file, the command, whether it started and the
PID; it errors when the project is not a C# project or dotnet is unavailable.

## Setting the main scene

`set_editor_main_scene` takes a scene `path` such as res://game.tscn and
persists it to project.godot directly - this tool writes the file itself, no
save step needed. Typical use right after creating a new level with
`create_editor_scene` and saving it.

## See also

- godot-autopilot-os-display - OS processes, files, dialogs, clipboard and
  sub-windows
- godot-autopilot-tips-gotchas - silent failures, size limits and known
  prompt-vs-implementation conflicts
- godot-autopilot-usage - connection prerequisites, tool discovery and the
  error protocol
)gda_skill";

const char *kTipsGotchasBody = R"gda_skill(# Tips, Traps and Known Pitfalls

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
)gda_skill";

} // namespace

std::vector<SkillSpec> make_system_skills() {
  return {
      {"godot-autopilot-os-display", kOsDisplayDescription,
       {{"SKILL.md", kOsDisplayBody}}},
      {"godot-autopilot-project-config", kProjectConfigDescription,
       {{"SKILL.md", kProjectConfigBody}}},
      {"godot-autopilot-tips-gotchas", kTipsGotchasDescription,
       {{"SKILL.md", kTipsGotchasBody}}},
  };
}

} // namespace godot_autopilot::skill_gen
