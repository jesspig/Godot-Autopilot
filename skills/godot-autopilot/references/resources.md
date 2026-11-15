# Resources and Files

Working with `.tres`/`.tscn`, `.import` sidecars, UIDs, and the `res://` layout,
plus the text-file tools. Discover and describe a tool before the first call;
the calling protocol lives in `references/tools.md`.

## Load, create, duplicate, save

- `load_resource` loads a file with optional `type_hint` and returns a locator
  (`class`, `path`, `object_id`, `object_id_str`).
- `create_resource` instantiates a Resource subclass by `type`; `name`
  registers it as `memory://name`.
- `duplicate_resource` duplicates a file-backed resource in memory (`deep` copies
  sub-resources); `name` registers it. Save it to materialize a file. In-memory
  instances must be saved and reloaded before they can be duplicated.
- `save_resource` requires `path`; `dest_path` overrides the destination.
  Locators are `object_id`, a registered `name`, or `path`. A path locator reads
  from disk, so unsaved memory changes are excluded; pass `name` or `object_id`
  to save the live instance. A differing `dest_path` is copy-on-write. Missing
  directories are created, the file system refreshes, and `verified: true` means
  the file loads.
- `reload_resource` force-reloads from disk and replaces the cached instance,
  so later reads and path-located saves see external edits.
- `copy_resource_file` copies bytes to `dest_path`, creating parents and
  overwriting the target. It does not rewrite references or copy
  `.import`/`.uid` sidecars. Use load/save when the instance must be
  re-serialized.
- `memory://` resources must never be assigned to a node property; save them to
  disk first, then assign by path. The rule also applies to TileSet and
  SpriteFrames built by the tilemap and animation tools.

## Introspection and search

- `get_resource_type` reports the class a file holds; `get_resource_types` lists
  instantiable Resource subclasses. `has_resource` asks the loader whether a path
  is loadable, not whether a file exists.
- `get_resource_uid` reads the numeric UID (`-1` when none); `set_resource_uid`
  assigns one and triggers a reimport so it persists. `get_resource_dependencies`
  lists external files; `has_resource_dependency` checks one edge.
  `get_resource_references` is the reverse lookup; pass exactly one of `path`,
  `uid`, or `class_name`.
- `read_file` returns full text (`res://`, `user://`, or absolute).
  `find_in_files` recursively searches text; `query` is required, `dir` defaults
  to `res://`, `extensions` to gd/tscn/tres/cs/md/json/h/cpp, and `max_results`
  to 500. Hitting the limit reports `truncated: true`. This is the standard way
  to prove a rename rewrote every reference.

## Transactional rename and move

`rename_resource_file` (`path`, `new_path`) is single-file only; a directory
fails fast and directs you to `move_resource_file` (`path`, `new_directory`).
Both are engine-aware transactions. For each affected file they:

1. Save open scenes that reference the file, rewrite it on disk, and reload
   those scenes (their undo history resets).
2. Rewrite `path=` and `uid=` tokens in every dependent `.tscn`/`.tres`, move the
   `.uid` sidecar (and `.import` of imported assets), and keep the UID stable.
3. Remap `project.godot` entries such as `application/run/main_scene` and
   `autoload/*`, preserving the leading `*` singleton marker, then notify the
   editor file system.

Read the structured report: `updated_files`, `stale_references`,
`uid_preserved`, `saved_scenes`, `reloaded_scenes`,
`unsupported_binary_references`, `closed_tabs`, and `import_sidecar_warning`.
A `script_class` token means a global class name changed; run
`scan_editor_file_system` so it re-registers.

`move_resource_file` keeps the file name and takes `new_directory` inside
`res://` (absolute or relative; trailing slashes trimmed; `user://` rejected).
A directory moves recursively, preserving the sub-folder layout, leaving empty
source folders, and running a scan afterwards.

Reloading a touched open scene clears its undo history. The transaction saves
first to narrow the window, but unsaved edits in unrelated open scenes are at
risk. A tab still pointing at a moved or force-deleted file is closed
automatically; the currently edited scene stays open and is reported in
`closed_tabs_warning`.

## Deleting safely

`remove_resource_file` is two-stage. The default dry-run scans every
`.tscn`/`.tres` under `res://` for path and uid references and returns
`would_delete` plus `dependents`; nothing is deleted. With `force=true`, the file
is preferentially moved to the OS trash (`trashed: true`; `permanent: true` only
when the trash is unavailable), and the `.uid` sidecar travels with it. To
discard a whole directory, use `move_os_file_to_trash` (OS domain).

## Imports and readiness

- `reimport_resource_files` queues one or more files and is asynchronous; it only
  affects files with a `.import` sidecar. For `.tres`/`.gd` the engine logs a BUG
  error, so refresh those with `reload_resource`. Poll
  `get_editor_file_system_status` until `scanning` is false.
- `set_resource_import_options` writes whitelisted `.import` `[params]` keys
  (audio loop/compression, texture filter/mipmap) and queues a reimport. Hand
  editing `.import` for parameters is deprecated.
- `scan_editor_file_system` rescans after out-of-band changes or a class-name
  rename. A scan request during another scan is silently dropped; use the
  retryable soft error and poll until scanning is false.

While importing or scanning, resource writes fail soft with
`retryable: true` and `retry_after_ms`. Wait that interval, then retry the same
call.

## Path rules

- Resource operations accept `res://` only.
- `write_file` and `read_file` also accept `user://`; `read_file` accepts
  absolute paths. Never splice one namespace into another.
- Paths are normalized and boundary-checked: traversal, absolute escapes,
  unknown schemes, and writes outside the project are rejected.
- `write_file` inside `res://` is engine-managed: imported assets are queued for
  reimport, plain text gets `update_file`, and script-like files return
  diagnostics. `user://` and absolute writes report `engine_managed: false`.
## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| Node loses a resource on save | Assigned a `memory://` resource | Save to disk, then assign by path |
| Rename leaves stale references | Unsupported binary resource or class name | Read `stale_references`; rescan |
| Write fails with `retryable` | Editor is importing or scanning | Wait `retry_after_ms`, retry |
| Undo lost after rename | Referencing scenes were reloaded | Save all scenes before the transaction |
