---
type: 示例工程指南
title: 示例工程
description: Example 文档/示例工程定位、project.godot 事实、docs 文档索引与素材清单
tags:
  - 示例工程
  - Example
timestamp: "2026-09-13T21:50:35+08:00"
resource: Example/
---

# 示例工程（Example/）

> 审计日期：2026-09-13（2026-08-12 初稿；08-17 补 YAML frontmatter 并核对 project.godot；08-22 15 时全量一致性审计——`[audio]` 段与 `default_bus_layout.tres` 已随清理消失，改注为"L2 运行后可能追加"；08-29 随 0.2.2 版本与全量审计同步；09-13 随文档集重构同步——旧 6 篇结构（game-overview/gameplay-spec/architecture/level-spec/asset-catalog）迁移为 pitch/gdd/art/tech/test 共 11 篇，重核 project.godot 的 `[input]` 残留与 L2 临时目录；09-13 晚随跨组一致性修复更新——`gda_tmp_rename/` 与 `~godot-autopilot_0.pdb` 测试残留已清理，改注为"运行 L2/编辑器热重载可能生成、可安全清理"；09-13 晚随 0.2.4 版知识库全量审计同步——素材条目归属修正（`asset-catalog.md` 实为"未把'文件存在'当作'功能已实现'"，补充当前工程无 .tscn/.tres 的事实），其余条目复核一致），基于当前工作树 `Example/` 目录与文档核对。
> 事实来源：`Example/project.godot`、`Example/docs/`（11 篇）、`Example/assets/` 目录枚举、`Example/.gitignore`。

## 定位

`Example/` 是插件的**文档/示例工程**，不是完整游戏：

- **文档即规格**：`Example/docs/` 是标准化设计文档集，同时作为插件测试床；目标是让读者仅靠文档与 `Example/assets/` 在 Godot 4.7 中搭建一个单关卡 2D 平台跳跃游戏闭环。
- **当前无游戏代码**：`Example/` 下没有 `.gd`、`.tscn` 文件，`project.godot` 无 `run/main_scene`。非游戏内容为 `addons/godot-autopilot/`（插件部署产物，含 `.dll` / `.gdextension` / `.pdb`）；运行 L2/编辑器热重载可能生成临时产物（如 `gda_tmp_rename/*.tres`、`addons/.../~*.pdb`），可安全清理，非游戏内容（当前工作树已清理）。
- **素材库**：`Example/assets/` 现有 **173 个 PNG**（目录枚举：Background 7、Main Characters 30、Items 25、Menu 63、Other 4、Terrain 1、Traps 43），素材来自 Pixel Adventure 1（Pixel Frog），项目当前没有运行时引用；逐文件清单见 `art/asset-catalog.md`。
- **插件部署目标**：`uv run build.py` 把构建产物部署到 `Example/addons/godot-autopilot/`（属插件运行依赖，非游戏内容）。
- **MCP 验收场地**：测试目的、目标、约定与记录规范见 `test/test-plan.md`；所有 MCP 调用均在本仓库 MCP 服务器上执行。

## project.godot 关键事实

文件路径 `Example/project.godot`（39 行，`config_version=5`）：

| 配置段 | 键 | 值 | 说明 |
|---|---|---|---|
| `[application]` | `config/name` | `Example` | 项目名 |
| `[application]` | `config/features` | `PackedStringArray("4.7", "Forward Plus")` | 引擎版本 4.7、渲染方法 Forward Plus |
| `[application]` | `config/icon` | `res://icon.svg` | 图标 |
| `[display]` | `window/stretch/mode` | `canvas_items` | 窗口伸缩模式 |
| `[display]` | `window/stretch/aspect` | `expand` | 伸缩纵横比 |
| `[dotnet]` | `project/assembly_name` | `Example` | .NET 程序集名 |
| `[input]` | `test` | `{deadzone: 0.0, events: Array[InputEvent]([])}` | 空事件占位动作（测试残留，非游戏输入设计） |
| `[physics]` | `3d/physics_engine` | `Jolt Physics` | 3D 物理引擎：Jolt |
| `[rendering]` | `rendering_device/driver.windows` | `d3d12` | Windows 渲染设备驱动：Direct3D 12 |
| — | `run/main_scene` | （无） | **未配置主场景** |
| — | `[audio]` | （无） | **未配置**音频总线（L2 运行后 headless 编辑器可能自动追加，`git checkout -- Example/project.godot` 可清理） |

注：L2 再次运行后 headless 编辑器可能自动追加 `[audio]` 段并生成 `default_bus_layout.tres`（见 AGENTS.md 测试段），`git checkout -- Example/project.godot` + 手动删除 tres 可清理。

## 文档索引表（Example/docs/ 共 11 篇）

| 文档 | 主题 | 职责 | 关键内容 |
|---|---|---|---|
| [README.md](../../Example/docs/README.md) | 导航与边界 | 文档总入口 | 文档地图、阅读路径、工程现状（只读事实）、关键约定（单一权威/视觉验证/偏差记录/顶层验收） |
| [pitch/one-page-design.md](../../Example/docs/pitch/one-page-design.md) | 一页纸设计 | 概念速览 | 概念、钩子、核心循环、范围、成功标准 |
| [gdd/gdd.md](../../Example/docs/gdd/gdd.md) | 游戏设计总纲 | 系统总览 | 游戏概念、系统总览、内容范围、完成定义 |
| [gdd/mechanics.md](../../Example/docs/gdd/mechanics.md) | 玩法机制 | **玩法数值唯一来源** | 规则、参数、玩家状态机、行为契约、不变式 |
| [gdd/level-design.md](../../Example/docs/gdd/level-design.md) | 关卡设计 | **关卡坐标唯一来源** | 关卡标识与坐标系、布置、逐步验收路线 |
| [gdd/ui-hud.md](../../Example/docs/gdd/ui-hud.md) | UI/HUD | 信息契约 | 界面信息契约与提示时机 |
| [art/asset-catalog.md](../../Example/docs/art/asset-catalog.md) | 美术资源统计 | **PNG 资产事实唯一来源** | 173 个 PNG 路径/尺寸/帧数、首版必需/可选标记 |
| [art/art-direction.md](../../Example/docs/art/art-direction.md) | 美术方向 | 呈现要求 | 风格定位与呈现要求 |
| [tech/tdd.md](../../Example/docs/tech/tdd.md) | 技术设计 | 自由度边界 | 硬约束与实现自由度（结构、工具、操作步骤由实现者决策） |
| [test/test-plan.md](../../Example/docs/test/test-plan.md) | 测试计划 | 测试规范 | 测试目的、目标、约定、证据与记录规范 |
| [test/acceptance.md](../../Example/docs/test/acceptance.md) | 验收标准 | 顶层验收 | 顶层验收场景 + 视觉验收 + 分项索引 |

文档间的唯一性约定（README 关键约定 1）：玩法数值只出自 `mechanics.md`，关卡坐标只出自 `level-design.md`，资产路径只出自 `asset-catalog.md`，其他文档只引用不复述。

## 素材要点

- 总览：Background 7、Main Characters 30、Items 25、Menu 63、Other 4、Terrain 1、Traps 43，合计 173（与 `art/asset-catalog.md` 数量总览表逐项核对一致）。
- 路径包含空格、括号、大小写差异与个别拼写怪癖（如 `Desappearing`、`Flag Idle)(64x64)`），引用时必须保持原样。
- 文件名中的 `Idle`/`On`/`Hit`/`Appearing` 只描述视觉状态，不能推断运行行为（`asset-catalog.md` 资源使用检查：未把"文件存在"当作"功能已实现"）；资源存在 ≠ 已导入/已切片/已配置 TileSet——当前工程无 `.tscn`/`.tres`，未配置任何 TileSet。

## 与根文档/代码的一致性

- 端口 9527、`uv run build.py` 部署到 `Example/addons/godot-autopilot/`、`GODOT_AUTOPILOT_PORT` 覆盖：与根 README、AGENTS.md、`server_context.cpp` 一致 ✓
- `project.godot` 的 "4.7" 与 AGENTS.md"目标引擎 Godot 4.7" 一致；根 README 前提 "Godot 4.3+" 为宽松下界（两者不冲突，但口径不一）
- 旧文档（architecture.md / game-overview.md / gameplay-spec.md / level-spec.md）已删除，本文旧链接同步移除；新文档互链以 `Example/docs/README.md` 文档地图为准 ✓
- 运行 L2/编辑器热重载可能生成临时产物（如 `gda_tmp_rename/*.tres`、`addons/.../~*.pdb`），可安全清理，非游戏内容；当前工作树另有 `project.godot` 的 `[input] test` 动作残留（测试产物，不影响 `Example/docs/README.md` 的"不包含任何游戏代码、场景或存档"论断）

## 相关页面

- 项目总览：[overview.md](./overview.md)
- 核心模块（CommandQueue/ServerContext）：[modules/core.md](./modules/core.md)
- 构建与部署：[build.md](./build.md)
- 测试体系：[tests.md](./tests.md)
