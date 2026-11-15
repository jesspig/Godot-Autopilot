# C# / .NET 工作流

C# 项目运行在编辑器的 .NET 程序集上：编辑与观测之间隔着构建步骤，特性通过程序集进入 Godot，运行中的游戏不会自动加载新程序集。

## 引导 C# 项目

C# 项目需要 `res://` 根下的 `.csproj`（或 `.sln`），以及 `project.godot` 中匹配的 `[dotnet] project/assembly_name`。

1. 运行 `dotnet_environment`；普通项目会返回错误并附带可写模板：`{path, content, target_framework, assembly_name}` 与提示。
2. 用 `write_file` 写模板；它面向已安装的 SDK。
3. 把 `[dotnet] project/assembly_name` 设为返回的 `assembly_name`。
4. 再跑 `dotnet_environment`；应报告 `project_file`、`dotnet_available` 与 `dotnet_version`。

`assembly_loaded` 只表示 .NET 模块已加载；用 `create_csharp_script.diagnostics.instantiable` 或运行时读回验证项目程序集。

## 创建与编辑 C# 文件

- `create_csharp_script`（`path`、`source_code`、可选 `overwrite`）创建 `.cs`。三道门禁会返回可执行补丁：文件名主干必须等于声明的类名；类必须是 `partial` 且继承 ClassDB 已知基类；根 `.csproj` 必须覆盖该文件。它返回 `verified`/`readback` 与 `diagnostics`。
- `write_file` 也能写 `.cs` 并返回诊断，但没有类名或 `partial` 门禁。
- `patch_script` 在一个锚点编辑并在写入前编译。

`diagnostics.ok` 表示文件可作为脚本资源加载；`diagnostics.instantiable` 表示类在已加载程序集中且 `can_instantiate()` 为 true。新类通常在 `build_csharp_assembly` 成功前是 `ok=true, instantiable=false`；在 `new()` 或挂载必须在编辑器运行的脚本前检查它。

## 开发环

1. 创建或编辑 `.cs`。
2. 用 `build_csharp_assembly` 构建，优先 `wait=true`（`timeout_ms` 1-25000）。它运行 `dotnet build --nologo`，返回 `BuildResult`：`success`、`errors`、`output_digest`、截断标记、`game_running` 与 `restart_required`。不带 `wait` 只启动进程并返回 `{pid, started, game_running}`。
3. 读编译器结论：`success`、`errors`、`output_digest`。`get_debugger_log` 覆盖非编译器输出的编辑器脚本错误。
4. 保存场景，用 `play_editor_current_scene` 运行，并用 `get_game_status` 把关。
5. 用 `get_debugger_errors`（`since_seq`/`next_seq`）与 `get_game_log_entries` 诊断。
6. 用 `capture_game_viewport` 观测，或用 `property_get` / `csharp_get_property` 读回。

错误水位线不是脚本错误列表：`new_errors_since_last_call` 只统计工具响应错误与游戏 `runtime_error`，不统计编译器输出或 GDScript 解析错误。用 `BuildResult.success`/`errors`、`get_debugger_log` 与带 `since_seq` 的 `get_debugger_errors`。

重启规则：

- 编辑器侧：新类通常无需重启即可实例化；签名变化可能需要编辑器 Build 动作或重启。
- 游戏侧：运行中的进程保留旧程序集。成功的 `wait=true` 构建报告 `restart_required=true`；判断新代码前停止并重启游戏。
- `reload_game_scripts` 只重载 GDScript。

## [Export] 属性

`property_set` 用与 GDScript 导出相同的 JSON 形状写 C# `[Export]`：

- 节点类型字段接受路径字符串；`converted_node_path` 确认转换。无法解析的路径是错误。
- 被拒绝的赋值报告 `value not applied`；工具会尝试恢复旧值并报告是否成功。
- 改变程序集的构建可能让编辑器重新实例化脚本场景；保存前重新读取导出值，保存后再读回 `.tscn`。

## C# 资源与挂载

- 类进入已加载程序集后，`create_resource` 可按名称接受 C# `[GlobalClass]`。`name` 注册 `memory://` 定位器。
- `set_resource_property` 用 `name` 或 `object_id` 定位实例；`save_resource` 先落盘，节点属性才能引用。
- `attach_script_to_node` 挂载 C# 脚本并记录一步 undo。替换脚本不会迁移导出引用，需要重新接线。

## 反射桥

`property_get` 与 `execute_game_script` 只能触达 GDScript 可见成员。C# getter、重载与事件要用 `csharp_list_members` 以及 `csharp_*` 属性、方法、事件工具，`runtime` 取 editor（默认）或 game。

- 编辑器侧无需手动注册单例；单例缺失时插件会加载 `res://addons/godot-autopilot/GdaCSharpBridge.cs`。
- 游戏侧需要 `GdaCSharpRuntime` 节点；它创建 `__gda_runtime` 载体。只有场景路径定位器能进入游戏；`object_id` 与 `memory` 是编辑器进程句柄。
- 解析为脚本资源的定位器会被拒绝；传活实例的节点路径或 `object_id_str`。

编辑、构建程序集、读调试日志，再查询活实例。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 新建脚本后 `instantiable: false` | 程序集未重建 | 用 `wait: true` 跑 `build_csharp_assembly` |
| `create_csharp_script` 拒绝文件 | 文件名/类名不匹配或不是 `partial` | 应用返回的补丁 |
| 游戏运行旧代码 | 进程保留了旧程序集 | 构建后停止并重启 |
| 反射找不到成员 | 成员不可见或 runtime 不对 | `csharp_list_members`；检查 editor/game |
