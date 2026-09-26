# 资源清单（磁盘逐项登记）

本文是 `demo/viewport_gui_in_3d/` 内文件的**唯一权威清单**。下表每一行都对应磁盘上真实存在的文件；引用路径时不得超出此表。

| 路径（res:// 相对） | 种类 | 说明 |
|---|---|---|
| `gui_in_3d.tscn` | 3D 主场景 | 根 `GUIin3D`；实例化面板；相机 + 漫游动画 + 灯光 + 天空 + 布景 |
| `gui_panel_3d.tscn` | 面板场景 | SubViewport + 2D 控件 + Quad + Area3D；挂载 `gui_3d.gd` |
| `gui_3d.gd` | GDScript | 输入转发脚本（`extends Node3D`，138 行）；机制见 [../gdd/mechanics.md](../gdd/mechanics.md) |
| `gui_3d.gd.uid` | UID 映射 | 内容 `uid://cgjafujbxj4i`，与面板场景中脚本引用一致 |
| `icon.webp` | 纹理/图标 | 工程图标（`project.godot` 指向）兼面板 `TextureRect` 纹理 |
| `icon.webp.import` | 导入元数据 | 编辑器生成；内容未核实 |
| `project.godot` | 工程配置 | **工作树现状为最小模板**（仅名称/特性/图标）；完整配置见源库原件，差异见 [../tech/tdd.md](../tech/tdd.md) |
| `README.md` | 官方说明 | 英文简介 + Asset Store 链接 + 截图引用 |
| `screenshots/gui.png` | 官方截图 | 视觉基线；验收对照用 |
| `screenshots/.gdignore` | 导入屏蔽 | 存在性已确认；内容未核实 |

## 数量口径

- 自研脚本：1 个（`gui_3d.gd`，138 行，含空行与注释）。
- 场景：2 个；外部纹理：1 个（`icon.webp`）；官方截图：1 张。
- 本演示**无音频、无外部模型、无自定义主题、无存档文件**；若磁盘出现此表之外的资源文件，以磁盘为准并更新此表，不得直接引用。
