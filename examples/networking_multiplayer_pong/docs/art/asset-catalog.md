# 美术资源清单

本文是美术资产的**唯一权威**。以下每个文件均在磁盘上真实存在；像素尺寸未用图片工具核实，凡未核实处如实标注。

## Gameplay PNG（3 个）

| 文件 | 用途 | 被谁引用 |
|---|---|---|
| `ball.png` | 球贴图（`ball.tscn` → `Sprite2D`） | `ball.tscn:4`（ext_resource） |
| `paddle.png` | 球拍贴图（`paddle.tscn` → `Sprite2D`，双拍共用，靠 `modulate` 区分颜色） | `paddle.tscn:4`（ext_resource） |
| `separator.png` | 中线贴图（`pong.tscn` → `Separator`） | `pong.tscn:4`（ext_resource） |

- 三图各带一个同名 `.import` 文件（`ball.png.import`、`paddle.png.import`、`separator.png.import`），为编辑器导入配置，测试时不直接引用。
- 像素尺寸：未核实（撰写时未运行 Godot/图片工具；以编辑器检查器显示为准）。

## 图标与截图

| 文件 | 用途 |
|---|---|
| `icon.webp` | 工程图标（`project.godot` 的 `config/icon`；源库原件，工作树模板亦保留该行） |
| `screenshots/pong_multiplayer.png` | 官方 README 引用的对战截图 |
| `screenshots/.gdignore` | 使截图目录不被编辑器导入（行为按文件名推断，未核实导入语义细节） |

## 完整引用关系

- `pong.tscn` 引用 `separator.png` + `paddle.tscn`（×2：`Player1`/`Player2`）+ `ball.tscn`（×1：`Ball`）。
- `ball.tscn` 引用 `ball.png` + `logic/ball.gd`；`paddle.tscn` 引用 `paddle.png` + `logic/paddle.gd`。
- `lobby.tscn` 无图片资源（纯 Control + `logic/lobby.gd`）。

## 资源总数

- gameplay PNG：**3**；图标 webp：1；截图 png：1。无音频、无字体、无动画表。
