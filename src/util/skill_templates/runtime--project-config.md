# Project Configuration

Pre-run project configuration - an extension of the godot-autopilot-runtime
skill. This page covers project-wide configuration: project.godot settings,
editor preferences, InputMap actions, the C# build trigger and the main
scene. All tools are called through `call_tool` unless your MCP client
exposes them directly.

Relation to the runtime skill: a running game loads its InputMap at startup,
so InputMap edits made here only reach games started afterwards - and
simulated keys in the running game go through the input-injection tools in
the godot-autopilot-runtime skill, not through these configuration tools.
The main scene set here is the scene `play_editor_current_scene` launches.

## Project settings: the two-step set-then-save flow

Read side:

- `get_project_settings` reads one setting by `name` (e.g.
  application/config/name), with an optional `default` returned when the
  setting does not exist.
- `has_project_settings` distinguishes a missing setting from a stored value.

Write side is a two-step flow - this is the single most important rule on
this page:

1. `set_project_settings` changes the setting IN MEMORY ONLY. The change is
   NOT written to project.godot automatically.
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
its InputMap at startup, so it never sees these changes until it is
restarted.

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
built-in keycode prompt has known errors (two built-in prompts disagree on
the KEY_ENTER value - see the godot-autopilot skill), so prefer KEY_* name
strings inside `event` objects; they resolve through the implementation's own
name-to-code table. Note that the input-injection tools used at runtime use a
different parser that silently maps unrecognized key names to KEY_NONE
instead of erroring.

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
`create_editor_scene` and saving it; the game then starts from that scene
when the runtime tools launch it.

## See also

- godot-autopilot-runtime - launching and stopping the game, input injection
  with frame-accurate timing, and runtime debugging
- godot-autopilot-scripting - the GDScript execution channels, including
  `code_execute`
