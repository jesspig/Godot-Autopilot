---
type: 模块文档
title: REST 接口面
description: 同端口 REST 端点契约、请求与响应形状、Bearer 令牌认证、状态码与错误信封归一、宽松设计与已知限制
tags:
  - 模块
  - REST
  - 接口
  - 认证
  - 错误信封
timestamp: "2026-10-21T00:37:30+08:00"
resource:
  - src/core/api_surface.cpp
  - src/core/api_envelope.cpp
  - src/core/api_auth.cpp
  - src/core/api_token.cpp
  - src/core/api_server.cpp
  - src/core/http_transport.cpp
  - src/tools/tool_execute.cpp
---

# REST 接口面（`/api`）

> 审计日期：2026-10-21（随开放接口面落地批次首建——本批在**同一个监听端口**上新增 `/api` 前缀的 REST 端点集，与既有 `/mcp`（MCP 协议）并列；端点实现集中在 [`src/core/api_surface.cpp`](../../../src/core/api_surface.cpp)，认证在 [`src/core/api_auth.cpp`](../../../src/core/api_auth.cpp) 与 [`src/core/api_token.cpp`](../../../src/core/api_token.cpp)，错误信封归一在 [`src/core/api_envelope.cpp`](../../../src/core/api_envelope.cpp)，路由挂载在 [`src/core/api_server.cpp`](../../../src/core/api_server.cpp)；端口模型与实例登记见 [端口与实例登记](ports.md)，统一执行入口与授权三档见 [工具注册表](tools_registry.md)；本轮校订补齐三处接线事实——① 端点清单 7 条、分页窗口 500/2000、批量上限 256、工具名长度上限 128 四个常量补入正文；② 补「执行入口与接线现状」小节，说明 `api_surface` 的执行器槽位在生产路径上未注册、REST 实际经 `default_execute_tool` 派发，导出拦截与参数校验两阶段尚未接线；③ 状态码表补 429，并标注 413 的两条来源（传输层 `Content-Length` 拒收 vs 端点层 `payload_too_large`））。
> 覆盖范围：`src/core/api_surface.{hpp,cpp}`（端点与路由匹配）、`src/core/api_server.{hpp,cpp}`（路由注册）、`src/core/api_envelope.{hpp,cpp}`（错误信封与状态码映射）、`src/core/api_auth.{hpp,cpp}` 与 `src/core/api_token.{hpp,cpp}`（令牌签发与校验）、`src/core/http_transport.{hpp,cpp}` 与 [`src/core/http_parse.hpp`](../../../src/core/http_parse.hpp)（自建 HTTP 传输与查询串解析）。
> 统计口径：端点数以 `api_surface::build_index_document()` 的端点清单为准（数值以运行时统计为准，见 [测试体系](../tests.md)「数值核算总表」）。

## 定位

插件在同一 TCP 端口上同时提供两套协议面：

| 协议面 | 路径 | 传输 | 认证 | 工具可见范围 |
|---|---|---|---|---|
| MCP | `/mcp` | Streamable HTTP（JSON-RPC / SSE） | 无（环回 + 可信客户端模型） | 顶层只有 7 个元工具；域工具经 `call_tool` 代理 |
| REST | `/api` | 普通 HTTP（`application/json`） | Bearer 令牌（除 `/api/health`） | **全部 catalog 工具**（451 域 + `system_status` + 7 元） |

**REST 是 MCP 的超集**：MCP 面上可达的工具（458 = 7 元 + 451 域）在 REST 面上同样可达，且 REST 额外直接暴露 `system_status` 与 7 个元工具，无需 `call_tool` 代理。因此「先 REST 发现、再 MCP 编排」是合法的组合用法。

两套面共用同一监听端口与同一个 `CommandQueue`——所有 Godot API 仍只在主线程执行，REST 的工具调用同样经 [`src/tools/dispatch.cpp`](../../../src/tools/dispatch.cpp) 的带预算派发入口排队，不存在绕过主线程的第二条路径。

### 执行入口与接线现状

REST 面的工具调用走 `api_surface::execute_tool(name, args)`。该函数有一个**独立的执行器槽位**（`api_surface::register_tool_executor`），槽位为空时回落到默认实现 `default_execute_tool`：先做「不得在编辑器主线程直接执行」的自检（命中即返回 `internal_error`），再经 `dispatch::call_handler` 派发。

统一执行入口 [`src/tools/tool_execute.cpp`](../../../src/tools/tool_execute.cpp) 的 `tools::execute_tool_chain`（导出拦截 → 工具存在 → 参数校验 → 授权 → 派发 → 信封）已实现并由 L1 `tool_execute_test` 覆盖，但它注册进的是 `tools::tool_executor_slot()`，**与 `api_surface` 的槽位不是同一个**；`api_surface::register_tool_executor` 在生产路径上目前没有调用点（只有 `api_server_test` 注入假实现）。因此当前 REST 请求的实际派发链路是：

```
POST /api/tools/<name> → api_surface::handle_request → api_surface::execute_tool
  → default_execute_tool → dispatch::call_handler → SpecTool::execute（能力门在此）
  → ApiEnvelope::build（信封在 REST 层施加）
```

导出拦截与参数校验两阶段在 REST 面上尚未接线（MCP 面的导出拦截在 7 个元工具回调入口，`SpecTool::execute` 内的能力门对两套面都生效）。阶段定义与判定口径见 [工具注册表](tools_registry.md)「统一执行入口」。

## 端点清单

路径前缀固定为 `/api`（常量 `GDA_API_PATH`，[`src/core/config.hpp`](../../../src/core/config.hpp)）。

| 方法 | 路径 | 认证 | 说明 |
|---|---|---|---|
| GET | `/api` | 需要 | 服务信息与端点清单（`service` / `version` / `endpoints[]`） |
| GET | `/api/tools` | 需要 | 工具清单；支持 `query` / `category` / `tags` 过滤与 `limit` / `offset` 分页 |
| GET | `/api/tools/<工具名>` | 需要 | 单个工具的详情（描述、类别、标签、`side_effect`、`input_schema`、`dynamic`、`mutating`） |
| POST | `/api/tools/<工具名>` | 需要 | **请求体直接就是参数对象**，调用该工具 |
| POST | `/api/batch` | 需要 | 批量调用，body 为 `{"operations":[{"tool","args"}],"stop_on_error"}` |
| GET | `/api/health` | **免认证** | 存活探针：只返回存活状态、版本、端口、进程号 |
| GET | `/api/openapi.json` | 需要 | OpenAPI 3.0.3 文档（`bearerAuth` 安全方案，`/api/health` 显式声明免安全要求） |

**方法不支持**返回 405：**路由不存在**返回 404。工具名段会先做百分号解码，再校验字符集（仅允许字母、数字、下划线、连字符、点号，长度上限 **128**，即 `api_surface::kMaxToolNameLength`）；非法工具名一律按 404「未知路由」处理，不做参数回显。

## 请求形状

### 工具调用（`POST /api/tools/<工具名>`）

**请求体直接是参数对象**——没有 `{"name": ..., "arguments": {...}}` 这样的外层包装：

```json
{"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}
```

- 空体或纯空白体等价于 `{}`（不传参数）。
- 非法 JSON 或非对象顶层 → 400 `invalid_argument`。
- **未知参数宽容**：参数校验器把请求体里 schema 未声明的键**不视为错误**——只记一条 Warning（`unknown parameters ignored: ...`）后忽略；缺必填参数或类型不符才返回 400。该校验器挂在统一执行入口上，接线现状见下「执行入口与接线现状」——REST 面上目前由各 handler 内部的 `Args` 取参器兜底，实际宽容口径以工具自身实现为准。

### 批量调用（`POST /api/batch`）

```json
{
  "operations": [
    {"tool": "create_scene_node", "args": {"type": "Node2D", "name": "Player"}},
    {"tool": "property_set", "args": {"path": "Root/Player", "property": "position", "value": {"x": 1, "y": 2}}}
  ],
  "stop_on_error": true
}
```

- `operations` 必填且必须是数组，每项必须是对象且带非空 `tool` 名；`args` 可省略（缺省 `{}`），给了必须是对象。
- `stop_on_error` 可选布尔，默认 `true`：首个失败项之后停止，剩余项不执行。
- 单批操作数上限 **256**（`api_surface::kMaxBatchOperations`），超出即 400 `invalid_argument`（`operations exceeds the maximum of 256 items`）。
- 响应为 `results[]`（每项带 `tool` / `status`（`ok` 或 `error`）/ `data` 或 `error`，并附一份 `result` 原样载荷）+ `total` / `succeeded` / `failed` / `skipped`；`total` 是**已执行**的操作数，`skipped` 是因早停而未执行的条数。`stop_on_error` 触发时另带 `note` 说明停在了第几项。**批量本身恒返回 200**——子调用的失败体现在每个 `results[]` 项的 `status` 与 `error` 上，不改 HTTP 状态码。

### 工具清单过滤（`GET /api/tools`）

| 参数 | 语义 |
|---|---|
| `query` | 在名称、描述、类别、标签上做大小写不敏感包含匹配 |
| `category` | 类别精确匹配（大小写不敏感） |
| `tags` | 逗号分隔；**全部命中**才算通过（AND 语义） |
| `limit` | 每页条数；非负整数，`0` 视为默认；超过上限时按上限截断 |
| `offset` | 跳过的条数；非负整数 |

`limit` / `offset` 传非数字或负值 → 400 `invalid_argument`。响应体为 `{tools:[...], total, count, offset, limit}`，其中 `total` 是**过滤后**的总数（未分页），`count` 是本页条数。

分页窗口的三个常量（[`src/core/api_surface.hpp`](../../../src/core/api_surface.hpp)）：默认页大小 **500**（`kDefaultToolListLimit`）、单页上限 **2000**（`kMaxToolListLimit`）、批量操作数上限 **256**（`kMaxBatchOperations`）。`limit=0` 按默认页大小处理，超过 2000 按 2000 截断。

## 认证

除 `/api/health` 外，**所有端点**都要求 `Authorization: Bearer <令牌>`；缺失、格式不对或值不匹配一律返回 **401 `unauthorized`**。

- 令牌来源优先级：环境变量 `GODOT_AUTOPILOT_API_TOKEN` → 工程目录下的令牌文件。环境变量一旦设置即完全覆盖文件。
- 令牌为 **32 字节随机值的十六进制**（64 个小写十六进制字符），由 `std::random_device` 填充后逐字节转十六进制。
- 签发时机：插件启动并绑定端口后随实例登记一起签发，**写入实例登记供 CLI 读取**；令牌文件落在工程级存储目录内（见 [端口与实例登记](ports.md)）。
- 比对使用**常量时间**比较（`constant_time_equals`），长度不同直接判否。
- 令牌文件权限：POSIX 平台写盘后 `chmod` 收紧为 `S_IRUSR | S_IWUSR`（0600）；Windows 无 POSIX 权限位，改为记一条 Warning，说明令牌文件**继承 GDA 集中根目录的 ACL**。
- 令牌是**本机信任凭据**，不是对外暴露的凭据：它只配合环回绑定使用。REST 端点不要转发、不要代理、不要暴露到非环回地址。

## 状态码

| 状态码 | 触发条件 |
|---|---|
| 200 | 成功（含批量请求，子项失败不改状态码） |
| 400 | 参数错误：请求体非 JSON 对象、缺 `operations`、`stop_on_error` 非布尔、`limit`/`offset` 非法、工具参数校验失败 |
| 401 | 未认证：缺 Bearer 头、格式非 `Bearer <token>`、令牌不匹配 |
| 403 | 授权拒绝：工具所需能力未在允许列表中（`authorization_required`） |
| 404 | 工具或路由不存在：未知 `/api` 路径、未知工具名、非法工具名字符 |
| 405 | 方法不支持：该路由不接受此 HTTP 方法 |
| 409 | 冲突（`conflict` 码） |
| 413 | 请求体过大（`payload_too_large` 码；亦由传输层在 `Content-Length` 超过 4 MiB 时直接拒绝） |
| 422 | 工具业务错误：工具自身返回的错误且未归入上述更具体的码 |
| 429 | 请求过于频繁（`too_many_requests` 码） |
| 500 | 内部错误（`internal_error`） |
| 503 | 导出期（`export_in_progress`）或主线程队列不可用（`queue_unavailable`） |
| 504 | 主线程超时（`main_thread_timeout`） |

**归一规则**（[`src/core/api_envelope.cpp`](../../../src/core/api_envelope.cpp)）：状态码由错误**码**决定，不由文案决定。码的来源优先级为——工具结果顶层的 `code` 字段 → 顶层 `authorization_required` 字段 → `structured_error.code` → 由错误文案特征推断（`authorization required` / `authorization_required` → `authorization_required`；`unauthorized` 或 `not authorized` → `unauthorized`；`editor is exporting` / `export_in_progress` → `export_in_progress`；`main_thread_timeout` / `main thread timeout` / `main thread task failed` → `main_thread_timeout`；`queue_unavailable` → `queue_unavailable`；`invalid parameter` / `invalid argument` / `missing required parameter` / `missing required field` / `unknown parameter` → `invalid_argument`；`not found` 且含 `tool` → `tool_not_found`；`internal error` / `unexpected c++ exception` → `internal_error`；其余归 `tool_error` → 422）。

`405` / `409` / `413` / `429` / `404`（`not_found`）由端点层直接产生，优先于上面的归一表。

> [!note] 413 的产生位置
>
> 413 有两条来源：传输层按 `Content-Length` 直接拒收（[`src/core/http_parse.hpp`](../../../src/core/http_parse.hpp) 的 `kMaxBodyBytes = 4 MiB`，此时响应体是传输层的纯文本，不是 REST 信封），以及端点层按 `payload_too_large` 码生成的信封。前者在请求进端点之前发生，客户端拿不到 `code` 字段。

## 错误信封

工具失败统一带 `error` 与 `code` 两个字段。信封构造（`ApiEnvelope::build`）在工具结果已有 `error` 时追加：

| 字段 | 说明 |
|---|---|
| `error` | 错误文案（字符串；非字符串形态原样保留为 JSON） |
| `code` | 归一后的机器可读错误码 |
| `warnings` | 恒存在（无警告时为空数组） |
| `tool` | 触发该错误的工具名（结果里原本没有时补上） |
| `trace_id` | 当前 trace 标识（有则带） |
| `request_id` | 当前协议请求标识（有则带） |

**超时标记为已取消且不可重试**：`main_thread_timeout` 的信封带 `cancelled: true` 与 `retryable: false`——任务可能已被取消、副作用可能仍在主线程落地，因此不可盲目重试。**队列不可用**返回 `queue_unavailable`（`retryable: true`），表示编辑器队列已满或已关闭，属通道侧暂时不可用。

## 宽松设计要点

REST 面刻意做得比 MCP 面宽松，目的是让「不做 MCP 握手的客户端」也能用得上：

1. **无会话、无握手**：REST 面不要求 `initialize`，不发 `notifications/initialized`，不维护会话状态。请求彼此独立，服务端不记住调用方。
2. **请求体即参数**：工具参数对象就是请求体本身，不套 `arguments` 包装层（与 MCP 面的 `call_tool` 不同——后者是 `{name, arguments}` 双层结构）。
3. **未知参数宽容**：参数校验器（[`src/tools/argument_validator.cpp`](../../../src/tools/argument_validator.cpp)）把未声明的键**不视为错误**——只记一条 Warning（`unknown parameters ignored: ...`）后忽略；缺必填参数或类型不符才返回 400。该校验器挂在统一执行入口上（接线现状见上），因此在 REST 面上目前由各 handler 内部的 `Args` 取参器兜底，宽容口径以工具自身实现为准。
4. **批量不因单项失败而改状态码**：批量恒 200，逐项成败看 `results[]`。
5. **工具名宽松**：路径里的工具名允许百分号编码，且字符集比严格标识符宽（多一个点号）。
6. **免认证的存活口**：只有 `/api/health` 免认证，且只回存活、版本、端口、进程号——不泄露任何工程内容。

## 已知限制

- **统一执行入口尚未接入 REST 请求路径**：`tools::execute_tool_chain` 已实现（六阶段、含导出拦截与参数校验），但 `api_surface` 的执行器槽位在生产路径上没有注册点，REST 请求实际经 `default_execute_tool` → `dispatch::call_handler` 派发。因此**导出期对 REST 工具调用的拒绝尚未生效**，参数校验也仍由各 handler 内部的 `Args` 抛错兜底；授权门（`SpecTool::execute` 内）两套面都生效。接线口径见上「执行入口与接线现状」。
- **只读探针不区分「活着」与「能干活」**：`/api/health` 只报告进程存活与绑定端口，不做主线程健康检查；编辑器主线程卡死时该端点仍会返回 200。
- **REST 面没有流式与异步编排**：`code_execute` 之类长阻塞工具在 REST 面上同样受主线程等待预算约束，超时即 504，不提供 MCP 面的 `await_async` 语义。
- **批量上限固定**：单批操作数上限 256，超出返回 400（常量见 [测试体系](../tests.md)「数值核算总表」）。
- **工具清单分页默认只覆盖前若干条**：未显式传 `limit` 时按默认页大小 500 返回；`total` 给出过滤后的总数，客户端需自行翻页。
- **授权档位不随请求传入**：允许列表来自环境变量与插件配置，REST 请求体无法临时提权；未授权的工具返回 403。
- **令牌与实例登记同生共死**：令牌写在实例登记里，实例被删除即令牌失效；CLI 读不到实例登记时拿不到令牌（`gda api` 会直接报「no API token is available」）。
- **端口是唯一的**：REST 与 MCP 共用端口，端口被占用或被改时两者同时受影响；端口模型见 [端口与实例登记](ports.md)。

## 相关页面

- [端口与实例登记](ports.md) —— 同端口双端点、端口解析优先级、实例登记与全局索引
- [工具注册表](tools_registry.md) —— 统一执行入口、授权三档、错误码归一
- [T0 安全边界与并发契约](../security_contract.md) —— REST 认证、环回绑定、导出期拒绝、令牌文件权限
- [核心模块](core.md) —— 集中根目录、工程级存储、遗留数据兼容
- [CLI 载体](cli.md) —— `gda api` 子命令与端点发现
- [测试体系](../tests.md) —— 数值核算总表
