---
type: 约定文档
title: 工具面别名约定
description: 隐藏别名的定义、参数适配、结果投影、弃用字段、冲突策略与目录/搜索排除规则
tags:
  - 工具面
  - 别名
  - 弃用
  - ToolSpec
timestamp: "2026-11-16T04:49:31+08:00"
resource: src/tools/base/tool_spec.hpp
---

# 工具面别名约定

> 本文是工具面压缩期间旧工具名的兼容契约；别名不占 catalog、不进 `search_tools`，但经 `call_tool` 与 `batch_execute` 仍可调用。

## 定义

`ToolAlias` 定义在 [tool_spec.hpp](../../src/tools/base/tool_spec.hpp)：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `name` | string | 旧工具名，调用入口 |
| `canonical` | string | 真实工具名 |
| `adapt_args` | function | 旧参数到 canonical 参数的适配；为空表示参数不变 |
| `project_result` | function | canonical 结果到旧结果形状的投影；为空表示结果不变 |
| `deprecated_since` | string | 开始弃用的版本 |
| `removal_version` | string | 计划移除版本；至少保留两个 minor 版本 |

`ToolSpec.aliases` 承载别名列表，由 `SpecTool` 构造时随工具进入 [tool_registry.hpp](../../src/tools/base/tool_registry.hpp) 的 `aliases_`。

## action 工具规范

- 单工具用 `action` 字符串枚举区分动作；`action` 必填，枚举值在 schema 中列出，单个工具最多 8 个 action。
- action 专用字段放在顶层可选参数，由 handler 按 action 做必填校验；错误统一为 `missing required parameter for action '{action}': {name}`。
- 公共定位字段（如 `path`、`node_path`）可作为全局必填；action 专用字段在 schema 中保持可选，避免跨 action 误报。
- 合并只在同一 `SideEffect` 档内进行；Read 工具不因合并触发 `confirm`，Destructive/GameRuntime 仍由 canonical 工具统一确认。
- 工具描述必须枚举 action 与至少一个示例；`ToolSpec.examples` 存示例字符串，经 `ToolInfo` 透传到 `get_tool_detail` 的 `tool.examples`。
- schema 辅助见 [schema_builder.hpp](../../src/tools/base/schema_builder.hpp) 的 `build_action_schema`：生成 `action` enum 与必填标记，同时保留现有 `ParamDef` / `build_schema` 兼容。

## 调用链

1. `call_tool` / `batch_execute` 经 `dispatch::call_handler` 进入统一入口。
2. [dispatch.cpp](../../src/tools/base/dispatch.cpp) 先用 `resolve_alias(name)` 命中别名，得到 canonical 工具名。
3. `apply_alias_args` 调用 `adapt_args`；若旧参数带 `confirm=true` 而适配结果丢失该字段，自动补回，保证 Destructive/GameRuntime 的确认门不被别名绕过。
4. canonical `SpecTool::execute` 执行原有的 confirm、主线程派发、后处理与撤销逻辑。
5. `apply_alias_result` 先调用 `project_result`，再在响应顶层追加：
   - `deprecated_alias`
   - `canonical_name`
   - `alias_removal_version`
   - `deprecated_since`（非空时）
6. 异步结果（`__gda_pending`、`__gda_wait`、`__gda_job`）先带弃用字段返回 pending；`resolve_deferred_wait` 在等待完成后对最终结果再做一次投影与字段追加。

## 目录与搜索

- `ToolRegistry::all()` / `all_any()` 只遍历真实工具；别名不计入 `catalog_total`、`tool_count` 或 `size()`。
- `ToolCatalog::replace_aliases` 只建立旧名到 canonical 的查询回退；`get_all_tools()`、`get_categories()`、`size()` 仍只含真实工具。
- `search_tools` 的 BM25 索引由 `registry->all_any()` 构建，因此不会索引别名。
- `get_tool_detail(alias)` 返回 canonical 工具详情，并在顶层附加 `deprecated_alias`、`canonical_name`、`alias_removal_version`、`deprecated_since`。

## 冲突与保留

- 别名与真实工具重名时拒绝别名，真实工具优先；真实工具后加入时 `add()` 会先移除同名别名。
- 别名与别名重名时先注册者生效；静态审计脚本会在同一源码树内检测重复别名并报错。
- 别名命中写 `Warning` 级 `Tools` 日志，消息包含 `alias` 与 `canonical`。
- 别名至少保留两个 minor 版本；到期前删除别名而不是改回正式工具。

## 验证

- 单元测试：[alias_resolution_test.cpp](../../tests/unit/alias_resolution_test.cpp)，覆盖改名、参数改名、单值到数组、结果投影、confirm 透传、冲突拒绝、catalog/search 排除与 `get_tool_detail` 元数据。
- 静态基线：[tool_surface_baseline.md](tool_surface_baseline.md) 的 `alias_count`；当前生产别名为 0，32 项旧名映射见 [capability_matrix.json](../../tests/data/capability_matrix.json)。
- 执行命令：`bun run gda-dev-tools audit-tools --json`、`bun run gda-dev-tools test --single AliasResolution`。
