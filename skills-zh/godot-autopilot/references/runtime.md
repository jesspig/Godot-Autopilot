# 运行时与调试

启动游戏、游戏运行时通道、输入注入、截图、UI 侦察、日志、错误水位线与内联测试。

## 启动与停止

- `editor_scene_control` 的 action=play 启动当前编辑场景。启动前先保存，否则可能失败。返回 `ok` 或 `already_playing` 与 `is_playing`；它不会重启已运行的游戏，需要全新运行时先调 action=stop。
- action=stop 回到编辑模式；即使没有运行游戏也可安全调用。

## 游戏通道前提

所有 game 域工具经编辑器调试会话到达运行中的游戏。游戏必须从编辑器启动，且游戏项目加载 GDA 扩展。没有附加游戏时返回 `game not running`。会改变状态的 game 工具属于 GameRuntime，需要 `confirm: true`；确认错误是停下审查的信号，不是通道故障。

已附加但未就绪时，默认 `wait_ready: true` 会排队，并在游戏报告带 `scene_ready`（当前场景已入树）的 `gda:ready` 后重放；`wait_ready: false` 立即失败。就绪一直不到时排队调用超时（`game_ready_timeout`）。force restart 后先等场景就绪再首调。已附加游戏且 `wait_ready: false` 持续失败，说明游戏项目未加载扩展。

游戏调用等待 `timeout_ms` 加 2000 ms，上限 25000；长任务拆成较短调用并轮询 `get_game_state` 的 action=status。

## 存活检查

`get_game_state` 的 action=status 报告引擎版本、当前场景、节点数、FPS、`paused`、`physics_frame`、`healthy` 与 `physics_stalled`。

- `healthy` 表示最近 3000 ms 内有活动；`last_activity_ms` 是间隔。
- `physics_stalled` 表示 FPS 为正但 `physics_frame` 不前进。
- `physics_frame` 是引擎全局计数，暂停时仍前进；要证明玩法冻结，必须结合 `paused` 与玩法自有值（节点位置、玩法 tick 或探针）。
- 场景重载失败表现为空场景与 `node_count` 骤降；停止并重跑，不要调试半死进程。

## 编辑器与游戏是两个进程

编辑器输入工具注入编辑器进程，运行中的游戏收不到；编辑器脚本也碰不到游戏的 `Input` 或 `SceneTree`。用 `game_input` 的 action=queue、action=wait、action=sequence，或 `game_script` 的 action=execute 驱动游戏。

## 输入注入

- `game_input` 的 action=queue 发送一次输入。`type` 为 `key`、`mouse_button`、`wheel`、`mouse_motion` 或 `action`；`mode` 为 `event`（解析事件）、`api`（直接动作状态）或 `hold`（每物理帧重按，`duration_ms` 到期自动释放）。未知参数报错；`api` 不创建 `InputEvent`，输入回调看不到，UI 必须用 `event`。`event` 配 `duration_ms=0` 只按下不释放。`click` 在两个物理帧后自动释放，`double_click` 再加一对按下/释放。
- action=wait 等待物理帧中的 `just_pressed`（默认）、`just_released` 或 `pressed`，上限 `timeout_ms`（默认 5000，最大 25000）。可选 `inject` 先发送输入；瞬态状态必须带它。
- action=sequence 按精确物理帧偏移触发时间线（最多 256 项，每项自带 `at_frame` 偏移），用于 playtest 自动化。

时序事实：

- `just_pressed` 有 process 与 physics 两套计数。瞬态状态在 `_physics_process` 轮询，不在 `_process`；物理上下文最早下一 tick 才一致。
- 解析事件在显示服务器事件阶段末尾刷新，因此在下一帧开头派发。不要假设注入后立即读取会落在同一帧。
- 瞬态键态的读回必须落在 hold 窗口内（注入到读回的间隔短于 `duration_ms`）；自动释放后 `pressed` 恢复 false，而持久效果（如累积位移）仍在。读回要配对玩法自有计数。
- 注入经 `Input.parse_input_event`，不会到达 `_unhandled_input`；改在 `_process` 轮询 `Input.is_action_pressed`。

帧精确配方：用一条带显式物理帧偏移的 `game_input` action=sequence 时间线，再做条件截图。

## 截图

- `capture_editor_viewport` 默认截编辑器视口；传 `target: "game"` 截运行中的游戏。game 目标在游戏进程内写 PNG 并返回路径；该目标的 `timeout_ms` 默认 5000（最大 25000）。
- 编辑器 2D 视口退化（渲染图像任一边 ≤8 px 或可见矩形为零）时回退 3D；两者都不可用则报 `no_scene_open`（没有打开场景）或 `viewport_degenerate` 并带尺寸诊断——不再静默返回占位图。
- 编辑器目标拒绝 `after_frames` 与 `when`。
- `save=true` 或 `save_dir` 写入 GDA 截图目录或项目内 `res://` 目录，其它路径被拒绝；最多保留最近 20 张。
- `scale`（1-8）用最近邻放大读取小字 HUD；顺序是区域裁剪、放大、`max_dimension`。
- `after_frames` 等待 N 个渲染帧。`when` 是每帧求值的 GDScript 表达式，首次为真时截图；`when_satisfied_frame` 早于 `captured_frame`。
- `when` 以当前场景为基准；表达式格式错误立即失败，运行时错误附在 `when_timeout` 上。文本条件必须同时检查可见性，隐藏标签仍保留文本。
- 截图每边上限 4096 像素、PNG 上限 8 MiB；base64 超过 4 MiB JSON 响应上限会被拒绝。

截图配合 `get_game_ui` 的 action=elements：先枚举控件，再注入点击，然后重新截图与枚举，因为固定坐标下的界面会变化。

## UI 侦察

`get_game_ui` 的 action=elements 返回游戏内每个 Control 的 `path`、`type`、`visible`、`text`、`global_rect`、`mouse_filter` 与 `z_index`。`mouse_filter` 与 `z_index` 是命中与绘制顺序事实；全屏 `PASS` 控件会吞点击，`IGNORE` 则放行。`max_elements` 默认 100（最大 1000）；`truncated` 表示还有更多。

`get_game_evidence` 的 action=validate 报告零尺寸、移出视口或可能被遮挡的控件。UI 验收先校验，再用 `click_game_ui_element` 点击并检查 `hit_path`；`hit_mismatch` 只在 `strict_hit: true` 时失败。

## 暂停、重载与游戏内代码

- `game_scene_tree` 的 action=set_pause 暂停或恢复；action=is_paused 确认。process mode 仍决定哪些节点运行，暂停还会停用物理服务器；瞬态输入状态在暂停期间丢失。
- action=reload_current_scene 从磁盘重载运行场景，脚本状态、连接与运行时修改都重置；没有运行场景时返回 `ERR_UNCONFIGURED`。
- `game_script` 的 action=execute 在游戏内运行。`script_action` 取 `script`（配 `source_code`）、`get_property`、`set_property` 或 `call_method`；脚本要有显式 return 才能产生值。裸语句包进 `@tool extends Node` + `func _run()`，在 `/root` 下的临时节点执行，不在当前场景内——访问场景节点用绝对路径（`/root/...`）或 `../`；调用后节点即释放，除非 `persist` 把它保留在 `/root/__gda_runtime` 下。

## 日志与错误路径

| 路径 | 工具 | 来源 | 需要游戏 |
|---|---|---|---|
| 编辑器引擎日志 | `get_debugger_stream` 的 action=log（source=editor） | 编辑器引擎日志缓冲 | 否 |
| 调试会话 | `get_debugger_stream` 的 action=errors / action=output、`get_debugger_scene_tree` | 运行时通道上的运行游戏 | live 数据需要 |
| 磁盘游戏日志 | `get_game_log_entries` | `user://logs/godot.log` 尾部 | 日志文件必须存在 |
| 插件日志 | `get_plugin_log`，或 `get_debugger_stream` 的 action=log（source=plugin） | 插件诊断：确认拒绝、超时、丢失响应 | 否 |

- action=log（`limit` 默认 50）读编辑器脚本错误与 print 输出；编辑任何 `.gd`、`.tscn`、`.tres` 或 `.cs` 后要读。
- action=log 配 source=plugin（`limit` 默认 100，最大 1000）读插件诊断，不是引擎日志。
- action=errors 返回结构化条目（`time`、`file`、`func`、`line`、`error`、`descr`、`stack`）；action=output 读捕获的 stdout/stderr；`get_debugger_scene_tree` 返回文本树。无会话时返回空结果与提示。
- `get_game_log_entries` 读磁盘尾部（`limit` 默认 50，最大 500），并回退到 `godot.log.1`。它绕过调试传输的丢弃门，是错误风暴的可靠路径。

## 错误水位线

每次工具响应都带 `new_errors_since_last_call`，统计自上次调用以来的错误，包括本次调用的错误。它是一次性的。

1. 用 `get_debugger_stream` 的 action=errors 或 `get_game_log_entries` 复现并读取错误。
2. 修复、重跑并走一遍故障路径。
3. 做任意下一次调用并读水位线：0 表示没有新错误；非零先读再继续。

零不证明什么都没发生。调试传输会丢弃连接前的消息、最多一秒的错误风暴、以及队列溢出时的消息。错误风暴路径要用 `get_game_log_entries` 交叉核对。

## 内联 GDScript 测试

`run_gdscript_tests` 运行内联 `tests` 用例（`{name, source}`），或按 `directory`（默认 `res://tests`，非递归）与 `pattern`（默认 `*.gda_test.gd`）运行磁盘文件。源码是顺序语句（不允许顶层 `func`、`static`、`class`），被包进临时 `@tool` 节点串行执行后删除。`SceneRoot` 是被编辑场景根。

断言：`check`、`check_equal`、`check_almost_equal`、`fail`、`fatal`。失败断言记录一行并继续；`fatal` 终止该用例。`timeout_ms` 预算（默认 10000，最大 30000）耗尽后剩余用例标记 `skip`。响应含 `summary`（`total`、`passed`、`failed`、`skipped`）与 `cases`（`status` 为 `pass`、`fail`、`skip`）；文件模式还返回执行的 `files`。

## 调试辅助

- `get_debug_info` 的 action=catalog 是权威 monitor id 列表；action=monitors 读值；action=custom 读一个自定义 monitor；action=object / action=node / action=memory 暴露计数与静态内存。
- `get_game_evidence` 的 action=sample 跨帧采样游戏节点属性。
- 断点：`debugger_breakpoints` 的 action=list / set / clear；`get_debugger_stream` 的 action=session 报告 breaked 会话。插件默认每会话自动继续最多 3 次，除非 `GDA_AUTO_CONTINUE` 为 `0` 或 `false`；被阻塞的游戏调用会超时，而不是返回陈旧状态。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| `game not running` | 游戏不是从编辑器启动 | `editor_scene_control` 的 action=play |
| 启动后游戏调用超时 | `gda:ready`/scene-ready 一直不到 | 检查游戏项目是否加载扩展；等场景就绪 |
| 没有调试器错误 | 无会话或传输丢弃 | 读 `get_game_log_entries` |
