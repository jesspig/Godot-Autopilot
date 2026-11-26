---
name: godot-autopilot
description: "Godot Autopilot（GDA）纯 CLI 入口技能卡：用 gda ping 探测健康，用 gda commands 发现命令，用 gda tool list/search/describe 发现工具，用 gda tool call --confirm 调用，遵循变更-观测-验证环。只读取与当前任务匹配的参考。"
---

# Godot Autopilot

Godot Autopilot（GDA）通过一个本地 `gda` CLI 把运行中的 Godot 编辑器暴露出来。没有第二套协议，也没有需要配置的端点：每个操作都是一条输出 JSON envelope 的 CLI 命令。只读取与当前任务匹配的那一份参考，不要一次加载全部。

## 从这里开始

```bash
gda ping
gda status --format json
gda commands --json
```

`gda ping` 是健康探测，不是握手；它只证明控制面可达。`gda status --format json` 是连接记录。`gda commands --json` 是已实现命令与 flag 的权威清单；清单里没有的命令一律不要臆造。

## 发现工具，不要猜名字

工具目录随 CLI 变化，名字和数量都不固定。先用 CLI 发现，首次调用前读 schema。

```bash
gda tool list
gda tool search <query>
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

1. `gda tool list` 列出当前 canonical 目录；可用 `--category` 或 `--tags` 过滤。`gda tool categories` 列出分类。
2. `gda tool search <query>` 先按对象搜索（“tile cell”“audio track”），结果太宽再补动词；下结论说工具缺失前先换同义词重试。
3. `gda tool describe <tool>` 显示 schema、必填的 `action` 值与副作用。首次调用前必读，禁止推断参数名。
4. `gda tool call <tool>` 执行一次调用。短参数用 `--args '<json>'`，长 JSON 用 `--args-file <path>`（或 `-` 从 stdin 读取）。
5. 记忆中的工具名可能是 legacy alias。用 `gda tool describe` 或 `gda tool search` 解析，优先使用 canonical 名称与 action；不要依赖记忆中的迁移清单。

## 引擎文档

禁止凭记忆写不熟悉的引擎 API。先查：

```bash
gda docs search <query>
gda docs class <name> --member <text>
```

`gda docs status` 显示解析到的引擎与缓存；`gda docs sync` 刷新缓存。项目 pin 与可用二进制不一致时传入 `--godot`。

## 确认与安全

高风险副作用不会执行，而是返回 `confirmation_required`，附带工具、目标、风险与参数摘要；审查该摘要后用 `--confirm` 重跑同一条命令。非交互会话缺少 `--confirm` 会停止命令且不发送任何内容。

```bash
gda tool call <tool> --args '<json>' --confirm
```

- `destructive`、`process`、`code_execute` 与 `game_runtime` 注解是停下审查的信号，不是形式。
- 结果未知时不要重发有副作用的调用；先读回状态。
- 每次文件或场景写入后读回；批量任务中的高风险项逐项确认。覆盖写需显式确认。
- 细节见 `references/safety.md`。

## 观察引擎与恢复

```bash
gda doctor
gda job
```

`gda doctor` 诊断环境；`gda doctor run --offline --ci` 给出稳定的机器可读报告。长任务会返回 job id：用 `gda job list`、`gda job wait <id>` 与 `gda job status <id>` 跟踪；用 `gda job cancel <id> --confirm` 停止。日志用 `gda logs editor`、`gda logs game`、`gda logs plugin` 或 `gda logs cli` 读取。

## 变更 / 观测 / 验证

1. 用 CLI 命令或工具调用改变项目。
2. 用读回（`gda scene tree`、`gda resource get`）、截图（`gda game capture`）或日志（`gda logs editor`）观测。
3. 根据观测诊断、修复，再次观测，直到结果符合意图。
4. 宣布成功前用 `gda test run`、`gda debug errors` 或 `gda scene verify` 验证。

## 任务路由

| 任务 | 参考 | 首步 |
|---|---|---|
| 安装、连接、技能、插件 | `references/setup.md` | `gda setup status` |
| CLI 语法、envelope、args-file、job | `references/cli.md` | `gda commands --json` |
| 确认与错误恢复 | `references/safety.md` | `gda tool describe <tool>` |
| 离线与空气隔离使用 | `references/offline.md` | `gda doctor run --offline` |
| 工具发现与别名 | `references/tools.md` | `gda tool search <query>` |
| 场景、节点、属性、信号 | `references/scene.md` | `gda scene tree` |
| GDScript 创建、补丁、运行、重载 | `references/scripting.md` | `gda script list` |
| 资源、导入、重命名、UID | `references/resources.md` | `gda resource inspect --path <path>` |
| 运行游戏、输入、截图、日志 | `references/runtime.md` | `gda game status` |
| 调试、trace、报告 | `references/debugging.md` | `gda debug status` |
| 测试与运行 | `references/testing.md` | `gda test list` |
| 工作流与长任务 | `references/workflows.md` | `gda workflow list` |
| TileMap、动画、音频、UI | `references/content.md` | `gda tool search <query>` |
| C# 构建环 | `references/csharp.md` | `gda doctor run --ci` |
| 服务器、RID、物理、导航 | `references/servers.md` | `gda tool search <query>` |
