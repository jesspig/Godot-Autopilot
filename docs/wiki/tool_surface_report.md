---
type: 审计报告
title: 工具面压缩审计报告
description: 工具面审计：catalog 459→252、生产别名 325、能力矩阵 561 行缺失 0、副作用分布（CodeExecute 5→6）、安全机制与未验证项
tags:
  - 工具面
  - 审计
  - 压缩
  - 别名
timestamp: "2026-11-23T17:28:29+08:00"
resource: devtools/commands/tool-audit.ts
---

# 工具面压缩审计报告

> 审计日期：2026-11-23（增量同步）。本轮不重跑 audit：catalog、别名、schema、能力矩阵计数取自当前源码 catalog 基线与能力矩阵逐项统计，副作用按逐工具声明重算；复现命令仍为 `bun run gda-dev-tools audit-tools --json`（摘要 `audit-tools --report`）。压缩前数字取自 T-01 的 `audit-baseline.json`（catalog 459）。

## 数据来源与命令

- 当前审计：`bun run gda-dev-tools audit-tools --json`；摘要报告：`audit-tools --report`；JSON 报告：`audit-tools --report --json`；落盘：`audit-tools --report [--json] --out <file>`。
- 压缩前基线：T-01 的 `audit-baseline.json`（域 451 + 元 7 + `system_status` 1 = 459）。
- 能力矩阵：[capability_matrix.json](../../tests/data/capability_matrix.json)，由 `audit-tools --target` 重算。
- 按域计数取 `by_domain`；副作用取 `by_side_effect`（catalog 口径）与 `domain_by_side_effect`（域工具口径）。

## 压缩前后对比

| 指标 | 压缩前 | 压缩后 | 变化 |
| --- | ---: | ---: | ---: |
| catalog 总数 | 459 | 252 | -207（-45.1%） |
| 域工具 | 451 | 246 | -205（-45.5%） |
| 元工具 | 7 | 5 | -2 |
| `system_status` | 1 | 1 | 0 |
| 域文件数 | 33 | 35 | +2（新增 Image、Mesh） |
| 生产别名 | 0 | 325 | +325 |
| schema 非空 | 393 | 235 | -158 |
| schema 空 | 66 | 17 | -49 |
| schema 非空占比 | 85.6% | 93.3% | +7.7 个百分点 |
| 能力矩阵域行 | 451 | 561 | +110 |
| 能力矩阵缺失 | 0 | 0 | 0 |

- catalog 净减 207：域工具净减 205，元工具净减 2；域工具计数已包含压缩期间净新增的 6 个工具（`pack_scene`、`animation_runtime`、`bake_navigation_mesh`、`astar_path`、`image_ops`、`mesh_ops`），域文件数 +2 为新增 Image、Mesh 两域（各 4 个工具）。
- 252 落在验收区间 250–300 内。
- schema 空从 66 降到 17，非空占比由 85.6% 升到 93.3%；空 schema 指未声明参数且无 action schema 的工具。

## Top 合并组

以下合并组定义见 [tool-audit.ts](../../devtools/commands/tool-audit.ts) 的 `DOMAIN_TARGETS`；括号内为压缩前→压缩后的域工具数。

- Scene（15→7）：`get_scene_tree`、`create_scene_node`、`edit_scene_node`、`delete_scene_node`、`camera`、`instantiate`。
- Resources（34→10）：`load`、`query`、`create`、`property`、`file`、`save`、`uid`、`import`、`curve`、`mesh`。
- Render（52→29）：`environment`、`instance`、`viewport`、`camera`、`canvas`、`light`、`particles`、`resource`、`inspect`、`free`。
- Editor（33→16）：`state`、`ui`、`files`、`scene_control`、`save`、`selection`、`undo`、`ui_action`、`config`、`inspect`、`csharp`、`script`、`breakpoints`、`session`、`info`。
- Game（19→9）：`state`、`job`、`ui`、`evidence`、`script`、`input`、`tween`、`localization`。
- Audio（24→5）：`bus`、`player`、`device`、`capture`、`wav`。
- OS（23→12）：`process`、`env`、`time`、`system`、`file`、`file_manager`、`alert`、`path`。
- Display（25→8）：`info`、`clipboard`、`dialog`、`mouse`、`tts`、`window`、`screen`。
- Input（15→6）：`state`、`action`、`key`、`mouse`、`text`、`gamepad`。
- Config（17→8）：`read`、`write`、`autoload`。

## 按域计数

| 域 | 压缩前 | 压缩后 | 变化 |
| --- | ---: | ---: | ---: |
| Resources | 34 | 10 | -24 |
| Render | 52 | 29 | -23 |
| Audio | 24 | 5 | -19 |
| Display | 25 | 8 | -17 |
| Editor | 33 | 16 | -17 |
| OS | 23 | 12 | -11 |
| Game | 19 | 9 | -10 |
| Config | 17 | 8 | -9 |
| Debug | 15 | 6 | -9 |
| Input | 15 | 6 | -9 |
| Scene | 15 | 7 | -8 |
| SceneTree | 8 | 1 | -7 |
| Text | 10 | 3 | -7 |
| Theme | 10 | 3 | -7 |
| InputMap | 9 | 4 | -5 |
| Physics | 51 | 46 | -5 |
| C# | 6 | 2 | -4 |
| Docs | 4 | 1 | -3 |
| Analysis | 3 | 1 | -2 |
| Debugger | 6 | 4 | -2 |
| Group | 3 | 1 | -2 |
| SpriteFrames | 3 | 1 | -2 |
| TileMap | 5 | 3 | -2 |
| Tileset | 3 | 1 | -2 |
| Animation | 11 | 10 | -1 |
| GridMap | 5 | 4 | -1 |
| Nav | 15 | 14 | -1 |
| Properties | 7 | 6 | -1 |
| Scripts | 11 | 10 | -1 |
| Testing | 2 | 1 | -1 |
| Tween | 4 | 3 | -1 |
| Capture | 2 | 2 | 0 |
| Log | 1 | 1 | 0 |
| Image | 0 | 4 | +4 |
| Mesh | 0 | 4 | +4 |
| **合计** | **451** | **246** | **-205** |

## 能力矩阵摘要

| 指标 | 数值 | 来源 |
| --- | ---: | --- |
| 矩阵域行 `row_count` | 561 | `capability_matrix.rows` |
| 含系统/元的 catalog 行 | 567 | `catalog_row_count` |
| `mapped` | 315 | 旧工具名 → canonical |
| `kept` | 252 | 246 域行 + 6 系统/元行 |
| `missing` | 0 | `catalog_row_count - mapped - kept` |

- 561 行 = T-01 的 451 个旧工具 + 压缩期间新增的 110 行；旧工具中 315 行 `mapped`、136 行 `kept`，全部为 `kept|mapped`，缺失 0。
- 旧名静态覆盖率为 100%（`missing = 0`）；运行时别名解析与参数适配的回归测试未执行，见“未验证项”。
- 矩阵按“工具行”计数，别名按“别名条目”计数（325），两者口径不同，不能直接相减。
- 系统与元工具单独计入 `meta_rows`（6 行：`system_status` + 5 个元工具），不计入 561 个域行。

## 别名窗口

- 325 个生产别名不占 catalog、不进 `search_tools`，仅经 `call_tool` / `batch_execute` 解析到 canonical；结果顶层附加 `deprecated_alias`、`canonical_name`、`alias_removal_version`、`deprecated_since`。契约见 [tool_surface_conventions.md](tool_surface_conventions.md)。
- 137 处 `ToolAlias{...}` 字面量与 15 个域 helper 构造（合计 152 处）全部设置 `deprecated_since = "0.4.0"`、`removal_version = "0.6.0"`；源码中这两个字段没有其它版本值。
- 325 条别名定义中 8 条与实名工具重名（`create_render_decal`、`create_render_material`、`create_render_reflection_probe`、`create_render_shader`、`create_render_sky`、`get_particles_state`、`restart_particles`、`set_particles_params`），按“实名工具优先”在注册时移除；审计按定义数计 325，实际可解析别名为 317。
- 根 `VERSION` 当前为开发版 0.3.4-dev；窗口从 0.4.0 起算，至少保留两个 minor 版本到 0.6.0，到期前删除别名而不是恢复正式工具。
- T-23 证据中的 `alias-matrix.json` 摘录为 319，与其 `catalog.json` 的 325 及当前 audit JSON 的 325 不一致；319 属旧摘录，以 audit JSON 的 325 为准。

## 副作用分布

catalog 口径（含元工具与 `system_status`）：

| SideEffect | 压缩前 | 压缩后 | 变化 |
| --- | ---: | ---: | ---: |
| None | 282 | 150 | -132 |
| WritesScene | 43 | 29 | -14 |
| ModifiesWindow | 42 | 20 | -22 |
| WritesFile | 27 | 14 | -13 |
| WritesConfig | 18 | 8 | -10 |
| Destructive | 6 | 7 | +1 |
| Process | 10 | 7 | -3 |
| GameRuntime | 18 | 7 | -11 |
| CodeExecute | 9 | 6 | -3 |
| ShowsAlert | 4 | 4 | 0 |

- 域工具口径与 catalog 口径仅两处不同：None 146（catalog 150）、CodeExecute 4（catalog 6）；差异来自 5 个元工具与 `system_status`（catalog 比域工具多 4 个 None、2 个 CodeExecute）。
- 需 `confirm` 的高风险档 Destructive、Process、CodeExecute、GameRuntime 合计 27 个 catalog 工具，默认安全姿态未放宽。
- 本轮唯一副作用口径变化来自 `run_gdscript_tests`（Testing）：`None`→`CodeExecute`、`kNone`→`kMutating`（[test_tools.hpp](../../src/tools/domains/test/test_tools.hpp) 的 ToolSpec）。`tests/data/tool_catalog_baseline.json` 顶层 `by_side_effect` / `domain_by_side_effect` 汇总字段仍是旧值（151/5 与 147/3），逐项 `tools[]` 才是当前值，本页按逐项重算。

## 工具面安全机制（增量）

- 注册期新增 `audit_destructive_declarations`（[register_all.cpp](../../src/tools/base/register_all.cpp)）：对名字/描述疑似破坏性却未声明 Destructive 的工具记 Warning 与 `destructive_declaration_audit` 监控，不增删 catalog 条目。
- `call_tool` 增加线程本地递归深度上限 8（`kMaxCallToolDepth`）；对目标工具按 `effective_side_effect` 计算确认档，避免经 `call_tool` 间接绕过高风险确认。
- 提示词导出期统一由 `prompts_detail::reject_if_exporting()` 拦截（[prompt_export_guard.hpp](../../src/prompts/prompt_export_guard.hpp)），覆盖 debugger 提示词与通用提示词入口；debugger 模板已改用 `get_debugger_stream`、`game_script`、`get_debug_info`、`get_game_state` 等 canonical 名，`tool-usage` 示例从 17 个工具扩到 19 个。
- `build_csharp_assembly` 改为有界 deferred：先回 `__gda_wait` marker 再由等待入口解析结果，输出 digest 与错误行按上限截断。

## 搜索与测试结论

- 搜索索引由 `registry->all_any()` 构建 BM25，别名不入索引、不占 catalog；搜索候选来自 252 个真实工具的 catalog，而不是 252 + 325 个别名条目。
- 查询集 [search_queries.json](../../tests/data/search_queries.json) 共 50 条，每条带 `expect_any`；T-03 证据 `search-queries-test.txt` 只验证查询集格式（1 个 Bun 用例、268 个 `expect()` 通过），不能作为压缩后排序质量不下降的证据。
- T-24 计划的 `SearchQuality`、`AliasCoverage`、`CapabilityMatrix` 三项回归测试由同阶段测试任务新增在 `tests/unit/`；本报告未执行这些测试，结果标记为未验证。
- 最近一次全量 ctest 为 T-23 轮次证据：880/880 通过（unit 584、boundary 190、integration 88、guard 16、cli 2），不含 T-24 新增测试。
- 计划引用的 `evidence/T-03/search-baseline.json` 在 T-03 证据目录中不存在；搜索质量“不下降”缺少可对比基线，属待补项，不是已通过结论。

## 未验证项

| 项 | 状态 | 说明 |
| --- | --- | --- |
| 真实引擎 e2e：`physics_query` | 未验证 | T-23 `e2e-status.json` 记为 `not_run`，当时无已连接 Godot 编辑器 |
| 真实引擎 e2e：nav bake / astar | 未验证 | 同上，`nav_bake_astar = not_run` |
| 真实引擎 e2e：image / mesh | 未验证 | 同上，`image_mesh = not_run` |
| 真实引擎 e2e：audio system | 未验证 | 同上，`audio_system = not_run` |
| `SearchQuality` 搜索质量回归 | 未验证 | 本报告未执行，需以 T-24 测试输出为准 |
| `AliasCoverage` 别名覆盖回归 | 未验证 | 本报告未执行，需以 T-24 测试输出为准 |
| `CapabilityMatrix` 能力矩阵回归 | 未验证 | 本报告未执行，需以 T-24 测试输出为准 |
| 本轮增量新增的边界/回滚单测 | 未验证 | 本页仅基于源码与 catalog 基线逐项统计同步，未运行 ctest / audit-tools |

真实引擎 e2e 留待 T-26 在已连接编辑器上执行；本节列出的 `not_run` 项不得当作通过。

## 相关文件

- 审计实现：[tool-audit.ts](../../devtools/commands/tool-audit.ts)
- 当前基线：[tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)
- 能力矩阵：[capability_matrix.json](../../tests/data/capability_matrix.json)
- 搜索查询集：[search_queries.json](../../tests/data/search_queries.json)
- 注册入口：[register_all.cpp](../../src/tools/base/register_all.cpp)
- 历史基线：[tool_surface_baseline.md](tool_surface_baseline.md)
- 别名契约：[tool_surface_conventions.md](tool_surface_conventions.md)
