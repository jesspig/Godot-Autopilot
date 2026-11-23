# 覆盖与组合测试协议

当统计覆盖、判定全部工具是否达标、设计组合链、处理反作弊或未达标时读取本文件。覆盖是端到端测试的核心验收目标，硬门槛为**覆盖全部 253 个 canonical 工具**。

## 覆盖口径与统计源

- 口径固定为 253 canonical 工具名单（`catalog_total=253`，由 `harness coverage` 导出）。本轮目标：`253/253`。
- 以 CLI 轨迹 `~/.godot-autopilot/cli-traces/trace-YYYYMMDD.jsonl` 为准，与 253 名单 join 后统计；插件轨迹有 20 文件 / 50MB 上限会漏早期数据，不作统计源。
- 按本轮真实起始时间过滤（`gda start` 返回的 `startedAt` 转毫秒）；不要用 `harness coverage` 的 `round_start_ms`（已知可能错到数天前）。
- 253 个工具中约 190 个是 create/get/set/apply 等细粒度 API。覆盖它们靠「用对应工具完成真实功能」，用 `assets/fixed-task-brief.md` 的 U1–U12 驱动，不靠硬塞。
- 禁止 GDScript 一把梭把本应由工具完成的工作内联到脚本；仅当工具确实不支持且有证据说明时才允许脚本内联，并在报告注明。

## 三要素与有效触碰（硬校验）

每个计入覆盖的工具必须同时具备：①服务于哪个功能；②真实操作步骤；③读回值或截图证据。缺任一判**无效触碰**并剔除；`missing required parameter` 等失败调用单列剔除。

反作弊一致性校验（触发即 `suspected_gaming=true`）：

- 证据无法指认到具体功能；
- 同一证据被重复用于多个无关功能；
- 功能清单与证据无法一一对应；
- 跨 run 拼凑数据：把不同次操作的读回值拼成一张「完整流程」表，或声称的连续流程实际由多次分段 run 组成。所有流程数据必须来自**同一次连续 run**，并给出唯一标识（起始时间戳 + `trace_id` 或 run 编号）。

`suspected_gaming=true` 的轮次覆盖不进经验库，报告写明依据，不自评「通过」。

两类「凑数」必须区分：为数字盲调无关工具、在 2D 游戏硬调 3D 物理工具，是禁止的「为测试而测试」，剔除并标无效触碰；而图像、音频、动画、资源、项目设置、细粒度 API 若被 GDScript 绕过，是**测试不充分**，导致缺口而不达标。

## 组合测试（连锁反应）

覆盖全部工具不能只靠孤立单点调用，必须把工具串成有开发意图的操作链，让前置工具的输出或副作用成为后置工具的输入/条件，观察连锁反应。强制场景见 `assets/fixed-task-brief.md`「强制组合场景」C1–C6，报告须有「组合链与连锁反应表」。

每条组合链记录：工具序列、意图、预期连锁、实际连锁、证据、是否发现连锁缺陷。一个工具的副作用意外破坏另一个工具的结果，登记 Q（类型=缺陷），根因标「待分析」交阶段 2。组合链同样受三要素与反作弊约束，禁止跨 run 拼凑。

## 分域必覆盖

20 个工具域（按 Godot 开发职能划分）在任务书 U1–U12 中均有对应功能单元；每域至少触达一个代表工具：

| 域 | 代表工具（至少触达其一） | 任务书单元 |
| --- | --- | --- |
| 场景与节点 | `editor_scene_control`、`create_scene_node`、`property_set`、`save_editor_scene` | U2 |
| 资源与图像 | `texture_gen`、`image_edit`、`image_save`、`resource_create/save/load`、`resource_import` | U3 |
| 音频 | `audio_player`、`audio_bus`、`audio_device`、`audio_capture` | U6 |
| 动画与 Tween | `game_tween`、`create_animation`、`create_animation_track`、`animation_runtime` | U5 |
| 2D 或 3D 核心 | 2D：`create_tilemap`、`create_physics_2d_body`、`physics_query`；3D：`create_physics_3d_body`、`apply_physics_3d_body_force` | U4 / U11 |
| 脚本 | `create_script`、`patch_script`、`attach_script_to_node`、`reload_game_scripts` | U7 |
| 项目与输入配置 | `set_input_map_action`、`update_project_settings`、`get_project_settings` | U1 |
| 运行时与输入 | `game_input`、`game_script`、`game_scene_tree`、`get_game_state` | U8 |
| 捕获与日志 | `capture_editor_viewport`、`get_game_evidence`、`get_game_log_entries` | U9 |
| 调试器与系统 | `search_tools`、`system_status`、`get_debug_info`、`execute_script` | U10 |
| 渲染与 RID 服务器 | `render_*`、`physics_*`、`nav_*`、`astar_path`、`free_render_rid` | U11 |
| 批处理与脚本执行 | `batch_execute`、`code_execute`、`execute_script` | U12 |

「不适用」须写明客观理由（如主题为 2D 则 3D 域不适用、无音频设备则 `audio_capture` 不适用），否则必须触达。

## 图像与音频的生成例外规则

允许用代码生成图像/音频内容，但必须走完整链路才算覆盖该域：

1. 生成内容 → 导出为 `*.png`/`*.svg`/`*.wav`/`*.ogg` 等文件（`image_save`、`os_file_write` 或对应工具）。
2. `resource_import` 导入，确认 `res://` 路径可 `resource_load`。
3. `property_set`（节点 `texture`/`stream`）或脚本 `preload`/`load` 引用；音频 `audio_player` 播放，图像显示在 Sprite2D/TextureRect/主题资源。
4. 读回验证：资源文件存在 + 节点属性指向该 `res://` 路径 + 运行截图/日志确认生效。

反例（不覆盖）：`_ready()` 里 `AudioStreamWAV.new()` 直接 `player.stream = 内存对象`，或 `ImageTexture.create_from_image()` 直接赋 `texture`。

## 未达标处理

收尾若仍有未覆盖工具：

- 先查是否因 GDScript 内联绕过——若是，补做工具化功能增量。
- 再查任务书 U1–U12 是否有可自然承载该工具的真实功能——若有，补做增量并复测。
- 确属模态/人工值守/无硬件/需特定场景，逐项写明客观理由；理由不充分的缺口不得标「不适用」。
- 仍不达标：GOAL `state=unmet`，报告如实写明缺口，不声称完成，通知用户决定后续。

## 报告固定口径行

报告固定写一行：`覆盖 <N>/253，有效 <M>，无效触碰 <K>，suspected_gaming=<是|否>（依据：...）`。目标 N=253。N 为本轮真实轨迹与 canonical join 后的触达数，M 为三要素通过数，K 为无效触碰数。

另写一行分域覆盖：`分域覆盖：场景=<达标|缺失|不适用>；资源图像=<...>；音频=<...>；动画=<...>；2D或3D=<...>；脚本=<...>；配置=<...>；运行时=<...>；捕获=<...>；调试=<...>；渲染RID=<...>；批处理=<...>`。
