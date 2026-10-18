---
type: 模块文档
title: CLI 载体（gda）
description: Bun + TypeScript 源码直跑的 MCP 命令行载体：分发、命令参考、寻址链、登记表、生命周期、协议握手与已知限制
tags:
  - 模块
  - CLI
  - Bun
  - TypeScript
  - MCP
timestamp: "2026-10-19T00:00:00+08:00"
resource:
  - cli/
  - package.json
  - bun.lock
---

# CLI 载体（`cli/`，命令 `gda`）

> 审计日期：2026-10-19（随 CLI 跨平台修复批次同步——`userDataBase()` 新增 `GODOT_AUTOPILOT_USERDATA` 显式覆写（未设或空白走原生分支，既有解析语义不变）；`target.test.ts` 改经覆写变量注入临时 userdata，不再依赖 Windows 专属 `APPDATA` 语义（Linux/macOS 寻址链用例原先必败）；`lifecycle.test.ts` 优雅停止失败用例改用显式忽略 SIGTERM 的子进程，去除内嵌 Windows `taskkill` 无 `/F` 语义、两平台同语义；寻址链小节补 userdata 目录解析与覆写变量说明）；2026-10-18（随 CLI 载体落地批次同步——新增 `cli/`（Bun + TS 源码直跑，零运行时依赖）、根 `package.json`/`bun.lock`；双载体（CLI + `gda.mjs`）协议握手升级为标准 MCP `initialize` 流程（SDK 0.3.7 兼容）；真机验收修复两个缺陷（Windows 脱父、start 就绪探测直达标志）；测试稳定性修复（串行化 + Windows 清理时序）；`version_sync_guard`/`cli_contract`/`cli_unit` 三个新 ctest 点；详见 [changelog/2026-10-18-log.md](../changelog/2026-10-18-log.md)）

> 覆盖范围：`cli/` 全部（`bin/gda.ts` 入口、`src/` 11 模块、`src/commands/` 10 命令、`src/mcp/client.ts` 协议客户端、`tests/` 6 测试文件）+ 根 [`package.json`](../../../package.json) 与 [`bun.lock`](../../../bun.lock)。CLI 是技能书调用阶梯的第二级（原生 MCP → **bunx gda CLI** → `gda.mjs` 脚本桥 → 手动），定位与分发见下文。

## 定位与分发

**Bun + TypeScript 源码直跑**：无构建步骤、零运行时依赖、不产出二进制。`package.json` 的 `bin` 把 `gda` 映射到 [`cli/bin/gda.ts`](../../../cli/bin/gda.ts)（shebang `#!/usr/bin/env bun`，4 行转发到 `src/main.ts`）；`devDependencies` 仅 `typescript ^5.6.0` 与 `@types/bun latest`（类型检查用，运行时不参与）。`bun.lock` 只含这两个 devDependency 的传递闭包，**无任何运行时依赖**。

两条分发路径：

| 路径 | 命令 | 用途 |
|---|---|---|
| bunx（技能书默认，钉版本） | `bunx "github:jesspig/Godot-Autopilot#<VERSION>" <cmd>` | 技能书调用阶梯第二级；`<VERSION>` 取根 `VERSION`（当前 `0.2.7`），由 `version_sync_guard` 兜底三处一致 |
| bun install -g（人类文档） | `bun install -g github:jesspig/Godot-Autopilot#<VERSION>` | 全局安装后直接用 `gda <cmd>` |

**不进 addon 包与 release 资产**：CLI 独立于 GDExtension 插件（`godot-autopilot.dll` 等），不参与 `main.py build`/`deploy`/`package`，不进 `dist/*.zip`。

## 缓存与更新

| 项 | 事实 |
|---|---|
| bunx 缓存位置 | `%TEMP%\bunx-*`（Windows） |
| `--no-install` 未缓存 | 0.07s 快失败（不命中缓存即报错退出） |
| 冷下载耗时 | 实测 76–242s（网络相关，首次 `bunx github:...` 需下载仓库 tarball） |
| PowerShell 引号 | 含 `#` 的 ref 必须加引号（`#` 在 PowerShell 起始注释），故技能书统一写 `bunx "github:jesspig/Godot-Autopilot#<VERSION>"` |
| 更新方式 | 钉版本天然隔离；同版本清 `%TEMP%\bunx-*` 后重新下载 |

## 命令参考

### 调用族（6 个，经 MCP 协议）

| 命令 | 映射到 MCP | 说明 |
|---|---|---|
| `ping` | `ping`（直连） | 连通性测试 |
| `status` | `call_tool` → `system_status` | 服务状态与工程信息 |
| `categories` | `list_categories`（直连） | 列出工具类别 |
| `search [query]` | `search_tools`（直连） | 搜索工具；`--category`/`--tags` 过滤 |
| `describe <tool>` | `get_tool_detail`（直连） | 工具详情 |
| `call <tool>` | 直连或 `call_tool` 包装 | 调用工具；`--args`/`--args-file` 传参 |

### 生命周期（4 个，本地状态管理）

| 命令 | 说明 |
|---|---|
| `list` | 列出已启动的 Godot 进程（registry 实例 + live 状态） |
| `start` | 启动 Godot 进程（自动选端口、可选导入预步、就绪探测） |
| `stop` | 停止进程（默认优雅，`--force` 强杀） |
| `logs` | 查看进程日志（state dir `logs/<id>.log`，默认末尾 50 行） |

### 全局选项（[`src/args.ts`](../../../cli/src/args.ts)）

| 选项 | 类型 | 适用 | 说明 |
|---|---|---|---|
| `--port <text>` | 字符串 | 全部 | MCP 服务端口（默认 9527） |
| `--id <text>` | 字符串 | 调用族 | 目标服务实例 ID（registry 查找） |
| `--project <text>` | 字符串 | 调用族/start/stop | 工程路径 |
| `--pretty` | 布尔 | 输出 | 缩进 2 空格 |
| `--raw` | 布尔 | 输出 | 原始信封（图片 data 脱敏为 `<redacted N bytes>`） |
| `--quiet` | 布尔 | 输出 | 静默模式 |
| `--timeout <text>` | 字符串 | 调用族 | 调用超时秒数（默认 60） |
| `--category <text>` | 字符串 | search | 按类别过滤 |
| `--tags <text>` | 字符串 | search | 按标签过滤（逗号分隔） |
| `--args <json>` | 字符串 | call | 工具参数（JSON 字符串） |
| `--args-file <path>` | 字符串 | call | 工具参数（JSON 文件） |
| `--save-images <dir>` | 字符串 | 调用族 | 保存返回图片到目录 |
| `--godot <path>` | 字符串 | start | 显式指定 Godot 可执行文件 |
| `--wait <sec>` | 字符串 | start/stop | 就绪/停止等待秒数（0=不等待） |
| `--tail <n>` | 字符串 | logs | 日志末尾行数（默认 50） |
| `--headless` | 布尔 | start | 无头模式启动编辑器 |
| `--play` | 布尔 | start | 就绪后播放当前场景 |
| `--no-import` | 布尔 | start | 跳过启动时资源导入 |
| `--force` | 布尔 | stop | 强制停止进程 |
| `--help` / `--version` | 布尔 | — | 帮助 / 版本 |

选项支持 `--opt=value` 与 `--opt value` 两种形式；`--` 之后全为位置参数。`--id`/`--port`/`--project` 互斥（同时指定报 `use only one of --id/--port/--project`）。

### 退出码（[`src/errors.ts`](../../../cli/src/errors.ts)）

| 码 | 语义 |
|---|---|
| 0 | 成功 |
| 1 | 工具失败（业务载荷含 `error`） |
| 2 | 传输失败（连接拒绝、超时、HTTP 非 2xx） |
| 3 | 用法错误（未知命令、缺参、非法选项） |

### 输出契约（[`src/output.ts`](../../../cli/src/output.ts) + [`src/payload.ts`](../../../cli/src/payload.ts)）

- **stdout 仅载荷**：默认紧凑 JSON（`JSON.stringify` 无缩进）；`--pretty` 缩进 2 空格；`--raw` 输出原始信封（`result.content` 中 image 的 `data` 替换为 `<redacted N bytes>`）。
- **诊断走 stderr**：`[gda]` 前缀（错误消息、suggestion、工具失败提示）。
- **载荷提取**：从 `result.content` 提取 text（`JSON.parse`）与 image（base64 字节数）；`--save-images DIR` 落盘 `gda_<stamp>_<n>.png`；`toolFailed` 判定（`result.isError` 或内层 `error` 字段）。
- **请求体上限**：4 MiB（`MAX_BODY_BYTES`），超限报传输错误。

## 寻址链（[`src/target.ts`](../../../cli/src/target.ts)）

`--id`/`--port`/`--project` 互斥。解析顺序：

| 优先级 | 来源 | 行为 |
|---|---|---|
| 1 | `--id` | registry 查找，须存活，否则报 `unknown instance` |
| 2 | `--port` | 直接端口（1–65535，非法报用法错误） |
| 3 | `--project` | registry 活实例 → trace(`server_ready`) → config.json → 9527 |
| 4 | `GODOT_AUTOPILOT_PORT` env | 环境变量端口 |
| 5 | cwd registry 活实例 | 当前工程已登记实例 |
| 6 | trace/config | 当前工程 trace(`server_ready`) → config.json |
| 7 | 默认 9527 | 兜底 |

即无显式选项时的环境链为 **env → cwd 登记 → trace → config → 9527**，对齐 legacy 脚本桥五级链（[`script-access.md`](../../../skills/godot-autopilot-tools/references/script-access.md)）。

**Godot userdata 目录解析**（trace/config 两级定位用，[`src/platform.ts`](../../../cli/src/platform.ts) `userDataBase()`）：`GODOT_AUTOPILOT_USERDATA` 显式覆写 > `%APPDATA%\Godot\app_userdata`（Windows）/ `~/Library/Application Support/Godot/app_userdata`（macOS）/ `~/.local/share/godot/app_userdata`（其他平台）。覆写变量供测试与非常规安装路径绕过平台推断，未设或为空白时走原生分支（既有解析语义不变）；其下再按 `<userdata>/<project_name>/godot_autopilot/` 读 `server_ready` trace 与 `config.json`。

## 登记表（[`src/registry.ts`](../../../cli/src/registry.ts)）

**state dir 解析**：`GODOT_AUTOPILOT_CLI_HOME` > `%LOCALAPPDATA%\godot-autopilot`（Windows）/ `$XDG_STATE_HOME/godot-autopilot` > `$HOME/.local/state/godot-autopilot`（非 Windows）。无法确定时报错并提示设 `GODOT_AUTOPILOT_CLI_HOME`。

**`instances.json` schema**：

```json
{
  "version": 1,
  "instances": [
    {
      "id": "p<pid>",
      "project": "绝对路径",
      "port": 9530,
      "pid": 12345,
      "startedAt": "ISO8601",
      "headless": true,
      "godot": "Godot 可执行文件绝对路径",
      "log": "state dir logs/<id>.log 绝对路径"
    }
  ]
}
```

**锁与死 pid 清理**：

- 锁文件 `instances.lock`，`wx` 排他创建；50ms 重试间隔、2000ms 总超时。
- 死锁窃取：mtime 超 10s 且锁内 pid 不存活时删除锁继续。
- 原子写：tmp 文件（`instances.json.<pid>.tmp`）+ `rename`。
- 加载时过滤死 pid 条目（`isAlive` 探测），有清理则写回。

## 生命周期语义

### start（[`src/commands/start.ts`](../../../cli/src/commands/start.ts) + [`src/process.ts`](../../../cli/src/process.ts)）

1. **Godot 可执行文件链**：`--godot` > `GODOT_PATH` env > `<project>/.env` 的 `GODOT_PATH=` > PATH 搜索（Windows: `godot.exe`/`godot4.exe`；其他: `godot`/`godot4`）。
2. **端口**：显式 `--port` 或 9530–9599 自动选（`pickFreePort`，bind `127.0.0.1` 探测空闲）。
3. **导入预步**：默认执行 `--headless --import --path`（120s 超时，失败非致命仅告警）；`--no-import` 跳过。
4. **脱父实现**（Windows）：经 `cmd /c start "" /b` 蹦床启动编辑器，再用 `netstat -ano -p tcp` 反查监听端口对应的真实 pid（规避 [bun#31603](https://github.com/oven-sh/bun/issues/31603)——`windowsKeepAlive` 在 1.4.2 不可用，直接 spawn 会导致 CLI 退出时编辑器进程随之退出）。非 Windows 直接 `spawn`。
5. **就绪探测**：TCP + MCP `ping`（`direct: true` 直连，500ms 间隔，默认 60s 超时，`--wait` 覆盖，0=不等待）。进程提前退出或超时均报传输错误并附日志路径。
6. **登记**：就绪后写入 registry（id = `p<pid>`），日志从 `.pending-<port>-<ts>.log` 重命名为 `<id>.log`。
7. **幂等**：同工程已有活实例直接返回，不重复启动。
8. **`--play`**：就绪后调 `play_editor_current_scene`（非致命，失败仅告警）。

### stop（[`src/commands/stop.ts`](../../../cli/src/commands/stop.ts)）

- **默认优雅**：Windows `taskkill /PID`，其他 `SIGTERM`；等待默认 10s（`--wait` 覆盖）。
- **失败只报告**：超时未退出时输出 `ok: false` + hint（`the process is still running; retry with --force`），退出码 2。
- **`--force` 强杀**：Windows `taskkill /F /T /PID`，其他 `SIGKILL`；强杀后仍存活报 `the process survived a force stop`。
- 停止后从 registry 移除条目。

### logs / list

- `logs <id>`：读 state dir `logs/<id>.log`，输出末尾 `--tail` 行（默认 50）。
- `list`：列 registry 全部实例（含 `live` 实时状态）。

## 协议（[`src/mcp/client.ts`](../../../cli/src/mcp/client.ts)）

**标准 MCP 握手**：`initialize`（protocolVersion `2025-03-26`，clientInfo `gda-cli`）→ `notifications/initialized` → `tools/call`。懒握手：首次调用遇 400 + `-32600`（未初始化）时自动补握手并重试一次。

**直连工具集**（`META_DIRECT_TOOLS`）：`ping`/`search_tools`/`list_categories`/`get_tool_detail`/`batch_execute`/`code_execute` 直接发 `tools/call`；其余工具经 `call_tool` 包装（`{name: "call_tool", arguments: {name, arguments}}`）。

**响应解析**：SSE（`text/event-stream`，`data:` 帧拼接）与纯 JSON 双通道。

**SDK 0.3.7 握手升级（历史问题）**：mcp-cpp-sdk 0.3.7 起严格要求完整握手，旧懒握手只发 `notifications/initialized` 被拒 `-32600`。两载体同步修复：CLI（`840e648`）与 `gda.mjs` 脚本桥（`6234a95`）均升级为标准 `initialize` 流程。`test_cli_contract.py` 的 mock 服务器严格模拟该语义（initialize 返回 200 SSE、notifications/initialized 返回 202、未初始化时 tools/call 被拒 400 + `-32600`）。

## 与技能书阶梯的关系

四级调用阶梯（[`godot-autopilot-tools/SKILL.md`](../../../skills/godot-autopilot-tools/SKILL.md)）：

1. **原生 MCP 工具**（客户端 harness 直连）
2. **bunx gda CLI**（本载体，Bun 可用时优先）
3. **`gda.mjs` 脚本桥**（Node 18+ 或 Bun，无 CLI 时）
4. **手动指南**（无脚本运行时，[`manual-fallback.md`](../../../skills/godot-autopilot-tools/references/manual-fallback.md)）

技能书钉版本 `github:jesspig/Godot-Autopilot#<VERSION>`，由 `version_sync_guard` 断言 `VERSION`/技能书 pin/`CLI_VERSION` 三处一致。

## 已知限制

- **`bun pm pack` 要求 `package.json` 有 `version`**：bunx 从 git 安装不需要该字段，故 `package.json` 未加 `version`（保持与根 `VERSION` 单一来源一致）。
- **`file:` 依赖 EPERM**：本机 `file:` 协议依赖有 EPERM 环境限制；git/tarball 路径（`github:...`）不受影响，故分发走 git 而非本地 pack。
- **Windows 蹦床 pid 反查**：`netstat` 反查有 200ms 轮询间隔，极端情况下端口被其他进程占用可能误判（实测未遇到）。

## 测试

| 层 | 位置 | 规模 | 说明 |
|---|---|---|---|
| TS 单测 | [`cli/tests/`](../../../cli/tests/) | 6 文件 102 用例（291 expect） | `bun test cli/tests`；client 17 / commands 38 / detach 1 / lifecycle 23 / registry 8 / target 15 |
| ctest `cli_unit` | [`tests/scripts/run_cli_unit.py`](../../../tests/scripts/run_cli_unit.py) | 封装 `bun test` | bun 缺失时 SKIP 并 exit 0 |
| ctest `cli_contract` | [`tests/scripts/test_cli_contract.py`](../../../tests/scripts/test_cli_contract.py) | 双载体 mock conformance | node 驱动 legacy `gda.mjs` + bun 驱动新 CLI；node/bun 缺失时 SKIP |
| ctest `version_sync_guard` | [`tests/guard/version_sync_guard.py`](../../../tests/guard/version_sync_guard.py) | 三处版本一致 | `VERSION`/技能书 pin/`CLI_VERSION` |

## 相关页面

- [支撑模块](support.md) — 技能书与脚本桥（`gda.mjs`）
- [安全边界与并发契约](../security_contract.md) — CLI 安全包络（§3.4）
- [测试体系](../tests.md) — ctest 注册点与 TS 单测计数
- [项目总览](../overview.md) — 目录树中的 `cli/`
