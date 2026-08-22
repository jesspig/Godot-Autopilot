# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 条。

## 2026-08-22

- **大规模死代码清理与跨域去重**：删 ArgReader/IExportGuard/IAsync/make_ok·error/add_meta/to_tool_info_all/is_registered/get_queue/start_time_/GDA_*_READY_WAIT_MS 等死代码；新建 util/json_godot、rid_registry、type_hint、gdscript_wrap 四个 header-only 头；7 份 find_node 归一 resolve_scene_node（tilemap 获 root/ 剥离修复）、RidStore 归一、json_number 约 80 处收敛；register_all 删 25 冗余 include 与 build_schema_for_none_by_name，dispatch 元名单派生化；修复 restart 后 BM25 索引翻倍（index.clear()）；Unity 构建接线生效 batch=8；L1 71/71 + L2 77/77，净删约 1400 行/56 文件。详见 `changelog/2026-08-22-log.md`
- **版本号单一来源化**：新增根 `VERSION` 文件为唯一真源；CMake `file(READ)` 喂 `project()`、`configure_file` 生成 `GDA_VERSION` 宏供 `server_info`/`system_status` 引用、`build.py` 运行时读取；版本升至 **0.2.0**；文档产物名泛化 `<version>` 占位。详见 `changelog/2026-08-22-log.md`

## 2026-08-21

- **遍历副作用排除 `side_effects()` 驱动**：`SideEffect` 扩 6 值（+Process）；`GDA_TOOL_CLASS_SIDE` 变体宏标记 35 个副作用工具；`get_tool_detail` 补 `side_effect` 字段；runner 读该字段排除、删除硬编码 `kExcludedSideEffectTools`。详见 `changelog/2026-08-21-log.md`
- **元工具接口+组合化**：新增 `IMetaTool` 标记接口 + `MetaTool`（ToolBase+IMetaTool，依赖组合注入）；`ToolRegistry::add()` 按 `dynamic_cast<IMetaTool>` 自动归类（实现接口即元工具）；7 个元工具改以 `MetaTool` 注册；新增 L1 路由断言。详见 `changelog/2026-08-21-log.md`
- **ToolBase 工具统一标准化全量落地**：新增 `tool_decl.hpp`（真类宏）+ 26 个 `<域>_tools.hpp`，**336 域工具悉数迁移为独立 `ToolBase` 子类**；`tool_defs.def` 删除，catalog(344)/index/分发/RegisterTool 全从 registry 派生。详见 `changelog/2026-08-21-log.md`
- **重构收尾文档同步**：清除 AGENTS/wiki 对已删 `tool_defs.def` 的遗留引用与旧计数，统一为 336 域/344 catalog/35 副作用驱动排除。详见 `changelog/2026-08-21-log.md`

## 2026-08-20

- **ToolBase 工具统一标准化落地（ToolRegistry 单一来源）**：新增 `tool_base.hpp`/`tool_registry.hpp`/`fn_tool.hpp`；修复函数局部对象悬垂（`g_active_registry` 升为文件级静态）。详见 `changelog/2026-08-20-log.md`
