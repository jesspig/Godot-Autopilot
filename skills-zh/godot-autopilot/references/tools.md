# 调用协议与文档 CLI

本篇是跨域调用知识：通道、CLI 面、引擎文档、confirm、错误与限制。领域行为见其它参考。

## 通道

- **GDA CLI。** 有 shell 时优先用 `gda`。它自动发现插件端口，优先 IPC，回退 HTTP，输出 JSON。
- **原生 MCP 工具。** harness 已列出 GDA 工具时直接用。语义与 CLI 相同，只是传输不同。
- **手动编辑器步骤。** 两者都不可用时，给用户一段简短、可核对的步骤，并要求一次只回报一个观测。禁止编造工具输出。

## CLI 主流程

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"path": "Level", "cells": []}'
gda tools list
```

- `gda search <query>` 按名称、描述、分类与标签搜索 live 服务；`--category` 与 `--tags` 收窄范围，默认最多 20 条。
- `gda describe <name>` 打印 schema 与 `side_effect` 标记；首次写入前必读，禁止推断参数名。
- `gda call <name> --args '<json>'` 或 `--args-file <path>` 执行一次调用。
- `gda tools list` 列出全部可直接调用的工具。发现只针对 live 实例；没有实例时用 `gda start --project <dir>` 启动编辑器。

目标解析顺序：显式 `--socket` 或 `--port`，再 `--id`，再匹配 `--project` 或当前目录的 live 实例。端口由插件拥有；`--port` 只是连接覆盖。`gda status --json` 报告解析后的 `mcp_url` 与通道。解析失败时，`gda list --all` 列出已知项目。

退出码：

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 工具运行并返回失败 envelope |
| 2 | 传输或环境失败：无实例、端点不可达、超时 |
| 3 | 用法错误：未知命令、flag 或参数格式错误 |

stdout 是 JSON，诊断进 stderr。退出 2 表示通道不可用；禁止在此后编造结果。

## 引擎文档 CLI

`gda docs` 是离线引擎参考：读取本机引擎 dump，按引擎 minor 缓存，并检索类、成员与全局符号。

```bash
gda docs search "tween"
gda docs class Node2D --member position
gda docs member Node2D look_at
gda docs sync
```

- `status` 只读：项目 pin、解析到的引擎、缓存路径、新鲜度。
- `search` 默认 20 条（`--limit` 上限 200），支持 `--kind`（`class`、`method`、`property`、`signal`、`enum`、`constant`、`utility`）与 `--class`。`class` 默认返回简要、描述与计数；用 `--section`、`--member` 或 `--full` 展开。`member` 自动判定类型，歧义名返回全部匹配。
- `sync` 总是导出。缓存缺失或解析版本不一致时，下一次查询自动导出；导出失败是错误，绝不回退到其它版本。
- `--docs-file <path>` 读取外部 `extension_api.json` 并绕过缓存。项目 pin 与可用二进制不一致时，传入匹配的 `--godot <path>`；命令拒绝混用 minor。
- `clear --engine <x.y>` 删除一个桶；`--all` 需要 `--force`。

## 确认

高风险副作用在执行前返回结构化 `confirmation_required`。审查描述与目标后，用 `--confirm`（CLI）或 `confirm: true`（MCP）重试。类别为 `destructive`（删除已存在对象）、`process`（控制 OS 进程）、`code_execute`（运行任意 GDScript）与 `game_runtime`（进入运行中的游戏）。

文件、配置与场景写入不需要确认，但仍改变内容：每次写入后读回目标。禁止换一个效果相同的工具绕过确认，也禁止把高风险调用藏进 batch。

## batch 与 code 执行

- `batch_execute` 顺序执行 `operations` 列表（最多 256 条）。每项有 `tool` 与可选 `args`；`stop_on_error` 默认 true，`rollback_on_error` 可选。用于确定性序列。
- `code_execute` 在编辑器里运行 GDScript，用于循环、数学与计算值。没有行首 `func ` 时把源码包进一个入口函数；有 `func ` 时保留源码并用 `function_name`。`timeout_ms` 默认 5000，上限由 schema 决定。
- 判据：少量已有调用 → `batch_execute`；循环、数学、条件或超大 payload → `code_execute`。

## 错误与水位线

每次工具失败都是带 `error` 键的 JSON 对象。部分带稳定 `code`；按 code 分支，不按文字。每个 meta 工具响应还带 `new_errors_since_last_call`，统计自上次调用以来的新错误，包括本次调用产生的错误。它是一次性的：读取即重置。

| Code | 触发条件 | 重试？ |
|---|---|---|
| `exec_timeout` | `code_execute` 脚本超过 `timeout_ms` | 拆分工作或在 schema 上限内提高超时 |
| `eval_timeout` | 游戏侧 eval 超过宿主等待 | 不要立即重试；游戏可能仍在执行 |
| `main_thread_timeout` | 编辑器主线程超出预算 | 先读状态；副作用可能已落地 |
| `queue_unavailable` | 主线程队列满或正在关闭 | 稍后重试 |
| `confirmation_required` | 高风险工具未确认 | 审查目标后用 `--confirm` 重试 |

编辑器导入或扫描资源期间，软错误带 `retryable: true` 与 `retry_after_ms`。至少等待建议间隔再重试同一调用。项目导出期间所有工具调用都会被拒绝；等待导出结束，不要紧密循环重试。

用错误水位线确认修复：复现、诊断、修复、重跑失败调用，再做 meta 调用直到水位线归零。成功调用后为零，是修复未产生新错误的收口证据。

## 资源、提示词与任务

`gda resources read <uri>` 读取编辑器与调试器状态；`gda resources list` 是 URI 权威。`gda prompts list|get` 渲染已注册提示词。`gda call --async` 配合 `gda tasks list|get|wait|cancel|clean` 管理本地记录的任务。

## 限制

- 默认操作超时 5000 ms；schema 报告当前上限。游戏操作 `timeout_ms` 上限 25000 ms。
- JSON 响应上限 4 MiB，并暴露截断字段；信任完整性前先检查。`batch_execute` 最多 256 条操作，最多 8 个请求并发在途。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 退出 2 且无 JSON | 无 live 实例或端点不可达 | `gda list --all`，再 `gda start --project .` |
| 退出 3 | 未知命令、flag 或 JSON 格式错误 | `gda --help`；校验 `--args` JSON |
| `confirmation_required` | 高风险调用未确认 | 审查目标后加 `--confirm` |
| `main_thread_timeout` | 编辑器主线程超出预算 | 先读状态再重试 |
