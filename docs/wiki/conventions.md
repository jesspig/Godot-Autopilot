---
type: 工程约定
title: 工程约定
description: 命名、日志、错误模式、线程纪律与添加工具流程等全仓库编码约定
tags:
  - 约定
  - 命名
  - 规范
timestamp: "2026-08-17T01:03:27+08:00"
---

# 工程约定

本文汇总代码中实际体现的工程约定（与 [overview.md](overview.md)、[AGENTS.md](../../AGENTS.md) 互为参照）。

## 命名体系

- **命名空间**：`godot_autopilot`（工具子命名空间 `<模块>_ops`，如 `godot_autopilot::scene_ops`）
- **前缀**：核心常量 `GDA_*`（如 `GDA_DEFAULT_PORT`）；运行时协议 `gda:*`；导出宏 `GDA_EXPORT`
- **产物与目录**：CMake 目标 `godot-autopilot`，部署到 `Example/addons/godot-autopilot/`
- **工具命名**：约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（snake_case，**动词置首**），如 `create_scene_node`、`intersect_physics_2d_ray`、`set_input_map_action_deadzone`；332 个工具名全部动词置首，段数随粒度自然变化（2-6 段），`signal_connect`/`property_set` 等短名属规范内省略类别段
- **头文件 include guard**：`GODOT_AUTOPILOT_<MODULE>_HPP`

## 日志

- 类别（仅 5 个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`（`LogCategory` 枚举）
- 级别：`Debug`、`Info`、`Warning`、`Error`（`LogLevel` 枚举）
- `LogSystem` 单例：环形缓冲上限 10000 条，仅内存 + 回调（`set_on_new_entry` 供 `McpLogDock` 消费），**无文件输出**
- 禁止记录密钥与 PII

## 错误模式

- 领域工具失败返回 `{"error": "消息"}` JSON（可含 `error_detail` 扩展），`call_tool` 元工具检查 `error` 字段并设 `is_error = true`
- 命名契约：`get/fetch/load` 目标必存在否则抛异常；`find/search/query` 允许为空；`validate/check` 失败即抛；`apply/compute` 为纯函数

## 线程纪律

- **所有 Godot API 调用必须通过 `queue.submit()`**（HTTP 线程直接调用会崩溃）；例外：`debugger_prompts`/`debugger_resources` 只读捕获缓冲不经队列（安全）
- `CommandQueue`：header-only，`submit()` 返回 `std::future`（异常经 `set_exception` 传播），主线程 `_process()` 每帧 `drain()`
- 导出期间（`ExportGuard` 原子标记）`dispatch` 拒绝工具调用

## 添加工具流程

1. `<category>_ops.hpp` 声明 `mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`
2. `<category>_ops.cpp` 实现（返回 `{"error": ...}` 或结果对象）
3. `tool_defs.def` 加 `TOOL_ENTRY` 行（宏被 `register_all.cpp` include 两次，自动写入 `g_handlers` 与 `ToolCatalog`，**register_all.cpp 无需手改**）；SCHEMA_BASIC 时同步在 `schema_*_ops.cpp` fill 表加 `schema::build_schema({ParamDef...})` 条目
4. `CMakeLists.txt` `add_library()` 添加 `.cpp`；新工具若有副作用需同步加入 `tests/runner/traversal.cpp` 排除清单

## 测试纪律

- L1（`gda_unit_tests`）禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃），只能测注册/分发/错误路径
- 子代理迭代：只写代码不编译，主代理统一 configure + build 再分批跑测试（并行编译会锁）
- 新增 `src/*.cpp` 同步更新 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`

## 其他

- 无 CI（`.github/` 不存在）；构建、测试均本地执行
- 依赖版本固定：godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.3.1（FetchContent，无子模块）
