# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 条。

## 2026-08-28

- **日志系统增强（仅日志，未涉及 MCP SDK / 环回绑定）**：日志 dock 标题 "MCP Log" → "GDA Log"；配置面板新增 "Show timestamps" 开关（默认开、show_time 持久化），每条前缀 `[HH:MM:SS]`，折叠合并行始终显示最新时间；ServerContext 关键流程补诊断日志、启动失败透传真实异常类型；call_tool 分发加 Debug 级日志。详见 `changelog/2026-08-28-log.md`

## 2026-08-24

- **竞品对齐批次 wiki 沉淀**：新建 plans/roadmap.md（竞品定位速记 + P0/P1/P2 交付清单 + P3 遗留）；tools_registry 类别表重算 27 类/363 域、371 条目口径、副作用 42 重分组；core.md 补 error_watermark/editor_readiness 小节；entry_runtime 补 input_sequence/ui_elements op 与 GameBridgeFrameSequence 帧调度；清理被删 debugger 工具死引用；AGENTS.md 数字同步（363/370/371/42/77/83）；reimport 字段名缺口确认已修。详见 `changelog/2026-08-24-log.md`

## 2026-08-23

- **CI/Release 工作流落地**：新增 `.github/workflows/ci.yml`（develop 触发，三平台 Debug + L1）与 `release.yml`（tag `v*` 触发：tag↔VERSION 校验 → 三平台 Release → 合并 `addons.zip` 发布）；build.py 增 `--package --libs-dir` 跨平台合并打包、gdextension macOS 条目改 universal；preset 设 `CMAKE_OSX_ARCHITECTURES` 双架构；版本升至 **0.2.1**；L1 71/71 验证通过。详见 `changelog/2026-08-23-log.md`

## 2026-08-22

- **全量代码-文档一致性审计**：零代码变更前提下逐页核对 24 个 wiki 页与 AGENTS/README/测试资产；修正 README ~339→~343、L1 分文件计数 77→71、L2 用例 5→6、监控项表 83→59、RENAME_HINTS 9→10、editor 计数 22→23、register_listener 注册类 4→6、排除清单分组 12+22+1、步数对齐实测 546；support.md 补四个 util 共享头小节；core/entry_runtime/build/overview 行号与语义残留清除。详见 `changelog/2026-08-22-log.md`
- **大规模死代码清理与跨域去重**：删 ArgReader/IExportGuard/IAsync/make_ok·error/add_meta/to_tool_info_all/is_registered/get_queue/start_time_/GDA_*_READY_WAIT_MS 等死代码；新建 util/json_godot、rid_registry、type_hint、gdscript_wrap 四个 header-only 头；7 份 find_node 归一 resolve_scene_node（tilemap 获 root/ 剥离修复）、RidStore 归一、json_number 约 80 处收敛；register_all 删 25 冗余 include 与 build_schema_for_none_by_name，dispatch 元名单派生化；修复 restart 后 BM25 索引翻倍（index.clear()）；Unity 构建接线生效 batch=8；L1 71/71 + L2 77/77，净删约 1400 行/56 文件。详见 `changelog/2026-08-22-log.md`
- **版本号单一来源化**：新增根 `VERSION` 文件为唯一真源；CMake `file(READ)` 喂 `project()`、`configure_file` 生成 `GDA_VERSION` 宏供 `server_info`/`system_status` 引用、`build.py` 运行时读取；版本升至 **0.2.0**；文档产物名泛化 `<version>` 占位。详见 `changelog/2026-08-22-log.md`

## 2026-08-21

- **遍历副作用排除 `side_effects()` 驱动**：`SideEffect` 扩 6 值（+Process）；`GDA_TOOL_CLASS_SIDE` 变体宏标记 35 个副作用工具；`get_tool_detail` 补 `side_effect` 字段；runner 读该字段排除、删除硬编码 `kExcludedSideEffectTools`。详见 `changelog/2026-08-21-log.md`
- **元工具接口+组合化**：新增 `IMetaTool` 标记接口 + `MetaTool`（ToolBase+IMetaTool，依赖组合注入）；`ToolRegistry::add()` 按 `dynamic_cast<IMetaTool>` 自动归类（实现接口即元工具）；7 个元工具改以 `MetaTool` 注册；新增 L1 路由断言。详见 `changelog/2026-08-21-log.md`
- **ToolBase 工具统一标准化全量落地**：新增 `tool_decl.hpp`（真类宏）+ 26 个 `<域>_tools.hpp`，**336 域工具悉数迁移为独立 `ToolBase` 子类**；`tool_defs.def` 删除，catalog(344)/index/分发/RegisterTool 全从 registry 派生。详见 `changelog/2026-08-21-log.md`
