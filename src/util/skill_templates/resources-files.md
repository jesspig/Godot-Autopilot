# Resource and File Operations

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
