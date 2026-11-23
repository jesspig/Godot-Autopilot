---
name: godot-autopilot
description: "Godot Autopilot（GDA）唯一入口技能卡：通过 gda status --json 与 mcp_url 连接，用 gda docs 查引擎 API，用 gda search/describe 发现 live 工具，用 gda call 调用，遵守 confirm 与错误水位线，并执行变更-观测-验证环。按任务读取 setup、tools、scene-system、scripting、resources、runtime、content、csharp 或 servers 参考。"
---

# Godot Autopilot

Godot Autopilot（GDA）是进程内 GDExtension，通过本地 `gda` CLI 与 MCP 端点把运行中的 Godot 编辑器暴露给 Agent。本卡是唯一入口；只读与当前任务匹配的参考，不要一次加载全部。

## 连接模型

- `gda` CLI 是一等接口：自动发现插件的随机端口，优先 IPC，失败回退 HTTP，输出 JSON。
- 端口由插件拥有并持久化。禁止自选监听端口、假设默认端口，或在编辑器重启后复用旧端口。
- `gda status --json` 是连接记录；`mcp_url` 是 MCP 客户端唯一权威端点，`transport` 与 `ipc_kind` 说明 CLI 使用的本地通道。
- 服务只绑环回地址；禁止转发、代理或公开其端口。
- 看不到实例时，`gda list --all` 列出已知项目，`gda start --project <dir>` 启动编辑器；每次重启后重新读取 `mcp_url`。
- MCP 面是已有原生工具列表的客户端兼容通道：`search_tools`、`get_tool_detail`、`call_tool`、`batch_execute`、`code_execute`。规则相同；客户端没有原生工具时默认用 CLI。

## 引擎文档

禁止凭记忆写不熟悉的引擎 API。属性名、方法签名、枚举值与默认值会随 Godot 版本变化，凭记忆写出的签名是静默失败的常见来源。

- 写或改涉及不熟悉 API 的脚本、设置名称或可接受值不清楚的属性、或排查 “unknown property” 与解析错误前，先运行 `gda docs search <query>`。
- 再用 `gda docs class <name> --member <text>` 或 `gda docs member <class> <name>` 缩小范围。
- 文档来自本机引擎二进制，按引擎 minor 缓存在 GDA home 下；首次查询可能导出缓存，之后很快。
- 项目 pin 与可用二进制不一致时命令会报错并建议 `--godot`；传入匹配的 Godot 二进制，不要猜。
- `gda docs status` 显示项目 pin、解析到的引擎与缓存状态；`gda docs sync` 强制导出；`--docs-file <path>` 读取外部 dump。

## 发现工具，不要猜名字

工具名是 snake_case，通常动词在前，但段数会变，名字也会随工具集增长而变化。发现是强制步骤。

live catalog 有 253 个 canonical 工具。另有 325 个弃用别名仍可经 `call_tool` 与 `batch_execute` 解析，但不进 `search_tools`，也不出现在 `gda tools list`。别名自 0.4.x 起弃用，将在 0.6.x 移除：始终调用 canonical 工具并传它的 action。常见旧名到新调用的映射见 `references/tools.md` 的别名说明。

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"node_path": "Level", "cells": [{"x": 0, "y": 0, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}]}'
```

1. `gda search <query>` 按名称、描述、分类与标签搜索 live 服务，默认最多 20 条。
2. `gda describe <name>` 打印 schema 与 `side_effect` 标记；首次调用前必读，禁止推断参数名。
3. `gda call <name> --args '<json>'` 或 `--args-file <path>` 对 live 编辑器执行一次调用。
4. 需要完整清单时，`gda tools list` 列出全部可直接调用的工具；弃用别名不在其中。

发现只针对 live 实例，没有离线 catalog；没有实例时先 `gda start --project <dir>`。先按对象搜索（“tilemap cell”“animation track”“import sidecar”），结果太宽再补动词；下结论说工具缺失前换同义词重试：tile/cell、sprite/texture、log/output、delete/remove/erase、create/add/new。

许多工具用必填 `action`（`create_scene_node` 与 `physics_query` 用 `mode`）把相关操作收进一个工具。读 `gda describe` 里的枚举，不要记单独的一次性工具名。

## 任务路由

先把任务归约到对象与意图，再读对应参考。首次调用前用 `gda search` 与 `gda describe` 确认名称和 schema。

| 任务 | 参考 | 首步 |
|---|---|---|
| 安装、启动、连接、安装技能 | `references/setup.md` | `gda status --json`；缺失则安装插件 |
| 调用协议、confirm、错误、docs CLI | `references/tools.md` | 首次调用前 `gda describe <tool>` |
| 场景、节点、属性、信号、undo | `references/scene-system.md` | `get_scene_tree` 与 `property_get` |
| GDScript 生命周期、执行通道、重载 | `references/scripting.md` | 检查 `@tool` 并选择通道 |
| 资源、导入、重命名/移动、UID | `references/resources.md` | 移动前检查依赖 |
| 运行游戏、注入输入、调试、日志 | `references/runtime.md` | `get_game_state` 的 action=status 与错误水位线 |
| TileMap、动画、音频、UI 主题 | `references/content.md` | 编辑前检查现有资源 |
| C#、csproj、构建环、反射桥 | `references/csharp.md` | 探测 `dotnet_environment` 后构建 |
| 服务器、RID、物理、导航 | `references/servers.md` | 检查 RID 生命周期与线程 |

## 症状直达

| 症状 | 参考 | 首步 |
|---|---|---|
| 属性写不进去 | `references/scene-system.md` | 读回；检查 owner 与值形状 |
| 场景保存后丢节点 | `references/scene-system.md` | 检查父节点、owner 与保存目标 |
| 游戏忽略注入的输入 | `references/runtime.md` | `get_game_state` 的 action=status；对齐输入帧 |
| 脚本在编辑器里不执行 | `references/scripting.md` | 加 `@tool` 或改用编辑器通道 |
| C# 改动没有生效 | `references/csharp.md` | 重新构建并读 `diagnostics.instantiable` |
| 重命名后资源路径断裂 | `references/resources.md` | 检查 UID、依赖与引用 |
| 瓦片或动画显示不对 | `references/content.md` | 检查 TileSet、轨道与 SpriteFrames |
| 服务器调用静默失败 | `references/servers.md` | 检查 RID 有效性与主线程执行 |

## 确认与安全

高风险副作用不执行，而是返回结构化 `confirmation_required`。类别为 `destructive`、`process`、`code_execute` 与 `game_runtime`。审查工具描述与确切目标后重试：

```bash
gda call code_execute --args '{"source_code": "return 1"}' --confirm
```

使用原生 MCP 工具时，在参数对象里传 `confirm: true`。文件与场景写入（`writes_file`、`writes_config`、`writes_scene`）不需要确认，但仍会改变项目内容：每次写入后读回目标。禁止换一个效果相同的工具绕过确认错误，包括弃用别名。把 `destructive` 与 `game_runtime` 注解视为停下审查的信号，而不是形式。

## 观察引擎状态

禁止盲目串联写入。每次改动都落在带导入队列、调试器与引擎日志的 live 编辑器里；没有观测到的状态就是未知状态。

- 每次调用后读 `new_errors_since_last_call`。非零表示记录了新错误；拉取真实日志行，修复或解释后再继续。计数器一次性，读取后重置。
- 游戏运行时，`get_debugger_stream` 的 action=errors 与 action=output 提供其错误与打印；没有运行游戏时，action=log 读编辑器输出与引擎日志，`get_game_log_entries` 在会话结束后读磁盘日志。
- 问题在插件自身（确认被拒、超时、响应丢失）时，读 `get_plugin_log` 或 `get_debugger_stream` 的 action=log 并设 source=plugin。
- 优先相信观测输出——日志行、截图、读回——而不是假设调用名等于效果。`ok` 只表示调用通过了。

## 变更-观测-验证环

1. 通过工具改变项目，或用 `batch_execute` / `code_execute` 编排确定性序列。
2. 用截图（`capture_editor_viewport`，运行中的游戏用 target=game）、读回（`property_get`、`get_scene_tree`、`resource_property` 的 action=get）或输出（`get_debugger_stream` 的 action=log，游戏运行时用 action=errors / action=output）观测。
3. 根据观测诊断、修复，再次观测，直到结果符合意图。
4. 用错误水位线归零确认修复，再运行相关验证工具（`analyze_project` 的 action=validate_scene、内联 GDScript 测试或游戏探针）后才宣布成功。

## 参考索引

- `references/setup.md` —— 安装、启动、连接、技能安装、`--prune`。
- `references/tools.md` —— CLI 协议、confirm、错误、限制、docs CLI、别名窗口。
- `references/scene-system.md` —— 场景、节点、属性、信号、undo。
- `references/scripting.md` —— GDScript 生命周期与执行通道。
- `references/resources.md` —— 资源、导入、重命名/移动、UID。
- `references/runtime.md` —— 运行游戏、输入、日志、调试。
- `references/content.md` —— tilemap、动画、音频、UI 主题。
- `references/csharp.md` —— C# 构建环与反射桥。
- `references/servers.md` —— RID、物理、导航、渲染。
