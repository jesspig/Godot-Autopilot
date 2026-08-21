---
type: 模块文档（设计+实现）
title: ToolBase 工具统一标准化（接口 + 组合 + 真类化）
description: 以接口 + 组合统一全体工具，已全量落地：ToolBase 接口、角色接口切片、GDA_TOOL_CLASS 真类宏、ToolRegistry 单一来源，336 域工具全部为独立 ToolBase 子类
tags:
  - 设计
  - 工具架构
  - 接口
  - 组合
timestamp: "2026-08-21T12:15:38+08:00"
---

# ToolBase 工具统一标准化（设计定稿 + 全量真类化实现）

> [!todo] 实现状态（2026-08-21 更新）
> **全部核心已落地**：`ToolBase`/`ToolMeta`/`ArgReader`/`make_ok/make_error`/角色接口（`IExportGuard/IAsync/ISideEffect`）、`GDA_TOOL_CLASS` 真类宏（`tool_decl.hpp`）、`ToolRegistry` 单一来源均已实现；
> - **336 域工具全部迁移为独立 `ToolBase` 子类**，分散于 `src/tools/<域>_tools.hpp`（26 个文件），`execute` 委托既有 domain handler、`input_schema` 统一经 `tool_input_schema` 取（零漂移）；
> - `tool_defs.def` 已删除；`register_all` 改为注册 26 个域的 `make_tools()` + `system_status`（FnTool）+ 7 元工具（`MetaTool`，接口 + 组合），catalog / BM25 index / 分发 map 全部从 registry 派生；
> - **元工具 = 接口 + 组合**：`IMetaTool` 标记接口 + `MetaTool`（`ToolBase`+`IMetaTool`，依赖组合注入），`ToolRegistry::add()` 用 `dynamic_cast<IMetaTool>` 自动归类——实现接口即元工具；
> - **副作用驱动遍历排除（已落地）**：`SideEffect` 枚举扩为 6 值（含 `Process`）；35 个副作用工具用 `GDA_TOOL_CLASS_SIDE` 宏标记（实现 `ISideEffect`）；`get_tool_detail` 返回 `side_effect` 字段；遍历 runner 读该字段排除，删除硬编码 `kExcludedSideEffectTools`。
> 验证：L1 **78/78** + L2 **6/6**（含 03_tools_contract 336 工具全遍历）通过。权威计数：域工具 336、`system_status` 1、元工具 7、catalog/index 344。
>
> 相关页面： [工具注册表](modules/tools_registry.md) · [工程约定](conventions.md) · [测试体系](tests.md) · [架构总览](overview.md)

## 目标与约束

- 一个工具收敛为一个 `ToolBase` 派生类，元信息 + schema + 行为内聚于同类。
- 以单一 `ToolRegistry` 取代当前的 `g_handlers` / `g_meta_handlers` / `ToolCatalog` 三套并存。
- **自动注册推导而非运行时发现**：C++ 无反射，不做"运行时扫描哪些类继承 ToolBase"（linker section 自注册与静态初值对 LTO/Unity 构建负资产、平台相关）。改为显式类名清单 + 元信息由类内静态成员自动推导，把"新增工具改 4 处"收敛为"新增类 + 清单 1 行 + CMake"。
- **行为对等（红线）**：不改变对外输出键、`{"error": msg}` 错误语义、参数默认值、`execute()` 仅在 Godot 主线程被调用的保证，以及 3 个 schema 契约缺口的现行为。

## 层形态：接口切片 + 组合助手（非继承域基类）

采用 **面向接口 + 组合优先**，弃用早期的"ToolBase + 域基类继承"方案：

- `ToolBase`：纯虚根接口，仅保留元信息、execute、schema 三件套。
- 横切关注点拆为小角色接口，按需实现（接口隔离）。
- 域共享逻辑做成**可注入助手对象**，经构造函数组合进工具；测试可注入替身隔离运行。

```cpp
namespace godot_autopilot {

struct ToolMeta {                      // 类内静态成员，注册器自动读取
  const char *name;
  const char *description;
  const char *category;
  std::vector<const char *> tags;
  bool basic_schema;                  // true=SCHEMA_BASIC, false=SCHEMA_NONE
};

class ToolBase {                      // 等价 Java 接口（纯虚，不可实例化）
public:
  virtual ~ToolBase() = default;
  virtual const ToolMeta &meta() const = 0;
  virtual mcp::JsonValue execute(const mcp::JsonValue &args) = 0;
  virtual mcp::JsonValue input_schema() const = 0;
};

// 结构体：
//   tool_meta（每类一个，注册信息）的运行时来源：各工具类的静态 meta()
}
```

## ToolBase 接口

```cpp
class ToolBase {
public:
  virtual ~ToolBase() = default;
  virtual const ToolMeta &meta() const = 0;                 // name/desc/category/tags/basic_schema
  virtual mcp::JsonValue execute(const mcp::JsonValue &args) = 0;
  virtual mcp::JsonValue input_schema() const = 0;          // 默认由 meta().basic_schema 生成，可覆盖
};
```

- **线程契约**：`execute()` 复用现 [dispatch.cpp](../src/tools/dispatch.cpp) 的 `queue.submit` 路由，仅主线程被调；类内部不得另起线程触碰 Godot API。
- 结果封装由 `make_ok()/make_error()` 收敛拼 JSON 样板，错误返回 `{"error": msg}`，`call_tool` 据此翻转 `is_error`。
- 参数安全读取由 `ArgReader`（`require/get_str/get_int/get_bool/optional`）收敛 `Find + IsXxx + 默认值 + 告警` 样板。

## 角色接口（横切切片）

```cpp
class IExportGuard  { public: virtual bool blocked_during_export() const = 0; };
class IAsync        { public: virtual bool is_async() const = 0; };   // pending 响应，如 capture_game_viewport
class ISideEffect   { public: virtual SideEffect side_effects() const = 0; };
```

- 导出禁用：默认 `blocked_during_export()==true`，由注册包装统一套 `ExportGuard`，删除各 meta handler 手写的 `if(is_exporting)`。
- 副作用：`side_effects()` 设为 `None/WritesFile/WritesConfig/ShowsAlert/ModifiesWindow`，作为 L2 遍历测试排除依据，替代 `tests/runner/traversal.cpp` 的硬编码清单。
- 异步：`is_async()` 统一下沉 `meta_call_tool_wait` 的 `__gda_pending` 逻辑。

## 组合助手对象（替代域基类继承）

域共享 Godot 访问封装为可注入对象，经构造函数组合，不建继承层：

```cpp
class SceneEditorAccess { /* get_edited_root / parse_node_path ... */ };

class SceneCreateTool final : public ToolBase, public ISchemaField {
public:
  explicit SceneCreateTool(SceneEditorAccess &access) : access_(access) {}
  const ToolMeta &meta() const override;         // {.name="create_scene_node", ...}
  mcp::JsonValue execute(const mcp::JsonValue &a) override;  // 用 access_
private:
  SceneEditorAccess &access_;
};
```

- 尊重 DIP：工具只依赖助手接口，不依赖深继承层级。
- 组合万物：schema、副作用、域 helper 均可插拔，利于 L2 单测注入替身。
- 若某域共享逻辑足够多，可保留一个"便捷基类"作为糖（已集成助手 + 默认接口），但主路径是**面向接口的组合**。

## ToolRegistry 与自动注册推导

```cpp
class ToolRegistry {
public:
  template<class... Ts> void install();              // 实例化域工具
  void add(std::unique_ptr<ToolBase>);               // 域工具
  void add_meta(std::unique_ptr<ToolBase>);          // 元工具（最后注册）
  const ToolBase *find(const std::string &name) const;
  const std::vector<const ToolBase*> &all() const;   // 供 catalog/bm25/RegisterTool 消费
};
```

`tool_defs.def` 由"含 handler 与 schema 的宏行"演化为纯类名清单：

```cpp
#define GD_TOOL_LIST(X)          \
  X(scene::SceneCreateTool)      \
  X(scene::SceneDeleteTool)      \
  X(resource::RenameResourceTool) \
  ...
```

`register_all_tools` 最终态：`registry.install<...>` → 遍历 `registry.all()` 填充 ToolCatalog + Bm25Index + `server.RegisterTool`（据 `IExportGuard`/`IAsync` 统一包装）。**新增工具 = 新增 1 类 + 清单 1 行 + CMake 1 行**。

## 元工具的递归处理

`search_tools`/`list_categories`/`get_tool_detail` 依赖全量 registry，`call_tool`/`batch_execute`/`code_execute` 依赖全表——统一继承 `ToolBase`，经构造函数注入 `ToolRegistry&`/`Bm25Index&`，用 `add_meta()` 排在所有域工具**之后**注册，消除"自己列自己"的递归。

## 迁移路线

1. **加层不改**：新增 `ToolBase`/`ArgReader`/`make_ok/make_error`/`ToolRegistry` + `FnTool` 适配器（把现有 free-function handler 原样包进 registry），域工具零改动、行为不变。
2. **逐域迁移**：`scene → property → resources → …` 将 `FnTool` 替换为真类，每域跑 L1（注册数/schema）/L2（契约）确认 parity。
3. **元工具迁移**：最后 7 个。
4. **收尾清理**：删 `tool_defs.def` 双 include、`schema_*_ops` fill 表、旧 map；更新 CMakeLists、L1 断言与计数，删过时描述并同步本页状态为"已实现"与日志。

> 行为对等由 L1 断言数字（343 工具、SCHEMA_NONE/BASIC 计数）与 L2 契约用例守住；迁移后需重新核算工具数与文档同步。