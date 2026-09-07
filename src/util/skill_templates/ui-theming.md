# UI Theming and Layout

UI theming and layout via godot-autopilot: create .tres theme files, set
color/constant/font-size/stylebox entries, apply them to Control nodes, apply
anchor presets, and enumerate the running game's Control tree.

## Theme file lifecycle

1. `create_theme_resource` - create the .tres file on disk.
2. `set_theme_color` / `set_theme_constant` / `set_theme_font_size` /
   `set_theme_stylebox_flat` - add theme items.
3. `apply_theme_to_control` - assign the theme to a Control node.
4. `get_theme_info` - inspect the saved file.

```json
{"name": "create_theme_resource", "arguments": {"path": "res://themes/main.tres", "base_type": "Control"}}
```

- `path` must be inside res:// (a .tres path; user:// is rejected). Missing
  directories are created and the editor file system is refreshed after saving.
- An existing file is never overwritten - pick a new path or edit the file with
  the set tools below.
- `base_type` is stored as reference metadata only; Godot themes have no global
  base type, so real items are added with the set tools.

### Set theme items

Every set tool takes `path` (the saved .tres file), `theme_type` (the widget
class the item belongs to, e.g. Button or Label) and `varname` (the item name,
e.g. font_color), then loads the file, modifies it and saves it back.

```json
{"name": "set_theme_color", "arguments": {"path": "res://themes/main.tres", "theme_type": "Button", "varname": "font_color", "color": "#e0e0e0"}}
```

- `color` accepts a hex string (#rrggbb or #rrggbbaa) or an object with
  r/g/b/a floats in 0..1.
- `set_theme_constant` takes an integer `constant` (e.g. separation or
  outline_size).
- `set_theme_font_size` takes an integer `font_size`.
- `set_theme_stylebox_flat` builds a StyleBoxFlat from the `stylebox` object:
  optional bg_color and border_color (hex string or rgba object), border_width
  {left, top, right, bottom}, corner_radius (four corners) and content_margin
  (four sides); border_width and content_margin also accept a single number
  applied to all sides. Every field is optional.

Not sure which type and item pairs are valid? Run `get_theme_info` on the file
before the set tools - it lists every theme type with its color, constant,
font size and stylebox item names.

### Apply to a control

```json
{"name": "apply_theme_to_control", "arguments": {"theme_path": "res://themes/main.tres", "control_path": "UI/Panel"}}
```

Only the in-memory scene changes - call `save_editor_scene` afterwards to
persist the assignment.

## Anchor presets

`set_control_anchor_preset` applies one of the 16 Control layout presets to a
node in the edited scene:

| Preset | Value | Preset | Value |
|---|---|---|---|
| top_left | 0 | left_wide | 9 |
| top_right | 1 | top_wide | 10 |
| bottom_left | 2 | right_wide | 11 |
| bottom_right | 3 | bottom_wide | 12 |
| center_left | 4 | vcenter_wide | 13 |
| center_top | 5 | hcenter_wide | 14 |
| center_right | 6 | full_rect | 15 |
| center_bottom | 7 | center | 8 |

```json
{"name": "set_control_anchor_preset", "arguments": {"control_path": "UI/Panel", "preset": 15, "keep_offsets": false}}
```

`keep_offsets` (default false) recomputes the node's offsets for the new
anchors; set it true to keep the current offsets instead. As with theme
application, only the in-memory scene changes - call `save_editor_scene` to
persist.

## Runtime UI reconnaissance

While the game runs, `get_game_ui_elements` walks the running Control tree and
returns one entry per Control: node path, type, visibility, optional text and
its global rectangle (position and size).

```json
{"name": "get_game_ui_elements", "arguments": {"max_elements": 200}}
```

- `max_elements` defaults to 100, hard cap 1000; when the cap is hit the
  response is flagged as truncated - raise the cap or narrow the search.
- `timeout_ms` (default 5000, max 30000) bounds the round trip to the game.

Use the returned rectangles to aim input injection - for example pressing a
button by its on-screen coordinates with `queue_game_input`. This requires the
game to be running via `play_editor_current_scene`; the game channel
prerequisites and input injection details are covered in the running-games
skill. Editor-side input tools do not reach the running game - only the game
channel tools do.

## Side effects

Five of the eight theme tools write to disk (`create_theme_resource`,
`set_theme_color`, `set_theme_constant`, `set_theme_font_size`,
`set_theme_stylebox_flat`): each loads the .tres file, modifies it and saves
it back. Confirm the `path` before calling - especially on shared theme files.
`create_theme_resource` refuses to overwrite an existing file, but the set
tools edit in place. `get_theme_info` is read-only; `apply_theme_to_control`
and `set_control_anchor_preset` only modify the in-memory edited scene
(persist with `save_editor_scene`).

## See also

- godot-autopilot-running-games - starting the game and injecting input at the
  UI you just inspected.
- godot-autopilot-animation - animating UI controls such as modulate or
  position.
- godot-autopilot-properties-signals - property JSON shapes and renames for
  Control properties.
- godot-autopilot-tilemap - the other content-domain skill in this family.
