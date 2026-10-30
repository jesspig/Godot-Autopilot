# T8 生成式强类型 wrapper spike（隔离目录，不进主线）

目标：回答 S3 是否值得立项的实测门禁——GDExtension 类是否可被发现到足以做代码生成。
结论：`go`（详见 `SpikeReport.json`，结论仅 go/no-go 一词）。

## 门禁问题与实测答案

1. `godot --dump-extension-api` 是否含 GDExtension 类？
   是。实测 `Godot v4.7.2.stable.mono.official.ed1daf0bf`
   （`C:\Users\jesspig\Documents\Godot\Current\Godot.exe`）在仓库根执行
   `--dump-extension-api`，落盘 `extension_api.json`（约 6.9MB，分析后已删除，
   仓库保持干净）。注意该 CLI 把 `extension_api.json` 写到当前目录，
   附加路径参数会被忽略（实测行为）。
   分析结果（`C:\Users\jesspig\AppData\Local\Temp\opencode\gda_dump\analyze_dump.py`
   输出的实测数）：
   - `class_count = 1037`，`has_GDExtension = True`，
     `has_GDExtensionManager = True`，`has_ClassDB = True`。
   - `GDExtension`：`inherits = Resource`，`is_instantiable = True`，
     `api_type = core`；方法 `is_library_open()`（0 参数）、
     `get_minimum_library_initialization_level()`（0 参数）；
     枚举 `InitializationLevel`。
   - `GDExtensionManager`：`inherits = Object`；方法含
     `load_extension(path: String)`、`reload_extension`、`unload_extension`、
     `is_extension_loaded`、`get_loaded_extensions`、`get_extension`。
   - `singletons` 共 41 项，其中 `ClassDB` 与 `GDExtensionManager`
     均在列（单例名字符串已用于样本中的 `Engine.GetSingleton`）。
2. `ClassDB.GetClassList` 在编辑器/游戏双态是否含 GDExtension 类？
   部分实测 + 代码级确认，游戏态未实测（见局限）：
   - 本仓 `src/tools/doc_ops.cpp:691` 的 `handle_search` 正是通过
     `ClassDBSingleton::get_class_list()`（即 `ClassDB.GetClassList`）枚举全类；
     dump 本就是引擎枚举 ClassDB 的产物，故 dump 含 GDExtension 即蕴含
     ClassDB 含 GDExtension（推断，已标注）。
   - 独立交叉证据：`third_party/godot-cpp/gdextension/extension_api-4-7.json`
    （版本锁定产物，非本次生成）同样含两者（1036 类，
     `bundled_has_GDExtension = True`）。
   - 未实测项：MCP 端口 9527 无监听（无运行中编辑器），且本任务禁编译，
     故编辑器 live 态与游戏运行态（`runtime=game`）的 `GetClassList` 未做
     真机复核；T1 DecisionRecord 的类枚举基线在仓内未检索到
    （已搜 `tests/`、`docs/`、`skills/`），基线暂以本次新鲜 dump 的
     1037 类代替，与锁定产物差 1 类归因于版本漂移（推测，已标注）。

## hot-reload：只验证，不改加载逻辑

引用 `src/tools/editor_ops.cpp:1415-1419` 的 `handle_build_csharp_assembly`
注释（kNote）：GDExtension（C++）无公共 API 触发编辑器内 C# 程序集重载，
该能力位于引擎 `modules/mono` 内部。因此生成式 wrapper 与现有
`csharp_bridge/*.cs` 走同一循环：改签名后仍需用户在编辑器点 Build 或重启
编辑器，`build_csharp_assembly` 仅做命令行编译验证。本 spike 未改动任何
加载逻辑（`src/tools/` 零改动）。

## wrapper 样本设计（`GdextensionWrapper.sample.cs`）

- 命名空间 `GDExtensionBindgen`；`GdextensionHandle` 为普通 sealed 类，
  持有一个 `GodotObject`，不继承 `GodotObject`；`GdextensionManager` 为
  静态类，经 `Engine.GetSingleton("GDExtensionManager")` 拿单例。
- 全部经公开 `Get`/`Set`/`Call`（本仓已有同拼写用法：
  `csharp_bridge/BridgeGetSet.cs:17,56` 的 `target.Get`/`target.Set`，
  `csharp_bridge/BridgeEvents.cs:64` 的 `_target.Call`）；不用 `unsafe`，
  不用内部 API。
- 覆盖的成员名与单例名全部来自本次实测 dump；返回值转换写法为示意，
  真正 codegen 须按 dump 的参数/返回类型表逐项映射。
- 对标说明（任务给定方向，未联网核验对方实现，不做事实断言）：
  走轻量手写封装路线（gilzoide、Delsin-Yu 思路的方向性对标），即组合优于
  继承——wrapper 持有对象而非成为 `GodotObject` 子类；与本仓既有
  `GdaCSharpBridge`（`RefCounted` 子类 + 反射）形成对照：既有桥解决
  “调到 C# 成员”，本 wrapper 解决“S3 为 GDExtension 形状生成强类型门面”，
  两者正交。
- `.sample.cs` / `.sample.csproj` 均为文档样本，不参与编译、不接入任何工程；
  `comment_guard` 仅扫描 `src/`、`tests/` 下 `*.cpp/*.hpp`
 （`tests/guard/comment_guard.py:12`），隔离目录不受影响。

## 文件清单

- `GdextensionWrapper.sample.cs`：最小 wrapper 样本。
- `SpikeReport.json`：机器可读结论（`dump_contains_gdext` /
  `classdb_contains_gdext` / `wrapper_sample` / `decision`）。
- `README.md`：本文件（含下述 csproj 接入形态片段）。

## csproj 接入形态片段（内嵌，不再设独立 `.csproj` 文件）

```xml
<!-- 文档片段：生成式 wrapper 接入形态示意，未接入任何实际工程，不参与编译。 -->
<!-- 形态：生成代码以 Compile Include 进入 .NET 项目，SDK 沿用 Godot.NET.Sdk。 -->
<Project Sdk="Godot.NET.Sdk/4.7.2">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <Nullable>enable</Nullable>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="Generated/GdextensionWrapper.sample.cs" />
  </ItemGroup>
</Project>
```

## 局限与后续

- 游戏态 `GetClassList` 与 T1 基线对账待 S3 立项后真机补测；本结论 `go`
  仅表示“形状可发现，codegen 有输入”，不承诺游戏态一致性。
- skills 下未追加引用行：spike 独立可取消，保持零扩散；S3 立项后再补。
- 样本原含独立 `GdextensionBindgen.sample.csproj`，但外部 .NET 自动 restore
  持续在其上生成 `.godot/mono/temp` 缓存（已随文件一并移除）；csproj 改为
  上节内嵌片段，勿将其还原为独立 `.csproj` 或纳入任何工程。
