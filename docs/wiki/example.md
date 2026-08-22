---
type: 示例工程指南
title: 示例工程
description: Example 文档/示例工程定位、project.godot 事实、文档索引与素材清单
tags:
  - 示例工程
  - Example
timestamp: "2026-08-22T15:10:00+08:00"
resource: Example/
---

# 示例工程（Example/）

> 审计日期：2026-08-22（2026-08-12 初稿；08-17 补 YAML frontmatter 并核对 project.godot；08-22 15 时全量一致性审计——`[audio]` 段与 `default_bus_layout.tres` 已随清理消失，改注为"L2 运行后可能追加"；PNG 173 与 docs 6 篇复核），基于当前工作树 `Example/` 目录与文档核对。
> 事实来源：`Example/project.godot`、`Example/docs/`（6 篇）、`Example/assets/` 目录枚举。

## 定位

`Example/` 是插件的**文档/示例工程**，不是完整游戏：

- **文档即规格**：`Example/docs/` 是"可执行的规格"，目标是让读者仅靠文档与 `Example/assets/` 在 Godot 4.7 中搭建一个完整的单关卡 2D 平台跳跃闭环。
- **当前无游戏内容**：`Example/` 下没有 `.gd`、`.tscn`、`.tres` 文件，没有主场景（`project.godot` 无 `run/main_scene`），没有 `[input]` 输入动作，没有音频。场景、脚本、输入均为文档中的 `TODO`/`TARGET`。
- **素材库**：`Example/assets/` 现有 **173 个 PNG**（目录枚举，与 `asset-catalog.md` 一致），素材来自 Pixel Adventure 1（Pixel Frog），项目当前没有运行时引用。
- **插件部署目标**：`uv run build.py` 把构建产物部署到 `Example/addons/godot-autopilot/`（当前已有 `.dll` / `.gdextension` / `.pdb` 部署产物，属插件运行依赖，非游戏内容）。
- **MCP 验收场地**：`Example/docs/architecture.md` 给出了用 MCP 工具从空项目到运行验证的调用顺序；本工程用于验证插件本身的能力（所有 MCP 调用均在本仓库 MCP 服务器上执行）。

## project.godot 关键事实

文件路径 `Example/project.godot`（32 行，`config_version=5`）：

| 配置段 | 键 | 值 | 说明 |
|---|---|---|---|
| `[application]` | `config/name` | `Example` | 项目名 |
| `[application]` | `config/features` | `PackedStringArray("4.7", "Forward Plus")` | 引擎版本 4.7、渲染方法 Forward Plus |
| `[application]` | `config/icon` | `res://icon.svg` | 图标 |
| `[display]` | `window/stretch/mode` | `canvas_items` | 窗口伸缩模式 |
| `[display]` | `window/stretch/aspect` | `expand` | 伸缩纵横比 |
| `[dotnet]` | `project/assembly_name` | `Example` | .NET 程序集名 |
| `[physics]` | `3d/physics_engine` | `Jolt Physics` | 3D 物理引擎：Jolt |
| `[rendering]` | `rendering_device/driver.windows` | `d3d12` | Windows 渲染设备驱动：Direct3D 12 |
| — | `run/main_scene` | （无） | **未配置主场景** |
| — | `[input]` / `[audio]` | （无） | **未配置**输入动作与音频总线（L2 运行后 headless 编辑器可能自动追加，`git checkout -- Example/project.godot` 可清理） |

注：当前工作树 `project.godot` 无 `[audio]` 段、无 `default_bus_layout.tres`（上次 L2 副作用已被清理）；L2 再次运行后 headless 编辑器可能自动追加 `[audio]` 段并生成 `default_bus_layout.tres`（见 AGENTS.md 测试段），`git checkout -- Example/project.godot` + 手动删除 tres 可清理。

## 文档索引表（Example/docs/ 共 6 篇）

| 文档 | 主题 | 权威职责 | 关键内容 |
|---|---|---|---|
| [README.md](../../Example/docs/README.md) | 导航与边界 | 文档总入口、事实等级定义 | 单关卡 Demo 范围表、FACT/TARGET/TODO/VERIFY 等级、文档关系图、搭建前置条件、非目标清单 |
| [game-overview.md](../../Example/docs/game-overview.md) | 游戏概念 | 范围、目标、非目标、完成定义 | 一句话目标（Pink Man 从 Start 出发收集水果、激活 Checkpoint、避开 Spikes、抵达 End）、范围表、核心循环、完成定义 |
| [gameplay-spec.md](../../Example/docs/gameplay-spec.md) | 玩法规格 | **玩法规则唯一来源** | 目标参数表（网格 16px、视口 320x180、重力 900 px/s²、跳跃 -320 px/s 等）、输入契约、玩家状态机、收集品/陷阱/Checkpoint/End 规则、边界情况 |
| [architecture.md](../../Example/docs/architecture.md) | 插件架构（Demo 侧） | **技术结构唯一来源** | 目标目录树、场景树、节点/脚本职责、信号契约、碰撞层位分配（World=1/Player=2/Hazard=4/Collectible=8/Progress=16）、MCP 从空项目到运行验证顺序、部署与运行边界 |
| [level-spec.md](../../Example/docs/level-spec.md) | 关卡实例 | **关卡实例唯一来源** | 关卡 ID `level_01`、世界 1600x320 px、六段路线、场景树实例坐标表、Terrain 摆放表、逐步验收路线 |
| [asset-catalog.md](../../Example/docs/asset-catalog.md) | 素材清单 | **PNG 资产事实唯一来源** | 173 个 PNG 路径/尺寸/帧数（Background 7、Main Characters 30、Items 25、Menu 63、Other 4、Terrain 1、Traps 43）、首版必选/可选标记 |

文档间的唯一性约定：玩法数字只出自 `gameplay-spec.md`，关卡坐标只出自 `level-spec.md`，PNG 路径只出自 `asset-catalog.md`，不得交叉复制一套数字。

## 事实等级（docs 内约定）

| 标记 | 含义 |
|---|---|
| `FACT` | 可从文件/项目配置/资产目录直接观察到的事实 |
| `TARGET` | 本次 Demo 要实现的目标规格（不代表仓库当前存在） |
| `TODO` | 需要创建的文件、节点、脚本或资源配置 |
| `VERIFY` | 必须在 Godot 中执行并确认的事项 |

文档反复强调：`Idle`/`On`/`Hit` 等文件名只描述视觉资源，不能推断行为；资源存在 ≠ 已导入/已切片/已配置 TileSet。

## 与根文档/代码的一致性

- 端口 9527、`uv run build.py` 部署到 `Example/addons/godot-autopilot/`、`GODOT_AUTOPILOT_PORT` 覆盖：docs 与根 README、AGENTS.md、`server_context.cpp` 一致 ✓
- `Example/docs/README.md` 声称"Example 当前没有 `.gd`、`.tscn`、`.tres`、`.gdextension`"：`.gd`/`.tscn`/`.tres` 属实，但 `addons/godot-autopilot/godot-autopilot.gdextension` 已存在（插件部署产物，非游戏内容），陈述在文档写作时点成立，现已有例外 ✗
- 173 个 PNG 与 `asset-catalog.md` 总览表一致 ✓
- `project.godot` 的 "4.7" 与 AGENTS.md"目标引擎 Godot 4.7" 一致；根 README 前提 "Godot 4.3+" 为宽松下界（两者不冲突，但口径不一）
- `architecture.md` 的线程模型（HTTP → CommandQueue → 主线程 `_process` 排空）与 `main.cpp:217` 代码一致 ✓

## 相关页面

- 项目总览：[overview.md](./overview.md)
- 核心模块（CommandQueue/ServerContext）：[modules/core.md](./modules/core.md)
- 构建与部署：[build.md](./build.md)
