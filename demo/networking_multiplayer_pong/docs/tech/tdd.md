# 技术设计文档（TDD）

本文记录工程配置、场景/脚本映射与 RPC 表；规则数值以 [../gdd/mechanics.md](../gdd/mechanics.md) 为准。

## 配置现状

权威配置来自源库原件（`godot-demo-projects/networking/multiplayer_pong/project.godot`）：

| 项 | 值 |
|---|---|
| 应用名 | `Pong Multiplayer` |
| 主场景 | `res://lobby.tscn` |
| 特性 | `PackedStringArray("4.7")` |
| 图标 | `res://icon.webp` |
| 视口 | 640×400 |
| 拉伸 | `canvas_items` + `integer` |
| 纹理过滤 | `textures/canvas_textures/default_texture_filter=0`（最近邻） |
| 渲染 | `gl_compatibility`（含 mobile） |
| 输入 | `move_up` / `move_down`（键位见 mechanics） |
| 警告 | `gdscript/warnings/untyped_declaration=1` |

> 注意：仓库工作树内 `project.godot` 当前为最小模板（仅 name/features/icon），**缺少**主场景、显示、输入、渲染段。GDA 测试若需运行，必须先以原件为准恢复上述段落；恢复前后以 `git diff` 留证。

## 场景/脚本映射

| 场景 | 脚本 | 关键节点 |
|---|---|---|
| `lobby.tscn`（根 `Control`） | `logic/lobby.gd`（挂于 `LobbyPanel`） | `Address`、`HostButton`、`JoinButton`、`StatusOk/Fail`、`PortForward`、`FindPublicIP` |
| `pong.tscn`（根 `Node2D`） | `logic/pong.gd`（挂于根） | `Player1/Player2`（`paddle.tscn` 实例）、`Ball`（`ball.tscn` 实例）、`ScoreLeft/Right`、`WinnerLeft/Right`、`ExitGame`、`Camera2D` |
| `ball.tscn`（根 `Area2D`） | `logic/ball.gd` | `Sprite2D`、`Shape3D`（Circle r=5.11969） |
| `paddle.tscn`（根 `Area2D`） | `logic/paddle.gd` | `Sprite2D`、`Shape3D`（Capsule r=4.78568 h=23.6064）、`You` |

## RPC 与权限总表

| RPC | 位置 | 标记 | 发起方 | 作用 |
|---|---|---|---|---|
| `update_score(add_to_left)` | `logic/pong.gd:31` | `any_peer + call_local` | 判出界的一侧 | 递增比分、刷新标签、胜负结算 |
| `bounce(left, random)` | `logic/ball.gd:45` | `any_peer + call_local` | 击球拍的权限方 | 反转横向、加速×1.1、随机纵向 |
| `stop()` | `logic/ball.gd:58` | `any_peer + call_local` | 胜负结算（`update_score` 内） | 冻结球 |
| `_reset_ball(for_left)` | `logic/ball.gd:63` | `any_peer + call_local` | 判出界的一侧 | 回中点、定向、复速 |
| `set_pos_and_motion(pos, motion)` | `logic/paddle.gd:37` | `unreliable` | 各拍权限方逐帧 | 同步拍位置+速度 |

- 权限：`Player1` 归 server（默认），`Player2` 在 `_ready` 中转交 client（server 交 `get_peers()[0]`，client 交自己）。
- 主线程约束：本仓库硬约束要求 Godot API 经 `CommandQueue` 在主线程执行；GDA 测试床内对本 demo 的一切读写（开场景、改节点、运行）须遵守该约束。

## 测试边界（实现自由度）

- 文档作者未运行 Godot：所有"未核实"标注即测试缺口，GDA 测试应优先覆盖。
- 测试中允许：恢复 `project.godot` 缺失段、开双实例联调、截图留证。
- 测试中不允许：修改联机端口/胜分等数值后仍声称"现状验证通过"（改数值即偏离实录，必须记录偏差）；删除 `_deps/` 缓存；将敏感信息（公网 IP、本地绝对路径）写入报告。
