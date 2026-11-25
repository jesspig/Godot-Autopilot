# C# / .NET Workflows

A C# project runs on the editor's .NET assembly, so a build sits between editing
and observing, attributes reach Godot via the assembly, and a running game keeps
its old assembly.

## Bootstrap a C# project

A C# project needs a root `.csproj` (or `.sln`) under `res://` and a matching
`[dotnet] project/assembly_name` in `project.godot`.

1. Run `dotnet_environment`; on a plain project it returns an error with a
   ready-to-write template: `{path, content, target_framework, assembly_name}`
   plus a hint.
2. Write the template with `os_file_write` action=write; it targets the installed
   SDK.
3. Set `[dotnet] project/assembly_name` to the returned `assembly_name`.
4. Re-run `dotnet_environment`; it should report `project_file`,
   `dotnet_available`, and `dotnet_version`.

`assembly_loaded` only means the .NET module is loaded in the editor; verify the
project assembly with `create_csharp_script.diagnostics.instantiable` or a
runtime read-back.

## Create and edit C# files

- `create_csharp_script` (`path`, `source_code`, optional `overwrite`) creates a
  `.cs` file. Three gates fail with an actionable patch: the file stem must
  equal the declared class name, the class must be `partial` and derive from a
  ClassDB-known base, and the root `.csproj` must cover the file. It returns
  `verified`/`readback` and `diagnostics`.
- `os_file_write` action=write also works for `.cs` and returns diagnostics, but
  has no class-name or `partial` gates.
- `patch_script` edits one anchored spot and compiles before writing.
- `create_script` writes GDScript only; do not use it for C#.

`diagnostics.ok` means the file loads as a script resource.
`diagnostics.instantiable` means the class is in the loaded assembly and
`can_instantiate()` is true. A new class normally reports `ok=true,
instantiable=false` until `build_csharp_assembly` succeeds; check `instantiable`
before `new()` or before attaching a script that must run in the editor.

## Development loop

1. Create or edit the `.cs` file (`create_csharp_script`, `os_file_write`
   action=write, or `patch_script`).
2. Build with `build_csharp_assembly` and prefer `wait=true`
   (`timeout_ms` 1-25000). It runs `dotnet build --nologo`, returning
   `BuildResult`: `success`, `errors`, `output_digest`, truncation flags,
   `game_running`, and `restart_required`. Without `wait` it only starts the
   process and returns `{pid, started, game_running}`.
3. Read the compiler verdict: `success`, `errors`, and `output_digest`.
   `get_debugger_stream` action=log covers editor-side script errors that are not
   compiler output.
4. Save the scene, run it with `editor_scene_control` action=play, and gate on
   `get_game_state` action=status.
5. Diagnose with `get_debugger_stream` action=errors (`since_seq`/`next_seq`) and
   `get_game_log_entries`.
6. Verify with `capture_editor_viewport` target=game or read values back with
   `property_get` or `csharp_member` action=get.
7. Fix and repeat.

The error watermark is not the script error list:
`new_errors_since_last_call` counts tool-response errors and game
`runtime_error` payloads; it does not count compiler output or GDScript parse
errors. Use `BuildResult.success`/`errors`, `get_debugger_stream` action=log, and
action=errors with `since_seq`.

Restart rules:

- Editor side: new classes usually instantiate without a restart; signature
  changes may need the editor's Build action or a restart.
- Game side: a running process keeps the old assembly. A successful `wait=true`
  build reports `restart_required=true`; stop and relaunch before judging.
- `reload_game_scripts` reloads GDScript only.

## [Export] properties

`property_set` writes C# `[Export]` fields through the same JSON shapes as
GDScript exports:

- Node-typed fields accept a path string; `converted_node_path` confirms it. An
  unresolvable path is an error.
- A rejected assignment is reported as `value not applied`; the tool attempts to
  restore the old value and reports whether it succeeded.
- After an assembly-changing build the editor may re-instantiate script scenes.
  Re-read exported values before saving, then save and read the `.tscn` back.

## C# resources and attachment

- `resource_create` action=create accepts C# `[GlobalClass]` classes by name once
  the class is in the loaded assembly. `name` registers a `memory://` locator.
- `resource_property` action=set addresses an instance by `name` or `object_id`;
  `resource_save` action=save writes it to disk before a node property can
  reference it.
- `attach_script_to_node` attaches a C# script and records one undo step.
  Replacing a script does not migrate exported references; rewire them.

## Reflection bridge

`property_get` and `game_script` action=execute only reach members visible to
GDScript. For C# getters, overloads, and events, use `csharp_member` action=list
/ get / set / call and `csharp_event` action=subscribe / unsubscribe with
`runtime` set to editor (default) or game.

- Editor side needs no manual singleton registration; the plugin loads
  `res://addons/godot-autopilot/GdaCSharpBridge.cs` when the singleton is absent.
- Game side requires a `GdaCSharpRuntime` node; it creates the `__gda_runtime`
  holder. Add it as an autoload with
  `gda call update_autoloads --args '{"action":"add","name":"GdaCSharpRuntime","path":"res://addons/godot-autopilot/GdaCSharpRuntime.cs","enabled":true}'`,
  or install with `gda install plugin --with-csharp-runtime`. Missing bridge
  now returns `missing_csharp_runtime` with this command as `next_action`.
  Only scene-path locators cross into the game; `object_id` and `memory` are
  editor-process handles.
- A locator that resolves to a script resource is rejected; pass a node path or
  `object_id_str` of a live instance.

Edit, build the assembly, read the debugger log, then query the live instance.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| `instantiable: false` after creating a script | Assembly not rebuilt | `build_csharp_assembly` with `wait: true` |
| `create_csharp_script` rejects the file | Stem/class mismatch or not `partial` | Apply the returned patch |
| Game runs old code | Running process kept the old assembly | Stop and relaunch after the build |
| Reflection member not found | Member not visible or wrong runtime | `csharp_member` action=list; check editor vs game |
