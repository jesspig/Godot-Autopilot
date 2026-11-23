# 固定覆盖任务书（不抽题）

本任务书是 `gda-e2e-loop` 每轮的唯一开发依据，替代 Game Jam 随机抽题。它定义一个**贯穿所有工具域的真实小项目**与一组**组合场景**。每轮在固定沙盒 `examples/e2e` 中按本任务书完成真实开发操作，目标是覆盖全部 252 个 canonical 工具。任务书本身固定不变；工具清单以当轮 `harness coverage` 导出的 252 名单为准。

## 项目：信号回路（Signal Circuit）—— 2D 实时解谜小样

一个可玩的小型 2D 游戏：玩家控制一个信号源，在限时内经过若干节点点亮回路，触发开门、通关；有陷阱导致失败与重玩。选用 2D 是为了自然触达场景、TileMap、2D 物理、动画、资源、音频等绝大多数域；3D 域按「不适用」处理并写明理由，除非某轮工具清单要求补 3D。

工程侧只改 `<project-dir>` 与构建产物；不得改 GDA 源码/技能/文档/测试。

## 强制功能单元（按序完成，每单元产出可指认的真实功能 + 证据）

每个单元列出「要做的真实事」与「应优先使用的工具类别」。工具名以 `gda search` / `gda describe` 实查为准，不凭记忆硬调。

| 单元 | 要做的真实事 | 优先工具类别 |
| --- | --- | --- |
| U1 项目与输入 | 设置主场景、窗口、输入动作（移动/交互/重玩），读回确认 | `editor_config`、`set_input_map_action`、`update_project_settings`、`get_project_settings` |
| U2 场景骨架 | 建根场景 + 玩家/节点/门/HUD 节点，设属性，存盘，读回结构 | `editor_scene_control`、`create_scene_node`、`property_set/get`、`save_editor_scene`、`get_scene_tree` |
| U3 资源与图像 | 代码生成像素/图形 → 导出 png/svg → 导入 → 挂到 Sprite/TextureRect → 截图确认 | `texture_gen`、`image_edit`、`image_save`、`resource_create/save/load`、`os_file_write`、`resource_import`、`resource_query` |
| U4 瓦片与 2D 物理 | 建 TileSet/TileMap 铺关卡地面与墙，给墙加碰撞，建立玩家刚体 | `create_tilemap`/`create_tilemap_tileset`/`set_tilemap_cells`/`update_tileset`、`create_physics_2d_body`、`physics_query` |
| U5 动画与 Tween | 节点闪烁/移动动画轨道，运行时用 Tween 做点亮反馈 | `create_animation`、`create_animation_track`、`animation_runtime`、`game_tween` |
| U6 音频 | 生成 wav/ogg → 导出 → 导入 → AudioPlayer 播放 + 总线配置 → 日志/读回确认 | `os_file_write`/`resource_import`、`audio_player`、`audio_bus`、`audio_device` |
| U7 脚本与配置 | 写玩法脚本（计时、碰撞点亮、胜负、重玩），挂载，配置脚本属性，重载 | `create_script`、`patch_script`、`attach_script_to_node`、`set_script_property`、`reload_game_scripts`、`get_script_property_list` |
| U8 运行时与输入 | 运行游戏，`game_input` 注入完整通关与失败流程，读回状态与场景树 | `editor_scene_control` action=play、`game_input`、`game_script`、`get_game_state`、`game_scene_tree` |
| U9 捕获与日志 | 截图验证画面，读日志确认无错误，检查 UI 控件 | `capture_editor_viewport`、`get_game_evidence`、`get_game_log_entries`、`get_game_ui` |
| U10 调试与系统 | 查工具、读调试信息、断点/监视、系统状态 | `search_tools`、`get_debug_info`、`get_debugger_stream`、`debugger_breakpoints`、`system_status`、`get_debug_info` |
| U11 渲染与服务器 | 用 RID 层 API 做 canvas/物理查询/导航等底层操作，核对 RID 有效性 | `render_*`、`physics_*`、`nav_*`、`astar_path`、`get_physics_*_space_direct_state`、`free_render_rid` |
| U12 批处理与脚本执行 | 用批量与脚本执行通道完成一组有依赖的编辑操作 | `batch_execute`、`code_execute`、`execute_script`、`call_script_node` |

> 各单元内部按「单环只做一个增量 → 基线 → 改动 → 同条件复测 → 三问」推进；每环更新 GOAL 与覆盖累计。

## 强制组合场景（组合测试，必做且记入报告）

每个场景把 2–4 个工具串成有开发意图的操作链，前置工具的输出/副作用作为后置工具的输入/条件，观察连锁反应。

- C1 全链路搭建：`create_scene_node` → `resource_create`/`resource_save` → `property_set`/`attach_script_to_node` → `editor_scene_control` action=play → `game_input` → `capture_editor_viewport`。验证从编辑态到运行态的状态是否贯通、截图是否反映注入输入。
- C2 写读一致性：`patch_script` → `reload_game_scripts` → `game_script`/`get_game_state` 读回；`update_project_settings` → `get_project_settings`；`property_set` → `property_get`。验证写入是否即时生效、读回是否一致。
- C3 资源副作用传播：`resource_file` action=rename/move → 原场景引用是否失效 → `resource_query` action=references 核对 → 用 `property_set` 修复引用 → 截图复验。验证重命名/移动对引用的连锁破坏。
- C4 节点删除/信号残留：`signal_connect` → `delete_scene_node` → `get_scene_tree`/`analyze_project` action=signal_flow 检查残留信号 → 读回确认。验证删除是否留下孤儿连接。
- C5 批处理事务性：`batch_execute` 内前序工具故意失败 → 观察 `stop_on_error` / `rollback_on_error` 对后续与已完成操作的影响 → 读回核对部分提交状态。验证部分失败时的连锁一致性。
- C6 资源重导入连锁：`image_save` 覆盖已有 png → `resource_import`/`resource_load` action=reload → 运行截图确认画面更新。验证重建资源后旧引用是否刷新。

每条组合链记录：工具序列、意图、预期连锁、实际连锁、证据、是否发现连锁缺陷。发现异常登记 Q（类型=缺陷），根因标「待分析」交阶段 2。

## 覆盖收口（每轮结束前）

1. 运行 `bun run gda-dev-tools harness coverage --round <轮目录>` 与 `harness digest --round <轮目录>`，按本轮起始时间过滤 CLI 轨迹。
2. 对照 252 名单列出未覆盖工具；对每个未覆盖项判断是否可由任务书的某个真实功能单元自然覆盖。
3. 可覆盖却未覆盖的，补做对应功能增量（不是裸调用）；确属模态/人工值守/无硬件/需特定场景的，逐项写明客观理由，reason 不充分记 `unmet`。
4. 三要素不齐、失败调用、跨 run 拼凑一律剔除；`suspected_gaming=true` 时本轮覆盖不进经验库。
5. 目标 `252/252`；未达标不得声称完成，GOAL `state=unmet` 并通知用户。
