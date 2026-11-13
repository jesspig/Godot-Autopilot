---
name: godot-autopilot-tools
description: "Godot Autopilot 的 CLI-first 调用参考：gda 命令与参数面、通过插件实例索引发现目标、实时 gda search/describe/tools list 与 gda call、作为兼容面的原生 MCP 工具、高风险副作用的 confirm 与 MCP annotations、错误水位协议与错误码、batch_execute/code_execute 语义、响应限制，以及动态用户工具。适用于查询调用协议事实。"
---

# Godot Autopilot 调用协议

本册是 GDA 的跨领域调用知识：通道阶梯、CLI 命令面、发现、确认、错误协议与
限制。本册不承载领域知识；场景、资源、脚本、运行时、服务器、内容与 C# 的
事实在各自领域册。

## 通道阶梯

1. **GDA CLI。** 有 shell 时一律用 `gda`。它是一等接口：从实例索引发现插件
   端口，优先本地 IPC，失败回退 HTTP，并输出 JSON。Windows、macOS、Linux
   都可用，退出码区分工具失败与传输失败。
2. **原生 MCP 工具。** 客户端已列出 GDA 工具时直接使用。它们是兼容面：
   完整 schema、结构化错误与图像内容。语义完全相同，只是传输不同。
3. **手工编辑器步骤。** 既无 shell 也无 MCP 客户端时，给用户精确的编辑器
   步骤，并要求回报结果。不要编造工具输出。

没有需要手写的 HTTP API，也没有代理命令。每次调用都走 `gda` 或客户端 MCP
工具。

## `gda` 命令面

完整文本以 `gda --help` 为准；下表是稳定形状。

| 命令 | 用途 |
|---|---|
| `gda ping` | 检查与 MCP 服务器的连通性。 |
| `gda status` | 显示服务器状态、工程、端口、传输与 `mcp_url`。 |
| `gda categories` | 列出工具分类。 |
| `gda search <query>` | 通过实时服务器搜索工具。 |
| `gda describe <tool>` | 显示一个实时工具的 schema 与 `side_effect`。 |
| `gda call <tool>` | 调用一个工具。 |
| `gda list` | 列出已知 Godot 实例与工程。 |
| `gda start` | 启动 Godot 编辑器并等待。 |
| `gda stop` | 按 id、端口或工程停止编辑器。 |
| `gda restart` | 用相同设置重启编辑器。 |
| `gda logs` | 显示实例日志。 |
| `gda skills list` | 列出已安装技能书。 |
| `gda skills show <name>` | 打印一册；`--file <rel>` 读取附属文件。 |
| `gda tools list` | 列出实时实例的全部直接工具；`--category`/`--tags` 可缩小范围。 |
| `gda projects` | 离线列出已知 Godot 工程。 |
| `gda cache list` | 列出缓存 manifest。 |
| `gda cache info` | 显示缓存根、条目数与大小。 |
| `gda cache clear` | 清理旧条目；需要 `--force`。 |
| `gda install skills` | 安装技能书到 `~/.agents/skills`。 |
| `gda install plugin` | 安装插件到工程。 |
| `gda update <target>` | 更新 `cli`、`skills`、`plugin` 或 `all`；`--check` 只检查。 |

常用全局参数：

- 目标选择：`--port <n>`（仅连接覆盖）、`--id <id>`、`--project <dir>`、
  `--socket <path>`、`--transport auto|ipc|http`、`--no-fallback`。
- 输入：`--args <json>`、`--args-file <path>`、`--confirm`、`--timeout <sec>`。
- 发现过滤：`--category <name>`、`--tags <a,b>`（用于 `gda search` 与 `gda tools list`）。
- 输出：`--json`、`--pretty`、`--raw`、`--save-images <dir>`。
- 生命周期：`--godot <path>`、`--headless`、`--play`、`--no-import`、
  `--wait <sec>`、`--force`、`--tail <n>`、`--probe <light|strict>`。
- 安装与更新：`--path <dir>`、`--source auto|online|cache|builtin`、
  `--offline`、`--zh`、`--all`、`--check`。

`--port` 只是连接覆盖，绝不是监听端口。端口由插件拥有。`--offline` 跳过网络，
使用缓存或内置技能书归档。`--zh` 选择中文技能书。`--force` 用于破坏性缓存
清理与覆盖。

退出码：

| 码 | 含义 |
|---|---|
| 0 | 成功。 |
| 1 | 工具已执行并返回失败信封。 |
| 2 | 传输或环境失败：无实例、端点不可用、超时。 |
| 3 | 用法错误：未知命令、参数或参数格式错误。 |

stdout 是 JSON，诊断走 stderr。退出码 2 表示通道不可用，不表示工具返回空。
退出码 2 之后不要报告编造的结果。

## 目标发现

CLI 按以下顺序解析目标：显式 `--socket`/`--port`、按 `--id` 查实例索引、
按 `--project` 或当前目录匹配存活实例、持久化插件记录。端口来自插件，不来自
默认值。`gda status --json` 显示解析结果：`port`、`port_source`、`mcp_url`、
`transport`、`ipc_kind`、`plugin_version`、`toolset_hash`。

解析失败时错误会说明原因。运行 `gda list --all` 查看已知工程，再
`gda start --project <dir>` 或打开编辑器。不要猜端口，也不要复用上一次会话
的端口。

## 发现协议——先搜索，禁止猜

1. `gda ping` 确认服务器响应。
2. `gda search <query>` 通过实时服务器按名称、描述、分类与标签搜索；用
   `--category` 或 `--tags` 缩小范围。服务端默认最多返回 20 条。
3. `gda describe <name>` 打印 schema、描述与 `side_effect` 标记。首次写入前
   必读。
4. `gda call <name> --args '<json>'` 执行；大参数用 `--args-file`。
5. `gda tools list` 以 `{tools, total}` 列出全部可直接调用的工具，排除 5 个
   MCP 协议工具。它用于全量清单，不用于关键词搜索。

发现只有实时通道：CLI 不保留离线工具目录。没有运行中的
实例时，用 `gda start --project <dir>` 启动。

先按对象搜索，再按操作搜索："tilemap cell"、"animation track"、
"import sidecar"。对象太宽时才加动词。下结论说工具不存在前，用同义词重试：
tile/cell、sprite/texture、log/output、delete/remove/erase、create/add/new。
不要发明看似合理的名字；未知工具会失败并提示先搜索。

示例：

```bash
gda describe create_scene_node
gda call create_scene_node --args '{"parent_path": "Root", "name": "Player", "type": "CharacterBody2D", "properties": {"position": {"x": 0, "y": -10}}}'
```

## 原生 MCP 工具

客户端列出 GDA 工具时，元工具是 `ping`、`search_tools`、`list_categories`、
`get_tool_detail`、`call_tool`、`batch_execute`、`code_execute`。所有领域工具
通过 `call_tool` 的 `name` 与 `arguments` 执行；`call_tool` 还接受顶层
`confirm` 布尔值。`search_tools` / `get_tool_detail` 的用法与
`gda search` / `gda describe` 相同。`search_tools` 默认最多 20 条；只有确需
全量枚举时才传 `limit: 0`。

## 确认与 annotations

高风险副作用需要显式确认。服务器在执行前返回
`{"code": "confirmation_required", "confirm_required": true, "side_effect":
"...", "hint": "..."}`。核对工具说明与目标后，用 `--confirm`（CLI）或
`confirm: true`（MCP）重试。需要确认的类：

- `destructive`：不可逆删除编辑器中已存在的对象，例如节点、动画、音频总线
  效果或显示窗口。
- `process`：启动、停止或以其他方式控制 OS 进程。
- `code_execute`：在编辑器中运行任意 GDScript（`code_execute`、
  `batch_execute`、`execute_script`）。
- `game_runtime`：进入运行中的游戏（`execute_game_script`、
  `queue_game_input`、`reload_game_scripts`）。

文件、配置与场景写入（`writes_file`、`writes_config`、`writes_scene`、
`shows_alert`、`modifies_window`）不需要确认，但仍会修改内容。每次写入后读回
目标。MCP 客户端还会收到工具 annotations；把 destructive 或 runtime 注解
当作“停下来核对”的信号。

不要用另一个效果相同的工具绕过确认错误，也不要把高风险调用藏进批处理。

## `batch_execute` 与 `code_execute`

- `batch_execute` 顺序执行 `operations` 列表（最多 256 条）。每项包含 `tool`
  与可选 `args`；`stop_on_error` 默认 true，`rollback_on_error` 可选。适合
  “设置五个属性再保存”这类确定性序列。包含异步游戏工具的批处理会被拒绝，
  除非传 `await_async` 并直接调用批处理。
- `code_execute` 在编辑器运行 GDScript。没有行首 `func ` 时把源码内联到一个
  入口函数；出现 `func ` 行时保留源码，用 `function_name`（默认 `_run`）选择
  入口。`timeout_ms` 默认 5000，上限以 schema 为准；超时在脚本运行时强制
  执行。适合批处理无法表达的循环、数学与计算值。
- 判定：少量现有调用 → `batch_execute`；循环、数学、条件或超大负载 →
  `code_execute`。

## 错误协议与水位

每个工具失败都是带 `error` 键的 JSON 对象。部分错误带稳定 `code`；按 code
分支，不要按自然语言消息分支。每个元工具响应还带
`new_errors_since_last_call`，表示自上次调用以来记录的新错误数，包含本次调用
自身产生的错误。

- 水位只读一次，读后归零。即使调用成功也不要跳过。
- 运行中游戏打印的错误也计入。
- 每次写入后、下一次调用前读水位。非 0 表示停下、取出真实日志行、修复或
  解释后再继续。

软错误在编辑器导入或扫描资源时携带 `retryable: true` 与 `retry_after_ms`。
至少等待建议间隔后重试同一调用。工程导出期间所有工具调用都会被拒绝；等待
导出结束，不要紧循环重试。

稳定错误码：

| Code | 触发条件 | 是否重试 |
|---|---|---|
| `exec_timeout` | `code_execute` 脚本超过 `timeout_ms`。 | 否；拆分任务或在 schema 上限内提高超时。 |
| `eval_timeout` | 游戏侧 eval 超过宿主等待。 | 不要立即重试；游戏可能仍在执行。 |
| `eval_cancelled` | 游戏侧 eval 被取消句柄拆除。 | 否；仍需要结果时重新发起。 |
| `main_thread_timeout` | 编辑器主线程未在预算内处理调用。 | 否；调用已取消，但副作用可能仍会落地，先读状态。 |
| `queue_unavailable` | 编辑器主线程队列已满或正在关闭。 | 是，稍后。 |
| `confirmation_required` | 高风险工具未带确认。 | 仅核对目标后，用 `--confirm` 重试。 |

没有 code 的失败是普通工具错误：读错误文本。`main_thread_timeout` 会报告
等待时长，可能还报告队列深度；`exec_timeout` 会报告实际执行时间与超限值。

用水位确认修复：复现失败、诊断、修复、重跑失败调用，并持续调用元工具直到
水位回到 0。成功调用后水位为 0，是“修复没有引入新错误”的收尾证据。

## 限制

- 默认操作超时 5000 ms。上限以工具 schema 为准；编辑器调度预算可能按部署
  下调，读 schema 而不是假设固定值。游戏操作把 `timeout_ms` 限制在 25000 ms。
- 编辑器调用按等待预算分三档：普通工具、健康探测（`ping`、`system_status`，
  5000 ms）与长阻塞元工具（`code_execute`、`batch_execute`）。长任务可能以
  `main_thread_timeout` 结束，而不是永久挂起。
- JSON 响应上限 4 MiB，截断可检测；信任完整性前先检查截断字段。
- `batch_execute` 每次最多 256 条操作。
- 最多 8 个并发在途请求；超限会快速失败。
- 工具名最长 128 个字符。

## 动态用户工具

工程脚本可以通过 `user_tools` 机制注册额外领域工具。它们走同一发现协议与
同一 `gda call` / `call_tool` 路径，并标记为 dynamic。目录扫描约定默认为
`res://addons/godot-autopilot-tools`，插件在用户工具示例目录提供样例。注册与
调用遵守与内置工具相同的确认规则。

## 手工回退

既无 CLI 也无 MCP 客户端时，给用户一段可检查的简短步骤：打开工程、启用插件、
在编辑器中执行动作，然后报告精确的节点路径、属性值或日志行。一次只要求一个
观察，不要编造截图、日志行或水位。首选恢复 CLI：安装独立 `gda` 二进制，然后
正常继续。

## 参见

- `godot-autopilot-setup` - 安装、首次连接、MCP 配置。
- `godot-autopilot` - 连接模型、路由、观察循环。
- `godot-autopilot-scene-system` - 场景、属性、信号、撤销。
- `godot-autopilot-resources` - 资源、导入、重命名与移动。
- `godot-autopilot-scripting` - GDScript 执行通道。
- `godot-autopilot-runtime` - 游戏进程、输入、日志、调试器。
- `godot-autopilot-servers` - 服务器 RID 工具。
- `godot-autopilot-content` - 瓦片、动画、音频、UI。
- `godot-autopilot-csharp` - C# 构建循环与互操作。
