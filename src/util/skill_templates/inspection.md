# Inspecting Godot Projects with godot-autopilot

Read-only queries for the edited scene, the running game, the engine and the
project layout — check what exists and what state something is in before you
modify it.

## Editor-side inspection

- `get_scene_tree` walks the currently edited scene, returning `name`,
  `type`, `path` and `children` for each node. Optional `max_depth` (default
  8, `-1` for unlimited) and `include_properties` (true adds up to 20
  properties per node, skipping `metadata/` keys, names starting with an
  underscore and Object-typed properties). Errors when no scene is open —
  create or open one first.
- `get_editor_edited_scene_root` returns the edited scene root's `name`,
  `type` and `path`; null when no scene is open. Start scene workflows here
  and combine with `get_scene_tree` for the full hierarchy.
- `property_get_list` lists every property of a node with its metadata
  (`name`, `type`, `hint`, `hint_string`, usage flags, `class_name`) —
  discover valid property names and enum ordering before `property_get` or
  `property_set`.
- `get_editor_selection` reports which nodes the user has selected (each
  with `name`, `class` and `path`; an empty array means nothing is
  selected). Use `set_editor_selection` to focus subsequent operations on
  specific nodes.
- `get_editor_file_system_tree` fetches the project file tree, optionally
  rooted at the given path; directory entries carry children, file entries
  carry `name`, `path` and `type`. Trees deeper than 12 levels are truncated
  and the response carries a `max_depth` field. After creating or modifying
  files outside the editor, call `scan_editor_file_system`, then poll
  `get_editor_file_system_status` until `scanning` is false.

## Running-game inspection

- `get_debugger_scene_tree` — the running game's scene tree (node names and
  types) as a formatted text tree. With an active debug session it reads the
  game over the runtime channel; without one it returns the last
  editor-captured tree. Takes no parameters.
- `get_game_ui_elements` — every Control-derived node in the running game
  with `path`, `type`, `visible`, `text` and `global_rect`; `max_elements`
  defaults to 100 (max 1000) and `truncated` signals more remain. See
  `godot-autopilot-running-games` for driving these elements.
- `get_game_status` — engine version, current scene, node count, FPS,
  `paused`, `physics_frame`, plus the `healthy` and `physics_stalled`
  liveness fields.

## Engine and system facts

All read-only:

- `get_engine_version` — `major`, `minor` and `patch` integers plus
  `string`, the full version label; use it to branch behavior on engine
  capabilities.
- `get_engine_fps` — the live measured frames per second;
  `get_engine_frames_drawn` — a monotonic total-frames counter (stall
  detection); `get_engine_time_scale` — the current global time scale.
- OS facts: `get_os_system_info`, `get_os_datetime`, `get_os_environment`,
  `get_os_locale`, `get_os_system_fonts`, `get_os_unique_id`,
  `get_os_unix_time`, `get_os_user_data_dir`.
- Display facts: `get_display_screen_count`, `get_display_screen_size`,
  `get_display_screen_dpi`, `get_display_screen_refresh_rate`,
  `get_display_screen_position`, `get_display_mouse_position`,
  `get_display_clipboard`, `get_display_tts_voices`.
- Project and editor settings: `get_project_settings` and
  `has_project_settings`; `get_editor_settings` and `has_editor_settings`.

## Query the Godot API by reflection, not from memory

Before calling an engine method or setting a property from memory, resolve
the real signature with the ClassDB reflection tools (signatures only — no
docstrings):

1. `find_docs_class` — case-insensitive substring search over all registered
   classes, up to 50 matches with `name`, parent and `api_type`. Confirm the
   exact class name or discover related classes first.
2. `get_docs_class` — the full reflected signature of one class:
   `parent_class`, `api_type`, `can_instantiate`, `methods`, `properties`,
   `signals`, `enums` and `constants`. Errors when the class does not exist.
3. `get_docs_method` — one method's signature (`name`, arguments, return
   type, flags); errors when the class or method does not exist.
4. `get_docs_property` — one property's info (`name`, type, hint, usage,
   `class_name`); errors when the class or property does not exist.

```json
{"name": "find_docs_class", "arguments": {"query": "AnimationPlayer"}}
{"name": "get_docs_class", "arguments": {"class": "AnimationPlayer"}}
```

This is cheaper and more reliable than guessing: an invented method or
property name only produces a confusing runtime error later.

## Project health checks

- `validate_scene_file` — dry-run validation of a `.tscn`/`.scn` file on
  disk without touching the currently edited scene: loads it bypassing the
  resource cache, checks every dependency for existence on disk, then
  instantiates and immediately frees the scene. Returns `valid`, `problems`
  (each `kind` is `load_failed`, `missing_dependency` or
  `instantiate_failed`), `missing_dependencies` and `dependency_count`. Use
  it after bulk edits or before committing scene changes.

```json
{"name": "validate_scene_file", "arguments": {"path": "res://levels/level_01.tscn"}}
```

- `find_unused_resources` — project files never referenced by any other
  file's dependencies under `directory` (default `res://`). Exempts
  `icon.svg`, the main scene and autoload paths read from project settings,
  `*.import` sidecars and project.godot; unresolvable `uid://` entries are
  listed in `unresolved_uids` instead of being misreported as unused.
  Read-only; large projects may take a few seconds.
- `trace_signal_flow` — signal wiring around a node of the edited scene:
  optional `direction` (`outgoing`, `incoming` or `both`, default both) and
  `max_depth` (default 3, cycles cut by a visited set). Each edge carries
  `from`, `signal`, `to`, `method` and `persisted` (true when the connection
  is saved in the scene file); non-node targets such as autoloads are shown
  as `Class#instance_id`.

## Screenshots and object lookup

- `capture_editor_viewport` — default `target` `editor` grabs the editor 2D
  viewport (falling back to the 3D viewport) and returns a base64 PNG with
  `data`, `format`, `width` and `height` — visually verify the scene while
  editing, e.g. after placing nodes or changing properties.
- `get_debug_object_info` — resolve an object instance ID to its identity:
  `class`, `name` and `node_path` for Nodes, `resource_path` for Resources.
  Use it on object IDs obtained from scene or physics tools; it errors on
  stale or invalid ids.

## See also

- `godot-autopilot-running-games` — launching the game and reading its UI.
- `godot-autopilot-debugging` — logs, monitors and tests once something
  looks wrong.
- `godot-autopilot-tool-map` — task-to-tool routing across all categories.
- `godot-autopilot-resources-files` — resource and file introspection
  details.
