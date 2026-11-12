---
name: godot-autopilot-scripting
description: "Godot GDScript 指南：脚本生命周期、四个执行通道（execute_script、call_script_node、execute_game_script、code_execute）、@tool 语义、静态初始化器与编辑器/游戏进程边界。适用于编写、挂载或运行 GDScript。"
---

# GDScript 工作流

调用方式：用 `gda tools search` 与 `gda tools describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

本册覆盖通过 godot-autopilot 创建、挂载、编辑与重载 GDScript，以及四个执行
通道加 `code_execute` 元工具的安全差异。占位实例、静态初始化器、编辑器与
游戏语义、帧顺序等引擎级坑在下文说明。

## 脚本生命周期

- `create_script` 保存前先编译：编译错误带解析器输出中止调用，成功意味着
  磁盘文件能解析。`overwrite` 默认 false。保存后从磁盘读回校验；文件被锁时
  返回 verified=false，而不是静默成功。不要对同一路径并行 `create_script`——
  串行写入没有同步。编译通过只证明能解析，不证明运行正确：每次改脚本后读
  `get_debugger_log`，确认编辑器进程没有解析或运行时错误。
- `attach_script_to_node` 与 `detach_script_from_node` 作用于当前编辑场景，
  注册到编辑器撤销/重做。`script_path` 接受 ResourceLoader 能解析为 Script 的
  任意资源，不限于 .gd。替换已有脚本不会迁移或清理旧脚本的导出属性：
  `node_paths` 等存储引用可能丢失，需要手工重连。没有 `@tool` 的脚本不能在
  编辑器中实例化，其方法只在游戏运行时执行——`call_script_node` 同样受限。
- `reload_script` 在外部编辑后从磁盘重载；`keep_state`（默认 false）跨重载
  保留实例状态。静态变量另有规则，见“静态初始化器”。
- `get_script_property_list` 列出脚本声明变量及 name、type、usage flags。
- `get_script_property` 读值：`script_path` 返回声明默认值，`node_path` 返回
  节点当前值；两者只能传一个。
- `set_script_property` 直接写节点属性——没有撤销、没有类型或只读校验。它是
  快速内存调整的轻量通道；需要撤销与校验时用 `property_set`。
- `patch_script` 在已有脚本中修补一个锚点（首次出现，替换或插入），不重写
  整个文件；和 `create_script` 一样先编译，并用磁盘读回哈希证明写入。先用
  `dry_run` 预览无写入 diff。

## 坑：非 @tool 脚本在编辑器中从不执行

“脚本没反应”最常见的原因：

- 没有 `@tool` 的脚本永远不会在编辑器进程中实例化。编辑场景中挂这种脚本的
  节点只有占位实例（`PlaceHolderScriptInstance`），只暴露导出属性默认值。
- 代码完全不运行：没有 `_init`、`_ready`、`_process`、工具期回调——也不报错
  不警告。场景看起来完全正常，脚本却是惰性的。
- 门控是 `can_instantiate = valid && !abstract && (tool || scripting_enabled)`：
  只有标了 `@tool`（或编译/抽象检查未失败）的脚本能在编辑器中实例化。
- 症状：`get_script_property` 用 `script_path` 一直返回声明默认值；期待的
  编辑器侧变更不出现；`call_script_node` 对非 `@tool` 脚本被拒。
- 修复：首行加 `@tool`，再 `reload_script`；或运行游戏走游戏通道。

## 四个执行通道

| 通道 | 运行位置 | 需要 @tool | 超时 | 适用 |
|---|---|---|---|---|
| `execute_script` | 编辑器进程，同步 | 否 | 无——长代码会阻塞编辑器 | 需要 `SceneRoot` 的编辑器自动化 |
| `call_script_node` | 编辑场景中已有节点实例 | 是 | 默认工具超时 | 带真实实例状态调用一个方法 |
| `execute_game_script` | 运行中的游戏进程 | 否 | `timeout_ms` 默认 5000，上限 25000 | 查询或修改游戏状态 |
| `code_execute` | 编辑器进程，经长阻塞元工具 | 否 | `timeout_ms` 默认 5000，上限见 schema | 循环、计算、批量编辑 |

`code_execute` 的编辑器脚本把当前编辑场景根暴露为 `SceneRoot`。`execute_script`
也在编辑器运行，但没有 `code_execute` 的超时中断与包装；长任务优先
`code_execute`。

## `code_execute` 元工具

`code_execute` 在编辑器运行 GDScript：

- 单函数模式：源码内联进一个入口函数体。写直线代码并以 `return` 结束。
- 多函数模式：出现行首 `func ` 时保留整段源码，用 `function_name`（默认
  `_run`）选择入口；入口不存在会补 `func <name>(): pass`。
- `timeout_ms` 默认 5000，上限以 schema 为准；截止时间在脚本运行时强制，
  长循环会被中断并以 `exec_timeout` 返回，而不是冻结编辑器。
- `auto_owner` 默认 true，创建的节点随场景保存。

```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "var n := SceneRoot.get_node(\"Player\")\nn.position = Vector2(0, -10)\nreturn n.position",
    "timeout_ms": 5000
  }
}
```

## 通过 code_execute 处理批量数据

大量瓦片、批量重命名或计算值用 `code_execute`，避免巨大 JSON 参数与逐条
往返。脚本里仍要通过 `SceneRoot` 与普通 Godot API 操作；不要假设工具层会
替你做类型转换。批量操作前先 `save_editor_scene`，或接受失败后从磁盘恢复。

## 静态初始化器

静态变量在脚本第一次加载时初始化，之后跨实例共享。`reload_script` 默认不
重置它们；`keep_state=true` 更不会。需要干净初始状态时，显式重置静态变量，
或重启编辑器。不要依赖静态变量在编辑器重载后保持某个业务值。

## 编辑器与游戏进程语义

`is_editor_hint()` 在整个编辑器进程为 true（包括 `@tool` 脚本与编辑器插件），
在 F5 启动的游戏子进程为 false。两个常见后果：
`Input.set_custom_mouse_cursor()` 在编辑器进程是 no-op；仅编辑器（API_EDITOR）
类在运行游戏中实例化会报错。需要在两种进程表现不同的 `@tool` 代码，应尽早
按 `is_editor_hint()` 分支。

## 树进入与退出顺序

4.7+：进入树时父先于子，`_ready` 子先于父，退出子先于父。节点不能在
`_enter_tree` 中依赖同级节点已 ready。组装复杂子树时，先给父节点补齐必需
子节点再让父节点进树；做不到时用 `get_node_or_null` 懒解析并判空，不要让
`get_node` 直接报错。

## C# 限制

C# 脚本的执行仍走游戏进程或编辑器程序集；GDScript 执行通道不受 .NET 程序集
状态影响。C# 代码修改后需要 `build_csharp_assembly` 并读 `get_debugger_log`
确认编译，不能靠 `reload_script` 热更程序集。C# 专属流程见
`godot-autopilot-csharp`。

## 运行中游戏的热重载

`reload_game_scripts` 在运行中的游戏里重载 GDScript，不重启游戏。会话中改
脚本后用它，再重跑验证调用。上面的静态变量规则适用于编辑器重载；游戏内
重载行为见 `godot-autopilot-runtime`。

## 选择通道

- 已有节点实例、要调一个方法 → `call_script_node`（需要 `@tool`）。
- 编辑器里跑一段临时代码、要超时保护 → `code_execute`。
- 同步编辑器自动化、需要 `SceneRoot` 且能接受阻塞 → `execute_script`。
- 查询或修改运行中的游戏 → `execute_game_script`。
- 需要文件级修改、保留撤销 → `patch_script` 或 `property_set`，不要用脚本
  绕开编辑器的撤销与校验。

## 参见

- `godot-autopilot-tools` - 发现、confirm、错误水位与超时限制。
- `godot-autopilot-scene-system` - 节点、属性与信号。
- `godot-autopilot-runtime` - 游戏进程、调试通道与热重载。
- `godot-autopilot-csharp` - C# 构建循环与反射桥。
