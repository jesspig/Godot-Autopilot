---
name: godot-autopilot-csharp
description: "Godot C#/.NET project guide: bootstrapping a csproj from a plain project, creating C# scripts with create_csharp_script, the dotnet_environment probe, the build_csharp_assembly wait loop with compiler output, reading diagnostics.instantiable, running the game and confirming with get_debugger_errors plus the error watermark, C# [GlobalClass] resources via create_resource, [Export] node and typed-array conversion in property_set, and the reflection bridge with csharp_get_property/call_method/subscribe_event. Use when working in a C# Godot project."
---

# C# / .NET Workflows

Invocation: use the native MCP tools first; when that channel is down, follow the calling ladder in the godot-autopilot-tools skill (the gda CLI, via bunx, a global install, or the standalone gda binary); with no CLI available, work through its references/manual-fallback.md.

C# projects run on the editor's .NET assembly, which changes the workflow in
three ways: a build step sits between editing and observing, C# declarations
such as `[GlobalClass]` and `[Export]` reach Godot through the assembly, and a
running game never picks up a new assembly on its own. This skill covers the
C#-specific paths. The GDScript execution channels (`execute_script`,
`call_script_node`, `execute_game_script`, `code_execute`) are unaffected by
.NET assembly state and are documented by `godot-autopilot-scripting`.

## Bootstrap: from a plain project to a C# project

A C# project needs a `res://` root `.csproj` (or `.sln`) and a matching
`[dotnet] project/assembly_name` in `project.godot`.

1. Run `dotnet_environment`. On a plain project it returns an error carrying a
   ready-to-write template:
   `{path, content, target_framework, assembly_name}` plus a `hint`.
2. Write the template with `write_file` (the content is a
   `Godot.NET.Sdk/<engine-version>` project targeting the installed .NET SDK).
3. Set `[dotnet] project/assembly_name` to the returned `assembly_name`.
4. Re-run `dotnet_environment`; it should now report `project_file`,
   `dotnet_available` and `dotnet_version`.

`dotnet_environment.assembly_loaded` only says the .NET module is loaded in
the editor; it does **not** mean the project assembly is loaded. Use
`create_csharp_script.diagnostics.instantiable` or a runtime read-back to
verify the project assembly.

## Creating and editing C# files

- `create_csharp_script` (`path`, `source_code`, optional `overwrite`) is the
  preferred way to create a `.cs` file. Three gates fail with an actionable
  patch instead of a silent ok: the file stem must equal the declared class
  name, the class must be `partial` and derive from a ClassDB-known Godot base,
  and the root `.csproj` must cover the file. It returns the file record plus
  `verified`/`readback` and a `diagnostics` object.
- `write_file` still works for `.cs`; it returns a `diagnostics` object for
  script-like files but has no class-name or `partial` gates.
- `patch_script` edits one anchored spot in an existing file and compiles
  before writing; `copy_resource_file` duplicates a compiling `.cs` as a
  starting point.
- `create_script` writes GDScript only; do not use it for C#.

### Diagnostics semantics

`diagnostics.ok` means the `.cs` file loads as a script resource.
`diagnostics.instantiable` means the class is present in the loaded assembly
and `can_instantiate()` is true. A newly created class normally reports
`ok=true, instantiable=false` until `build_csharp_assembly` succeeds; read
`instantiable` before calling `new()` or attaching a script that must run in
the editor.

## The C# development loop

1. Create or edit the `.cs` file (`create_csharp_script`, `write_file` or
   `patch_script`).
2. Build with `build_csharp_assembly` and prefer `wait=true`:
   `{wait: true, timeout_ms?: 1..25000}` runs `dotnet build --nologo` in the
   foreground and returns `BuildResult`:
   `{project_file, project_path, command, exit_code, success, errors,
   output_digest, output_empty, output_truncated?, errors_truncated?, note,
   game_running, restart_required?}`. Without `wait` the call only starts the
   process and returns `{pid, started, game_running}`.
3. Read the result, not a guess: `success`, `errors` and `output_digest` are
   the compiler verdict. `get_debugger_log` remains the place for editor-side
   script errors that are not compiler output.
4. Run the scene with `play_editor_current_scene` (save it first) and gate on
   `get_game_status`; a half-dead process is stopped and restarted, not
   debugged.
5. Diagnose while it runs: `get_debugger_errors` returns structured entries
   with `seq`; pass `since_seq` and use the top-level `next_seq` to read only
   new errors. `get_game_log_entries` reads the on-disk game log when the
   debug channel may have dropped messages.
6. Verify with a real observation: `capture_game_viewport` for the visible
   result, or read values back with `property_get` / `get_script_property` /
   `csharp_get_property`.
7. Fix and repeat.

### The error watermark is not the script error list

`new_errors_since_last_call` counts tool-response errors and game
`runtime_error` payloads. It does **not** count compiler output or GDScript
parse errors logged by the engine. Do not treat it as "the C# fix is clean";
use `BuildResult.success` / `errors`, `get_debugger_log` (editor process) and
`get_debugger_errors` with `since_seq` (running game).

### Restart rules

- Editor side: after a successful build, new classes can usually be
  instantiated without restarting the editor; signature changes may still
  require the editor's own Build action or a restart because GDExtension cannot
  trigger the engine's .NET assembly reload.
- Game side: a running process keeps the old assembly. `build_csharp_assembly`
  reports `game_running` and, for a successful `wait=true` build, sets
  `restart_required=true`; stop and relaunch the game before judging the new
  code.
- `reload_game_scripts` reloads GDScript files only.

## [Export] properties and save timing

`property_set` writes C# `[Export]` fields through the same JSON shapes as
GDScript exports (full table in `godot-autopilot-scene-system`):

- Node-typed fields accept a node path string; `converted_node_path` confirms
  the conversion. An unresolvable path is an error, not a silent ok.
- Typed arrays (`[Export] Node[]`, `Array[Resource]`) convert element by
  element; node elements accept a path string or `{"__node_ref__": "..."}`,
  resource elements accept a `res://` path or a `memory://` locator.
- A rejected assignment (read-only property, setter that clears the value) is
  reported as "value not applied"; the tool attempts to restore the old value
  and reports whether the restore succeeded.
- After an assembly-changing build the editor may re-instantiate script
  scenes. Re-read the exported values before saving, then save and read the
  `.tscn` back; do not assume a value written before the build survived it.

## C# resources and script attachment

- `create_resource` accepts C# `[GlobalClass]` classes by name once the class
  is in the loaded assembly. Pass `name` to register a `memory://` locator;
  abstract or invalid scripts are rejected.
- `set_resource_property` addresses an instance by `name` or `object_id`;
  `save_resource` writes it to disk before a node property can reference it.
- `attach_script_to_node` attaches a C# script resource and registers one undo
  step. Replacing a node's script does not migrate exported references; rewire
  them with `property_set` afterwards.

## Reflection bridge: C# members that GDScript cannot see

`property_get` and `execute_game_script` only reach members visible to
GDScript. For C# getters, overloads and events use `csharp_list_members` to
discover the member, then `csharp_get_property`, `csharp_set_property`,
`csharp_call_method`, `csharp_subscribe_event` and `csharp_unsubscribe_event`
with `runtime` editor (default) or game.

- Editor side needs no manual singleton registration: the plugin loads and
  instantiates `res://addons/godot-autopilot/GdaCSharpBridge.cs` when the
  engine singleton is absent.
- Game side requires a `GdaCSharpRuntime` node in the running scene; it creates
  the `__gda_runtime` holder the tools locate. Only scene-path locators cross
  into the game (`object_id`, `object_id_str` and `memory` are editor-process
  handles).
- A locator that resolves to a script resource (for example
  `res://scripts/Player.cs`) is rejected with a precise error; pass a node path
  or `object_id_str` of a live instance.
- A running game's `__gda_runtime` holder is excluded from
  `get_game_status.node_count` and reported separately as
  `instrumentation_node_count`.

Endpoints, reflection limits, export-build trimming and subscription token
lifetime are documented in references/csharp-bridge.md.

## See also

- `godot-autopilot-scripting` - the GDScript execution channels, unaffected
  by .NET assembly state.
- `godot-autopilot-scene-system` - node CRUD, property JSON shapes and the
  C# notes in its property reference.
- `godot-autopilot-resources` - the create/save/duplicate flows behind
  `create_resource`.
- `godot-autopilot-runtime` - launching the game, debug channel semantics
  and the log/error fallbacks used above.
- `godot-autopilot-tools` - tool discovery, the error watermark and retryable
  soft errors.
