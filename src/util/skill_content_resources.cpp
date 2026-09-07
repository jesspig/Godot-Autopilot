#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kResourcesFilesDescription =
    R"gda_skill(Resource and file operations via godot-autopilot: load, save, create, duplicate and delete resources; rename and move files with automatic reference rewriting; dependency queries; imports; and directory management under res://. Use when working with .tres/.tscn/.import files or the project file layout.)gda_skill";

const char *kScriptingDescription =
    R"gda_skill(GDScript workflows via godot-autopilot: create, attach, edit and reload scripts; the three execution channels (execute_script in the editor, call_script_node on nodes, execute_game_script in the running game) and the code_execute meta tool, with their safety differences. Use when writing or running GDScript.)gda_skill";

const char *kResourcesFilesBody =
    R"gda_skill(# Resource and File Operations

Working with .tres/.tscn files, .import sidecars, UIDs and the res:// layout through the resource domain tools plus three text-file tools (`write_file`, `read_file`, `find_in_files`). Every tool is invoked through `call_tool`; see the godot-autopilot-usage skill for the discovery protocol.

## Load, create, duplicate and save

- `load_resource` loads a file from disk into memory, with optional `type_hint`, and returns a locator (`class`, `path`, `object_id`, `object_id_str`) that the property tools accept.
- `create_resource` instantiates a Resource subclass by class name (`type`); pass `name` to register the instance so later calls can reference it as `memory://name`.
- `duplicate_resource` copies a file-backed resource; `deep` (default false) controls sub-resource copying. In-memory instances must be saved and reloaded before they can be duplicated.
- `save_resource` writes a resource to `dest_path` (defaults to `path`). Locators: `object_id`/`object_id_str` (from `create_resource`), in-memory `name`, or `path`; if none resolves, `class_type` plus `name` creates and registers a new instance. Missing directories are created recursively, the editor file system is refreshed, and the saved file is loaded back for verification.

```
create_resource  type=Resource name=player_stats
save_resource    name=player_stats path=res://data/player_stats.tres
```

The save response reports directories_created and a verified flag; verified=true means the file was read back and loads.

memory:// caveat: a memory:// resource must never be assigned to a node property (that corrupts the scene file). Save it to disk with `save_resource` first, then assign by file path. The same rule applies to TileSet and SpriteFrames resources built by the tilemap and animation tools.

`create_directory` creates a res:// directory including missing parents, is idempotent (reports already_existed when present), and triggers a file system scan so subsequent calls see it immediately.

## Threaded loading

For large resources use the three-step chain so the editor stays responsive:

```
1. load_resource_threaded             starts the background load (use_sub_threads enables parallel loading)
2. get_resource_load_threaded_status  poll until the status is loaded (values: invalid_resource, in_progress, failed, loaded)
3. get_resource_load_threaded         fetches the resource; blocks until complete
```

## Introspection and search

Type, existence and directory listings:

- `get_resource_type` reports the engine class a file actually holds; `get_resource_types` lists every instantiable Resource subclass (valid `type` values for `create_resource`); `get_resource_extensions` lists recognized extensions per type.
- `has_resource` asks the engine loader whether a path is loadable - it is not a plain file-existence check.
- `get_resource_dir_files` lists a directory's entries; subdirectories appear as plain names, so combine with `has_resource` to tell resources from folders.

UIDs and the dependency graph:

- `get_resource_uid` reads the numeric UID behind a uid:// reference (-1 when the file has none); `set_resource_uid` assigns one (auto-generated when `uid` is omitted) and triggers a reimport so the assignment persists.
- `get_resource_dependencies` lists the external files a resource references; `has_resource_dependency` checks one specific edge.
- `get_resource_references` is the reverse lookup - who references this resource. Pass exactly one of `path`, `uid` or `class_name`; each hit reports the matched kinds (path, uid, script_class).

Plain-text search:

- `read_file` returns a file's full text (res://, user:// or absolute path).
- `find_in_files` recursively searches text files: `query` is required; `dir` defaults to res://; `extensions` defaults to gd/tscn/tres/cs/md/json/h/cpp; `max_results` defaults to 500 and hitting the limit reports truncated: true. This is the standard way to prove that a rename or move rewrote every reference.

## Transactional rename and move

`rename_resource_file` (`path` plus `new_path`; from and to are accepted as aliases) and `move_resource_file` (`path` plus `new_directory`) are engine-aware transactions, not raw file moves. For each affected file they:

1. Save open scenes that equal or reference the file, rewrite the file on disk, then reload those scenes (their undo history resets).
2. Rewrite path= and uid= tokens in every dependent .tscn/.tres under res://.
3. Move the .uid sidecar with the file (the .import sidecar of imported assets migrates too), keeping the UID stable.
4. Remap project.godot entries pointing at the old path - application/run/main_scene and autoload/* values, preserving the leading * singleton marker - and save the file.
5. Notify the editor file system.

`move_resource_file` keeps the file name and takes `new_directory` inside res:// (absolute like res://assets/sfx or relative like assets/sfx; trailing slashes are trimmed; user:// is rejected). When `path` is a directory, every file moves recursively preserving the sub-folder layout, *.uid files follow their owners, empty source folders are left behind, and a file system scan runs afterwards.

Read the structured report instead of assuming success. `rename_resource_file` returns updated_files (per-file change counts), stale_references ({file, token} leftovers that could not be rewritten), uid_preserved, remapped_settings, saved_scenes, reloaded_scenes, unsupported_binary_references (binary resources such as .scn/.res/.csv/.translation cannot be rewritten in place) and import_sidecar_warning when a .import move failed. `move_resource_file` returns moved ({from, to, updated_files, uid_preserved}), failed, plus the aggregated stale_references, unsupported_binary_references, saved_scenes, reloaded_scenes and import_sidecar_warnings.

A script_class token in stale_references means a global class_name changed: run `scan_editor_file_system` afterwards so the class re-registers.

Verified transaction behavior (asserted by the L2 integration suite): after a rename, find_in_files finds the new path and no longer finds the old path anywhere under the directory, `get_resource_references` flips from old to new, and uid_preserved is true. After a move, moved is non-empty while failed and stale_references are empty, reloaded_scenes lists the referencing scenes that were saved and reloaded, and `has_resource_dependency` immediately reports the new location.

```
rename_resource_file:
  path:     res://assets/enemy.tres
  new_path: res://assets/actors/enemy.tres

move_resource_file:
  path:          res://assets/sfx/boom.wav
  new_directory: res://assets/audio
```

## Deleting files safely

`remove_resource_file` is a two-stage flow:

1. Dry-run (default, no `force`): a reverse-reference scan over every .tscn/.tres under res:// (path and uid tokens) returns would_delete plus dependents ({file, matched} objects) and a hint. Nothing is deleted - even with zero dependents an explicit force=true is required.
2. force=true: the file is preferentially moved to the OS trash (trashed=true; permanent=true only when the trash is unavailable or fails), its cached reference path is cleared, the .uid sidecar travels with it, and the editor file system is updated.

To discard a whole directory, `move_os_file_to_trash` (OS domain) trashes a res:// or user:// folder in one call.

## Imports and editor readiness

- `reimport_resource_files` (editor only) queues one or more files (`files` array or a single `path`) for reimport. It is asynchronous: the response confirms the queued count, not completion. Poll `get_editor_file_system_status` (editor domain) until the scan/import is idle.
- `set_resource_uid` triggers a filesystem reimport so the assignment persists.
- `scan_editor_file_system` rescans the project - run it after a class_name rename that left script_class tokens, or after out-of-band file changes; directory moves trigger one automatically.

While the editor is importing or scanning, resource write operations fail soft with a readiness error instead of corrupting state:

```
{"error": "editor is currently importing/scanning resources; retry shortly",
 "retryable": true, "retry_after_ms": 500}
```

Wait for retry_after_ms, then retry the same call.

## Path rules

- Resource operations - load, save, create, duplicate, rename, move, delete, reimport and the dependency/reference queries - accept res:// paths only.
- `write_file` and `read_file` also accept user:// (and `read_file` accepts absolute paths). Never splice a path from one namespace into the other.
- All paths are normalized and boundary-checked before use: path traversal, absolute-path escapes, unknown schemes and writes outside the project are rejected with a structured error.
- `write_file` inside res:// is engine-managed: imported asset types are queued for reimport, plain text files get update_file, and script-like files (.gd/.gdshader/.gdshaderinc/.cs) additionally return a diagnostics object reporting whether the script still loads. Writes to user:// or absolute targets are plain I/O and report engine_managed: false.

## See also

- `godot-autopilot-properties-signals` - assigning resources to node properties (memory:// is rejected)
- `godot-autopilot-scene-building` - scene lifecycle; open scenes are saved and reloaded by rename/move
- `godot-autopilot-tilemap` - memory:// TileSet that must be saved before it can be assigned
- `godot-autopilot-tool-map` - task-to-tool routing
- `godot-autopilot-tips-gotchas` - size limits and silent-failure catalog
)gda_skill";

const char *kResourcesToolReferenceBody =
    R"gda_skill(# Resource Tool Reference

The 24 resource domain tools plus the three text-file tools from the OS domain. Domain tools are invoked through `call_tool`; see the godot-autopilot-usage skill for the protocol.

## Resource domain (24 tools)

| Tool | Purpose |
|---|---|
| `load_resource` | Load a resource file into memory with optional `type_hint`; returns a locator for the property tools. |
| `load_resource_threaded` | Start an asynchronous background load and return immediately; step 1 of the threaded-load chain. |
| `get_resource_load_threaded_status` | Poll a threaded load: invalid_resource, in_progress, failed or loaded. |
| `get_resource_load_threaded` | Blockingly fetch the resource from a completed threaded load; step 3 of the chain. |
| `save_resource` | Save a resource to `dest_path`; creates directories recursively, refreshes the file system, read-back verified. |
| `create_resource` | Instantiate a Resource subclass by `type` in memory; `name` registers it as a memory:// locator. |
| `duplicate_resource` | Duplicate a file-backed resource; `deep` (default false) copies sub-resources. |
| `get_resource_type` | Report the engine class name a file actually holds. |
| `has_resource` | Check loadability through the engine loader (not plain file existence). |
| `get_resource_types` | List every instantiable Resource subclass registered in the engine. |
| `get_resource_extensions` | List recognized file extensions for a resource `type` (all known types when omitted). |
| `get_resource_dir_files` | List a directory's entries through the resource loader. |
| `get_resource_uid` | Read the numeric UID behind a uid:// reference (-1 when absent). |
| `set_resource_uid` | Assign a UID (auto-generated when `uid` is omitted); triggers a reimport. |
| `remove_resource_file` | Two-stage delete: dry-run impact pre-check by default; force=true moves the file to the OS trash. |
| `rename_resource_file` | Transactional rename: rewrites path=/uid= references, migrates sidecars, remaps project.godot, reports updated_files and stale_references. |
| `move_resource_file` | Move a file or directory tree to another res:// directory through the same transaction pipeline. |
| `create_directory` | Create a res:// directory idempotently, then trigger a file system scan. |
| `get_resource_dependencies` | List the external files a resource references. |
| `has_resource_dependency` | Check whether a resource depends on one specific file. |
| `get_resource_references` | Reverse lookup: which .tscn/.tres files reference a resource (by `path`, `uid` or `class_name`). |
| `reimport_resource_files` | Queue reimport of one or more files; asynchronous, editor only. |
| `set_resource_property` | Set a property on a resource located by `object_id`/`object_id_str`, `name` or `path`. |
| `get_resource_property` | Read a property value from a located resource. |

## Text-file tools (OS domain)

| Tool | Purpose |
|---|---|
| `write_file` | Write or append text to a file; res:// writes are engine-managed (reimport or update_file), user:// and absolute paths are plain I/O. |
| `read_file` | Read a text file (absolute, res:// or user://) and return its full content. |
| `find_in_files` | Recursively search text files for a query string, with per-file occurrence counts and a truncated flag. |

Property locators: `object_id`/`object_id_str` (from `create_resource` or `load_resource`), in-memory `name`, or file `path`.
)gda_skill";

const char *kScriptingBody =
    R"gda_skill(# GDScript Workflows

Creating, attaching, editing and reloading GDScript through godot-autopilot, and the safety differences between the three execution channels plus the code_execute meta tool.

## Script lifecycle

- `create_script` compiles before saving: compilation errors abort the call with parser output, so a success means the file on disk parses. `overwrite` defaults to false. The saved content is read back from disk and verified; a locked file surfaces as verified=false instead of silent success. Never issue parallel `create_script` calls for the same path - serial writes to one file are not synchronized.
- `attach_script_to_node` and `detach_script_from_node` operate on the edited scene and are registered with the editor undo/redo. A script without `@tool` cannot be instantiated in the editor, so its methods only run once the game runs - `call_script_node` reports the same restriction.
- `reload_script` reloads from disk after an external edit; `keep_state` (default false) preserves instance state across the reload.
- `get_script_property_list` lists a script's declared variables with name, type and usage flags.
- `get_script_property` reads a value: with `script_path` it returns the declared default; with `node_path` the node's current value. Pass exactly one of the two.
- `set_script_property` writes a node property directly - no undo entry, no type or read-only validation. It is the lightweight channel for quick in-memory tweaks; use `property_set` when undo support and validation matter.

## Three execution channels

| Channel | Runs where | @tool needed | Timeout | Sweet spot |
|---|---|---|---|---|
| `execute_script` | editor process, synchronous | no | none - long code blocks the editor | editor automation with `SceneRoot` access |
| `call_script_node` | an existing node instance in the edited scene | yes | default tool timeout | calling one method with real instance state |
| `execute_game_script` | the running game process | no | runtime channel default | game-state queries and mutations |
| `code_execute` | editor process, wrapped temporary node | wrapped for you | `timeout_ms`, default 5000, cap 30000 | loops, bulk edits, computed results |

All four execute arbitrary GDScript and are treated as highest-risk side effects.

- `execute_script` auto-returns a single expression's value; multi-line code needs an explicit return. print() output lands in the output field, errors in the errors field. The environment exposes `SceneRoot` (the edited scene root); reach scene nodes via SceneRoot.get_node(...).
- `call_script_node` runs inside the existing node instance, so its state is real, not reconstructed. Non-`@tool` scripts error - run the game instead for those.
- `execute_game_script` runs inside the running game. Unlike the code_execute sandbox, temporary nodes it creates can persist in the game after the call returns. Game-channel prerequisites are covered by the running-games skill.

## The code_execute meta tool

code_execute wraps your source into a `@tool` extends Node script, runs it on a temporary editor node, then cleans the node up.

- Single-function mode (default): your code is indented into an implicit entry function, so top-level func definitions are not supported. Write straight-line code ending in return:

```gdscript
var total = 0
for child in SceneRoot.get_children():
    total += 1
return total
```

- Multi-function mode: when the source contains func definitions they are preserved; pick the entry point with `function_name`:

```gdscript
func build_report():
    var lines = []
    for child in SceneRoot.get_children():
        lines.append(child.name)
    return lines
```

Pass `function_name` set to build_report to select it.

- Indentation (tabs or spaces) is auto-detected per source; mixing both aborts with an error.
- `timeout_ms` defaults to 5000 and is clamped to 30000. The check runs once before execution: synchronous GDScript cannot be interrupted, so an infinite loop still blocks the editor until it returns.
- A source calling `close_scene(` is rejected outright - it would destroy the node executing the code and crash the editor. Use `close_editor_scene` to close scenes.
- `SceneRoot` is injected as the edited scene root; the wrapper node itself lives under /root, so scene nodes are reachable only through SceneRoot.get_node(...).
- Node cleanup: with `auto_owner`=true, nodes created during execution get their owner assigned recursively and survive (reported in auto_owner_set); with false, newly created ownerless children are deleted after the run.
- Results: the return value is serialized into result, alongside execution_time_ms, output (capped at 8192 bytes) and wrapped_source; compile errors arrive with line-mapped messages and the wrapped source. When the result is a Resource it is auto-registered and returned through registered_resource.

## Bulk data through code_execute

Anything touching many objects - filling tile cells, rewriting hundreds of properties, computing over large arrays - belongs in one code_execute loop instead of hundreds of individual tool calls. It avoids oversized JSON payloads (responses cap at 4 MiB) and runs far faster.

## C# limitations

- `build_csharp_assembly` only triggers a `dotnet build` of the .csproj or .sln found at res:// (asynchronous; the response reports started and a pid). There is no public GDExtension API to hot-reload a .NET assembly inside the editor, so after changing C# class signatures someone must click Build in the editor or restart it. The GDScript channels are unaffected.
- C# properties without a getter are invisible to GDScript: reading one returns null, which is easy to misread as a missing property.

## Hot reload in the running game

`reload_game_scripts` reloads GDScript files inside the running game without restarting it. Use it after editing scripts mid-session, then re-run your verification calls.

## Choosing a channel

- Editor-side inspection or one-shot automation: `execute_script`.
- One method on one node with real instance state: `call_script_node` (`@tool` required).
- Anything inside the running game: `execute_game_script`.
- Loops, bulk edits, computed results, anything wanting a timeout guard: `code_execute`.

## See also

- `godot-autopilot-running-games` - the game channel, input injection and status
- `godot-autopilot-debugging` - log paths and inline GDScript tests
- `godot-autopilot-properties-signals` - property JSON shapes and undo semantics
- `godot-autopilot-usage` - discovery protocol and the error watermark
)gda_skill";

} // namespace

std::vector<SkillSpec> make_resource_skills() {
  return {
      {"godot-autopilot-resources-files", kResourcesFilesDescription,
       {{"SKILL.md", kResourcesFilesBody},
        {"references/TOOL_REFERENCE.md", kResourcesToolReferenceBody}}},
      {"godot-autopilot-scripting", kScriptingDescription,
       {{"SKILL.md", kScriptingBody}}},
  };
}

} // namespace godot_autopilot::skill_gen
