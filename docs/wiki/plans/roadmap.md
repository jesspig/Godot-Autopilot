---
type: 路线图文档
title: 竞品对齐路线图
description: 面向主流 Godot MCP/AI 插件的竞品定位速览与本仓 P0-P3 批次交付状态
tags:
  - 规划
  - 竞品对齐
  - 工具扩展
timestamp: "2026-08-24T04:57:00+08:00"
---

# 竞品对齐路线图

> 记录 2026-08 功能分支 `feature/engine-aware-fs-and-tool-expansion` 的竞品对齐批次（P0/P1/P2 已交付，83 个 ctest 全绿），与后续 P3 方向。工具侧实现细节见 [工具注册表](../modules/tools_registry.md)，运行时协议见 [入口与运行时](../modules/entry_runtime.md)。

## 竞品全景（一行定位速记）

| 项目 | 形态 | 一行定位 |
|---|---|---|
| [hi-godot/godot-ai](https://github.com/hi-godot/godot-ai) | Python FastMCP(HTTP) + WebSocket → 编辑器插件 | 约 43 工具/120+ op，主打客户端一键 attach 自动配置与 AssetLib/Snap 分发 |
| Godot MCP Pro | 商业 MCP 服务器 | 宣称 162 工具/23 类别，开箱即用面最广的付费方案 |
| GodotPilot | 社区开源 MCP（Node.js） | 以自然语言驱动编辑器启动/运行、场景节点与 GDScript 增删改查 |
| [Coding-Solo/godot-mcp](https://github.com/Coding-Solo/godot-mcp) | stdio MCP 服务器（npx 分发） | 社区使用最广的轻量方案：launch/run/调试输出捕获 + 打包 GDScript 操作脚本 |
| [IvanMurzak/Godot-MCP](https://github.com/IvanMurzak/Godot-MCP) | C# 编辑器插件 + 共享 MCP 服务端 | Unity-MCP 同源栈：42 工具/12 族，云托管（ai-game.dev）或自托管 GameDev-MCP-Server |

> 定位以各仓库 README 自述为准，功能数字随对方版本演进，仅作对齐参照不构成评测结论。

## 已交付：P0 文件操作收编

资源文件 CRUD 从"裸磁盘写"收编为引擎感知事务（`resource_ops.cpp`）：

- **新增** `move_resource_file` / `create_directory`（Resources 类 22→24）：目录级递归移动保布局，`.uid` sidecar 随行
- **`remove_resource_file` 两段式**：默认 dry-run 返回影响预检；`force=true` 才删，优先 OS 回收站（trashed/permanent 标注）
- **rename/move 补全 remap**：project.godot 的 main_scene/autoload 命中自动改写（保留 `*` 前缀），依赖 `.tscn/.tres` 内 path=/uid= token 重写，残留引用以结构化 `stale_references: {file, token}` 回传
- **`write_file` 引擎感知化**：res:// 内写入自动判定 reimport/update_file 并附脚本写入诊断（布尔级）；user:// 维持裸写并标 `engine_managed:false`

## 已交付：P1 AI 易用性

- **错误水印 doorbell**：所有 MCP 响应顶层附 `new_errors_since_last_call`（一次性消费）；编辑器侧计 error 结果、游戏侧计 runtime_error（`src/core/error_watermark.hpp`）
- **undo 接入**：create/delete/rename/reparent 场景节点全走 `EditorUndoRedoManager`（响应带 `undoable` 字段）；`property_set` 可撤销（Object 型值降级 `undoable:false`）；`batch_execute` 新增 `rollback_on_error`（按 EditorUndoRedo version 差值整批撤销）
- **readiness 门控**（`src/core/editor_readiness.{hpp,cpp}`）：导入/扫描进行中时 reimport 类调用返回 `retryable:true` 软错误（retry_after_ms=500），scan 幂等跳过
- **`code_execute` timeout 钳制 30000ms**（此前 schema 声称上限但实现未钳制）
- **BM25 中文检索**：分词器支持 CJK bigram（`src/util/bm25_index.cpp`），`search_tools` 中文查询可用
- **`capture_editor_viewport` target='game' 接通**：委托 `runtime_ops::game_capture_blocking`，新增 `timeout_ms` 参数

## 已交付：P2 覆盖扩展

领域工具 336→**363**（净 +27，类别 23→27）：

- **Animation 域新设 10**：create_scene_animation_player / create_animation / remove_animation / get_animation_list / create_animation_track / insert_animation_keyframe / remove_animation_track / create_scene_animation_tree / add_animation_machine_state / connect_animation_states
- **Theme 域新设 8**：create_theme_resource / set_theme_color / set_theme_constant / set_theme_font_size / set_theme_stylebox_flat / get_theme_info / apply_theme_to_control / set_control_anchor_preset
- **Testing 域新设 2**：run_gdscript_tests / run_gdscript_test_files（内置断言框架 check/check_equal/check_almost_equal/fail/fatal）
- **Analysis 域新设 3**：validate_scene_file / find_unused_resources / trace_signal_flow
- **Scene 域 +2**：rename_scene_node / reparent_node；`create_scene_node` 支持 `properties` 参数
- **Game 域 +2**：sequence_game_inputs（逐物理帧时间线注入）/ get_game_ui_elements（运行中 Control 树枚举）
- **删除 2 死工具**：get_debugger_stack_dump / get_debugger_monitors（ScriptDebugger 未暴露给 GDExtension，数据源不可达）

## 遗留：P3 待规划

- **客户端一键配置扩展**：配置面板现支持 8 客户端（见[支撑模块](../modules/support.md)），对标 godot-ai 的 attach 桥与"Configure all"全家桶仍有差距（stdio bridge、状态点探测、更多客户端预设）
- **AssetLib 上架**：当前经 GitHub Release zip 分发（`uv run build.py --package`），未进 Godot Asset Library

## 出站链接

- [工具注册表](../modules/tools_registry.md)（类别分布与数值核算总表）
- [支撑模块](../modules/support.md)（客户端配置生成现状）
- [入口与运行时桥接](../modules/entry_runtime.md)（input_sequence/ui_elements 协议 op）
