# C# / .NET Workflows

C# projects run on the editor's .NET assembly, which changes the workflow in
three ways: a build step sits between editing and observing, C# declarations
such as `[GlobalClass]` and `[Export]` reach Godot through the assembly, and
nothing hot-reloads. This skill covers the C#-specific paths. The GDScript
execution channels (`execute_script`, `call_script_node`, `execute_game_script`,
`code_execute`) and the script lifecycle tools are not affected by .NET
assembly state - use them for editor-side automation even in a C# project -
and they are documented by `godot-autopilot-scripting`; its C# section is only
a pointer back to this skill.

## Prerequisite: the editor must have loaded the assembly

- C# support is provided by the editor's .NET assembly. Global class
  registration, C# script loading and GDScript-to-C# interop all require the
  editor to have loaded a successfully built assembly.
- `create_resource` instantiates C# `[GlobalClass]` resource classes by name
  only once the editor has them registered; until then the call errors with
  "not a known class" or "not instantiable".
- The plugin cannot trigger the in-editor .NET assembly reload: that ability
  lives inside the engine's Mono module and is not exposed to GDExtension.
  After changing C# class or method signatures, the editor-side view (script
  load, validation, GDScript interop) still needs the user to press Build in
  the editor or to restart the editor.

## Creating and editing C# files

There is no create-script tool for C#: `create_script` writes GDScript only.
Use `write_file` for .cs content - res:// writes are engine-managed, and for
script-like files (.gd, shaders, .cs) the response includes a `diagnostics`
object reporting whether the written script still loads. When its ok is false
the file on disk failed to load; read `get_debugger_log` for details instead
of guessing. `copy_resource_file` duplicates an existing .cs when you want a
starting point that already compiles.

## The C# development loop

1. Edit the .cs file with `write_file` (new files are created the same way).
2. Build with `build_csharp_assembly` (no parameters). It launches
   `dotnet build --nologo <project>` as an async OS process on the res://
   root .csproj/.sln; the response reports the project file, command, started
   and pid, and the call errors when the project is not a C# project or
   dotnet is unavailable. It is compile verification only: it does not stand
   in for the editor's Build action and does not hot-reload the loaded
   assembly.
3. Read the build outcome. The tool returns as soon as the process starts and
   does not stream the compiler output, so confirm through
   `get_debugger_log` - the editor-process log buffer where C# compile and
   parse errors are recorded - before running anything.
4. Run the scene with `play_editor_current_scene` (save it first) and gate on
   `get_game_status`; a half-dead process is stopped and restarted, not
   debugged.
5. Diagnose while it runs: `get_debugger_errors` for the structured error
   list plus the one-shot `new_errors_since_last_call` watermark on your next
   call. `get_game_log_entries` reads the on-disk game log when the debug
   channel may have dropped messages.
6. Verify with a real observation: `capture_game_viewport` for the visible
   result, or read values back with `property_get` / `get_script_property`.
   Fix and repeat; the watermark confirms a fix produced no new errors.
7. Remember the no-hot-reload rule: `reload_game_scripts` reloads GDScript
   files only. A .cs change needs a fresh build, and the running game keeps
   using the previous assembly until it is restarted - changes never reach a
   live process on their own.

Because the build runs asynchronously and reports only the PID, give it time
to finish writing the assembly before launching (or relaunching) the game;
otherwise the new process still loads the old assembly.

## C# resources and script attachment

- `create_resource` accepts C# `[GlobalClass]` classes by name alongside
  ClassDB classes and GDScript global classes. Pass `name` to register the
  instance as a memory:// locator for later `set_resource_property` /
  `save_resource` calls; abstract or invalid scripts are rejected.
- `set_resource_property` addresses the instance by `name` or `object_id`
  and uses the usual JSON value shapes, including `{"path": "res://..."}` for
  resource-typed fields. `save_resource` writes it to disk; a memory://
  resource must be saved before a node property can reference it.
- `attach_script_to_node` attaches any resource the loader resolves as a
  Script - including a C# script - and registers the change with the editor
  undo history. Replacing a node's script does not migrate the old script's
  exported values: stored references such as node_paths can be lost and must
  be re-wired with `property_set` afterwards.

## [Export] properties and the conversion rules

`property_set` writes C# `[Export]` fields through the same JSON value shapes
as GDScript exports (full table in `godot-autopilot-scene-system`):

- Node-typed fields accept a node path string; the response reports
  converted_node_path when that string was converted to a node reference.
  An unresolvable path is an error, not a silent ok.
- Typed arrays (`[Export] Node[]`, Array[Resource]) convert element by
  element; node elements accept a path string or `{"__node_ref__": "..."}`,
  resource elements accept a res:// path or a memory:// locator.
- An assignment the engine does not actually apply (a C# property whose
  setter clears it back to null, or a read-only / private-setter property) is
  reported as an error "value not applied": the tool detects the rejected
  read-back, attempts to restore the old value and says whether the restore
  succeeded. Check the response instead of assuming the write landed.
- C# properties without a getter are invisible to GDScript: reading one
  returns null, which is easy to misread as a missing property. Validate
  through readable exported fields rather than derived or write-only ones.

## See also

- `godot-autopilot-scripting` - the GDScript execution channels, unaffected
  by .NET assembly state.
- `godot-autopilot-scene-system` - node CRUD, property JSON shapes and the
  C# notes in its property reference.
- `godot-autopilot-resources` - the create/save/duplicate flows behind
  `create_resource`.
- `godot-autopilot-runtime` - launching the game, debug channel semantics
  and the log/error fallbacks used above.
- `godot-autopilot` - tool discovery, the error watermark and retryable
  soft errors.
