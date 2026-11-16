# GDScript 工作流

创建、挂载、编辑与运行 GDScript，以及四条执行通道及其安全差异。

## 脚本生命周期

- `create_script` 保存前先编译；错误会带解析器输出中止。它从磁盘读回并校验；文件被锁时返回 `verified: false`。禁止并行写同一路径。编译通过只证明能解析；编辑后读 `get_debugger_stream` 的 action=log 检查解析或运行时错误。
- `attach_script_to_node` 与 `detach_script_from_node` 可 undo。`script_path` 接受任何解析为 `Script` 的资源。替换脚本不会迁移导出属性，已存引用可能丢失。
- `reload_script` 从磁盘重载；`keep_state` 默认 false。
- `get_script_property_list` 列出声明变量。`get_script_property` 读声明默认值（`script_path`）或节点当前值（`node_path`），二者只能传一个。`set_script_property` 无 undo、无类型校验；需要这些时用 `property_set`。
- `patch_script` 在一个锚点替换或插入，写入前编译；先用 `dry_run` 预览。

## 非 @tool 脚本在编辑器里是惰性的

没有 `@tool` 的脚本不会在编辑器进程中实例化。其节点只有暴露导出默认值的占位实例。`_init`、`_ready`、`_process` 与工具回调都不运行，也不报警。`get_script_property` 一直返回声明默认值，`call_script_node` 会拒绝该脚本。把 `@tool` 放到首行再 `reload_script`，或运行游戏改用游戏通道。

## 四条执行通道

| 通道 | 运行位置 | @tool | 超时 | 适用 |
|---|---|---|---|---|
| `execute_script` | 编辑器，同步 | 否 | 无；长代码会阻塞编辑器 | 需要 `SceneRoot` 的编辑器自动化 |
| `call_script_node` | 被编辑场景中的现有节点 | 是 | 默认工具超时 | 带真实状态调用单个方法 |
| `game_script` 的 action=execute | 运行中的游戏 | 否 | `timeout_ms` 默认 5000，上限 25000 | 游戏状态查询与修改 |
| `code_execute` | 编辑器，包装临时节点 | 自动包装 | `timeout_ms` 默认 5000，schema 上限 | 循环、批量编辑、计算值 |

四条通道都执行任意 GDScript，都是最高风险副作用。

- `execute_script` 自动返回单个表达式；多行代码要显式 return。裸语句会包进 `extends Node` + `func _run()`；完整脚本必须定义 `func _run()`。编译错误会映射回行号。`print()` 进 `output`，错误进 `errors`，`SceneRoot` 是被编辑场景根。
- `call_script_node` 在现有节点实例内运行，状态是真实的。参数为 `node_path`、`function` 与可选 `args`；非 `@tool` 脚本报错。
- `game_script` 的 action=execute 在运行中的游戏里执行，`script_action: "script"`。`source_code` 接受定义 `func _run()` 的完整脚本或同样包装的裸语句。值需要显式 return；包装调用返回 `wrapped: true`。它创建的临时节点可能在调用后保留。

## code_execute

`code_execute` 把源码包装成 `@tool` 脚本，在临时编辑器节点上运行后清理。

- 单函数模式：没有任何行以 `func `（尾随空格）开头，代码缩进进隐式入口函数。写以 return 结尾的直线代码。
- 多函数模式：出现 `func ` 行时保留整份源码；用 `function_name`（默认 `_run`）选入口。`func(` 或 `func` 加制表符不会进入该模式。
- 缩进按源码自动识别；混用制表符与空格会中止。
- `timeout_ms` 默认 5000，并钳制到 schema 上限；运行期间检查截止时间。
- 源码调用 `close_scene(` 会被拒绝；关闭场景用 `editor_scene_control` 的 action=close。
- `SceneRoot` 是被编辑场景根；包装节点在 `/root` 下，场景节点只能通过 `SceneRoot.get_node(...)` 访问。
- `auto_owner` 为 true 时，执行期间创建的节点递归设置 owner 并保留；为 false 时，新创建的无 owner 子节点在运行后删除。
- 返回值序列化进 `result`，并带 `execution_time_ms`、`output`（上限 8192 字节）与 `wrapped_source`。Resource 结果作为 `registered_resource` 返回。

批量工作用一个 `code_execute` 循环，而不是数百次调用；可避免超大 JSON payload，速度也快得多。

## 静态初始化

`_static_init` 在每次成功重载后运行，而不是每会话一次，因此缓存与注册会在每次 `reload_script` 时重跑。`self` 在其中不可用。静态变量只有 `keep_state=true` 的热重载才保留；普通重载会重新初始化并再次运行 `_static_init`。

## 编辑器与游戏进程

`is_editor_hint()` 对整个编辑器进程为 true（包括 `@tool` 脚本与编辑器插件），对启动的游戏进程为 false。`Input.set_custom_mouse_cursor()` 在编辑器里是 no-op，编辑器专用类在游戏里实例化会报错。需要区分的 `@tool` 代码要尽早分支。

## 树进入与退出顺序

`_enter_tree` 父先于子；`_ready` 子先于父；`_exit_tree` 子先于父。`tree_exited` 在整个分支被切下后触发一次。需要的子节点要在父节点进入前准备好，或用 `get_node_or_null` 懒解析。

## 选择通道

- 编辑器检查或一次性自动化：`execute_script`。
- 单个节点、真实状态的单方法：`call_script_node`（需要 `@tool`）。
- 运行中的游戏内：`game_script` 的 action=execute。
- 循环、批量编辑、计算值或需要超时保护：`code_execute`。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 脚本在编辑器里毫无反应 | 缺少 `@tool` | 加 `@tool` 后 `reload_script` |
| `call_script_node` 被拒绝 | 脚本不是 `@tool` | 改用游戏通道 |
| `create_script` 成功但代码失败 | 编译只证明能解析 | 读 `get_debugger_stream` 的 action=log |
| `execute_script` 卡住编辑器 | 该通道没有超时 | 长任务移到 `code_execute` |
| 每次重载静态缓存都重建 | `_static_init` 重跑 | 把副作用移出 |
