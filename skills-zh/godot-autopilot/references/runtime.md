# 运行时与调试

启动游戏、游戏运行时通道、输入注入、截图、UI 侦察、日志、错误水位线与内联测试。

## 启动与停止

- `play_editor_current_scene` 启动当前编辑场景。启动前先保存，否则可能失败。返回 `ok` 或 `already_playing` 与 `is_playing`；它不会重启已运行的游戏，需要全新运行时先 `stop_editor_playing`。
- `stop_editor_playing` 回到编辑模式；即使没有运行游戏也可安全调用。

## 游戏通道前提

所有 `game_*` 工具经编辑器调试会话到达运行中的游戏。游戏必须从编辑器启动，且游戏项目加载 GDA 扩展。没有附加游戏时返回 `game not running`。

已附加但未就绪时，默认 `wait_ready: true` 会排队并在游戏报告 `gda:ready` 后重放；`wait_ready: false` 立即失败。若 `gda:ready` 一直不到，排队调用会超时。已附加游戏且 `wait_ready: false` 持续失败，说明游戏项目未加载扩展。

游戏调用等待 `timeout_ms` 加 2000 ms，上限 25000；长任务拆成较短调用并轮询 `get_game_status`。

## 存活检查

`get_game_status` 报告引擎版本、当前场景、节点数、FPS、`paused`、`physics_frame`、`healthy` 与 `physics_stalled`。

- `healthy` 表示最近 3000 ms 内有活动；`last_activity_ms` 是间隔。
- `physics_stalled` 表示 FPS 为正但 `physics_frame` 不前进。
- `physics_frame` 是引擎全局计数，暂停时仍前进；要证明玩法冻结，必须结合 `paused` 与玩法自有值（节点位置、玩法 tick 或探针）。
- 场景重载失败表现为空场景与 `node_count` 骤降；停止并重跑，不要调试半死进程。

## 编辑器与游戏是两个进程

编辑器输入工具注入编辑器进程，运行中的游戏收不到；编辑器脚本也碰不到游戏的 `Input` 或 `SceneTree`。用 `queue_game_input`、`wait_game_input`、`sequence_game_inputs` 或 `execute_game_script` 驱动游戏。

## 输入注入

- `queue_game_input` 发送一次输入。`type` 为 `key`、`mouse_button` 或 `action`；`mode` 为 `event`（解析事件）、`api`（直接动作状态）或 `hold`（按住 `duration_ms`）。未知参数报错；`api` 不创建 `InputEvent`，输入回调看不到，UI 必须用 `event`。
- `wait_game_input` 等待物理帧中的 `just_pressed`（默认）、`just_released` 或 `pressed`，上限 `timeout_ms`（默认 2000，最大 25000）。可选 `inject` 先发送输入；瞬态状态必须带它。
- `sequence_game_inputs` 按精确物理帧偏移触发时间线（最多 256 项，每项含 `kind` 与 `at_frame`），用于 playtest 自动化。

时序事实：

- `just_pressed` 有 process 与 physics 两套计数。瞬态状态在 `_physics_process` 轮询，不在 `_process`；物理上下文最早下一 tick 才一致。
- 解析事件在显示服务器事件阶段末尾刷新，因此在下一帧开头派发。不要假设注入后立即读取会落在同一帧。

帧精确配方：用一条带显式 `at_frame` 的 `sequence_game_inputs` 时间线，再做条件截图。

## 截图

- `capture_game_viewport` 截取运行中的游戏；`timeout_ms` 默认 5000（最大 25000）。
- `capture_editor_viewport` 默认截编辑器视口，也接受 `target: game`。`save=true` 或 `save_dir` 写入 GDA 截图目录或项目内 `res://` 目录，其它路径被拒绝；最多保留最近 20 张。
- `scale`（1-8）用最近邻放大读取小字 HUD；顺序是区域裁剪、放大、`max_dimension`。
- `after_frames` 等待 N 个渲染帧。`when` 是每帧求值的 GDScript 表达式，首次为真时截图；`when_satisfied_frame` 早于 `captured_frame`。
- `when` 以当前场景为基准；表达式格式错误立即失败，运行时错误附在 `when_timeout` 上。文本条件必须同时检查可见性，隐藏标签仍保留文本。
- 截图每边上限 4096 像素、PNG 上限 8 MiB；base64 超过 4 MiB JSON 响应上限会被拒绝。

截图配合 `get_game_ui_elements`：先枚举控件，再注入点击，然后重新截图与枚举，因为固定坐标下的界面会变化。

## UI 侦察

`get_game_ui_elements` 返回游戏内每个 Control 的 `path`、`type`、`visible`、`text`、`global_rect`、`mouse_filter` 与 `z_index`。`mouse_filter` 与 `z_index` 是命中与绘制顺序事实；全屏 `PASS` 控件会吞点击，`IGNORE` 则放行。`max_elements` 默认 100（最大 1000）；`truncated` 表示还有更多。

`validate_game_ui_layout` 报告零尺寸、移出视口或可能被遮挡的控件。UI 验收先校验，再用 `click_game_ui_element` 点击并检查 `hit_path`；`hit_mismatch` 只在 `strict_hit: true` 时失败。

## 暂停、重载与游戏内代码

- `set_scene_tree_pause` 暂停或恢复；`is_scene_tree_paused` 确认。process mode 仍决定哪些节点运行，暂停还会停用物理服务器；瞬态输入状态在暂停期间丢失。
- `reload_scene_tree_current_scene` 从磁盘重载运行场景，脚本状态、连接与运行时修改都重置；没有运行场景时返回 `ERR_UNCONFIGURED`。
- `execute_game_script` 在游戏内运行。`script` 接受定义 `func _run()` 的完整脚本或同样包装的裸语句；值需要显式 return。`get_property`、`set_property` 与 `call_method` 读写节点状态。`persist` 把临时节点保留在 `/root/__gda_runtime` 下。

## 日志与错误路径

| 路径 | 工具 | 来源 | 需要游戏 |
|---|---|---|---|
| 编辑器引擎日志 | `get_debugger_log` | 编辑器引擎日志缓冲 | 否 |
| 调试会话 | `get_debugger_errors`、`get_debugger_output`、`get_debugger_scene_tree` | 运行时通道上的运行游戏 | live 数据需要 |
| 磁盘游戏日志 | `get_game_log_entries` | `user://logs/godot.log` 尾部 | 日志文件必须存在 |
| 插件日志 | `get_plugin_log` | 插件诊断：确认拒绝、超时、丢失响应 | 否 |

- `get_debugger_log`（`limit` 默认 50）读编辑器脚本错误与 print 输出；编辑任何 `.gd`、`.tscn`、`.tres` 或 `.cs` 后要读。
- `get_plugin_log`（`limit` 默认 100，最大 1000）读插件诊断，不是引擎日志。
- `get_debugger_errors` 返回结构化条目（`time`、`file`、`func`、`line`、`error`、`descr`、`stack`）；`get_debugger_output` 读捕获的 stdout/stderr；`get_debugger_scene_tree` 返回文本树。无会话时返回空结果与提示。
- `get_game_log_entries` 读磁盘尾部（`limit` 默认 50，最大 500），并回退到 `godot.log.1`。它绕过调试传输的丢弃门，是错误风暴的可靠路径。

## 错误水位线

每次工具响应都带 `new_errors_since_last_call`，统计自上次调用以来的错误，包括本次调用的错误。它是一次性的。

1. 用 `get_debugger_errors` 或 `get_game_log_entries` 复现并读取错误。
2. 修复、重跑并走一遍故障路径。
3. 做任意下一次调用并读水位线：0 表示没有新错误；非零先读再继续。

零不证明什么都没发生。调试传输会丢弃连接前的消息、最多一秒的错误风暴、以及队列溢出时的消息。错误风暴路径要用 `get_game_log_entries` 交叉核对。

## 内联 GDScript 测试

`run_gdscript_tests` 用内置断言框架运行 `{name, source}` 用例。源码是顺序语句（不允许顶层 `func`、`static`、`class`），被包进临时 `@tool` 节点串行执行后删除。`SceneRoot` 是被编辑场景根。

断言：`check`、`check_equal`、`check_almost_equal`、`fail`、`fatal`。失败断言记录一行并继续；`fatal` 终止该用例。`timeout_ms` 预算（默认 10000，最大 30000）耗尽后剩余用例标记 `skip`。`run_gdscript_test_files` 对 `directory`（默认 `res://tests`，非递归）按 `pattern`（默认 `*.gda_test.gd`）运行同一框架。响应含 `summary`（`total`、`passed`、`failed`、`skipped`）与 `cases`（`status` 为 `pass`、`fail`、`skip`）。

## 调试辅助

- `get_debug_monitor_catalog` 是权威 monitor id 列表；`get_debug_monitor` 读一个；`get_debug_monitors` 快照全部。
- `sample_game_property` 跨帧采样游戏属性。`get_debug_object_count` 与 `get_debug_node_count` 暴露泄漏；`get_debug_memory_usage` 是静态引擎内存。
- 断点：`get_debugger_session_info` 报告 breaked 会话。插件默认每会话自动继续最多 3 次，除非 `GDA_AUTO_CONTINUE` 为 `0` 或 `false`；被阻塞的游戏调用会超时，而不是返回陈旧状态。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| `game not running` | 游戏不是从编辑器启动 | `play_editor_current_scene` |
| 启动后游戏调用超时 | `gda:ready` 一直不到 | 检查游戏项目是否加载扩展 |
| 没有调试器错误 | 无会话或传输丢弃 | 读 `get_game_log_entries` |
