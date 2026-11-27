# Content

## 前置检查

```bash
gda tool search "tilemap"
gda tool describe <tool>
gda resource inspect --path <path>
```

Content work means TileMap, animation, audio and UI theming. Discover the exact
tools for the engine feature first, then inspect the existing resource before
editing it; do not rely on a remembered tool name.

## 操作：发现并调用内容工具

```bash
gda tool search "animation"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

Search by the concrete object ("tile set", "animation track", "sprite frames",
"theme stylebox"), describe the chosen tool to read its required action and
argument schema, then call it. Short arguments go inline; long JSON goes in the
file flag.

```json
{"node_path": "Level", "path": "res://tiles.tres"}
```

## 操作：保存与截取

```bash
gda resource save --path <path> --dest <dir>
gda game capture --target game --save --dir <dir>
```

Save edited resources to an explicit destination and capture the running game
to see the visual result. Overwriting an existing target needs explicit
confirmation.

## 观察

```bash
gda resource inspect --path <path>
gda game capture --target game --save --dir <dir>
```

Read the resource back and look at the capture rather than assuming the edit
rendered correctly.

## 验证

Confirm the resource contains the expected tiles, tracks or theme values, and
that the game capture shows them.

## 失败分支

- Tiles or animations look wrong: inspect the TileSet, tracks or SpriteFrames
  before editing again.
- Tool not found: re-run discovery with a synonym for the object.
- Overwrite refused: review the destination and confirm explicitly.
