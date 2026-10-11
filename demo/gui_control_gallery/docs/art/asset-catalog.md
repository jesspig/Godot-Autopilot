# 资源清单

本文是 `demo/gui_control_gallery/` 内真实文件的**完整清单**。只列磁盘上存在的文件；数量即现状，无虚构。

| 路径 | 说明 |
|---|---|
| `demo/gui_control_gallery/control_gallery.tscn` | 主场景（唯一场景，634 行，根 `ControlGallery`） |
| `demo/gui_control_gallery/tree.gd` | 唯一脚本（`@tool`，`extends Tree`，填充 Tree 节点） |
| `demo/gui_control_gallery/tree.gd.uid` | 脚本 UID（`uid://cidbihqfdp2ne`） |
| `demo/gui_control_gallery/project.godot` | 工程配置（工作树最小模板：名称/特性 `4.7`/图标） |
| `demo/gui_control_gallery/README.md` | 项目说明原文（三面板、拖拽、GTK 灵感） |
| `demo/gui_control_gallery/icon.webp` | 工程图标；场景内复用于 TextureProgressBar 与 Tree 按钮 |
| `demo/gui_control_gallery/icon.webp.import` | 图标导入配置 |
| `demo/gui_control_gallery/screenshots/control_gallery.webp` | 官方展示截图（视觉验收对照基准） |
| `demo/gui_control_gallery/screenshots/.gdignore` | 截图目录忽略标记（内容未核实） |

## 资源映射

- 进度纹理：`TextureProgressBar.texture_progress` → `res://icon.webp`（场景外部资源 `1_8tycj`）。
- Tree 按钮图标：`tree.gd` 内 `preload("res://icon.webp")` → 缩放 16×16 → 根项两按钮。
- 脚本挂载：`Tree.script` → `res://tree.gd`（外部资源 `2_68sc3`）。
- 项图标：`ItemList` 禁用项与 Item 3 的图标 → `res://icon.webp`。

## 引用注意事项

- 场景与脚本均使用 `uid://` 引用（场景 `uid://dqguuy0aao4cr`、图标 `uid://t1yntfol7yga`、脚本 `uid://cidbihqfdp2ne`）；改名/移动须经编辑器保持 UID 有效，勿手改 `.uid` 文件。
- `screenshots/` 下仅上述两文件；验收截图应另存他处，不得写入本目录（受 `.gdignore` 影响，行为未核实）。
