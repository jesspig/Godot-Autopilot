# 美术资源清单

本文是资产文件的**唯一权威**（逐文件枚举）。条目数量以目录枚举为准（2026-09-26 查看时）。

## art/（13 个条目：10 资产 + 3 导入文件）

| 文件 | 说明 |
|---|---|
| `art/player.glb` | 玩家角色模型，由 `Player.tscn` 实例化（`Pivot/Character`） |
| `art/player.blend` | 玩家 Blender 源文件；与 `player.glb` 同目录共存，导出关系未核实 |
| `art/mob.glb` | 小怪模型，由 `Mob.tscn` 实例化（`Pivot/Character`） |
| `art/mob.blend` | 小怪 Blender 源文件；与 `mob.glb` 同目录共存，导出关系未核实 |
| `art/body.tres` | 玩家体材质（橙色，见 art-direction） |
| `art/eye.tres` | 玩家眼部材质（以文件为准） |
| `art/pupil.tres` | 瞳孔材质（以文件为准） |
| `art/mob_body.tres` | 小怪体材质（深蓝紫） |
| `art/mob_eye.tres` | 小怪眼部材质（以文件为准） |
| `art/House In a Forest Loop.ogg` | 背景音乐，由 `MusicPlayer.tscn` 引用播放 |
| `art/mob.glb.import` | `mob.glb` 的导入文件 |
| `art/player.glb.import` | `player.glb` 的导入文件 |
| `art/House In a Forest Loop.ogg.import` | 音频导入文件 |

## fonts/（3 个条目）

| 文件 | 说明 |
|---|---|
| `fonts/Montserrat-Medium.ttf` | UI 默认字体，由 `ui_theme.tres` 引用 |
| `fonts/Montserrat-Medium.ttf.import` | 字体导入文件 |
| `fonts/LICENSE.txt` | SIL OFL 1.1 许可文本（Montserrat，2011） |

## screenshots/（2 个条目）

| 文件 | 说明 |
|---|---|
| `screenshots/squash_the_creeps.webp` | 官方游戏截图（仓库 `README.md` 引用展示） |
| `screenshots/.gdignore` | 阻止编辑器导入该目录的标记文件 |

## 根目录相关文件

| 文件 | 说明 |
|---|---|
| `icon.webp`（+ `icon.webp.import`） | 工程图标（源库配置 `config/icon="res://icon.webp"`） |
| `ui_theme.tres` | 主题：默认字体指向 `fonts/Montserrat-Medium.ttf`，被 `Main.tscn` 的 `UserInterface` 引用 |
| `README.md` | 官方英文说明（玩法、键位、教程链接、许可） |
| `project.godot` | 工作树内最小模板（仅名称/特性/图标），源库原件见 [../tech/tdd.md](../tech/tdd.md) |

## 引用关系

- `Player.tscn` → `art/player.glb`；`Mob.tscn` → `art/mob.glb`；`MusicPlayer.tscn` → `art/House In a Forest Loop.ogg`；`Main.tscn` → `ui_theme.tres` → `fonts/Montserrat-Medium.ttf`。
- `.blend` 文件无任何场景直接引用（引用关系以 tscn/ext_resource 为准）。
