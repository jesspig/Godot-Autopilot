# Scene File Format Tokens (.tscn)

The text scene format the editor writes and reads: section structure, header
tokens, ext_resource id handling, uid resolution and the rules that decide
what a save silently drops. Knowing these makes hand-written or tool-generated
`.tscn` edits predictable.

## Section structure

A `.tscn` is a sequence of bracketed section headers, each followed by
`key = value` property lines:

```ini
[gd_scene format=3 uid="uid://b4q3kq2vxtmw"]

[ext_resource type="Texture2D" path="res://icon.svg" id="1_abcde"]

[sub_resource type="RectangleShape2D" id="RectangleShape2D_x7k2p"]

[node name="Player" type="CharacterBody2D"]

[node name="Sprite" type="Sprite2D" parent="."]
texture = ExtResource("1_abcde")

[connection signal="pressed" from="UI/StartButton" to="Game" method="_on_start"]

[editable path="Instances/Enemy"]
```

- `.tres` files use the same format with a `[gd_resource type="Resource" ...]`
  header instead of `[gd_scene]` and a final `[resource]` section holding the
  main resource's properties.
- `[ext_resource]` — an external dependency (texture, script, another scene),
  loaded before the rest of the file.
- `[sub_resource]` — a resource embedded in the file (materials, shapes),
  referenced as `SubResource("<id>")`.
- `[node]` — one node; order of sections is the parent-before-child order of
  the tree, and the first `[node]` is the scene root.
- `[connection]` — one persisted signal connection.
- `[editable]` — marks an instanced child scene as editable in place.
- `load_steps=N` appears in files saved by older engines; recent savers omit
  it (4.8).

## ext_resource ids and their persistent cache

- The id is `"<index>_<random suffix>"` (e.g. `1_abcde`): an increasing
  number kept in resource order for better thread-loading performance, plus a
  random string. `sub_resource` ids are `"<Class>_<random>"`.
- The editor remembers the id assigned to each resource path per file after
  loading and reuses it on the next save. Practical effect: adding and
  removing nodes does not reshuffle unrelated ext ids, so scene files stay
  diff-friendly.
- A property line referencing an external resource reads
  `texture = ExtResource("1_abcde")` — the id string, not the index.

## uid wins over path

An `[ext_resource]` may carry a `uid` field. At load time:

- If the uid is valid and known to the editor's UID cache, the uid's resolved
  path replaces whatever `path` the file line contains. The textual path is
  then only a fallback.
- If the uid is invalid or unknown, loading falls back to the literal `path`
  and prints a warning ("invalid UID - using text path instead").

Consequences for rename/move: moving or renaming a referenced file through
the editor keeps its uid, so scenes referencing it keep loading even before
their `path` lines are rewritten, and the cached (current) path is written
back on the next save. Conversely, hand-editing a `uid` value, or losing the
editor's UID cache (`.godot/uid_cache.bin`), makes loading fall back to the
path — a stale path then fails as a missing resource with a warning.

## uid:// token format

- The uid text form is `uid://` followed by base-34 digits from the
  characters `a`–`y` and `0`–`8`, at most 13 characters. The characters `z`
  and `9` never occur (an off-by-one in the digit tables, kept for
  compatibility, 4.7+).
- When validating or generating uid references, treat `z`/`9` as invalid
  characters rather than assuming full lowercase alphanumerics.

## node header tokens

All tokens a `[node]` header may carry (besides `name`, everything is
conditional):

| Token | Meaning |
|---|---|
| `name` | node name; always present |
| `type` | node class; omitted for instanced-scene children whose type comes from the instance |
| `parent` | NodePath to the parent, relative; the root has no `parent` |
| `owner` | owner NodePath when it differs from the scene root |
| `index` | explicit sibling order, when one was recorded |
| `groups` | persistent group list |
| `instance` | embedded PackedScene snapshot for instanced child scenes |
| `instance_placeholder` | editable-children placeholder form of `instance` |
| `unique_id` | scene-unique numeric id used by id-based paths |
| `node_paths` | properties whose NodePath values are stored as node references |
| `parent_id_path` / `owner_uid_path` | id-sequence equivalents of `parent`/`owner` used by newer files |

`[connection]` headers carry `signal`, `from`, `to`, `method`, plus optional
`flags` (omitted for the default CONNECT_PERSIST), `unbinds`, `binds`, and
`from_uid_path`/`to_uid_path` id-sequence equivalents.

## format=3 vs format=4

- The header carries `format=3` or `format=4`. The loader accepts both
  silently and rejects unknown higher versions.
- The saver chooses one per resource — format=3 is the compatibility
  encoding, format=4 the current one — and switches without any warning, so
  re-saving a scene can change its format number in the diff. Neither
  direction indicates a problem with your edit.

## Binary .scn/.res are whole-file round trips

The binary format stores sub-resources behind `local://` internal paths that
only make sense inside the file (they expand to `file.ext::<id>` built-in
paths at load). There is no readable token structure to patch: reading,
modifying or diffing a binary scene means loading it in the engine and saving
it back as a whole. godot-autopilot's file-level tools handle binary files
only via full load/instantiate/save round trips (e.g. `validate_scene_file`
checks them by loading, never by parsing text).

## What a save silently drops

Saving a scene packs it first, and packing drops content without warnings:

- Nodes whose owner is not the saved scene root (and are not editable-instance
  overrides) — a node created with `owner` unset or pointing elsewhere is
  simply not in the file.
- Signal connections without the CONNECT_PERSIST flag.
- Properties equal to the node's computed default value, unless the property
  is pinned. Exception: arrays and dictionaries that contain embedded
  sub-resources are kept even at default value.
- Nodes of an instanced sub-scene that have no local property or group
  changes.
- At write time, properties without the STORAGE usage flag are never
  serialized at all, and main-resource properties equal to their default are
  omitted line-by-line ("missing line = matches the default").

## See also

- references/property-json-shapes.md — default-value omission from the property side
- references/undo-history.md — what saving and reloading do to undo histories
- godot-autopilot-resources — uid management, rename/move and dependency rewriting
