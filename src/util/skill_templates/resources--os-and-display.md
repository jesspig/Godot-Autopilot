# OS and Display Integration

OS and display integration - an extension of the godot-autopilot-resources
skill. These tools talk to the operating system and the desktop around the
editor: processes, environment, files, dialogs, clipboard, mouse, screens,
text-to-speech and editor sub-windows. All are registered domain tools; call
them through `call_tool` unless your MCP client exposes them directly.

Four file tools - `write_file`, `read_file`, `find_in_files` and
`move_os_file_to_trash` - echo the resource and file operations documented in
the godot-autopilot-resources skill (imports, rename with reference
rewriting, path rules). This page covers their OS-facing behavior; everything
else here is a standalone OS integration surface.

## Side-effect classes

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

- `create_os_process` starts an executable as a detached background process
  and returns immediately with the PID. Use it for long-running or
  fire-and-forget programs. Keep the PID to stop it later with
  `kill_os_process`.
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
server-wide JSON response size limit (see the godot-autopilot skill).

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
  PNG (`screen` index, default 0). When this tool is invoked through
  `call_tool`, the PNG is delivered as an MCP image content block and the
  text JSON's `data` field is replaced with `<attached-as-image-content>`
  plus image_attached: true; inside `batch_execute`/`code_execute` the JSON
  keeps the full base64 string. This is different from
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

Every tool on this page acts on the editor process and the desktop around it.
None of it reaches a game launched with `play_editor_current_scene`: the game
is a separate process with its own display server. To run shell-like logic
inside the game or touch its OS integration, use `execute_game_script` (see
the godot-autopilot-scripting skill) or the game channel tools (see the
godot-autopilot-runtime skill).

## See also

- godot-autopilot-resources - resource load/save/rename, imports and path
  rules; shares the four file tools described above
- godot-autopilot-runtime - launching the game, input injection and runtime
  debugging
