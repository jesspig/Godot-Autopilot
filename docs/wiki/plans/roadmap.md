---
type: 路线图文档
title: 竞品对齐路线图
description: 面向主流 Godot MCP/AI 插件的竞品定位速览与本仓 P0-P3 批次交付状态
tags:
  - 规划
  - 竞品对齐
  - 工具扩展
timestamp: "2026-09-16T17:06:25+08:00"
resource: src/
---

# 竞品对齐路线图

> 记录 2026-08 功能分支 `feature/engine-aware-fs-and-tool-expansion` 的竞品对齐批次（P0/P1/P2 已交付；当前 ctest 共 292 项——L1 267 + L2 25，见 [测试体系](../tests.md)），与后续 P3 方向。工具侧实现细节见 [工具注册表](../modules/tools_registry.md)，运行时协议见 [入口与运行时](../modules/entry_runtime.md)。

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

- **新增** `move_resource_file` / `create_directory`（Resources 类 22→24；后续 09-13 批次增至 26）：目录级递归移动保布局，`.uid` sidecar 随行
- **`remove_resource_file` 两段式**：默认 dry-run 返回影响预检；`force=true` 才删，优先 OS 回收站（trashed/permanent 标注）
- **rename/move 补全 remap**：project.godot 的 main_scene/autoload 命中自动改写（保留 `*` 前缀），依赖 `.tscn/.tres` 内 path=/uid= token 重写，残留引用以结构化 `stale_references: {file, token}` 回传
- **`write_file` 引擎感知化**：res:// 内写入自动判定 reimport/update_file 并附脚本写入诊断（布尔级）；user:// 维持裸写并标 `engine_managed:false`

## 已交付：P1 AI 易用性

- **错误水印 doorbell**：所有 MCP 响应顶层附 `new_errors_since_last_call`（一次性消费）；编辑器侧计 error 结果、游戏侧计 runtime_error（`src/core/error_watermark.hpp`）
- **undo 接入**：create/delete/rename/reparent 场景节点全走 `EditorUndoRedoManager`（响应带 `undoable` 字段）；`property_set` 可撤销（Object 型值降级 `undoable:false`）；`batch_execute` 新增 `rollback_on_error`（按 EditorUndoRedo version 差值整批撤销）
- **readiness 门控**（`src/core/editor_readiness.{hpp,cpp}`）：导入/扫描进行中时 reimport 类调用返回 `retryable:true` 软错误（retry_after_ms=500），scan 幂等跳过
- **`code_execute` timeout 上限 30000ms**（越界值直接报错而非静默接受；此前 schema 声称上限但实现未校验）
- **BM25 中文检索**：分词器支持 CJK bigram（`src/util/bm25_index.cpp`），`search_tools` 中文查询可用
- **`capture_editor_viewport` target='game' 接通**：08-24 初始接通；09-13 下午改为非阻塞 pending 协议——返回 `{"__gda_pending": id, "timeout_ms": ...}`，由 `call_tool` 等待并经 `finalize_capture_response` 定型（阻塞式 `runtime_ops::game_capture_blocking` 已删除），`timeout_ms` 参数保留（默认 5000、钳制 30000）

## 已交付：P2 覆盖扩展

领域工具 336→**363**（本批净 +27，类别 23→27；后续 09-13 批次新增至 366，见[工具注册表](../modules/tools_registry.md)）：

- **Animation 域新设 10**：create_scene_animation_player / create_animation / remove_animation / get_animation_list / create_animation_track / insert_animation_keyframe / remove_animation_track / create_scene_animation_tree / add_animation_machine_state / connect_animation_states
- **Theme 域新设 8**：create_theme_resource / set_theme_color / set_theme_constant / set_theme_font_size / set_theme_stylebox_flat / get_theme_info / apply_theme_to_control / set_control_anchor_preset
- **Testing 域新设 2**：run_gdscript_tests / run_gdscript_test_files（内置断言框架 check/check_equal/check_almost_equal/fail/fatal）
- **Analysis 域新设 3**：validate_scene_file / find_unused_resources / trace_signal_flow
- **Scene 域 +2**：rename_scene_node / reparent_node；`create_scene_node` 支持 `properties` 参数
- **Game 域 +2**：sequence_game_inputs（逐物理帧时间线注入）/ get_game_ui_elements（运行中 Control 树枚举）
- **删除 2 死工具**：get_debugger_stack_dump / get_debugger_monitors（ScriptDebugger 未暴露给 GDExtension，数据源不可达）

## 已交付：Agent Skills 生成

- **skill_gen 一键生成 Agent Skills**（2026-09-08 交付首版 7 册，09-13 扩至 8 册；生成器 + UI 按钮 + 7 用例 L1）：写入项目根 `.agents/skills/`，覆盖写仅限自有命名空间

## 已交付：Computer Use grounding 增强（09-15）

领域工具 366→**379**（净 +13；Editor 23→29、Input 11→15、Display 24→25、Scene 6→7、Game 9→10），MCP 可达 386 / catalog-index 387：

- **编辑器 UI 语义自动化**：`get_editor_ui_elements`（元素枚举：语义 path/类型/文本/客户区矩形）与 `hit_test_editor_point`（客户区点命中链）负责定位，`click_editor_element` / `type_editor_element_text` / `run_editor_shortcut` 执行动作；原始坐标回退 `click_input_mouse` / `scroll_input_mouse` / `drag_input_mouse` / `type_input_text`（合成点击/滚轮/拖拽/文本）
- **坐标映射**：`get_editor_viewport_geometry`（截图像素↔客户区 `window = offset + image * scale`）、`get_scene_node_screen_rect`（编辑场景节点→客户区，2D/3D）、`get_display_window_rect`（窗口客户区矩形与屏幕几何）
- **截图增强**：`capture_editor_viewport` 新增 `region` / `max_dimension` / `space` / `annotate` / `diff_against_last`（与上次编辑器截图对比；尺寸不同按公共左上区域比较并标 `size_mismatch`；不可比较时 `reason` 为 `no_baseline`/`unsupported_format`/`baseline_too_large`）；游戏侧 `capture_game_viewport` 的 region/max_dimension/annotate 在游戏进程内执行
- **游戏侧输入与点击**：`queue_game_input` / `sequence_game_inputs` 增 wheel 与单步 mouse_motion；`click_game_ui_element` 按节点 path 在游戏进程内完成枚举 + 坐标注入（单次协议往返）
- **安全分级**：新增 8 个副作用工具全部标记（编辑器侧 7 个 `ModifiesWindow`、游戏侧 1 个 `GameRuntime`），未新增授权能力门；详见 [T0 安全边界与并发契约](../security_contract.md)

## 已交付：失败修复批次（09-16，feature/failure-remediation）

领域工具 379→**384**（净 +5；Editor 29→31、TileMap 7→8、Game 10→12），MCP 可达 391 / catalog-index 392；L1 172→229、L2 11→17 份、ctest 183→246。行为变更（eval 编译错误结构化返回、game 工具超时预算链、batch_execute await_async、capture after_frames/when/scale、内联子资源、scene_path 回显、godot://skills 资源等 8 项）详见 [changelog/2026-09-16-log.md](../changelog/2026-09-16-log.md)：

- **编辑器场景树**：`scene_tree_items`（Scene dock 行枚举）/ `select_scene_tree_node`（按 path 选中并联动 Inspector）
- **瓦片地图**：`fill_tilemap_rect`（矩形铺砖/擦除，>100000 格报错、单次 undo）
- **长任务异步**：`start_game_job` / `get_game_job`（游戏脚本异步提交+轮询，job 表上限 16、过期宽限 2s，需 `game_runtime` 授权）
- **MCP 资源**：新增 `godot://skills` 目录/单册/单文件资源组（技能经协议层可发现）

## 遗留：P3 待规划

- **客户端一键配置扩展**：配置面板现支持 8 客户端（见[支撑模块](../modules/support.md)），对标 godot-ai 的 attach 桥与"Configure all"全家桶仍有差距（stdio bridge、状态点探测、更多客户端预设）
- **AssetLib 上架**：当前经 GitHub Release zip 分发（`uv run build.py --package`），未进 Godot Asset Library

## 出站链接

- [工具注册表](../modules/tools_registry.md)（类别分布与数值核算总表）
- [支撑模块](../modules/support.md)（客户端配置生成现状）
- [入口与运行时桥接](../modules/entry_runtime.md)（input_sequence/ui_elements 协议 op）
