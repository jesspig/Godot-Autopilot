---
name: godot-autopilot-csharp
description: "Godot C#/.NET 工程指南：从普通工程生成 csproj、用 create_csharp_script 创建 C# 脚本、dotnet_environment 探测、带编译器输出的 build_csharp_assembly 等待循环、读取 diagnostics.instantiable、运行游戏并用 get_debugger_errors 与错误水位确认、通过 create_resource 使用 C# [GlobalClass] 资源、property_set 中的 [Export] 节点与类型数组转换，以及 csharp_get_property/call_method/subscribe_event 反射桥。适用于 C# Godot 工程。"
---

# C# / .NET 工作流

调用方式：用 `gda tools search` 与 `gda tools describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

C# 工程跑在编辑器的 .NET 程序集上，因此工作流有三个差异：编辑与观察之间多了
构建步骤；`[GlobalClass]`、`[Export]` 等声明通过程序集进入 Godot；运行中的
游戏不会自己拾取新程序集。本册覆盖 C# 专属路径。GDScript 执行通道
（`execute_script`、`call_script_node`、`execute_game_script`、`code_execute`）
不受 .NET 程序集状态影响，见 `godot-autopilot-scripting`。

## 引导：把普通工程变成 C# 工程

C# 工程需要 `res://` 根下的 `.csproj`（或 `.sln`），以及 `project.godot` 中
匹配的 `[dotnet] project/assembly_name`。

1. 运行 `dotnet_environment`。普通工程会返回一个带模板的错误：
   `{path, content, target_framework, assembly_name}` 加 `hint`。
2. 用 `write_file` 写模板（内容是 `Godot.NET.Sdk/<engine-version>` 形式）。
3. 在 `project.godot` 中设置 `[dotnet] project/assembly_name`，与 csproj 的
   程序集名一致。
4. 重新打开工程或让编辑器扫描；`dotnet_environment` 再次运行应报告可用。
5. 第一次构建用 `build_csharp_assembly`；读 `get_debugger_log` 确认无编译
   错误。

## 创建与编辑 C# 文件

- `create_csharp_script` 在写盘前校验：文件 stem 必须等于声明的类名；类必须
  `partial` 且继承 ClassDB 已知的 Godot 基类；根 `.csproj` 必须引用该文件
  （SDK 风格 glob 算引用）。失败返回可操作的补丁，而不是静默 ok。
- 返回 `CSharpFileRecord {path, class_name, csproj_ref_exists}` 加
  `base_class`、`project_file`、`is_dotnet_editor`、`verified/readback` 与
  `diagnostics`。`diagnostics.hint` 报告 .NET 编辑器状态、程序集构建状态与
  去 `get_debugger_log` 哪里看。
- `write_file` 写 .cs 时返回 `diagnostic_scope=resource_load` 与
  `compilable=unknown`；编辑器不能编译单个 .cs。构建后用
  `build_csharp_assembly` 与 `get_debugger_log` 判断。
- `patch_script` 也可修补 .cs，但同样只做文本写入；构建另行执行。

### diagnostics 语义

`diagnostics.ok` 不是笼统成功标志：

- `.gd`：`diagnostic_scope=compile`，`compilable=true/false`；编译失败时
  `ok=false` 并带 `parse_error{line,message}`，文件仍在磁盘上，修好再写。
- `.gdshader`/`.gdshaderinc`：只检查资源加载，`compilable=unknown`。
- `.cs`：`diagnostic_scope=resource_load`，`compilable=unknown`；
  `assembly_checked` 表示当前 .NET 程序集是否加载，
  `is_dotnet_editor`/`assembly_loaded` 描述编辑器状态。

`diagnostics.instantiable` 在资源加载成功时报告编辑器能否实例化它；C# 全局类
在程序集加载前可能为 false。

## C# 开发循环

1. 编辑/创建 .cs。
2. `build_csharp_assembly`，等待编译完成；读取返回的编译器输出与
   `get_debugger_log`。
3. 构建失败时按 `file:line` 修复，不要继续运行游戏。
4. 构建成功后运行游戏（`play_editor_current_scene`），用
   `get_debugger_errors` 与 `get_debugger_output` 观察。
5. 修改脚本后：先 `reload_game_scripts`，再 `reload_scene_tree_current_scene`
   （或重试失败操作）；只重载场景不保证重新读取磁盘版本。
6. 用 `get_game_status` 确认进程存活；半死进程停止后重启，不要继续探测。

### 错误水位不是脚本错误列表

错误水位统计自上次调用以来的新错误；C# 编译错误通常先出现在
`build_csharp_assembly` 的返回与 `get_debugger_log` 中，而不是水位。构建后
先读这两处，再用水位确认运行时没有新错误。

### 重启规则

- 程序集构建成功后，新脚本/类通常无需重启编辑器即可实例化；签名变更可能
  仍需要编辑器的 Build 动作或重启，因为 GDExtension 不能热替换程序集。
- 运行中的游戏不会拾取新程序集；必须停止并重新启动游戏。构建结果带
  `restart_required=true` 时，停止并重启后再判断新代码。
- 修改 `[Export]` 字段或 `[GlobalClass]` 后，场景中的序列化值可能需要重新
  保存；旧场景可能保留旧字段名。

## [Export] 属性与保存时机

- C# `[Export]` 节点字段带 NodeType hint，节点路径字符串会转换成节点引用。
- 引擎未实际应用的 object 类型赋值（C# 把它清回 null）现在报
  "value not applied"，工具会尝试恢复旧值并报告是否成功；只读或私有 setter
  属性同样报错。
- `[Export]` 值写入后需要保存场景；仅内存修改不会写盘。
- 校验时优先读可读的导出字段：没有 getter 的 C# 属性对 GDScript 不可见，
  读取返回 null，容易误判为属性不存在。

## C# 资源与脚本挂载

- `create_resource` 支持 C# `[GlobalClass]` 资源；要求编辑器已加载程序集，
  否则按未知类拒绝。
- `attach_script_to_node` 接受 `.cs` 脚本资源；替换脚本不会迁移旧导出属性。
- `create_resource` 的 `type` 必须是 ClassDB 或全局脚本类；抽象类与无效脚本
  拒绝。

## 反射桥：GDScript 看不到的 C# 成员

`csharp_get_property`、`csharp_set_property`、`csharp_call_method`、
`csharp_subscribe_event`、`csharp_unsubscribe_event` 通过反射访问 C# 成员，
包括 GDScript 不可见的私有/派生成员。

- `object_id`/`object_id_str` 是编辑器进程句柄；运行中的游戏对象要用游戏侧
  通道或 `execute_game_script`。
- 解析到脚本资源（如 `res://scripts/Player.cs`）的 locator 会被精确拒绝；
  传节点路径或活实例的 `object_id_str`。
- 运行中游戏的 `__gda_runtime` 持有节点不计入 `get_game_status.node_count`，
  单独报告为 `instrumentation_node_count`。
- 订阅事件返回 token；取消订阅必须用同一 token。导出构建可能裁剪未引用的
  反射目标，发布前验证。

## 参见

- `godot-autopilot-scripting` - GDScript 执行通道，不受 .NET 程序集状态影响。
- `godot-autopilot-scene-system` - 节点 CRUD、属性 JSON 形状与 C# 属性注意
  事项。
- `godot-autopilot-resources` - `create_resource` 背后的创建/保存/复制流程。
- `godot-autopilot-runtime` - 启动游戏、调试通道语义与日志回退。
- `godot-autopilot-tools` - 工具发现、错误水位与可重试软错误。
