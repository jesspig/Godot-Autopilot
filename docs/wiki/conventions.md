---
type: 工程约定
title: 工程约定
description: 命名、日志、错误模式、线程纪律与添加工具流程等全仓库编码约定
tags:
  - 约定
  - 命名
  - 规范
timestamp: "2026-08-28"
---

# 工程约定

本文汇总代码中实际体现的工程约定（与 [overview.md](overview.md)、[AGENTS.md](../../AGENTS.md) 互为参照）。

## 命名体系

- **命名空间**：`godot_autopilot`（工具子命名空间 `<模块>_ops`，如 `godot_autopilot::scene_ops`）
- **前缀**：核心常量 `GDA_*`（如 `GDA_DEFAULT_PORT`）；运行时协议 `gda:*`；导出宏 `GDA_EXPORT`
- **产物与目录**：CMake 目标 `godot-autopilot`，部署到 `Example/addons/godot-autopilot/`
- **工具命名**：约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（snake_case，**动词置首**），如 `create_scene_node`、`intersect_physics_2d_ray`、`set_input_map_action_deadzone`；336 个工具名全部动词置首，段数随粒度自然变化（2-6 段），`signal_connect`/`property_set` 等短名属规范内省略类别段
- **头文件 include guard**：`GODOT_AUTOPILOT_<MODULE>_HPP`

## 日志

- 类别（仅 5 个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`（`LogCategory` 枚举）
- 级别：`Debug`、`Info`、`Warning`、`Error`（`LogLevel` 枚举）
- `LogSystem` 单例：环形缓冲上限 10000 条，仅内存写入（`McpLogDock` 轮询消费），**无文件输出**
- 禁止记录密钥与 PII

## 错误模式

- 领域工具失败返回 `{"error": "消息"}` JSON（可含 `error_detail` 扩展），`call_tool` 元工具检查 `error` 字段并设 `is_error = true`
- 命名契约：`get/fetch/load` 目标必存在否则抛异常；`find/search/query` 允许为空；`validate/check` 失败即抛；`apply/compute` 为纯函数

## 线程纪律

- **所有 Godot API 调用必须通过 `queue.submit()`**（HTTP 线程直接调用会崩溃）；例外：`debugger_prompts`/`debugger_resources` 只读捕获缓冲不经队列（安全）
- `CommandQueue`：header-only，`submit()` 返回 `std::future`（异常经 `set_exception` 传播），主线程 `_process()` 每帧 `drain()`
- 导出期间（`ExportGuard` 原子标记）`dispatch` 拒绝工具调用

## 添加工具流程

`ToolRegistry` 单一来源（无 `tool_defs.def`）：

1. `<category>_ops.hpp` 声明 `mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`
2. `<category>_ops.cpp` 实现（返回 `{"error": ...}` 或结果对象）
3. 该域 `<category>_tools.hpp` 用 `GDA_TOOL_CLASS`（有副作用用 `GDA_TOOL_CLASS_SIDE`）声明独立 ToolBase 子类并入 `make_tools()`——`register_all.cpp` 自动注册，无需手改；schema 由 `tool_input_schema(name, basic)` 单一源（转发 `build_schema_for`，`basic` 参数保留签名但已为 no-op）
4. `CMakeLists.txt` `add_library()` 添加 `.cpp`（header-only 则无需）；带副作用（枚举 `SideEffect` 选值）用 `GDA_TOOL_CLASS_SIDE` 标记即自动进入遍历排除，无需手改 `tests/runner/traversal.cpp`

## 测试纪律

- L1（`gda_unit_tests`）禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃），只能测注册/分发/错误路径
- 子代理迭代：只写代码不编译，主代理统一 configure + build 再分批跑测试（并行编译会锁）
- 新增 `src/*.cpp` 同步更新 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`

## 能力边界（08-20 确立）

gda 聚焦「编辑器内的引擎操作」，以下能力**明确不做**，避免重复造轮子，交由外部编辑器/CLI 工具承担：

- **文本级增量编辑**：gda 不做文件内精确 diff/apply 式编辑；对场景/资源内容的语义级读写走 `scene_*`/`resource_*` 工具，纯文本查看/搜索用 `read_file`/`find_in_files`，深度编辑交给外部编辑器。
- **glob 风格文件匹配**：文件列举/目录遍历由 `get_resource_dir_files`、`get_editor_file_system_tree`、`find_in_files` 覆盖，不单独提供 glob 匹配工具。
- **C# 程序集热重载**：`build_csharp_assembly` 仅能触发 `dotnet build`（进程副作用，已入遍历排除清单）；GDExtension(C++) **无公共 API** 触发编辑器内 .NET 程序集重载（该能力位于引擎 `modules/mono` 内部）。因此 C# 类/签名变更后，编辑器内 `load()`/`get_script()`/校验渲染仍必须由用户在编辑器点 Build 或重启编辑器；gda 侧的 `code_execute`/GDScript 通道不受影响。

## 知识局限（文档提示）

- **C# 只读/私有 setter 属性对 GDScript 不可见**（Godot .NET 绑定既有行为，非 gda 缺陷）：`get("MaxHealth")` 对无 getter 的属性返回 null。AI 用 GDScript/`property_get` 校验 C# 派生值时易误判——应改为检查可读导出引用而非派生值。

## 其他

- CI/Release 工作流见 [build.md](./build.md)「CI 与 Release」；CI 仅跑 L1，L2 引擎用例仍仅本地执行（需 `GODOT_PATH`）
- 依赖版本固定：godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.3.2（FetchContent，无子模块）
