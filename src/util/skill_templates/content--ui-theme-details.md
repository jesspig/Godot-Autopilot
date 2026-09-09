# UI Theme Details

Reference for the UI theming workflow in the content skill: theme file
lifecycle payloads, the full preset enums, what the .tscn actually
persists, and the theme lookup chain verified against engine source.

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
  directories are created and the editor file system is refreshed after
  saving.
- An existing file is never overwritten - pick a new path or edit the file
  with the set tools below.
- `base_type` is stored as reference metadata only; Godot themes have no
  global base type, so real items are added with the set tools.

### Set theme items

Every set tool takes `path` (the saved .tres file), `theme_type` (the
widget class the item belongs to, e.g. Button or Label) and `varname` (the
item name, e.g. font_color), then loads the file, modifies it and saves it
back.

```json
{"name": "set_theme_color", "arguments": {"path": "res://themes/main.tres", "theme_type": "Button", "varname": "font_color", "color": "#e0e0e0"}}
```

- `color` accepts a hex string (#rrggbb or #rrggbbaa) or an object with
  r/g/b/a floats in 0..1.
- `set_theme_constant` takes an integer `constant` (e.g. separation or
  outline_size).
- `set_theme_font_size` takes an integer `font_size`.
- `set_theme_stylebox_flat` builds a StyleBoxFlat from the `stylebox`
  object: optional bg_color and border_color (hex string or rgba object),
  border_width {left, top, right, bottom}, corner_radius (four corners) and
  content_margin (four sides); border_width and content_margin also accept
  a single number applied to all sides. Every field is optional.

Not sure which type and item pairs are valid? Run `get_theme_info` on the
file before the set tools - it lists every theme type with its color,
constant, font size and stylebox item names.

### Apply to a control

```json
{"name": "apply_theme_to_control", "arguments": {"theme_path": "res://themes/main.tres", "control_path": "UI/Panel"}}
```

Only the in-memory scene changes - call `save_editor_scene` afterwards to
persist the assignment.

## Theme lookup order

How a Control resolves a theme item for its type, verified against the
engine's theme owner:

1. Local overrides first: a value set with the control's
   theme_color_override-style methods wins.
2. Then the ancestor chain, node by node: at each ancestor with a Theme,
   the ENTIRE type dependency chain is walked inside that one Theme (the
   requested type, its variation, then base types) before moving on to the
   next ancestor.
3. After the ancestor chain: the global project theme, then the engine
   default.

Two consequences that surprise people:

- A near ancestor's generic-type item BEATS a far ancestor's exact type
  variation, because each ancestor is fully exhausted - across its whole
  type chain - before the next one is consulted.
- Type variation chains only resolve WITHIN the same Theme - a variation's
  base type is not followed into a different Theme further up the tree.

## Anchor presets: the full enums

`set_control_anchor_preset` applies one of the 16 Control layout presets to
a node in the edited scene:

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

The engine's preset machinery also has a preset MODE with 4 values, used by
the preset APIs to decide how offsets are derived: PRESET_MODE_MINSIZE (0,
size to minimum size), PRESET_MODE_KEEP_WIDTH (1), PRESET_MODE_KEEP_HEIGHT
(2) and PRESET_MODE_KEEP_SIZE (3).

```json
{"name": "set_control_anchor_preset", "arguments": {"control_path": "UI/Panel", "preset": 15, "keep_offsets": false}}
```

`keep_offsets` (default false) recomputes the node's offsets for the new
anchors; set it true to keep the current offsets instead. As with theme
application, only the in-memory scene changes - call `save_editor_scene` to
persist.

### The two preset APIs disagree on keep_offsets

- `Control.set_anchors_preset(preset, keep_offsets)` called on its own
  defaults `keep_offsets` to TRUE - anchors change, offsets stay, and the
  rect can end up somewhere unexpected.
- `Control.set_anchors_and_offsets_preset(...)` calls it internally with
  keep_offsets forced to FALSE and then recomputes the offsets for the new
  anchors.

The autopilot tool's `keep_offsets` parameter defaults to false, matching
the recomputing behavior. When porting editor-recorded GDScript, check
which API the code calls before assuming either default.

## What the .tscn actually stores

For anchored Controls:

- `position` and `size` carry an editor-only usage bit and are NOT
  serialized for anchored controls - a .tscn diff will not show them.
- The persisted truth is the `anchor_*`, `offset_*` and `grow_*`
  properties; those fully determine the laid-out rect.
- `layout_mode` and `anchors_preset` are internal pseudo-properties (editor
  bookkeeping), not real layout inputs - do not script against them.

Setting `position` or `size` via `property_set` on an anchored control is
overridden by the anchor math on the next layout pass; set anchors and
offsets instead.

## Runtime UI reconnaissance

While the game runs, `get_game_ui_elements` walks the running Control tree
and returns one entry per Control: node path, type, visibility, optional
text and its global rectangle (position and size).

```json
{"name": "get_game_ui_elements", "arguments": {"max_elements": 200}}
```

- `max_elements` defaults to 100, hard cap 1000; when the cap is hit the
  response is flagged as truncated - raise the cap or narrow the search.
- `timeout_ms` (default 5000, max 30000) bounds the round trip to the
  game.

Use the returned rectangles to aim input injection - for example pressing a
button by its on-screen coordinates with `queue_game_input`. This requires
the game to be running via `play_editor_current_scene`; the game channel
prerequisites and input injection details are covered in the runtime skill.
Editor-side input tools do not reach the running game - only the game
channel tools do.

## Side effects

Five of the eight theme tools write to disk (`create_theme_resource`,
`set_theme_color`, `set_theme_constant`, `set_theme_font_size`,
`set_theme_stylebox_flat`): each loads the .tres file, modifies it and
saves it back. Confirm the `path` before calling - especially on shared
theme files. `create_theme_resource` refuses to overwrite an existing file,
but the set tools edit in place. `get_theme_info` is read-only;
`apply_theme_to_control` and `set_control_anchor_preset` only modify the
in-memory edited scene (persist with `save_editor_scene`).

## See also

- The [content skill](../SKILL.md) overview for the condensed theming
  workflow.
- [Animation details](animation-details.md) - animating UI controls such as
  modulate or position.
- godot-autopilot-runtime - starting the game and injecting input at the UI
  you just inspected.
- godot-autopilot-scene-system - property JSON shapes and renames for
  Control properties.
