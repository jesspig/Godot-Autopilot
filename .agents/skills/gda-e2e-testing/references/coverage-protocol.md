# 工具覆盖协议

当统计工具覆盖、判定分域与总覆盖率是否达标、或处理反作弊与未达标时读取本文件。覆盖是端到端测试的核心验收目标，门槛为硬性，不可用「覆盖是自然结果」「不是 KPI」为不达标开脱。

## 覆盖口径与统计源

- 覆盖口径固定为 252 canonical 工具名单（`catalog_total=252`，由 `harness coverage` 导出）。两条硬门槛：分域必覆盖、总体覆盖率 ≥50%（有效覆盖工具数 / 252 ≥ 0.5）。
- 覆盖以 CLI 轨迹 `~/.godot-autopilot/cli-traces/trace-YYYYMMDD.jsonl` 为准，与 252 canonical 名单 join 后统计；插件轨迹有 20 文件 / 50MB 上限会漏早期数据，不作统计源。
- 统计必须按本轮真实起始时间过滤轨迹（`gda start` 返回的 `startedAt` 转成毫秒时间戳）；不要用 `harness coverage` 的 `round_start_ms`（已知可能错到数天前）。
- 252 个工具中约 190 个是 create/get/set/apply 等细粒度 API。覆盖它们靠「用对应工具完成真实功能」，不靠硬塞：创建物理实体用 `create_physics_*` 而非 GDScript 构造；配资源用 `resource_create`/`resource_save` 落盘再 `resource_load` 引用；设属性用 `property_set`/`update_project_settings`；做动画用 `create_animation`/`game_tween` 而非手写 `lerp`。
- 禁止用「GDScript 一把梭」把本应由工具完成的工作全部内联到脚本——既降低覆盖，也回避测试工具可靠性的目的。仅当工具确实不支持且有证据说明时才允许脚本内联，并在报告注明。

## 分域必覆盖表

「域」按 Godot 开发职能划分（不同于 `coverage.json` 按 create/get/set 动词前缀的机械分组）。「适用」指该主题的游戏通常需要该域；若主题确实不适用，须在报告写明理由并标注不适用，否则必须触达该域的代表工具。

| 域 | 代表工具（至少触达其一，鼓励多个） | 不适用判定示例 |
| --- | --- | --- |
| 场景与节点 | `editor_scene_control`、`create_scene_node`、`property_set`、`save_editor_scene` | 几乎总是适用 |
| 资源与图像 | `texture_gen`、`image_edit`、`image_save`、`resource_create`、`resource_save`、`resource_load` | 无视觉资源的纯音频游戏 |
| 音频 | `audio_player`、`audio_bus`、`audio_device`、`audio_capture` | 无声音的静音谜题 |
| 动画与 Tween | `game_tween`、`create_animation`、`create_animation_track`、`animation_runtime` | 完全静态画面 |
| 2D 或 3D 核心（按主题二选一） | 2D：`create_tilemap`、`create_tilemap_tileset`、`set_tilemap_cells`、`create_physics_2d_body`；3D：`create_physics_3d_body`、`apply_physics_3d_body_force`、`create_nav_3d_map` | 选一域即可；另一域不适用须说明主题为 2D/3D |
| 脚本 | `create_script`、`patch_script`、`attach_script_to_node`、`reload_game_scripts` | 几乎总是适用 |
| 项目与输入配置 | `set_input_map_action`、`update_project_settings`、`get_project_settings`、`editor_config` | 几乎总是适用 |
| 运行时与输入 | `game_input`、`game_script`、`game_scene_tree`、`get_game_state` | 几乎总是适用 |
| 捕获与日志 | `capture_editor_viewport`、`get_game_evidence`、`get_game_log_entries` | 几乎总是适用 |
| 调试器与系统 | `search_tools`、`system_status`、`get_debug_info`、`execute_script` | 几乎总是适用 |

## 图像与音频的生成例外规则

允许用代码或工具生成图像/音频的**内容**（像素、PCM 采样），但必须把结果写成资源文件并走完整链路，才算覆盖该域：

1. 生成内容 → 导出为 `*.png` / `*.svg` / `*.wav` / `*.ogg` 等文件（用 `image_save`、`os_file_write` 或对应工具）。
2. 通过 `resource_import` 导入为引擎资源，确认 `res://` 路径可 `resource_load`。
3. 通过 `property_set`（场景节点的 `texture` / `stream` 属性）或脚本 `preload` / `load` 引用该资源；音频用 `audio_player` 播放，图像显示在 `Sprite2D` / `TextureRect` / 主题资源上。
4. 读回验证：资源文件存在 + 节点属性指向该 `res://` 路径 + 运行截图/日志确认生效。

反例（不覆盖该域）：在 `_ready()` 里 `AudioStreamWAV.new()` 生成采样后直接 `player.stream = 内存对象`，或 `ImageTexture.create_from_image()` 直接赋给 `texture`。这类内存态用法绕过了导出、导入与引用，必须改为资源文件流程。

围绕 `GAMEJAM.md` 的概念灵感选择跨多域的主题，让覆盖面从真实需求中长出。若当前主题只能触达少数域导致总覆盖 <50%，记为不合格并按「未达标处理」办理，不得用无关工具硬凑。

## 证据三要素与反作弊（硬校验）

每个计入覆盖的工具必须同时具备：①服务于哪个玩法功能；②真实操作步骤；③读回值或截图证据。缺任一判**无效触碰**并剔除；`missing required parameter` 等失败调用同样单列剔除。

证据一致性硬校验（触发即 `suspected_gaming=true`）：

- 证据无法指认到具体玩法功能；
- 同一证据被重复用于多个无关功能；
- 玩法功能清单与证据无法一一对应；
- 跨 run 拼凑数据：把不同次通关的读回值拼成一张「完整流程」表，或声称的连续流程实际由多次分段 run 组成。报告中所有流程数据必须来自**同一次连续 run**，并给出该 run 的唯一标识（起始时间戳 + `trace_id` 或 run 编号）。

`suspected_gaming=true` 的轮次覆盖不进经验库，报告中写明依据，不自评「通过」。

两类「凑数」必须区分：在 2D 游戏里硬调 3D 物理工具、为数字盲调无关工具，是禁止的「为测试而测试」，剔除覆盖并标无效触碰；而图像、音频、动画、资源、项目设置这些真实开发必用工具若被绕过，是**测试不充分**，导致分域缺失而不达标。

## 未达标处理

收尾时若分域缺失或总覆盖 <50%：

- 先检查是否因 GDScript 内联绕过工具——若是，补做对应的工具化开发增量（如把代码生成的贴图改为 `texture_gen` 生成并落盘）。
- 若主题确实不适用某些域，在报告写明理由并标注不适用，其余适用域必须全部达标。
- 仍不达标：GOAL `state` 记 `unmet`，报告如实写明覆盖缺口，不声称完成，通知用户决定后续。

## 报告固定口径行

报告固定写一行口径：`覆盖 N/252，有效 M，无效触碰 K，达标=是/否，suspected_gaming=是/否及依据`。N 为本轮真实轨迹与 canonical join 后的触达数，M 为三要素通过数，K 为无效触碰数。另写一行分域覆盖：`分域覆盖：场景/资源图像/音频/动画/2D或3D/脚本/配置/运行时/捕获/调试 = 各域 达标|缺失|不适用`。
