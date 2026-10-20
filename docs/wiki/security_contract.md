---
type: 安全与并发契约
title: T0 安全边界与并发契约
description: MCP 可信客户端与 REST 令牌认证、监听范围、授权三档、工具风险、Godot 主线程、队列生命周期、路径与响应大小的可执行约束
tags:
  - 安全
  - 并发
  - 契约
  - 认证
timestamp: "2026-10-21T00:37:30+08:00"
resource:
  - src/core/server_context.cpp
  - src/core/command_queue.hpp
  - src/core/log_persist.hpp
  - src/core/sanitize_policy.hpp
  - src/core/monitor.hpp
  - src/core/api_auth.cpp
  - src/core/api_token.cpp
  - src/core/api_surface.cpp
  - src/core/api_envelope.cpp
  - src/core/gda_home.cpp
  - src/core/port_resolver.cpp
  - src/core/instance_registry.cpp
  - src/tools/tool_base.hpp
  - src/tools/tool_spec.hpp
  - src/tools/authorization.hpp
  - src/tools/tool_execute.cpp
---

# T0 安全边界与并发契约

本页是 MCP 服务器进程内 GDExtension 的最小安全与并发契约。实现、评审和后续测试以本页条款为验收标准；代码事实见 [核心模块](modules/core.md) 与 [工具注册表](modules/tools_registry.md)。

> 审计日期：2026-10-21（10-21 随开放接口面落地批次同步——① §1 补两套协议面的信任模型差异（`/mcp` 仍无认证、`/api` 要 Bearer 令牌，认证不替代授权）；② §2 端口解析顺序由「环境变量 > 旧 config.json > 9527」改为「环境变量 > 工程级配置偏好 > 自动分配」，并写明显式端口绑定失败**不自动更换**；③ §3 风险表补 `destructive` 类，`side_effect` 枚举由八值改九值；④ §3.1 改写为**授权三档**（读档默认放行、写档**默认拒绝**、危险档需显式开启），能力名由四条扩为七条，配置路径改到集中根下并注明旧路径回退，**写档默认拒绝标为对外行为变更**；⑤ §3.2 配置路径同步；⑥ §3.3 补 REST 参考不是新脚本化通道的说明；⑦ §3.4 CLI state dir 路径改为 `<集中根>/cli/`；⑧ 新增 §3.5「REST 接口面的安全条款」（令牌来源与强度、常量时间比对、令牌文件权限、令牌不外泄、免认证面只有 health、认证不替代授权、环回约束不变、请求体上限、导出拦截未接线缺口）；⑨ §4 把 `main_thread_timeout` 的 `retryable` 由 true 改为 **false** 并说明副作用可能仍落地；⑩ §7 验收清单补 REST 认证、写档默认拒绝、超时不可重试、REST 导出接线四条。10-20 随 `code_execute` 超时上限收敛批次同步——长阻塞预算收敛为**单值 29000ms**（`GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS`，删去「回退常量 30000 / 实际生效 29000」双值写法与不存在的 `kDispatchWaitMaxMs` 符号引用）；§4 补记 `queue_unavailable` 结构化错误（字段 `error`/`code`/`retryable`）与 `code_execute` 的 `timeout_ms` 上界口径（`min(27000, 生效长阻塞预算)`）；10-20 随技能书调用阶梯收敛批次同步——技能书自带的脚本桥 `gda.mjs` 与 `references/script-access.md` 删除，§3.3 改写为「技能通道与手动兜底」：脚本化调用通道只剩 GDA CLI，脚本桥的安全包络条款并入 §3.4（环回绑定、退出码四态、`--godot` 引擎定位收敛、失败回滚不残留孤儿进程）；10-20 随执行预算与并发契约治理批次同步——① 修正两条与代码冲突的断言：§4「Godot `FileAccess` 仅在 flush 主线程路径调用」与「`call_tool` 编排回调不触碰 Godot API」均按修复后的事实重写；② §3 补记 `side_effect` 全量非空与能力门只按 `side_effect` 判定、§3.1 补记 `batch_execute` 纳入 `code_execute` 能力门（**对外行为变更**）；③ §4 补记长阻塞预算档、等待全预算化、`code_execute` 协作式执行期超时、`main_thread_guard` 静态守卫与 `ExportGuard` 实例级异常安全复位、导出期拒绝覆盖三类入口；④ 补记守卫的固有局限；⑤ §3.4 按本轮 CLI 启动路径修复改写「Windows 脱父」断言——`cmd /c start "" /b` 蹦床 + `netstat` 端口反查**已移除**，改为直连 `spawn` + `detached: true`、pid 取 `spawn` 返回值、存活判定改解析 `tasklist /FO CSV`，并补记启动失败回滚三态与可执行补救指令。详见 [changelog/2026-10-20-log.md](changelog/2026-10-20-log.md)；原记（2026-10-19）：随 CLI 引擎定位收窄批次同步——`gda start` 不再读 `GODOT_PATH` 与 `<project>/.env`（前者仅 L2 引擎测试使用，生产环境不存在），可执行文件链收敛为 `--godot` > PATH，未命中即报错提示 `--godot`；新增 `cli/tests/process.test.ts` 3 项（TS 108→111、expect 301→310）；原记（2026-10-18）：随 CLI 载体落地批次同步——§3.3 补记 SDK 0.3.7 握手升级与双载体修复，新增 §3.4 CLI 安全包络；详见 [changelog/2026-10-18-log.md](changelog/2026-10-18-log.md)）

## 1. 可信客户端模型

- 默认模型是“同一用户账户下、同一台机器上的受信 MCP 客户端”。默认监听 `127.0.0.1:9527/mcp`，这不是身份认证；能够连接本机端口的其他本地进程也视为可调用方。
- 服务器不应把客户端名称、版本或 MCP 初始化信息当作认证凭据。日志中的客户端信息只用于诊断，不改变授权边界。
- 暴露给非环回地址等价于把编辑器控制权暴露给网络可达方；在没有认证、授权和加密层时不得作为安全的远程部署方式。
- **两套协议面的信任模型不同**（10-21 起）：同一端口上并存 `/mcp`（MCP）与 `/api`（REST）。
  - **`/mcp` 仍无认证**——沿用「环回 + 可信客户端」模型不变。
  - **`/api` 要求 Bearer 令牌**：除 `/api/health` 外所有端点都要 `Authorization: Bearer <令牌>`，缺失或不匹配即 401。这是**纵深防御**，不是把 REST 面变成可对外暴露的服务——令牌只配合环回绑定使用，仍禁止把端口转发、代理或暴露到非环回地址。
  - REST 面的令牌**不替代**授权门：认证解决「谁在调用」，授权（§3.1）解决「能做什么」，两者都过才放行。

## 2. 监听要求

- 默认必须绑定环回地址 `127.0.0.1`；`GODOT_AUTOPILOT_HOST` 是显式覆盖入口，不得静默改变默认值。
- 截至 2026-09-02 实现，非环回地址在 `ServerContext::start()` 直接拒绝启动并返回 `refusing non-loopback listen address` 错误，不会进入工具注册或传输启动；该策略避免无认证远程控制面。后续如需支持远程，需先提供应用层认证并显式允许。
- `GODOT_AUTOPILOT_HOST=0.0.0.0` 表示监听所有接口，只能作为明确的运维选择；文档、配置界面和诊断日志必须让该风险可见，当前实现会拒绝该值。
- 端口解析顺序（10-21 起改为三级）：**环境变量 `GODOT_AUTOPILOT_PORT` ＞ 工程级配置偏好（集中根下工程目录的 `config.json` 的 `port` 键）＞ 自动分配**。自动分配先试默认端口 9527，冲突则从 `20000 + (进程号 mod 10000)` 起顺延到 29999 并回绕。**显式指定端口（环境变量 / 配置偏好 / 面板 Apply）绑定失败时不自动更换**，直接启动失败并报 `the requested port N is unavailable: <原因>`；只有自动分配才顺延。监听地址与端点为独立配置，端点固定为 `/mcp` 与 `/api`，端口是唯一可配项。端口模型与候选序列见 [端口与实例登记](modules/ports.md)。
- 环回约束是**两层**，缺一不可，改任一层都要重新验证：
  - **绑定层**（本项目）：`ServerContext::start()` 用 `is_loopback_host()` 判定 `127.0.0.1`/`::1`/`[::1]`，其余值拒绝启动（`src/core/server_context.cpp:28-29`、`:75-76`）。
  - **请求层**（依赖）：mcp-cpp-sdk 在 HTTP 层校验请求 `Host` 头，默认白名单仅 `localhost`/`127.0.0.1`/`::1`（`Impl::IsLocalhostHost`），不匹配即拒绝；仅当构造 HTTP 服务时显式传入 `allowed_hosts` 才会改用自定义白名单，本项目未设置该选项。当前锁定版本见 `third_party/mcp-cpp-sdk` 子模块（tag `0.3.8`，commit `d79f9b16ffe512282a62b9bd6c786d0887ee6963`，由 `.gitmodules` 与 gitlink 确定）。
  - 因此“把绑定地址放宽”并不能让非环回 Host 的请求可用；反向的 DNS 重绑定（用本机主机名或局域网 IP 访问）由请求层拦下。历史上 2026-08-28 的 changelog 曾把 `GODOT_AUTOPILOT_HOST` 描述为“可覆盖为 `0.0.0.0`”，该表述已被现行拒绝行为取代。

## 3. 高风险工具分类

工具风险必须可从 `get_tool_detail` 的 `side_effect` 字段识别；新增会产生同类影响的工具必须在 `ToolSpec` 声明对应 `side_effect`（如 `SideEffect::WritesFile`）并按需加 `tool_flags::kMutating`，不得依赖调用方自行猜测。

**`side_effect` 恒非空（10-20 起）**：`side_effect_name()` 对 `SideEffect::None` 返回 `"none"`（此前返回空串），九个枚举值与兜底分支全部返回非空字符串，因此**任何**工具的 `side_effect` 都可被机器读取——「无副作用」表达为字面量 `"none"`，不再是「字段缺失或空串」这种二义状态。调用方判定规则因此简化为：读 `side_effect` 字符串，空值即表示契约缺口，不得用「空串=只读」反推。该不变量由两层测试锁定：L1 `register_all_test.cpp` 的 `EveryToolDetailReportsNonEmptySideEffect` 逐个工具经 `get_tool_detail` 断言非空（另有 `ToolDetailSideEffectIsMachineReadable` 断言 `code_execute` 精确等于 `code_execute`），L2 `tests/config/40_contract_assert.json` 经 MCP 通道复验同一断言。

| 风险类 | 机器可读标记/范围 | 处理要求 |
|---|---|---|
| 文件写入 | `writes_file` | 调用前确认路径在允许工程范围内；不得默认覆盖任意路径。 |
| 配置写入 | `writes_config` | 视为持久化变更；调用方应明确知道目标文件和可逆性。 |
| 用户可见提示 | `shows_alert` | 视为会阻塞或打断用户的操作；禁止在无明确请求时批量触发。 |
| 窗口/剪贴板/鼠标 | `modifies_window` | 视为影响编辑器或桌面状态的副作用；含编辑器 UI 元素点击/文本/快捷键注入与合成鼠标输入（09-15 扩展、09-16 增 `select_scene_tree_node`，共 20 个）。 |
| 进程与环境 | `process` | 视为最高风险；包括启动、终止、构建、打开 OS 路径和修改环境。 |
| 任意脚本 | `code_execute`（`code_execute` / `batch_execute` / `execute_script` / 6 个 `csharp_*` 反射工具） | 最高风险处理；脚本可读写场景、资源、文件并调用方法，未标记的等价脚本执行能力同样按此处理。 |
| 运行时变更 | `game_*` 输入、求值的 set/call、SceneTree 转发的 8 个 `*_scene_tree_*` 工具等 | 视为目标游戏状态变更；必须受超时、取消和响应关联约束；无运行中游戏时返回结构化错误，不落回编辑器树。 |
| 破坏性操作 | `destructive`（10-21 新增；当前唯一声明者 `delete_display_window`） | 销毁既有对象的不可逆操作；与 `modifies_window` 同属危险档，需显式授权，且不得在无明确请求时触发。 |

只读工具也必须遵守路径、结果大小和主线程条款；“无副作用”不等于“无需限制”。

**10-20 元工具副作用声明修正（对外行为变更）**：`code_execute` 与 `batch_execute` 两个元工具此前声明 `SideEffect::None`，现均声明 `SideEffect::CodeExecute`（`src/tools/register_all.cpp:504`、`:552`）。二者的实际行为本就是执行任意脚本/批量编排任意工具，声明与行为此前不一致。修正后 `batch_execute` **自身也受 `code_execute` 能力门保护**（此前它无任何能力门）；后果是**只授权了 `process` / `game_runtime` 而未授权 `code_execute` 的客户端无法使用 `batch_execute`**——这是收紧方向的行为变更，需要 `batch_execute` 的客户端须同步把 `code_execute` 加入授权列表。`code_execute` 本身此前已受能力门保护，声明修正只是把“按工具名特判”换成“按 `side_effect` 统一判定”，对外表现不变。

近期新增工具的分级（09-13）：`copy_resource_file` 注册为 `SideEffect::WritesFile` + `tool_flags::kMutating`（可覆盖已存在的 `dest_path`，属写文件类，须先确认路径在工程范围内）；`reload_resource` 未声明 `SideEffect`（仅以 `ResourceLoader CACHE_MODE_REPLACE` 刷新编辑器资源缓存，不写盘），其余约束仍按只读工具处理。

09-15 新增 13 个工具的分级：7 个编辑器 UI/输入工具（`click_editor_element`、`type_editor_element_text`、`run_editor_shortcut`、`click_input_mouse`、`scroll_input_mouse`、`drag_input_mouse`、`type_input_text`）标记 `ModifiesWindow`（`modifies_window` 清单 12→19，会移动物理光标并注入桌面输入）；游戏侧 `click_game_ui_element` 标记 `GameRuntime`（`game_runtime` 清单 5→6）；其余 5 个只读查询工具（`get_display_window_rect`、`get_scene_node_screen_rect`、`get_editor_viewport_geometry`、`get_editor_ui_elements`、`hit_test_editor_point`）不标副作用，仍按只读工具的路径/大小/主线程条款处理。本批未新增授权能力门（`ModifiesWindow` 不在 `code_execute`/`game_runtime`/`process` 能力集）。

09-29 E2E 韧性批次：8 个 SceneTree 工具（`set_scene_tree_pause`、`is_scene_tree_paused`、`reload_scene_tree_current_scene`、`set_scene_tree_debug_collisions_hint`、`call_scene_tree_group`、`notify_scene_tree_group`、`get_scene_tree_nodes_in_group`、`create_scene_tree_timer`）由操作编辑器树改为经游戏通道 `scene_tree` op 转发到运行中游戏进程，全部标记 `SideEffect::GameRuntime`（`game_runtime` 授权面 +8，含只读的 `is_scene_tree_paused`/`get_scene_tree_nodes_in_group`）；无游戏进程时返回结构化错误，编辑器树不再被这些工具触碰。本批未放宽任何默认安全姿态（授权仍默认拒绝、脱敏仍默认开启、监听仍仅环回）。

### 3.1 能力授权门（09-02 引入，09-13 扩展，10-20 收敛为纯 `side_effect` 判定，10-21 扩为三档）

高风险能力除 `side_effect` 标记外还受调用级授权门约束，检查发生在**每次工具调用**（无缓存）：

- 解析优先级：`GODOT_AUTOPILOT_ALLOW` 环境变量（逗号分隔，`all` 全放行）> 插件配置的 `allow` 键（**全局级** `config.json`，位于家目录集中根下；新路径缺该键时回退读旧 `user://godot_autopilot/config.json` 并注明来源）> **默认拒绝**；环境变量一旦设置即完全覆盖持久化配置。
- **能力门只按 `side_effect` 判定（10-20 起）**：`authorization::capability_for_tool`（`src/tools/tool_base.hpp`）**只读 `SideEffect`，不再按工具名特判**。此前该函数还额外匹配 `name == "code_execute"` / `"execute_script"` / `"execute_game_script"`，等于给「声明漏标 `side_effect` 的高风险工具」打补丁；补丁一旦与声明不一致就会出现「工具名在名单里但 `side_effect` 是 `none`」的两套真相。收敛后唯一真相是 `ToolSpec::side_effect`：声明对了，门就跟着对；声明错了，门就跟着错——因此**新增高风险工具必须声明 `side_effect`**，这正是 §3 的要求被强制执行的原因。收敛不放松任何现有门：`execute_script`（`SideEffect::CodeExecute`）、`execute_game_script`（`SideEffect::GameRuntime`）等原本靠名字特判入门的工具，其 `side_effect` 本就正确，门照旧生效。
- **授权三档（10-21 起）**：`authorization::tier_for_side_effect()` 按 `SideEffect` 分档，`capability_for_tool()` 把档位映射成能力名，`tier_permits()` 决定是否放行。

  | 档位 | 覆盖的 `side_effect` | 能力名 | 默认 | 放行条件 |
  |---|---|---|---|---|
  | **读**（Read） | `none` | `read` | **默认放行** | `tier_permits` 对 `read` 直接返回 true，无需进允许列表 |
  | **写**（Write） | `writes_file` / `writes_config` / `shows_alert` / `modifies_window` | `write` | **默认拒绝** | 允许列表含 `write` 或 `all` |
  | **危险**（Danger） | `process` / `code_execute` / `game_runtime` / `destructive` | `process` / `code_execute` / `game_runtime` / `destructive` | **默认拒绝，需显式开启** | 允许列表含对应能力名或 `all` |

  - **能力名全集**（`kKnownCapabilities`，7 项）：`read` / `write` / `process` / `code_execute` / `game_runtime` / `destructive` / `user_tools`。`user_tools` 是**独立于 `side_effect` 派生**的一条能力，由 `AutopilotTools`（用户脚本动态工具，`src/tools/autopilot_tools.cpp`）在注册与调用两侧检查——未授权时注册拒绝、调用返回 `user_tools` 授权错误。
  - **面板开关**只覆盖 `code_execute` / `game_runtime` / `user_tools` 三项（`capability_has_dock_toggle`）。**`write` 与 `destructive` 没有面板开关**——须用环境变量或手改配置的 `allow` 键。
- **写档默认拒绝是收紧方向的行为变更（10-21）**：`writes_file` / `writes_config` / `shows_alert` / `modifies_window` 类工具此前**不需要任何能力**即可调用，10-21 起需要允许列表含 `write`（或 `all`）。既有客户端若不做适配，调用写类工具会收到 403 `authorization_required`。**该变更必须写进版本说明与技能书**——技能书侧的同步见 [REST 接口面](modules/api.md) 与调用专册。
- 拒绝响应含 `error`、`code`（`authorization_required`）、`authorization_required`（能力名）与 `enable`（启用指引）四个字段，并写 Warning 日志（Tools 类，可经 `get_plugin_log` 读取）。
- 启用入口：环境变量（须重启引擎）或配置 `allow` 键；MCP Config 面板的 "Allow code_execute"、"Allow game_runtime" 与 "Allow user tools" 复选框分别管理对应能力，勾选/取消经 `authorization::allow_list_add`/`allow_list_remove`（`add` 对已生效项含 `all` 保持原样；`remove` 先把 `all` 展开为全部已知能力再逐项删除），写入配置后下一次调用生效、无需重启。`process` 与 `destructive` 无面板开关，仍须环境变量或手改 `allow` 键。拒绝响应的 `enable` 文案只对 `code_execute`/`game_runtime`/`user_tools` 提及 dock（`capability_has_dock_toggle`），其余仅给环境变量 + 重启指引。
- 拒绝可审计（09-20 起）：授权门命中时除返回结构化拒绝与 Warning 日志外，另记录一条 `TraceEvent`（`auth="denied"`、`error_code="denied"`、`duration_ms=0`，含工具名/类别/副作用/flags），随 trace jsonl 落盘，供事后区分"未授权被拒"与"handler 业务错误"；09-21 起 McpConfigDock 的 allow 变更另发 `monitor::security` 事件（与脱敏变更同口径）。

### 3.2 数据脱敏开关（09-20 引入）

工具调用参数会进入本地持久化与 trace 事件，故新增调用侧数据边界开关：

- 解析优先级：`GODOT_AUTOPILOT_DESENSITIZE` 环境变量（`0`/`false`/`off` 关闭，其余非空值开启）> 配置 `desensitize` 键（**全局级** `config.json`，位于家目录集中根下；新路径缺该键时回退读旧 `user://godot_autopilot/config.json` 并注明来源）> **默认开启**；入口 `sanitize_policy::initialize()` 读取一次，之后由 MCP Config 面板 "Desensitize data" 复选框实时改写并持久化（下一次工具调用生效，无需重启）。
- 开启（默认）：`args_digest` 剥离敏感键的字符串值（替换为 `<stripped len=N>`），长度上限 4000 字符；09-21 起敏感键判定为大小写不敏感的通用集合——`data`/`base64`/`script_content`/`token`/`secret`/`password`/`passwd`/`key`/`api_key`/`apikey`/`authorization`/`credential`/`cookie`/`session_token` 及 `*_token`/`_key`/`_secret`/`_password` 后缀；`traces/images/` 不落盘任何截图，`image_ref` 为空，jsonl 只保留 `image_hash`/`image_bytes`/`image_width`/`image_height`。
- 关闭：`args_digest` 保留原始参数，上限 64000 字符，超限置 `args_truncated`；PNG 结果以 `traces/images/trace-<session>-<span>-<kind>.png` 落盘并在 jsonl 记 `image_ref`。调用方应把它视为"把工程数据以明文/原始截图写入**本地存储目录**"，仅在明确需要取证时关闭。
- 环境变量与面板的作用范围：`initialize()` 在启动时一旦发现 env 就完全忽略配置 `desensitize` 键（与 `allow` 能力门同构）；但面板复选框在运行期直接调用 `sanitize_policy::set_enabled`，仍会改变本会话后续调用的实际口径（重启后 env 再次生效）。
- 变更可审计（09-21 起）：McpConfigDock 的脱敏切换与 allow 变更发 `monitor::security` 事件，其余交互（端口应用/生成配置/生成技能等）发 `monitor::ui_action`。

### 3.3 技能通道与手动兜底（09-25 新增）

技能书是 MCP 客户端失效时的说明书，同时是**唯一**的脚本化调用载体；而脚本化调用通道只有一条——GDA CLI（`bunx`、全局安装、独立二进制是同一载体的三种分发方式）。GDA CLI 不是第二服务端，它只经 MCP 协议与编辑器进程内的同一台服务器对话：

- 目标固定为 `127.0.0.1` 字面量（`src/mcp/client.ts` 的 `HOST = "127.0.0.1"`），仅端口可调（1–65535）；无 URL 选项、无代理配置，代理环境变量被刻意忽略，保持环回直连。
- 请求头仅 `Content-Type: application/json` 与 `Accept`，无代理配置、无认证头；响应内容只做 JSON 解析，永不求值或执行。
- 写盘只有两处：`--save-images DIR` 内的解码图片（自生成文件名）与 CLI 自身的 state dir（实例登记表与日志，见 §3.4）；`--args-file` 只读 JSON，不写盘。
- 技能文档禁手写 HTTP：技能文档的全部 .md 不得出现 `curl -` 与 `Invoke-WebRequest`（L1 逐文件断言），调用统一走 GDA CLI；无 CLI 可用则停止自动化、按 `manual-fallback.md` 出手动指南，不得编造请求。
- 握手：mcp-cpp-sdk 0.3.7 起严格要求完整 MCP 握手（`initialize` → `notifications/initialized`），旧懒握手只发 `notifications/initialized` 被拒 `-32600`；CLI 已升级为标准 `initialize` 流程（`840e648`），`test_cli_contract.py` 的 mock 服务器严格模拟该语义（未初始化时 `tools/call` 返回 400 + `-32600`）。
- **REST 参考是接口说明，不是新的脚本化通道（10-21）**：技能书另有一份 REST 接口参考（`references/rest-api.md`），示范 Python 与 JavaScript 两段最小调用。它服务于「客户端只能发普通 HTTP、无法做 MCP 握手」的场景；能跑 CLI 的机器仍应走 GDA CLI。参考中不得出现 `curl -` 与 `Invoke-WebRequest`。
- 本节不改变默认安全姿态：监听仍仅环回（§2）、高风险能力仍默认拒绝（§3.1）、脱敏仍默认开启（§3.2）。

### 3.4 CLI 安全包络（2026-10-18 新增）

gda CLI（Bun + TS 源码直跑，详见 [CLI 载体](modules/cli.md)）是技能书调用阶梯第二级，其安全包络如下：

- **目标固定为 `127.0.0.1` 字面量**（`src/mcp/client.ts` 的 `HOST = "127.0.0.1"`），仅端口可调（1–65535）；无 URL 选项、无代理配置。
- **退出码四态**：0 = 成功；1 = 工具失败（业务载荷含 `error`，读载荷不读传输层）；2 = 传输或环境失败（连接拒绝、超时、HTTP 错误、引擎缺失）；3 = 用法错误（未知命令、缺参、非法选项）。1 属业务层，2 与 3 属通道自身不可用——客户端据此决定重试还是改用法，不得把 2 读成「工具无返回」。
- **无 shell 拼接**：进程 spawn 全部经 argv 数组（`node:child_process` 的 `spawn`/`spawnSync`），不经过 shell 解释；参数均为字面量或数字 pid，无可注入面。
- **启动即脱父，pid 就是本次拉起的编辑器（10-20 修正）**：Windows 上的编辑器启动是**直连 `spawn(编辑器, args, { detached: true })`**（`cli/src/process.ts:68-102`）——`detached` 让编辑器脱离父进程与控制台，随后 `child.unref()`；`spawn` 的返回值即**本次拉起的编辑器自身 pid**。此前 Windows 走 `cmd /c start "" /b` 蹦床 + `netstat` 端口反查真实 pid 的做法**已移除**：端口反查是按端口/映像名/时间窗匹配，既可能匹配到无关进程，也拿不到「本次拉起」的确切 pid。现路径下**不再有任何端口反查**，登记表里的 pid 精确等于被拉起的编辑器。
- **存活判定与系统语言无关（10-20 修正）**：`isAlive` 在 Windows 上解析 `tasklist /FO CSV` 的 pid 字段（`cli/src/process.ts:26-39`，正则 `^"[^"]*","(\d+)"`），**不依赖输出文本**。此前靠「输出是否以 `INFO`/`消息` 开头」判断「无匹配任务」的做法在中文 Windows 上恒判为存活（实际输出是「信息:」）；现按 CSV 的 pid 列比对，行为与系统语言无关。进程表查不到时**保守返回存活**——不得因查不到就跳过终止动作而留下孤儿进程。
- **启动失败不留孤儿进程（10-20 修正）**：`gda start` 的失败回滚是**三态**——`the editor process (pid N) had already exited`（已自行退出）/ `the editor process (pid N) was terminated`（已终止）/ `the editor process (pid N) could not be terminated, stop it manually: taskkill /F /T /PID N`（Windows；POSIX 为 `kill -9 N`）。引擎不可执行时报 `failed to launch the editor executable: ENOENT: …` 并提示检查 `--godot` 路径。CLI 退出后编辑器仍存活（脱父语义），由 `cli/tests/detach.test.ts` 覆盖；三态与补救指令由 `cli/tests/process.test.ts` 覆盖。
- **状态文件仅落 state dir**：`instances.json`、`instances.lock`、`logs/` 全部限定在 `GODOT_AUTOPILOT_CLI_HOME` 覆写值或 `<集中根>/cli/`（集中根 = `GODOT_AUTOPILOT_HOME` 覆写 > `os.homedir()`，其下固定目录名 `.godot-autopilot`）解析出的目录内，不写工程目录、不写仓库。
- **生命周期动作约束**：`start` 只启动 Godot 可执行文件链（`--godot` > PATH）指向的编辑器进程，注入 `GODOT_AUTOPILOT_PORT` 环境变量；**引擎定位只此一条链**——**不读 `GODOT_PATH`**（该变量仅服务 L2 引擎测试，生产环境不存在，避免把测试引擎路径带入生产启动），**也不读 `<project>/.env`**；`stop` 默认优雅（`taskkill /PID` / `SIGTERM`），失败只报告并提示 `--force`，**强杀必须显式 `--force`**（`taskkill /F /T /PID` / `SIGKILL`），不静默升级。
- **不收集个人信息**：CLI 不读取工程内容、不上报任何遥测；唯一本地状态是实例登记表（id/project/port/pid/startedAt/headless/godot/log），且只存本机路径与 pid。
- 不改变默认安全姿态：监听仍仅环回（§2）、高风险能力仍默认拒绝（§3.1）、脱敏仍默认开启（§3.2）；CLI 自身不绕过 MCP 授权门（仍经 `call_tool` 受 `code_execute`/`game_runtime`/`user_tools` 门约束）。

### 3.5 REST 接口面的安全条款（10-21 新增）

REST 面（`/api`）与 MCP 面共用同一监听端口，但**认证要求不同**。条款如下：

- **令牌来源与强度**：令牌是 **32 字节随机值的十六进制**（64 个小写十六进制字符，由 `std::random_device` 填充后逐字节转十六进制，熵 ≥ 256 位）。来源优先级：`GODOT_AUTOPILOT_API_TOKEN` 环境变量 ＞ 工程级令牌文件 `<集中根>/projects/<工程目录>/api_token`。**环境变量一旦设置即完全覆盖文件**，此时既不签发新令牌也不写文件。
- **签发时机**：插件成功绑定端口后随实例登记一起签发（`ServerContext::write_instance_record` → `ApiAuth::issue_token`），并写入实例登记的 `api_token` 字段，供 CLI 读取。令牌文件写入失败或路径未解析时记一条 Error 日志，**REST 端点保持未认证状态**（令牌为空即所有受保护端点返回 401）。
- **比对方式**：`ApiAuth::verify` → `api_token::constant_time_equals`，逐字节异或累积后统一比较，**长度不同直接判否**；令牌为空一律判否。
- **令牌文件权限**：POSIX 平台写盘后 `chmod` 收紧为 `S_IRUSR | S_IWUSR`（0600），失败记一条 Warning；Windows 无 POSIX 权限位，改为记一条 Warning，明确说明令牌文件**继承 GDA 集中根目录的 ACL**。令牌文件内容是明文，任何能读该目录的进程都能取到令牌——它只配合同机环回使用。
- **令牌不外泄**：令牌只落在实例登记与工程级令牌文件两处，**不写环境变量、不写工程文件、不进日志**；`trace_recorder` 的敏感键集合已含 `token` / `*_token` 后缀，脱敏开启时 `api_token` 会被剥离。
- **免认证面只有一个**：`GET /api/health` 免认证，且只返回存活状态、服务名、版本、端口与进程号——**不泄露任何工程内容**。其余端点（含 `GET /api/openapi.json`）都要令牌。
- **认证不替代授权**：令牌通过后仍要走 §3.1 的能力门；未授权的工具返回 403（`authorization_required`），而不是 401。
- **环回约束不变**：REST 端点与 MCP 端点挂在同一个环回监听上，非环回地址在 `ServerContext::start()` 被拒绝；REST 请求层同样受传输层的 Host 校验约束。**不得转发、代理或把该端口暴露到非环回地址**——令牌只防「误连」，不构成远程部署方案。
- **请求体上限**：传输层按 `Content-Length` 拒绝超过 4 MiB 的请求体（413），防止用超大请求体耗尽编辑器内存。
- **已知缺口（10-21）**：统一执行入口 `tools::execute_tool_chain` 的**导出拦截阶段尚未接入 REST 请求路径**（`api_surface` 的执行器槽位在生产路径上无注册点），因此**导出期对 REST 工具调用的拒绝当前不生效**；导出期的 MCP 侧三类入口拒绝仍完整有效（见 §4 末条）。该缺口须在接线后复核，详见 [REST 接口面](modules/api.md)「已知限制」。

## 4. Godot API 与线程

- 所有 Godot API、场景/资源/编辑器对象访问和会触发引擎状态的操作，必须在 Godot 主线程执行。HTTP/SDK 线程不得直接调用。
- 主线程停摆快速失败（09-29 起）：插件节点进程模式为 `PROCESS_MODE_ALWAYS`（`src/main.cpp`），编辑器树暂停时 `_process()` 仍会排空队列；非主线程工具调用统一经 `dispatch::run_on_main_thread_with_budget`（`dispatch.cpp`）提交——默认预算 27000ms（`GODOT_AUTOPILOT_DISPATCH_TIMEOUT_MS`，钳制 1000-29000）、健康探针 `ping`/`system_status`（`tool_flags::kHealthProbe`）5000ms（`GODOT_AUTOPILOT_HEALTH_TIMEOUT_MS`）、长阻塞 `code_execute`/`batch_execute`（`tool_flags::kLongBlocking`）**29000ms**（见下条）；超时对未 drain 任务调 `CommandQueue::cancel(id)`（future 抛 `cancelled before execution`，计入 `Stats::cancelled`），返回结构化 `main_thread_timeout` 错误（**`retryable=false`**，含 `queue_depth`/`oldest_pending_ms`/`last_drain_age_ms`/`cancelled=true`/`cancelled_before_execution`）、写 Warning 日志并向 trace 即时追加 `dispatch_timeout` 事件（`LogPersist::write_trace_now`，纯 std `std::ofstream` 直接追加，OS 路径在 `init_session` 主线程求得，不触碰 Godot API）；编辑器队列缺失/已关闭/已满时返回结构化 `queue_unavailable` 错误（`error`/`code`/`retryable=true` 三字段，不进入等待、不在发起线程直跑 handler）；`server.RegisterTool()` 直连路径与 `call_tool` 代理路径共用该预算入口。`system_status.result.main_thread` 暴露 `last_drain_age_ms`/`queue_depth`/`queue_cancelled`/`queue_rejected_full`/`oldest_pending_ms` 供诊断。
- **超时不可盲目重试（10-21 变更）**：`main_thread_timeout` 的 `retryable` 由 `true` 改为 **false**，并新增 `cancelled: true` 与 `cancelled_before_execution` 两个字段。原因：超时后任务可能已被取消、但**副作用仍可能稍后在主线程落地**（取消命中的任务没跑，未命中的任务已被主线程取走）；此时重发可能造成双写。正确处置是**先读编辑器状态确认副作用是否已落地，再决定是否重发**。队列不可用（`queue_unavailable`）仍带 `retryable=true`——那属通道侧暂时不可用，重试安全。
- **长阻塞档不再无限等待（10-20 起）**：`main_thread_wait_budget_ms`（`src/tools/dispatch.cpp:288-298`）此前对 `kLongBlocking` 直接返回 `0`，语义是「无限等待」（`run_on_main_thread_with_budget` 见到 `budget_ms <= 0` 就 `future.get()` 无限阻塞）。现改为返回**长阻塞预算**：`GDA_DISPATCH_WAIT_LONG_BLOCKING_MS`（`src/core/config.hpp:20`，取值 `GDA_DISPATCH_WAIT_MAX_MS` = 29000），可由环境变量 `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 覆盖，经 `env_int_clamped` 钳制在 `[1000, 29000]`（`src/tools/dispatch.cpp:148-165`、`src/tools/dispatch.cpp:280-286`）。**未设环境变量时生效 29000ms**（常量即生效值，不再有「回退常量 30000 被钳制上限压到 29000」的双值情形）；`static_assert` 保证该常量不超过 `GDA_TRANSPORT_TIMEOUT_MS`（30000），使结构化超时结果仍能在 HTTP 传输超时前回到客户端。若长阻塞预算被传成非正数，`main_thread_wait_budget_ms` 回退到默认预算 27000ms（不再退化为无限等待）。该生效值同时决定 `code_execute` 的 `timeout_ms` 参数上界——取 `min(GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000, 生效长阻塞预算)`（`src/tools/code_exec_ops.cpp:464-470`），默认环境下为 27000ms。
- 标准路径是 `CommandQueue::submit()` 入队（带预算的调用走 `submit_tracked()` 以获得任务 id 与取消能力），由 `GodotAutopilotPlugin::_process()` 的 `drain()` 在主线程排空，再通过 `future` 返回结果。唯一排空点是插件 `_process()`；09-20 起 `_process()` 在 `drain()` 前先执行 `LogPersist::flush_on_main_thread()`（本地日志/trace 增量写盘，同为 Godot 文件 API，必须主线程），09-21 起再在其前执行 `perf_sampler::tick(delta)`（周期采样与请求超时看门狗）。
- trace 上下文跨线程纪律（09-20 起）：`dispatch::call_handler` 在提交侧 `capture_trace_context()`（纯 std 线程局部变量，不触碰 Godot API）、执行侧用 `ScopedTraceContext` 恢复，并用 `steady_clock` 测量排队等待（`queue_wait_ms`）；`LogPersist` 的入队缓冲与游标虽被多线程访问，但均有互斥保护且只存纯 std 字符串。
- **trace 截图与轨迹的落盘通道（10-20 修正）**：`LogPersist` 内所有 Godot 文件 API 调用**只在主线程**执行，落盘路径唯一：`flush_on_main_thread()` → `append_persist_file()`（`godot::FileAccess` 写日志文件）、`append_trace_payload()`（纯 std `std::ofstream` 追加 trace jsonl）、`flush_trace_images()`（`godot::FileAccess` 写截图 PNG，`godot::DirAccess` 建 `traces/images/`）；会话初始化侧的 `init_session()` / `rotate_on_init()` / `prune_persist_dir()` 同样只在主线程调用。此前 `store_trace_image()` 会在调用方线程（`SpecTool::execute` 所在线程，即 transport 线程）直接 `godot::FileAccess::open` + `store_buffer` 落盘，属**在非主线程调用 Godot API**，与本节首条契约冲突。10-20 起改为：`store_trace_image()` 只做 base64 解码与**入主线程缓冲**（`enqueue_trace_image()`，互斥保护的 `image_buffer_`，上限 64 条 / 64 MiB，超限丢最旧并计 `dropped_trace_images`），真正写盘由主线程 `flush_on_main_thread()` 末端的 `flush_trace_images()` 完成；`write_trace_now()`（超时等即时 trace 追加）也收敛到同一条持锁通道 `append_trace_payload()`，与 `flush_on_main_thread()` 的 jsonl 追加共用同一把 `mutex_`，并发追加不会再撕裂 jsonl 行。**当前口径**：transport 线程只写纯 std 内存缓冲与 trace 事件，`FileAccess`/`DirAccess` 一律主线程。
- 协议侧钩子线程（09-21 起）：`ServerContext` 接入 `opts.on_request`（`monitor::begin_request`，含 `_meta` 的 traceparent 提取）与 `opts.on_notification`，响应侧经 `opts.outgoing_filters`（`mcp::FilterPipeline` + `MessageFilterFuncAdapter`）在 SDK worker 线程捕获出站 `JsonRpcResponse`/`JsonRpcErrorResponse` 完成 `monitor::end_request`/`note_protocol_error`（SDK 的 `on_response` 仅用于「服务端发起请求的应答」，不覆盖我方回复客户端的响应）；这些钩子与 `monitor` 门面均为纯 std、不触碰 Godot API（仅写 `TraceRecorder`/`LogSystem` 内存缓冲，落盘仍由主线程 flush），`request_id→trace/span` 关联表 `RequestRegistry` 有互斥保护（上限 4096、FIFO 淘汰）。
- 明确例外（09-13 起）：`call_tool` 元工具的编排回调在 MCP/transport 线程执行——等待运行时响应（`runtime_ops::wait_pending_response`）与截图定型不再占用主线程，从而保证等待期间调试器消息泵与编辑器主线程不被阻塞。**回调自身不触碰 Godot API** 这条仍然成立：领域工具 handler 经 `dispatch::call_handler` 路由回主线程，截图读盘经队列提交。**但「不触碰 Godot API」不等于「不写 trace」**：`call_tool` 在 transport 线程上直接执行 `SpecTool::execute`（`src/tools/tool_spec.hpp:168`），该函数在 transport 线程记录 `TraceEvent`（`monitor::tool_call`）、写 `LogSystem` 内存缓冲、并调用 `LogPersist::store_trace_image()` 把截图载荷**入队**（10-20 起只是入队，落盘已移到主线程）。因此 transport 线程侧的轨迹与日志写入是常态而非例外，契约的准确表述是：**transport 线程可写纯 std 的 trace/log 内存缓冲，但不得直接调用 Godot API，也不得直接落盘**。
- 只有不访问 Godot API 的纯 C++ 逻辑可留在 HTTP 线程；只读缓冲区若由代码明确保证线程安全，才可使用该例外。新增例外必须在代码和文档中同时说明。
- 主线程判定以队列首次 `drain()` 记录的线程为准；队列必须在插件正常生命周期内先完成主线程初始化，再处理依赖 Godot API 的任务。
- **等待全预算化（10-20 起）**：编辑器内不再有无限等待点。`CommandQueue` 新增 `submit_tracked_bounded(fn, budget_ms)`（`src/core/command_queue.hpp:260-290`）——带预算的提交，预算耗尽时 `cancel(id)` 并置 `timed_out`/`cancelled`；`dispatch::run_bounded_on_main_thread()`（`src/tools/dispatch.hpp:45-68`）是上层统一包装，资源读取（`resource_handlers.cpp` 8 处 handler、`debugger_resources.cpp` 7 处 handler）、MCP 提示词（`prompt_handlers.cpp` 7 个提示词）、用户脚本动态工具（`autopilot_tools.cpp` 3 处）、`code_execute`/`batch_execute` 的主线程快照（`code_exec_ops.cpp` 2 处）与游戏通道（`runtime_ops.cpp` 2 处）均已改走该入口，超时返回结构化 `main_thread_timeout`（资源/提示词路径分别映射为 `main_thread_timeout` / `resource_read_failed` 错误码）。`CommandQueue::execute_sync()`（`src/core/command_queue.hpp:292-298`）的实现仍是 `submit(...).get()` 无界等待形态，但**在 `src/` 内已无任何调用点**（`grep -rn execute_sync src/` 只命中定义本身）——它是遗留入口，**新增代码不得再调用它**，否则会重新引入无界等待。
- **协作式执行期超时（10-20 起）**：主线程等待预算只约束「排队等主线程」这一段，管不到「主线程上正在跑的长脚本」。`code_execute` 因此在包装脚本里注入截止时间常量 `__gda_deadline_ms` 与超时哨兵，并在循环体与入口函数注入检查点（`src/util/gdscript_wrap.hpp` 的 `deadline_declaration` / `deadline_check_line` / `deadline_break_line` / `deadline_loop_condition` / `inject_deadline_checks`），脚本自行在检查点退出；执行后按哨兵与实测耗时判定超时，返回结构化错误 `code="exec_timeout"`（`retryable=false`，含 `timeout_ms` / `execution_time_ms`，`src/tools/dispatch.cpp:333-343`）。游戏侧 eval 同理：宿主在 `eval_timeout_budget_ms()`（上限 `GDA_MAX_GAME_OP_TIMEOUT_MS` = 25000ms）内装配截止时间并注入检查点，超时返回 `structured_error.code="eval_timeout"`（附 `game_may_be_busy=true`——脚本无法被强行中断，游戏主线程要等脚本自己返回），被宿主取消时返回 `structured_error.code="eval_cancelled"`。协作式的边界要说清楚：**它依赖脚本执行到检查点**，不含 `await`、不返回的脚本无法被强制中止。
- **主线程契约静态守卫（10-20 新增）**：`tests/guard/main_thread_guard.py` 扫描 `src/` 下全部 `*.cpp`/`*.hpp`，按「文件级线程归类 + 作用域级放行」判定：每个源文件被显式归入主线程（`main`）、受管（`managed`）或外线程（`foreign`）三类；外线程文件里的 Godot API 调用（`godot::FileAccess` / `DirAccess` / `EditorInterface` / `Engine` / `SceneTree` / `ClassDB` / `ProjectSettings` / `Input` / `DisplayServer` / `OS` / `Performance` / `Resource` / `ResourceLoader` / `ResourceSaver` 等）默认违规，只有落在该文件显式登记的主线程作用域里才放行（当前仅 `src/core/log_persist.cpp` 登记了 `init_session` / `flush_on_main_thread` / `flush_trace_images` / `rotate_on_init` / `append_persist_file` / `prune_persist_dir`）。词法层面先剥离字符串/字符/raw 字面量与注释再做标识符匹配，避免把字面量里的 API 名误判为调用。守卫在 ctest 阶段拦（`main_thread_guard` 用例），与 `comment_guard` 同级；退出码 0 = 无违规、1 = 存在违规、2 = 扫描输入不可读。
- **守卫的固有局限（如实记录，不得夸大）**：判定是**文件级线程归类 + 作用域级放行**，**不做跨函数数据流分析**。因此：① 一个函数若被登记为「主线程作用域」，其内部对 Godot API 的调用一律放行，不校验它被谁调用——**同一文件内「主线程函数」被「transport 线程函数」调用时，作用域级放行会放行前者**；② 外线程文件调用一个位于主线程分类文件里的辅助函数，不会被本守卫拦下（跨文件调用链不参与判定）；③ 守卫只认 `godot::` 限定名的 API 调用，经别名、`using` 改写或第三方封装绕过的调用不在覆盖范围。它是防回归的**最小静态网**，不是线程安全的证明；线程安全的最终保证仍是「所有 Godot API 经 `CommandQueue` 在主线程执行」这条人工契约与 L2 行为测试。
- **导出期拒绝（10-20 加固）**：`ExportGuard` 的导出标志改为**实例级**（`ExportState state_` 是 `ExportGuard` 的成员，`is_exporting()` 遍历已注册实例查各自的标志），不再是全局单例标志；`_export_begin` 用 RAII 的 `ExportState::Scope` 包裹，`Scope` 析构时复位，因此**导出回调抛异常也不会残留 true**（`_export_end` 仍显式复位）。导出期拒绝现覆盖三类入口：**工具调用**（`src/tools/tool_invoke.cpp:182` 的 `invoke_tool` 与 `src/tools/register_all.cpp:656` 的元工具注册回调）、**MCP 资源**（`src/resources/resource_handlers.cpp` 9 处 + `src/resources/debugger_resources.cpp` 7 处）、**MCP 提示词**（`src/prompts/prompt_handlers.cpp:40`）。拒绝响应统一为 `editor is exporting; retry after export completes`（`dispatch::export_blocked_error_text()`），**10-21 起该拒绝带机器可读 `code="export_in_progress"`**。L2 侧的覆盖边界：`tests/config/41_thread_guard.json` **不覆盖导出期拒绝**——真实导出由引擎导出流程在编辑器主线程触发，MCP 通道无法可靠构造，该场景仍缺 L2 证据。**REST 面的导出期拒绝当前尚未接线**（统一执行入口未接入 REST 请求路径），见 §3.5 末条。

## 5. 停止与重载

- 停止顺序是：阻止服务器继续接受新请求，调用 `McpServer::Close()`，再调用 transport `Close()`，清理 handler 与 registry，最后释放 `ServerContext`。插件退出时先停服务器，再 `CommandQueue::close()` 并清空 `runtime_ops` 指针，最后释放其他插件组件（`main.cpp:_exit_tree`）。
- 重载/改端口是原子意图而非并行启动：先执行 `stop()`，再更新端口并 `start()`；`start()` 任意阶段失败会清理已创建的 transport/server 并重置 handler/registry，不会假报运行中。
- `CommandQueue` 已具备 `open/close/is_closed`、容量限制（默认 1024）、关闭时拒绝未执行任务并通过 `future` 抛异常、首次 `drain()` 固定主线程且错误线程 `drain()` 返回 false 的语义；09-29 起另有 `submit_tracked()`（返回任务 id + future）、`cancel(id)`（仅取消尚未 drain 的任务并在 future 抛 `cancelled before execution`）、`last_drain_age_ms()`（最近一次 drain 距今毫秒数，未 drain 为 -1）、`oldest_pending_age_ms()`（最旧排队任务年龄，空队为 -1）与 `Stats::cancelled` 计数；析构自动 `close()`。实现保证停止期间未完成 future 有明确结果，且不会在 `ServerContext` 已销毁后触碰 Godot 对象。
- 停止/重载与队列排空并发时，禁止产生悬垂引用、重复响应或把旧实例任务交给新实例。异步运行时请求必须以 request id 关联，`runtime_ops` 保证先登记 pending 再广播、超时后标记 `Cancelled`、迟到响应丢弃并告警。

## 6. 路径与响应大小

- 外部路径必须先通过 `util/project_path.hpp::normalize_project_path` 规范化并校验边界，再交给 Godot 文件/资源 API；拒绝路径穿越、绝对路径逃逸、未知 scheme 和工程范围外的写入，`text_ops`/`resource_ops` 全量入口已接入该校验。路径校验失败返回结构化 `error`，不得以空路径或当前目录兜底。
- MCP 工具面之外存在一处工程内写盘途径：编辑器面板 "Generate Skills" 按钮（`skill_gen`），固定写入 `<res://>/.agents/skills/`，仅覆盖 godot-autopilot 自有 9 册命名空间、不触碰其他文件（更新时先清理 `godot-autopilot-*` 前缀的既有目录再重建）；该途径不经 MCP 暴露、无远程触发面。
- 读写工具应区分“工程资源路径”（如 `res://`）与 OS 文件路径，`resource_ops` 仅允许 `res://`，`text_ops` 允许 `res://`/`user://`；禁止把一个 namespace 的路径直接拼接到另一个 namespace。返回路径应使用稳定、可复现的规范形式，避免泄露无必要的本机绝对路径。
- 每个入口必须限制输入深度、条目数、等待时间和输出字节数；限制应在解析/遍历前生效，截断结果必须带 `truncated`/`scan_truncated`/`scan_limit` 或等价可检测字段，不能静默丢数据。
- 已落地的边界包括：默认/最大超时 `5000/30000 ms`（通用工具口径；`code_execute` 的 `timeout_ms` 上界另算，为 `min(27000, 生效长阻塞派发预算)`、默认环境下 27000ms）、eval/错误文本截断 `8192` 字节、运行时错误/输出缓冲 `200/500` 条、场景树深度/节点数 `64/2000`、截图单边 `4096` 像素、`GDA_CAPTURE_MAX_PNG_BYTES=8 MiB`、`GDA_VARIANT_MAX_STRING_BYTES=64 KiB`、`GDA_VARIANT_MAX_ARRAY_ELEMENTS=10000`、`GDA_MAX_JSON_RESPONSE_BYTES=4 MiB`、`GDA_SCAN_MAX_FILES=10000`/`FILE_BYTES=2 MiB`/`TOTAL_BYTES=32 MiB`/`DEPTH=64`、`batch_execute`/`sequence` 上限 `256`。这些是实现上限，不是允许无限扩大的理由；新增响应应复用同一原则并避免把大结果一次性构造成无限 JSON。
- 本地持久化边界（09-20 起）：日志/trace 两目录（外加 `traces/images/`）各保留最近 `20` 个文件且总量 ≤ `50 MiB`（`LogPersist::should_prune`，会话初始化时按 mtime 从旧到新修剪）；内存侧日志缓冲 `LogSystem::MAX_ENTRIES=10000`、trace 缓冲 `TraceRecorder::kCapacity=20000`、待落盘队列 `kBufferCap=20000`（超限丢最旧）；**trace 截图入队缓冲（10-20 新增）`kTraceImageBufferMaxEntries=64` / `kTraceImageBufferMaxBytes=64 MiB`（超限丢最旧并计 `dropped_trace_images`，该计数经 `LogPersist::health_summary()` 对外暴露）**；单条日志 detail 上限 `8192` 字符，`args_digest` 按脱敏开关取 `4000`/`64000` 字符；写失败一次告警后停用本会话持久化，不无限重试。

## 7. 验收清单

- [ ] 默认启动日志和配置可证明监听为 `127.0.0.1`；非环回监听只能由显式配置触发。
- [ ] 每个高风险工具都能通过 `get_tool_detail.side_effect` 或本页补充规则识别；**任何**工具的 `side_effect` 都非空（无副作用者为字面量 `"none"`）；破坏性工具声明 `destructive`。
- [ ] 能力门只按 `side_effect` 判定：改一个工具的 `side_effect` 即改变它受哪个能力门约束，不存在「按工具名特判」的旁路；**写档默认拒绝**（未授权时写类工具返回 403），危险档需显式开启。
- [ ] REST 面的受保护端点拒绝无令牌请求（401），令牌比对为常量时间，令牌文件权限已收紧（POSIX 0600 / Windows 记 Warning 说明继承 ACL），令牌不出现在日志与 trace 中。
- [ ] HTTP 线程路径中没有直接 Godot API 调用；任务只由主线程 `drain()` 执行；trace 截图与 jsonl 落盘只在主线程发生。
- [ ] 编辑器内不存在无界等待点：所有主线程等待都带预算（`execute_sync` 无调用点，`kLongBlocking` 有上限）；超时结果标 `retryable=false` 且带 `cancelled`，不得按可重试处理。
- [ ] 主线程停摆时工具调用快速失败并返回可诊断的结构化错误（`main_thread_timeout` + 队列深度/心跳/取消标记），默认安全姿态未被放宽。
- [ ] 长脚本有执行期兜底：`code_execute` 超时返回 `exec_timeout`，游戏侧 eval 超时/取消返回 `eval_timeout` / `eval_cancelled`。
- [ ] 导出期三类入口（工具调用 / MCP 资源 / MCP 提示词）均被拒绝；导出回调异常后标志已复位；**REST 面的导出期拒绝已接线**（当前未接线，见 §3.5）。
- [ ] stop/restart 的请求接入、队列任务、future、异步响应和对象释放顺序有可观察且不悬挂的结果。
- [ ] 路径边界、遍历/输入上限和响应截断行为均可被调用方检测。
- [ ] 本地日志/trace 落盘符合边界约定：两目录保留上限生效；默认脱敏开启时 jsonl 不含内联 base64，`args_digest` 已剥离敏感字段；关闭脱敏仅由显式配置触发。
- [ ] 技能通道符合 §3.3 包络：脚本化调用只有 GDA CLI，目标为 `127.0.0.1` 仅端口可调、无代理与认证头、响应不执行、写盘仅限 `--save-images` 目录与 CLI state dir；技能文档无手写 HTTP 请求。
- [ ] CLI 符合 §3.4 包络：目标为 `127.0.0.1` 字面量、spawn 无 shell 拼接、退出码四态（0/1/2/3）语义稳定、状态文件仅落 state dir、`stop` 强杀须显式 `--force`、不收集个人信息；引擎定位只走 `--godot` > PATH 一条链（不读 `GODOT_PATH` 与 `<project>/.env`）；启动路径为直连 `spawn` + `detached` 脱父且 pid 取自 `spawn` 返回值（无端口反查），存活判定与系统语言无关，`gda start` 失败回滚不留孤儿进程。

## 相关页面

- [核心模块](modules/core.md)
- [工具注册表](modules/tools_registry.md)
- [REST 接口面](modules/api.md)（§3.5 的契约细节）
- [端口与实例登记](modules/ports.md)（端口解析与实例登记）
- [入口与运行时桥接](modules/entry_runtime.md)
- [CLI 载体](modules/cli.md)
- [工程约定](conventions.md)
- [测试体系](tests.md)（数值核算总表）
- [2026-10-21 维护日志](changelog/2026-10-21-log.md)
