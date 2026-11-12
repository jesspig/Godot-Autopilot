---
name: godot-autopilot-runtime
description: "Godot 运行时与调试指南：启动和停止游戏、帧精确的输入注入、暂停语义、日志与错误路径、错误水位确认循环以及运行时检查。适用于运行游戏、模拟输入或诊断问题。"
---

# 运行时与调试

调用方式：用 `gda tools search` 与 `gda tools describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

本册覆盖游戏进程生命周期：启动与停止、`game_*` 运行时通道、输入注入、视口
截图与视觉验证循环、UI 侦察、四条日志/错误路径、错误水位确认循环与内联
GDScript 测试。时序、日志路径与检查细节在下文说明。

## 启动与停止游戏

- `play_editor_current_scene` 把当前编辑场景作为游戏启动。先
  `save_editor_scene`，否则播放可能失败。
- 返回 `result` 为 `ok` 或 `already_playing`，以及 `is_playing`。游戏已运行
  时得到 `already_playing`——工具不会重启游戏。想要全新运行，先
  `stop_editor_playing`。
- `stop_editor_playing` 停止游戏并回到编辑模式。没有游戏运行时也返回 `ok`，
  所以会话结束或再次修改场景前可以安全调用。

## game_* 运行时通道的前置条件

所有 `game_*` 工具（以及 `capture_editor_viewport` 的 `target` 为 `game`）
都通过运行时调试通道请求游戏进程。要求：

- 游戏由编辑器启动，且工程加载 godot-autopilot 扩展。
- 游戏仍在运行；停止后调用返回结构化错误，而不是空结果。
- 游戏进程没有被断点阻塞；被阻塞时调用会超时。

没有运行中的游戏时，用编辑器侧工具；不要假设 `game_*` 会回退到编辑器。

## 用 get_game_status 检查存活

`get_game_status` 返回场景、`node_count`（排除保留的 `__gda_runtime` 子树）、
`instrumentation_node_count`、FPS、`paused` 与 `physics_frame`。用它确认游戏
是否活着、是否暂停，以及当前场景是不是预期场景。场景重载失败会表现为空场景
与 `node_count` 骤降。不要调试半死进程：`stop_editor_playing` 后重新
`play_editor_current_scene`。存活但 `node_count` 正常、玩法不推进，是卡死，
不是重载失败。

## 编辑器与游戏是两个进程

运行中的游戏是独立 OS 进程。对编辑器进程做的一切不会到达游戏：

- 编辑器侧输入工具（`press_input_action`、`release_input_action`、
  `is_input_action_pressed` 等）作用于编辑器，不会进入游戏。
- 编辑器侧节点工具操作编辑场景，不是游戏场景树。游戏场景树用
  `get_debugger_scene_tree` 等调试通道工具。
- 编辑器截图与游戏截图是不同目标；`capture_game_viewport` 才反映游戏画面。

## 向运行中的游戏注入输入

- `queue_game_input` 在游戏下一帧开始注入事件。它不保证与随后读取同一帧：
  注入后才发起的读取可能看到旧状态。用 `get_game_input_status` 轮询，或把
  动作与读取放进一个 `execute_game_script`。
- `wait_game_input` 等待某个输入状态；`sequence_game_inputs` 按顺序注入一组
  输入。两者都通过运行时通道，遵守 `timeout_ms` 上限。
- 注入事件在下一帧开始时派发。需要精确验证时，先注入，再在游戏脚本里
  `await` 若干帧后读取，不要把“注入后立即读”当作同一帧。
- `get_game_input_status` 返回当前动作与设备状态；用它确认注入是否生效。
- `api` 模式伪造事件与真实按键事件在引擎中路径不同；需要真实输入语义时用
  真实事件模式。完整语义遵循同一规则：事件属于下一帧，不属于当前帧。

## 截图与视觉验证

截图是变更-观察-验证循环中的观察步骤：放置节点、改属性、注入输入或修 UI 后，
看实际画面，而不是假设效果。

- `capture_game_viewport` 抓取游戏视口，返回 base64 PNG。可用 `region`
  `{x, y, width, height}` 裁剪，`max_dimension`（64-4096）限制最长边，
  `scale`（1-8）最近邻放大；最终尺寸在 `width`/`height` 中报告。
- `capture_editor_viewport` 抓编辑器视口；`target` 为 `game` 时走游戏通道。
- `after_frames` 等待若干渲染帧后截图；`when` 是每渲染帧求值的 GDScript
  表达式，为 true 时截图。用于短暂反馈（2 秒横幅、0.7 秒死亡画面）：先动作，
  再用条件截图。
- 文本条件必须同时检查可见性：隐藏标签不会清空文本，只按文本判断会永远为
  true。把传送、await 与读取放进一个脚本，避免跨调用时序。

## 枚举运行中游戏的 UI

`get_game_ui_elements` 返回运行中游戏的 Control 树、矩形与可见性。用矩形
`global_rect` 定位按钮与面板，再通过 `queue_game_input` 注入点击。注意全屏
`PASS` 控件可能吞掉本应落到下层控件的点击；点击前检查命中路径与遮挡关系。
编辑器 UI 自动化用 `get_editor_ui_elements`、`hit_test_editor_point` 与
`click_editor_element`，不要混用两套坐标。

## 暂停、重载与在游戏中运行代码

- `set_scene_tree_pause` 暂停/恢复游戏场景树；`is_scene_tree_paused` 确认
  状态。暂停语义是矩阵不是开关：节点按 process mode 继续处理（`INHERIT`
  向上解析，根固定 `PAUSABLE`），暂停还会停用物理服务器——模拟本身停止，不
  只是节点回调。瞬时输入状态在暂停期间丢失。`physics_frame` 是引擎全局计数，
  暂停时仍前进，所以证明玩法冻结要结合 `paused` 与玩法值（节点位置或玩法
  自己的 tick）。
- `reload_scene_tree_current_scene` 从磁盘重载运行中的场景，重建场景根——
  脚本状态、连接与运行时修改重置为保存文件。没有运行场景时返回
  `ERR_UNCONFIGURED` (3)。
- `execute_game_script` 在游戏进程运行代码：`action` 为 `script` 时接受完整
  GDScript（定义 `func _run()`，原样使用）或裸语句（自动包成
  `extends Node` + `func _run()`，需要显式 `return`，包装时报告
  `wrapped: true`）；`get_property`、`set_property`、`call_method` 用于检查
  或修改节点状态。`persist`（默认 false）把临时节点保留在
  `/root/__gda_runtime` 下。
- `reload_game_scripts` 在运行中的游戏里重载 GDScript（传 `paths` 数组），
  请求在游戏下一次 idle 轮询应用，不返回确认——用 `get_game_log_entries` 或
  `get_game_status` 验证。

## 四条日志与错误路径

| 路径 | 工具 | 来源 | 需要运行中的游戏 |
|---|---|---|---|
| 编辑器引擎日志 | `get_debugger_log` | 编辑器进程引擎日志缓冲：脚本错误与消息 | 否——总有数据 |
| 调试会话捕获 | `get_debugger_errors`、`get_debugger_output`、`get_debugger_scene_tree` | 活动调试会话中来自运行游戏；没有会话时返回空结果与 note，无编辑器回退 | 实时游戏数据需要会话 |
| 磁盘游戏日志 | `get_game_log_entries` | 游戏进程日志文件 `user://logs/godot.log` 的尾部窗口 | 日志文件必须存在——先启动过一次游戏 |
| 插件 LogSystem | `get_plugin_log` | 插件进程内诊断缓冲（确认被拒、超时记录、迟到的游戏响应） | 否——始终可用 |

- `get_debugger_log`（可选 `limit`，默认 50）读编辑器引擎日志——编辑器进程的
  脚本错误与 `print`。不需要运行游戏；改任何 `.gd`、`.tscn`、`.tres` 或
  `.cs` 后先读它，再继续改代码。
- `get_plugin_log`（`limit` 默认 100、最大 1000；`level`、`category`、
  `filter`、`since_index`）读插件自身诊断，返回条目数组、count 与
  next_index。它不读引擎日志缓冲；确认被拒、超时记录与迟到的游戏响应在这里。
- `get_debugger_errors`（`limit` 默认 20）在有活动会话时返回结构化列表，
  每项含 `time`、`file`、`func`、`line`、`error`、`descr`、is_warning 与
  `stack`。
- `get_debugger_output` 读游戏进程 stdout/stderr（`limit` 默认 50）；
  `get_debugger_scene_tree` 无参数，返回游戏场景树。

## 用水位确认修复

错误水位 `new_errors_since_last_call` 统计自上次调用以来记录的新错误，包括
本次调用产生的错误。它是只读一次、读后归零的计数器。

- 每次写入后读水位，非 0 就停下，取真实日志行。
- 修复后重跑失败调用；成功且水位为 0 才算闭环。
- 游戏打印的错误也计入。启动早期错误可能在任何工具连接前发生；消息洪泛会
  被丢弃；消息队列溢出会整批丢消息。水位为 0 只表示“自上次调用以来没有记录
  新错误”，不表示“什么都没发生”：错误风暴路径要交叉检查
  `get_game_log_entries`，它绕过这些门。

## 用内联 GDScript 测试收尾

`run_gdscript_tests` 用内置断言框架运行内联测试。`tests` 每项是
`{name, source}`；source 必须是普通顺序语句（顶层 `func`/`static`/`class`
不支持）。`run_gdscript_test_files` 运行磁盘上的测试文件。测试通过后仍要读
`get_debugger_log` 确认没有额外错误。

## 运行中的调试辅助

- `get_debug_monitor_catalog` 列出可用监视器；`get_debug_monitors` 读取值；
  `get_debug_memory_usage`、`get_debug_object_count`、`get_debug_node_count`
  读取内存与对象/节点数量。
- `set_debug_physics_fps` 调整物理帧率——仅运行时生效，验证后恢复 60。
- `get_debugger_session_info` 报告调试会话状态；断点命中时默认自动继续，
  由 `GDA_AUTO_CONTINUE` 控制（`0` 或 `false` 关闭），每个会话最多 3 次。

## 断点与自动继续

游戏在调试断点停下时，`get_debugger_session_info` 报告 breaked。默认自动继续
breaked 会话，让 `game_*` 调用继续工作；自动继续由 `GDA_AUTO_CONTINUE` 控制
（`0` 或 `false` 关闭），每个会话最多 3 次。断点会暂停游戏进程直到自动继续
或手动继续；被它阻塞的 `game_*` 调用会超时，而不是返回旧状态。

## 参见

- `godot-autopilot-scripting` - 四个脚本执行通道，含
  `execute_game_script`。
- `godot-autopilot-tools` - 发现协议、水位细节与可重试软错误。
