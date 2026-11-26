# Content

## 前置检查

```bash
gda tool search "tilemap"
gda tool describe <tool>
gda resource inspect --path <path>
```

内容工作指 TileMap、动画、音频与 UI 主题。先发现该引擎特性对应的确切工具，编辑前检查现有资源；不要依赖记忆中的工具名。

## 操作：发现并调用内容工具

```bash
gda tool search "animation"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

按具体对象搜索（“tile set”“animation track”“sprite frames”“theme stylebox”），描述所选工具以读取必填 action 与参数 schema，然后调用。短参数用行内 flag，长 JSON 用文件 flag。

```json
{"node_path": "Level", "path": "res://tiles.tres"}
```

## 操作：保存与截取

```bash
gda resource save --path <path> --dest <dir>
gda game capture --target game --save --dir <dir>
```

把编辑过的资源保存到显式目标，并截取运行中的游戏查看视觉效果。覆盖已有目标需要显式确认。

## 观察

```bash
gda resource inspect --path <path>
gda game capture --target game --save --dir <dir>
```

读回资源并查看截图，不要假定编辑已正确渲染。

## 验证

确认资源包含预期的瓦片、轨道或主题值，且游戏截图显示了它们。

## 失败分支

- 瓦片或动画显示不对：再次编辑前检查 TileSet、轨道或 SpriteFrames。
- 工具找不到：换对象的同义词重跑发现。
- 覆盖被拒：审查目标后显式确认。
