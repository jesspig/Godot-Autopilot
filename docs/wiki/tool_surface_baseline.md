---
type: 基线文档
title: 工具面基线
description: 工具面最终静态基线：catalog 252（246 域 + 5 元 + system 1）、别名 325、schema 235/17、能力矩阵 561 行缺失 0
tags:
  - 工具面
  - 基线
  - 审计
timestamp: "2026-11-16T13:38:53+08:00"
resource: devtools/commands/tool-audit.ts
---

# 工具面基线

> 审计日期：2026-11-16。数据由 `devtools/commands/tool-audit.ts` 静态解析源码生成，汇总报告见 [tool_surface_report.md](tool_surface_report.md) 与 T-24 的 `audit-report.json`；本页数字均可在 [tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)、[tool_surface_target.json](../../tests/data/tool_surface_target.json) 与 [capability_matrix.json](../../tests/data/capability_matrix.json) 中找到来源。

## 数据来源与命令

- 域工具：`src/tools/domains/*/*_tools.hpp` 的 246 个 `ToolSpec` 块，分布在 35 个域文件。
- 系统与元工具：`src/tools/base/register_all.cpp` 的 `system_status` 与 5 个 `kMeta` 工具。
- 基线：`bun run gda-dev-tools audit-tools --baseline` → [tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)。
- 目标：`bun run gda-dev-tools audit-tools --target` → [tool_surface_target.json](../../tests/data/tool_surface_target.json) 与 [capability_matrix.json](../../tests/data/capability_matrix.json)。
- 查看：`bun run gda-dev-tools audit-tools --json` 直接输出完整基线。

## 基线计数（最终）

| 指标 | 数值 | 来源 |
| --- | ---: | --- |
| catalog 总数 | 252 | `catalog.total` / `catalog_total` |
| 域工具 | 246 | `catalog.domain` / `domain_total` |
| 元工具 | 5 | `catalog.meta` / `meta_total` |
| `system_status` | 1 | `catalog.system` / `system_total` |
| 域文件数 | 35 | `catalog.domain_count` / `domain_count` |
| 生产别名 | 325 | `alias_count` |
| schema 非空 | 235 | `parameter_schema.non_empty` / `schema_non_empty` |
| schema 空 | 17 | `parameter_schema.empty` / `schema_empty` |
| 能力矩阵域行 | 561 | `capability_matrix.row_count` |
| 含系统/元的 catalog 行 | 567 | `capability_matrix.catalog_row_count` |
| `mapped` | 315 | `capability_matrix.mapped_count` |
| `kept` | 252 | `capability_matrix.kept_count` |
| `missing` | 0 | `capability_matrix.missing_count` |

- 252 落在验收区间 250–300 内；schema 空 = 未声明参数且无 action schema 的工具（17 个）。
- 325 个别名不占 catalog、不进 `search_tools`，只经 `call_tool` / `batch_execute` 解析；契约见 [tool_surface_conventions.md](tool_surface_conventions.md)。其中 8 条与实名工具重名（Render 的 5 个 `create_render_*` 与 `get_particles_state`/`restart_particles`/`set_particles_params`），按冲突策略实名工具优先、别名被遮蔽；审计仍按 325 条定义计数。
- 能力矩阵按“工具行”计数，别名按“别名条目”计数，两者口径不同，不可直接相减。

## 域分布（35 个域文件，246 工具）

| 域 | 工具数 |
| --- | ---: |
| Physics | 46 |
| Render | 29 |
| Editor | 16 |
| Nav | 14 |
| OS | 12 |
| Animation | 10 |
| Resources | 10 |
| Scripts | 10 |
| Game | 9 |
| Config | 8 |
| Display | 8 |
| Scene | 7 |
| Debug | 6 |
| Input | 6 |
| Properties | 6 |
| Audio | 5 |
| Debugger | 4 |
| GridMap | 4 |
| Image | 4 |
| InputMap | 4 |
| Mesh | 4 |
| Text | 3 |
| Theme | 3 |
| TileMap | 3 |
| Tween | 3 |
| Capture | 2 |
| C# | 2 |
| Analysis | 1 |
| Docs | 1 |
| Group | 1 |
| Log | 1 |
| SceneTree | 1 |
| SpriteFrames | 1 |
| Testing | 1 |
| Tileset | 1 |
| **合计** | **246** |

category 口径为 28 类、求和 252（含元与系统）：域与 category 并非一一对应——`Image`/`Mesh` 域工具归 Resources 类，`C#` 域归 Scripts 类，`GridMap`/`Tileset` 归 TileMap 类，`SceneTree` 归 Scene 类，`Log` 域的 `get_game_log_entries` 归 System 类；完整表见 [overview.md](overview.md) 的「MCP 工具面」。

## 副作用分布

| SideEffect | catalog（252） | 域工具（246） |
| --- | ---: | ---: |
| None | 151 | 147 |
| WritesScene | 29 | 29 |
| ModifiesWindow | 20 | 20 |
| WritesFile | 14 | 14 |
| WritesConfig | 8 | 8 |
| Destructive | 7 | 7 |
| Process | 7 | 7 |
| GameRuntime | 7 | 7 |
| CodeExecute | 5 | 3 |
| ShowsAlert | 4 | 4 |

- catalog 与域工具口径仅两处不同：None 151/147、CodeExecute 5/3；差异来自 5 个元工具与 `system_status`（catalog 比域工具多 4 个 None、2 个 CodeExecute）。
- 需 `confirm=true` 的高风险档 Destructive、Process、CodeExecute、GameRuntime 合计 26 个 catalog 工具，默认安全姿态未放宽。

## 历史基线（已被本页取代）

- T-01 严格冗余基线：catalog 459（451 域 + 7 元 + `system_status`）、33 个域文件、别名 0、schema 393/66、严格冗余候选 32。
- ST-03 中间快照：catalog 308（302 域 + 5 元 + `system_status`），catalog 内新增 6 个工具（`pack_scene`、`animation_runtime`、`bake_navigation_mesh`、`astar_path`、`image_ops`、`mesh_ops`）。
- 上述历史数字只用于压缩前后对比，完整演进见 [tool_surface_report.md](tool_surface_report.md) 与 [changelog/2026-11-16-log.md](changelog/2026-11-16-log.md)；当前口径一律以本页「基线计数（最终）」为准。

## 相关文件

- 审计实现：[tool-audit.ts](../../devtools/commands/tool-audit.ts)
- 注册入口：[register_all.cpp](../../src/tools/base/register_all.cpp)
- 基线数据：[tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)
- 目标数据：[tool_surface_target.json](../../tests/data/tool_surface_target.json)
- 能力矩阵：[capability_matrix.json](../../tests/data/capability_matrix.json)
- 别名契约：[tool_surface_conventions.md](tool_surface_conventions.md)
- 最终报告：[tool_surface_report.md](tool_surface_report.md)
