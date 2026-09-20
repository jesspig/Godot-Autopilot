---
type: 工程约定
title: 工程约定
description: 命名、日志、错误模式、线程纪律与添加工具流程等全仓库编码约定
tags:
  - 约定
  - 命名
  - 规范
timestamp: "2026-09-20T22:11:06+08:00"
resource: src/
---

# 工程约定

本文汇总代码中实际体现的工程约定（与 [overview.md](overview.md)、[AGENTS.md](../../AGENTS.md) 互为参照）。安全边界与并发生命周期以 [T0 安全边界与并发契约](security_contract.md) 为准。

## 命名体系

- **命名空间**：`godot_autopilot`（工具子命名空间 `<模块>_ops`，如 `godot_autopilot::scene_ops`）
- **前缀**：核心常量 `GDA_*`（如 `GDA_DEFAULT_PORT`）；运行时协议 `gda:*`；导出宏 `GDA_EXPORT`
- **产物与目录**：CMake 目标 `godot-autopilot`，部署到 `Example/addons/godot-autopilot/`
- **工具命名**：约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（snake_case，**以动词置首为主导**，如 `create_scene_node`、`intersect_physics_2d_ray`、`set_input_map_action_deadzone`）；391 个工具名段数随粒度自然变化（2-8 段，如 `read_file` / `add_physics_3d_body_collision_exception`），`signal_connect`/`property_set` 等少量短名为规范内保留的名词前置形态
- **头文件 include guard**：`GODOT_AUTOPILOT_<MODULE>_HPP`

## 日志

- 类别（仅 5 个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`（`LogCategory` 枚举）
- 级别：`Debug`、`Info`、`Warning`、`Error`（`LogLevel` 枚举）
- `LogSystem` 单例：环形缓冲上限 10000 条，缓冲本体仅内存（`McpLogDock` 轮询消费）；磁盘落盘经 `LogPersist` 增量拉取到 `user://godot_autopilot/logs/gda-<stamp>.log`（人类可读，含 `log_detailed` 的 detail）与 `traces/trace-<stamp>.jsonl`（结构化事件），两目录各保留最近 20 文件 / 50MB
- `log_detailed(level, category, summary, detail)` 用于带诊断上下文的日志（detail 上限 8192 字符，超出截断）；`query` 的 `filter_text` 同时匹配 message 与 detail
- 禁止记录密钥与 PII；参数/trace 数据默认经 `sanitize_policy` 脱敏（`GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize` > 默认 true），关闭后原始参数与截图会落盘 `user://`

## 错误模式

- 领域工具失败返回 `{"error": "消息"}` JSON（构造经 `util::error_json`，成功结果经 `util::ok_result` 包 `result`，见 `src/util/error_util.hpp`；个别执行类响应另带 `error_details`/`structured_error` 诊断字段），`call_tool` 元工具检查 `error` 字段并设 `is_error = true`
- 命名契约：`get/fetch/load` 目标必存在否则抛异常；`find/search/query` 允许为空；`validate/check` 失败即抛；`apply/compute` 为纯函数

## 线程纪律

- **所有 Godot API 调用必须通过 `queue.submit()`**（HTTP 线程直接调用会崩溃）；例外：`debugger_prompts` 为纯静态模板文本、不触碰 Godot API 亦不经队列，`debugger_resources` 读取捕获缓冲仍经 `queue.execute_sync()`；`call_tool` 元工具编排回调在 MCP 线程执行但自身不触碰 Godot API（领域工具经 dispatch 路由回主线程，2026-09-13 起）
- `CommandQueue`：header-only，`submit()` 返回 `std::future`（异常经 `set_exception` 传播），主线程 `_process()` 每帧 `drain()`
- 导出期间（`ExportGuard` 原子标记）`dispatch` 拒绝工具调用

## 添加工具流程

`ToolRegistry` 单一来源 + 全量 `ToolSpec` 数据记录（无 `tool_defs.def`、无宏/真类；设计见 [tool_base_design.md](tool_base_design.md)）：

1. `<域>_ops.hpp` 声明 `mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`
2. `<域>_ops.cpp` 实现（返回 `{"error": ...}` 或结果对象；取参用 `Args(raw, kXxxParams)` 的 opt_/require_/get_ 系列，必要时 `reject_unknown()` 拒未知键）
3. 该域 `<域>_tools.hpp`：定义 `const std::vector<ParamSpec> kXxxParams = {...}` 参数表，并在 `make_tools()` 中以 `v.push_back(make_spec_tool(ToolSpec{name, description, category, {tags}, side_effect, flags, kXxxParams, handler}))` 一行注册——`register_all.cpp` 经各域 `make_tools()` 自动注册，无需手改；schema 从参数表自动派生（仅嵌套结构才用 `raw_schema` 手写 JSON）
4. `side_effect`（`SideEffect` 枚举选值）与 `flags`（如 `kMutating`）按工具实际影响声明即自动进入遍历排除，无需手改 `tests/runner/traversal.cpp`；合成输入类可加 `kObserve` 复用执行管线的截图后处理
5. 仅新增 `.cpp` 才需动 `CMakeLists.txt:65` 的 `add_library()`（header-only 无需）；迁移守卫 `tests/guard/` 拒绝旧宏（`GDA_TOOL_CLASS` 等）与 `schema_*_ops.cpp` 残留

## 测试纪律

- L1（`gda_unit_tests`）禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃），只能测注册/分发/错误路径
- 子代理迭代：只写代码不编译，主代理统一 configure + build 再分批跑测试（并行编译会锁）
- 新增 `src/*.cpp` 同步更新 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`
- 计数口径（L1 gtest 数、L2 `tests/config/*.json` 份数、ctest 注册点、遍历候选/步骤数）以运行时统计为准；迁移守卫经 `ctest --preset debug -R migration_guard` 零依赖运行

## 能力边界（08-20 确立）

gda 聚焦「编辑器内的引擎操作」，以下能力**明确不做**，避免重复造轮子，交由外部编辑器/CLI 工具承担：

- **文本级增量编辑**：gda 不做文件内精确 diff/apply 式编辑；对场景/资源内容的语义级读写走 `scene_*`/`resource_*` 工具，纯文本查看/搜索用 `read_file`/`find_in_files`，深度编辑交给外部编辑器。
- **glob 风格文件匹配**：文件列举/目录遍历由 `get_resource_dir_files`、`get_editor_file_system_tree`、`find_in_files` 覆盖，不单独提供 glob 匹配工具。
- **C# 程序集热重载**：`build_csharp_assembly` 仅能触发 `dotnet build`（进程副作用，已入遍历排除清单）；GDExtension(C++) **无公共 API** 触发编辑器内 .NET 程序集重载（该能力位于引擎 `modules/mono` 内部）。因此 C# 类/签名变更后，编辑器内 `load()`/`get_script()`/校验渲染仍必须由用户在编辑器点 Build 或重启编辑器；gda 侧的 `code_execute`/GDScript 通道不受影响。

## 知识局限（文档提示）

- **C# 只读/私有 setter 属性对 GDScript 不可见**（Godot .NET 绑定既有行为，非 gda 缺陷）：`get("MaxHealth")` 对无 getter 的属性返回 null。AI 用 GDScript/`property_get` 校验 C# 派生值时易误判——应改为检查可读导出引用而非派生值。

## 其他

- CI/Release 工作流见 [build.md](./build.md)「CI 与 Release」；CI 仅跑 L1，L2 引擎用例仍仅本地执行（需 `GODOT_PATH`）
- 依赖版本固定：godot-cpp 10.0.0-stable、mcp-cpp-sdk 0.3.4（FetchContent，无子模块；另以 `GODOTCPP_API_VERSION "4.7"` FORCE 锁定目标 API，见 [build.md](./build.md)）
