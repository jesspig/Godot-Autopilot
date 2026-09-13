---
type: 安全与并发契约
title: T0 安全边界与并发契约
description: MCP 可信客户端、监听范围、工具风险、Godot 主线程、队列生命周期、路径与响应大小的可执行约束
tags:
  - 安全
  - 并发
  - 契约
timestamp: "2026-09-13T17:34:29+08:00"
resource:
  - src/core/server_context.cpp
  - src/core/command_queue.hpp
  - src/tools/tool_base.hpp
  - src/tools/meta_tools.hpp
---

# T0 安全边界与并发契约

本页是 MCP 服务器进程内 GDExtension 的最小安全与并发契约。实现、评审和后续测试以本页条款为验收标准；代码事实见 [核心模块](modules/core.md) 与 [工具注册表](modules/tools_registry.md)。

## 1. 可信客户端模型

- 默认模型是“同一用户账户下、同一台机器上的受信 MCP 客户端”。默认监听 `127.0.0.1:9527/mcp`，这不是身份认证；能够连接本机端口的其他本地进程也视为可调用方。
- 服务器不应把客户端名称、版本或 MCP 初始化信息当作认证凭据。日志中的客户端信息只用于诊断，不改变授权边界。
- 暴露给非环回地址等价于把编辑器控制权暴露给网络可达方；在没有认证、授权和加密层时不得作为安全的远程部署方式。

## 2. 监听要求

- 默认必须绑定环回地址 `127.0.0.1`；`GODOT_AUTOPILOT_HOST` 是显式覆盖入口，不得静默改变默认值。
- 截至 2026-09-02 实现，非环回地址在 `ServerContext::start()` 直接拒绝启动并返回 `refusing non-loopback listen address` 错误，不会进入工具注册或传输启动；该策略避免无认证远程控制面。后续如需支持远程，需先提供应用层认证并显式允许。
- `GODOT_AUTOPILOT_HOST=0.0.0.0` 表示监听所有接口，只能作为明确的运维选择；文档、配置界面和诊断日志必须让该风险可见，当前实现会拒绝该值。
- 端口解析顺序保持为环境变量 > `user://godot_autopilot/config.json` > `9527`；监听地址与端点为独立配置，端点固定为 `/mcp`。

## 3. 高风险工具分类

工具风险必须可从 `get_tool_detail` 的 `side_effect` 字段识别；新增会产生同类影响的工具必须标记 `GDA_TOOL_CLASS_SIDE`，不得依赖调用方自行猜测。

| 风险类 | 机器可读标记/范围 | 处理要求 |
|---|---|---|
| 文件写入 | `writes_file` | 调用前确认路径在允许工程范围内；不得默认覆盖任意路径。 |
| 配置写入 | `writes_config` | 视为持久化变更；调用方应明确知道目标文件和可逆性。 |
| 用户可见提示 | `shows_alert` | 视为会阻塞或打断用户的操作；禁止在无明确请求时批量触发。 |
| 窗口/剪贴板/鼠标 | `modifies_window` | 视为影响编辑器或桌面状态的副作用。 |
| 进程与环境 | `process` | 视为最高风险；包括启动、终止、构建、打开 OS 路径和修改环境。 |
| 任意脚本 | `code_execute`（`execute_script`） | 最高风险处理；脚本可读写场景、资源、文件并调用方法，未标记的等价脚本执行能力同样按此处理。 |
| 运行时变更 | `game_*` 输入、求值的 set/call 等 | 视为目标游戏状态变更；必须受超时、取消和响应关联约束。 |

只读工具也必须遵守路径、结果大小和主线程条款；“无副作用”不等于“无需限制”。

近期新增工具的分级（09-13）：`copy_resource_file` 注册为 `GDA_TOOL_CLASS_SIDE` + `SideEffect::WritesFile`（可覆盖已存在的 `dest_path`，属写文件类，须先确认路径在工程范围内）；`reload_resource` 未标记 `SideEffect`（仅以 `ResourceLoader CACHE_MODE_REPLACE` 刷新编辑器资源缓存，不写盘），其余约束仍按只读工具处理。

### 3.1 能力授权门（09-02 引入，09-13 扩展）

高风险能力除 `side_effect` 标记外还受调用级授权门约束，检查发生在**每次工具调用**（无缓存）：

- 解析优先级：`GODOT_AUTOPILOT_ALLOW` 环境变量（逗号分隔，`all` 全放行）> 插件配置 `user://godot_autopilot/config.json` 的 `allow` 键 > **默认拒绝**；环境变量一旦设置即完全覆盖持久化配置。
- 能力名与工具映射（`tool_base.hpp:capability_for_tool`）：`code_execute`（`code_execute`、`execute_script`）、`game_runtime`（`execute_game_script`、`reload_game_scripts`、`queue_game_input`、`wait_game_input`、`sequence_game_inputs`）、`process`（`SideEffect::Process` 标记的 6 个：`build_csharp_assembly`、`create_os_process`、`execute_os_process`、`kill_os_process`、`open_os_path`、`set_os_environment`）。
- 拒绝响应含 `error`、`authorization_required`（能力名）与 `enable`（启用指引）三个字段，并写 Warning 日志（Tools 类，可经 `get_plugin_log` 读取）。
- 启用入口：环境变量（须重启引擎）或配置 `allow` 键；MCP Config 面板的 "Allow code_execute" 复选框只管理 `code_execute` 能力，写入配置后下一次调用生效、无需重启（`process`/`game_runtime` 仍须环境变量或手改配置）。

## 4. Godot API 与线程

- 所有 Godot API、场景/资源/编辑器对象访问和会触发引擎状态的操作，必须在 Godot 主线程执行。HTTP/SDK 线程不得直接调用。
- 标准路径是 `CommandQueue::submit()` 入队，由 `GodotAutopilotPlugin::_process()` 的 `drain()` 在主线程排空，再通过 `future` 返回结果。唯一排空点是插件 `_process()`。
- 明确例外（09-13 起）：`call_tool` 元工具的编排回调在 MCP 线程执行——等待运行时响应（`runtime_ops::wait_pending_response`）与截图定型不再经 `execute_sync` 占用主线程；回调自身不触碰 Godot API（领域工具 handler 经 `dispatch` 路由回主线程、截图读盘经 `queue.submit`），从而保证等待期间调试器消息泵与编辑器主线程不被阻塞。
- 只有不访问 Godot API 的纯 C++ 逻辑可留在 HTTP 线程；只读缓冲区若由代码明确保证线程安全，才可使用该例外。新增例外必须在代码和文档中同时说明。
- 主线程判定以队列首次 `drain()` 记录的线程为准；队列必须在插件正常生命周期内先完成主线程初始化，再处理依赖 Godot API 的任务。

## 5. 停止与重载

- 停止顺序是：阻止服务器继续接受新请求，调用 `McpServer::Close()`，再调用 transport `Close()`，清理 handler 与 registry，最后释放 `ServerContext`。插件退出时先停服务器，再 `CommandQueue::close()` 并清空 `runtime_ops` 指针，最后释放其他插件组件（`main.cpp:_exit_tree`）。
- 重载/改端口是原子意图而非并行启动：先执行 `stop()`，再更新端口并 `start()`；`start()` 任意阶段失败会清理已创建的 transport/server 并重置 handler/registry，不会假报运行中。
- `CommandQueue` 已具备 `open/close/is_closed`、容量限制（默认 1024）、关闭时拒绝未执行任务并通过 `future` 抛异常、首次 `drain()` 固定主线程且错误线程 `drain()` 返回 false 的语义；析构自动 `close()`。实现保证停止期间未完成 future 有明确结果，且不会在 `ServerContext` 已销毁后触碰 Godot 对象。
- 停止/重载与队列排空并发时，禁止产生悬垂引用、重复响应或把旧实例任务交给新实例。异步运行时请求必须以 request id 关联，`runtime_ops` 保证先登记 pending 再广播、超时后标记 `Cancelled`、迟到响应丢弃并告警。

## 6. 路径与响应大小

- 外部路径必须先通过 `util/project_path.hpp::normalize_project_path` 规范化并校验边界，再交给 Godot 文件/资源 API；拒绝路径穿越、绝对路径逃逸、未知 scheme 和工程范围外的写入，`text_ops`/`resource_ops` 全量入口已接入该校验。路径校验失败返回结构化 `error`，不得以空路径或当前目录兜底。
- MCP 工具面之外存在一处工程内写盘途径：编辑器面板 "Generate Skills" 按钮（`skill_gen`），固定写入 `<res://>/.agents/skills/`，仅覆盖 godot-autopilot 自有 8 册命名空间、不触碰其他文件（更新时先清理 `godot-autopilot-*` 前缀的既有目录再重建）；该途径不经 MCP 暴露、无远程触发面。
- 读写工具应区分“工程资源路径”（如 `res://`）与 OS 文件路径，`resource_ops` 仅允许 `res://`，`text_ops` 允许 `res://`/`user://`；禁止把一个 namespace 的路径直接拼接到另一个 namespace。返回路径应使用稳定、可复现的规范形式，避免泄露无必要的本机绝对路径。
- 每个入口必须限制输入深度、条目数、等待时间和输出字节数；限制应在解析/遍历前生效，截断结果必须带 `truncated`/`scan_truncated`/`scan_limit` 或等价可检测字段，不能静默丢数据。
- 已落地的边界包括：默认/最大超时 `5000/30000 ms`、eval/错误文本截断 `8192` 字节、运行时错误/输出缓冲 `200/500` 条、场景树深度/节点数 `64/2000`、截图单边 `4096` 像素、`GDA_CAPTURE_MAX_PNG_BYTES=8 MiB`、`GDA_VARIANT_MAX_STRING_BYTES=64 KiB`、`GDA_VARIANT_MAX_ARRAY_ELEMENTS=10000`、`GDA_MAX_JSON_RESPONSE_BYTES=4 MiB`、`GDA_SCAN_MAX_FILES=10000`/`FILE_BYTES=2 MiB`/`TOTAL_BYTES=32 MiB`/`DEPTH=64`、`batch_execute`/`sequence` 上限 `256`。这些是实现上限，不是允许无限扩大的理由；新增响应应复用同一原则并避免把大结果一次性构造成无限 JSON。

## 7. 验收清单

- [ ] 默认启动日志和配置可证明监听为 `127.0.0.1`；非环回监听只能由显式配置触发。
- [ ] 每个高风险工具都能通过 `get_tool_detail.side_effect` 或本页补充规则识别。
- [ ] HTTP 线程路径中没有直接 Godot API 调用；任务只由主线程 `drain()` 执行。
- [ ] stop/restart 的请求接入、队列任务、future、异步响应和对象释放顺序有可观察且不悬挂的结果。
- [ ] 路径边界、遍历/输入上限和响应截断行为均可被调用方检测。

## 相关页面

- [核心模块](modules/core.md)
- [工具注册表](modules/tools_registry.md)
- [入口与运行时桥接](modules/entry_runtime.md)
- [工程约定](conventions.md)
