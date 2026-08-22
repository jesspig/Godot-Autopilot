---
type: 模块文档（设计+实现）
title: ToolBase 工具统一标准化（接口 + 组合 + 真类化）
description: 以接口 + 组合统一全体工具：ToolBase 接口、角色接口切片、GDA_TOOL_CLASS 真类宏、ToolRegistry 单一来源，336 域工具全部为独立 ToolBase 子类
tags:
  - 设计
  - 工具架构
  - 接口
  - 组合
timestamp: "2026-08-22T06:57:00+08:00"
---

# ToolBase 工具统一标准化（设计定稿 + 全量真类化实现）

> **当前 API 面（2026-08-22 复核）**
> `tool_base.hpp`：`SideEffect` 枚举 + `side_effect_name`、`ToolMeta`、`ISideEffect`、`IMetaTool`、`ToolBase`（meta/execute/input_schema 三件套）、`side_effect_of` 自由函数。
> `tool_decl.hpp`：`GDA_TOOL_CLASS` / `GDA_TOOL_CLASS_SIDE` 真类宏；`fn_tool.hpp`：`FnTool(ToolMeta, HandlerFn, schema, SideEffect=None)`（实现 `ISideEffect`）+ `make_fn_tool`。
> `tool_registry.hpp`：`make_tool_info` + `ToolRegistry`（`add` 按 `dynamic_cast<IMetaTool*>` 自动归类 / `find` / `find_meta` / `find_any` / `all` / `all_meta` / `all_any`）。
> - **336 域工具全部为独立 `ToolBase` 子类**，分散于 `src/tools/<域>_tools.hpp`（26 个文件），`execute` 委托既有 domain handler、`input_schema` 统一经 `tool_input_schema` 取；
> - `tool_defs.def` 已删除；`register_all` 注册 26 个域的 `make_tools()` + `system_status`（FnTool）+ 7 元工具（`MetaTool`，接口 + 组合），catalog / BM25 index / 分发 map 全部从 registry 派生；
> - **元工具 = 接口 + 组合**：`IMetaTool` 标记接口 + `MetaTool`（`ToolBase`+`IMetaTool`，依赖组合注入），`ToolRegistry::add()` 用 `dynamic_cast<IMetaTool>` 自动归类——实现接口即元工具；
> - **副作用驱动遍历排除**：`SideEffect` 枚举扩为 6 值（含 `Process`）；35 个副作用工具用 `GDA_TOOL_CLASS_SIDE` 宏标记（实现 `ISideEffect`）；`get_tool_detail` 返回 `side_effect` 字段；遍历 runner 读该字段排除，删除硬编码 `kExcludedSideEffectTools`。
> 验证：L1 **71/71** + L2 通过（含 03_tools_contract 336 工具全遍历）。权威计数：域工具 336、`system_status` 1、元工具 7、catalog/index 344。
>
> 相关页面： [工具注册表](modules/tools_registry.md) · [工程约定](conventions.md) · [测试体系](tests.md) · [架构总览](overview.md)

## 目标与约束

- 一个工具收敛为一个 `ToolBase` 派生类，元信息 + schema + 行为内聚于同类。
- 以单一 `ToolRegistry` 取代早期的 `g_handlers` / `g_meta_handlers` / `ToolCatalog` 三套并存。
- **自动注册推导而非运行时发现**：C++ 无反射，不做"运行时扫描哪些类继承 ToolBase"（linker section 自注册与静态初值对 LTO/Unity 构建负资产、平台相关）。改为各域 `make_tools()` 显式清单 + 元信息由类内 `meta()` 自动推导，把"新增工具改多处"收敛为"新增类 + `make_tools()` 一行 + CMake"。
- **行为对等（红线）**：不改变对外输出键、`{"error": msg}` 错误语义、参数默认值、`execute()` 仅在 Godot 主线程被调用的保证，以及 3 个 schema 契约缺口的现行为。

## ToolBase 接口与角色切片（最终形态）

```cpp
class ToolBase {
public:
  virtual ~ToolBase() = default;
  virtual const ToolMeta &meta() const = 0;                 // name/desc/category/tags/basic_schema
  virtual mcp::JsonValue execute(const mcp::JsonValue &args) = 0;
  virtual mcp::JsonValue input_schema() const = 0;
};

class ISideEffect {
public:
  virtual SideEffect side_effects() const = 0;              // None/WritesFile/WritesConfig/ShowsAlert/ModifiesWindow/Process
};

class IMetaTool {};                                         // 纯标记接口

inline SideEffect side_effect_of(const ToolBase &t);        // dynamic_cast<ISideEffect*>，未实现返回 None
```

- **线程契约**：`execute()` 复用现 [dispatch.cpp](../src/tools/dispatch.cpp) 的 `queue.submit` 路由，仅主线程被调；类内部不得另起线程触碰 Godot API。
- 错误直接返回 `{"error": msg}` JSON 对象（无独立封装助手——早期设计的 `make_ok()/make_error()` 与参数读取助手 `ArgReader` 曾短暂落地，2026-08-22 清理时删除：样板量有限，收敛收益不抵抽象成本）。
- 导出禁用不做角色接口：RegisterTool 回调统一前置 `ExportGuard::is_exporting()` 检查；异步同样不做接口——`__gda_pending` 约定由 `call_tool` 的等待逻辑统一处理（`runtime_ops::wait_pending_response`）。早期设计的 `IExportGuard`/`IAsync`/`blocks_export`/`tool_is_async` 已随清理删除。
- 副作用：`side_effects()` 作为 L2 遍历测试排除依据，替代 `tests/runner/traversal.cpp` 的硬编码清单。

## 组合注入（替代域基类继承）

域共享 Godot 访问经构造函数组合进工具类，不建继承层；尊重 DIP，工具只依赖助手对象，schema、副作用、域 helper 均可插拔。`MetaTool` 即该模式的范本：index/catalog/schema/handler 全部构造注入。

## ToolRegistry（单一来源）

```cpp
class ToolRegistry {
public:
  void add(std::unique_ptr<ToolBase>);   // dynamic_cast<IMetaTool*> 自动归类域/元
  ToolBase *find(name);                  // 仅域
  ToolBase *find_meta(name);             // 仅元
  ToolBase *find_any(name);              // 先域后元
  std::vector<ToolBase*> all();          // 域（供 catalog/index/g_handlers 派生）
  std::vector<ToolBase*> all_meta();     // 元（供 RegisterTool/g_meta_handlers 派生）
  std::vector<ToolBase*> all_any();      // 域+元（供 catalog/BM25 index 派生）
};
```

`register_all_tools` 最终态：重建 registry → 遍历派生填充 ToolCatalog + Bm25Index + `server.RegisterTool` + 分发 map。**新增工具 = 新增 1 类 + 该域 `make_tools()` 一行 + CMake（header-only 无需）**。早期设计的 `install<Ts...>()` 模板与 `GD_TOOL_LIST` X 宏类名清单被各域 `make_tools()` 方案取代（按域聚合更贴合 26 个 `*_tools.hpp` 的组织）。

## 迁移路线（已完成）

1. **加层不改**：FnTool 适配器包 free-function handler → registry 单一来源。
2. **逐域迁移**：26 个域全部真类化（`tool_decl.hpp` 宏生成）。
3. **元工具迁移**：7 个 `MetaTool`（接口 + 组合）。
4. **收尾清理（2026-08-22）**：删除未兑现或已被更简机制取代的抽象（`ArgReader`/`make_ok`/`make_error`/`IExportGuard`/`IAsync`/`blocks_export`/`tool_is_async`/`add_meta`/`to_tool_info_all`），FnTool 构造收缩为四参，L1 断言与计数同步。

> 行为对等由 L1 断言数字（344 catalog 条目）与 L2 契约用例守住；后续改动需重新核算工具数与文档同步。