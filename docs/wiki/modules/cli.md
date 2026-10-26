---
type: 模块文档
title: CLI 载体（gda）
description: Bun + TypeScript 源码直跑的 MCP / REST 命令行载体：分发、命令参考、七级寻址链、登记表与集中根位置、REST 令牌读取、生命周期、控制面代理与重启、协议握手与已知限制
tags:
  - 模块
  - CLI
  - Bun
  - TypeScript
  - MCP
  - REST
  - 控制面
timestamp: "2026-10-23T02:37:06+08:00"
resource:
  - cli/
  - package.json
  - bun.lock
---

# CLI 载体（`cli/`，命令 `gda`）

> 审计日期：2026-10-23（**10-23 随控制面文档同步批次同步**——命令总数 12→**14**：新增 [`restart`](../../../cli/src/commands/restart.ts) 与 [`serve`](../../../cli/src/commands/serve.ts)；`serve` 是**纯代理**（不复制工具语义），新增「控制面（`gda serve`）」与 `restart` 两小节，写全监听与绑定约束、路由表、认证与令牌文件、转发与流式透传、日志口径与已知限制；控制面端口 9700 与实例端口的分工见 [端口与实例登记](ports.md)「控制面端口」；CLI TS 单测 193→**257**（12→**16** 文件，Windows 跳过 2 项，实测 255 pass / 2 skip）；覆盖范围同步 `src/serve/` 3 个 .ts 与 `src/commands/` 17 个 .ts；**10-23 随真机冒烟缺陷修复校订**——控制面 `/i/<ref>` 寻址补全为插件 `instance_id` → CLI 标识 `p<pid>` → 端口三级（`gda list` 显示的 `p<pid>` 可直接用于 `/i/<ref>/...`），`stop` / `restart` 补「无窗口实例直接强制停止、有窗口实例保持优雅且超时提示 `--force` 并保留登记」的口径）2026-10-22（10-22 随本地收口批次同步——**`gda start` 新增同工程活跃实例预检**：启动编辑器**之前**先查该工程是否已有活跃实例，命中即**拒绝启动**并给出可操作提示（已有实例的端口与进程号、改用其它端口的办法、豁免开关）；用户显式指定 `--port` 时按用户意图继续启动，但仍给出冲突提示。预检复用了既有的三件能力：CLI 本地注册表（`src/registry.ts` 的 `findLiveByProject`）、插件全局实例索引（`src/instances.ts` 的容错读取）与两级存活校验（`src/probe.ts` 的轻量 TCP / 严格 HTTP）；新模块 [`src/preflight.ts`](../../../cli/src/preflight.ts) 承载判定与提示文案。**行为变更**：`gda start` 对同工程二次启动由「幂等返回既有实例」改为「拒绝并提示」。CLI TS 单测 180→**193**（11→**12** 文件，新增 `preflight.test.ts` 10 项）；技能书里「所钉 tag 早于 CLI 存在」的限制说明按事实改写——`0.3.0` 的树已含 `cli/` 与根 `package.json`（`bin` 映射 `gda`），发布作业也随 `addons.zip` 挂出 5 个目标的独立二进制，剩余限制只是「该 tag 尚未发布」）2026-10-21（10-21 随开放接口面落地批次同步——新增 **REST 调用族** `gda api list` / `gda api describe` / `gda api call` 与端点展示命令 `gda endpoints`；**寻址链由六级改为七级**（`--port` ＞ `--id` ＞ 环境变量 ＞ CLI 本地注册表 ＞ 插件全局实例索引 ＞ 旧路径回退 ＞ 默认 9527），每一级都记录来源（`source`）与链位（`chain`），`gda endpoints` 一次性打印端口/来源/链位/两个端点/存活结果；新增全局选项 `--query`（`api list` 关键字过滤）与 `--probe`（存活校验级别：`light` TCP / `strict` HTTP 探活）；**CLI state dir 迁到集中根下** `<集中根>/cli/`（`GODOT_AUTOPILOT_CLI_HOME` 仍可覆写），**插件全局实例索引**新增为第 5 级来源（`<集中根>/instances.json`），REST 令牌从该索引的 `api_token` 字段读取；集中根由 `GODOT_AUTOPILOT_HOME` 覆写、否则取 `os.homedir()`，其下固定目录名 `.godot-autopilot`；端口模型与登记结构的完整口径见 [端口与实例登记](ports.md)，REST 端点契约见 [REST 接口面](api.md)；本轮校订补三处事实——① 正文「寻址链」小节按 `src/target.ts` 的七级实现重写（旧正文仍是六级链，且 `--id` / `--port` 次序与实现相反），补 `source` / `chain` 两列与两级存活校验；② state dir 解析按 `src/registry.ts` 改正为 `GODOT_AUTOPILOT_CLI_HOME` > `<集中根>/cli/`（旧正文写的是 `%LOCALAPPDATA%` / `XDG_STATE_HOME`），并新增「集中根与两套注册表」小节写令牌读取方式；③ 全局选项表删去已移除的 `--quiet`、补 `--query` 与 `--probe`；10-20 随技能书调用阶梯收敛批次同步——技能书自带的脚本桥 `gda.mjs` 与 `references/script-access.md` 删除，调用阶梯由五级收敛为三级（原生 MCP → GDA CLI → 手动兜底），本载体成为唯一的脚本化调用通道；`bunx`、全局安装与独立二进制是同一载体的三种分发方式，合为阶梯第二级；10-20 随守卫覆盖范围措辞修正批次同步——`version_sync_guard` 已扩展为扫描 `skills/` 全部文本文件与根 `README*.md` 的每一处版本钉（另含 `VERSION`/`CLI_VERSION` 两个值源），本页与 [测试体系](../tests.md) 描述的守卫覆盖范围本轮按守卫脚本实况改正，固定处数的旧口径已删除，覆盖范围以守卫脚本为准；10-20 随 CLI 启动回滚修复批次同步——Windows 启动路径**不再**用 `cmd /c start "" /b` 蹦床 + `netstat` 端口反查 pid，改为直连 `spawn` 并置 `detached: true` 脱父，pid 由 `spawn` 直接返回（编辑器从未监听端口时端口反查必然失败，回滚拿不到 pid、失败后编辑器进程成为孤儿）；`isAlive` 改为解析 `tasklist /FO CSV` 的 pid 字段，**与系统语言无关**（旧实现按输出是否以 `INFO`/`消息` 开头判断，中文 Windows 实际输出「信息:」导致恒判存活，终止确认轮询超时并把已终止进程报成「无法终止」）；失败回滚的三种结果 `was terminated` / `had already exited` / `could not be terminated`（后者附可执行补救指令：Windows `taskkill /F /T /PID N`、POSIX `kill -9 N`）；引擎不可执行时报 `failed to launch the editor executable: ENOENT: …` 而非笼统的 failed to spawn；CLI TS 单测 126→132（9 个文件）；10-19 随 CLI 引擎定位收窄批次同步——`gda start` 不再读 `GODOT_PATH` 与 `<project>/.env`（前者仅 L2 引擎测试使用，生产环境不存在），可执行文件链收敛为 `--godot` > PATH，未命中即报错提示 `--godot`；新增 `cli/tests/process.test.ts` 3 项（TS 108→111、expect 301→310）；随 MCP 协议版本协商修正批次同步——`cli/src/mcp/client.ts` 与技能书 `gda.mjs` 脚本桥声明版本 `2025-03-26`→`2025-11-25`（SDK 旧世代当前版本，GDA 未钉 `ServerOptions::protocol_version` 时服务端对旧版声明原样尊重）；2026-10-19（随 CLI 跨平台修复批次同步——`userDataBase()` 新增 `GODOT_AUTOPILOT_USERDATA` 显式覆写（未设或空白走原生分支，既有解析语义不变）；握手状态由进程级单例改按目标 URL 键控（多目标进程各自握手）；新增 initialize 响应 `result.protocolVersion` 协商版本学习（仅已知五版本表内），后续 POST 按学习值携带 `MCP-Protocol-Version` 头（modern 声明学习值不发头，规避 stateless 传输 hold-open 长连接）；新增 `cli/tests/client_handshake.test.ts` 6 项，TS 单测 102→108 用例 / 6→7 文件；`test_cli_contract.py` mock initialize 响应同步 `2025-11-25`）；2026-10-18（随 CLI 载体落地批次同步——新增 `cli/`（Bun + TS 源码直跑，零运行时依赖）、根 `package.json`/`bun.lock`；双载体（CLI + `gda.mjs`）协议握手升级为标准 MCP `initialize` 流程（SDK 0.3.7 兼容）；真机验收修复两个缺陷（Windows 脱父、start 就绪探测直达标志）；测试稳定性修复（串行化 + Windows 清理时序）；`version_sync_guard`/`cli_contract`/`cli_unit` 三个新 ctest 点；详见 [changelog/2026-10-18-log.md](../changelog/2026-10-18-log.md)）

> 覆盖范围：`cli/` 全部（`bin/gda.ts` 入口、`src/` 13 个顶层 .ts + `src/api/client.ts`、`src/mcp/client.ts`、`src/serve/` 3 个 .ts、`src/commands/` 17 个 .ts 共 35 个 .ts、`tests/` 16 测试文件）+ 根 [`package.json`](../../../package.json) 与 [`bun.lock`](../../../bun.lock)。CLI 是技能书调用阶梯的第二级（原生 MCP → **GDA CLI** → 手动兜底），定位与分发见下文。

## 定位与分发

**Bun + TypeScript 源码直跑**：开发期无构建步骤、零运行时依赖；发布期另有可选的 `bun build --compile` 独立二进制（内嵌 Bun 运行时，见下表第三行）。`package.json` 的 `bin` 把 `gda` 映射到 [`cli/bin/gda.ts`](../../../cli/bin/gda.ts)（shebang `#!/usr/bin/env bun`，4 行转发到 `src/main.ts`）；`devDependencies` 仅 `typescript ^5.6.0` 与 `@types/bun latest`（类型检查用，运行时不参与）。`bun.lock` 只含这两个 devDependency 的传递闭包，**无任何运行时依赖**。

三条分发路径，同属阶梯第二级「GDA CLI」：

| 路径 | 命令 | 用途 |
|---|---|---|
| bunx（技能书默认，钉版本） | `bunx "github:jesspig/Godot-Autopilot#<VERSION>" <cmd>` | 阶梯第二级的 bunx 分发；`<VERSION>` 取根 `VERSION`（当前 `0.3.0`），由 `version_sync_guard` 兜底与全部版本钉同值 |
| bun install -g（人类文档） | `bun install -g github:jesspig/Godot-Autopilot#<VERSION>` | 全局安装后直接用 `gda <cmd>` |
| 独立二进制（无需任何 JS 运行时） | 从 GitHub Release 下载 `gda-<version>-<platform>.zip`（Windows）或 `.tar.gz`（其余），对照 `sha256sums.txt` 校验后直接运行 | 阶梯第二级的独立二进制分发；目标五个：`windows-x64` / `linux-x64` / `linux-x64-musl` / `macos-arm64` / `macos-x64`；macOS 产物为 ad-hoc 签名（非公证） |

**不进 addon 包**：CLI 独立于 GDExtension 插件（`godot-autopilot.dll` 等），不参与 `main.py build`/`deploy`/`package`，不进 `addons.zip`；独立二进制作为**单独的 release 资产**与 `addons.zip` 并列发布（`release.yml` 的 `cli` 作业，5 个目标；`ci.yml` 另有 `cli-bins` 作业做三平台冒烟）。**已发布的 release 尚未带上这些资产**：最新一次发布（`0.2.7`，2026-10-11）早于 `cli` 作业接入发布流程，只含 `addons.zip`；二进制资产与 `sha256sums.txt` 自下一次发布起随 `addons.zip` 一并挂出。

**输出语言**：CLI 面向国际用户，全部用户可见文本（帮助、错误消息、suggestion、退出码说明）均为英文；源码内注释不受此约束。

## 缓存与更新

| 项 | 事实 |
|---|---|
| bunx 缓存位置 | `%TEMP%\bunx-*`（Windows） |
| `--no-install` 未缓存 | 0.07s 快失败（不命中缓存即报错退出） |
| 冷下载耗时 | 实测 76–242s（网络相关，首次 `bunx github:...` 需下载仓库 tarball） |
| PowerShell 引号 | 含 `#` 的 ref 必须加引号（`#` 在 PowerShell 起始注释），故技能书统一写 `bunx "github:jesspig/Godot-Autopilot#<VERSION>"` |
| 更新方式 | 钉版本天然隔离；同版本清 `%TEMP%\bunx-*` 后重新下载 |

## 命令参考

`gda --help` 共列 **14** 个命令（`api` 按一条命令计，其下有 3 个子命令）：调用族 6、生命周期与控制面 6、REST 族 1 + 端点展示 1。下表按族分组。

### 调用族（6 个，经 MCP 协议）

| 命令 | 映射到 MCP | 说明 |
|---|---|---|
| `ping` | `ping`（直连） | 连通性测试 |
| `status` | `call_tool` → `system_status` | 服务状态与工程信息 |
| `categories` | `list_categories`（直连） | 列出工具类别 |
| `search [query]` | `search_tools`（直连） | 搜索工具；`--category`/`--tags` 过滤 |
| `describe <tool>` | `get_tool_detail`（直连） | 工具详情 |
| `call <tool>` | 直连或 `call_tool` 包装 | 调用工具；`--args`/`--args-file` 传参 |

### 生命周期与控制面（6 个，本地状态管理 / 本地监听）

| 命令 | 说明 |
|---|---|
| `list` | 列出已启动的 Godot 进程（registry 实例 + live 状态） |
| `start` | 启动 Godot 进程（启动前做同工程活跃实例预检、自动选端口、可选导入预步、就绪探测） |
| `stop` | 停止进程（默认优雅；无窗口实例与 `--force` 直接强制） |
| `restart` | 重启进程（定位规则同 `stop`；按窗口状态停止后按原端口 / headless / godot 路径重新拉起） |
| `serve` | 启动控制面代理（默认 `127.0.0.1:9700`；多实例路由与 MCP / REST 双面转发） |
| `logs` | 查看进程日志（state dir `logs/<id>.log`，默认末尾 50 行） |

### REST 调用族（3 个 + 1 个端点展示，10-21 新增）

这四个命令**不走 MCP 协议**，直连同端口的 `/api` 端点面（端点契约见 [REST 接口面](api.md)）。

| 命令 | 映射到 REST | 说明 |
|---|---|---|
| `api list [query]` | `GET /api/tools` | 列工具；`--query` / `--category` / `--tags` 过滤（这三个选项只对本子命令有效） |
| `api describe <tool>` | `GET /api/tools/<tool>` | 单工具详情（schema 与 `side_effect`） |
| `api call <tool>` | `POST /api/tools/<tool>` | 调用工具；`--args` / `--args-file` 传参（**请求体就是参数对象**，无外层包装） |
| `endpoints` | 存活探针（`GET /api`，严格级别） | 展示发现到的 MCP 与 REST 两个端点、端口来源、链位与存活结果 |

- `api` 必须带子命令（`list` / `describe` / `call`），缺省或未知子命令报用法错误（退出码 3）。
- `api call` 的 `--args` / `--args-file` 只对本子命令有效，传给其它子命令报用法错误。
- **令牌来源**：从插件全局实例索引（`<集中根>/instances.json`）匹配到的记录里读 `api_token`；取不到时报传输错误 `no API token is available for port N (source …)`，并提示从实例登记的 `api_token` 字段读取。
- **状态码 → 退出码映射**（[`src/api/client.ts`](../../../cli/src/api/client.ts) 的 `mapStatusToExitCode`）：2xx → 0；**400 / 404 / 405 / 409 / 422 → 3（用法错误）**；其余（401 / 403 / 413 / 429 / 5xx）→ 2（传输失败）。

### 全局选项（[`src/args.ts`](../../../cli/src/args.ts)）

| 选项 | 类型 | 适用 | 说明 |
|---|---|---|---|
| `--port <text>` | 字符串 | 全部 | MCP 服务端口（默认 9527）；`serve` 下为控制面端口（默认 9700） |
| `--id <text>` | 字符串 | 调用族/stop/restart | 目标服务实例 ID（registry 查找） |
| `--project <text>` | 字符串 | 调用族/start/stop/restart | 工程路径 |
| `--pretty` | 布尔 | 输出 | 缩进 2 空格 |
| `--raw` | 布尔 | 输出 | 原始信封（图片 data 脱敏为 `<redacted N bytes>`） |
| `--timeout <text>` | 字符串 | 调用族/serve | 调用超时秒数（默认 60）；`serve` 下为上游等待秒数（默认 60） |
| `--category <text>` | 字符串 | search / api list | 按类别过滤 |
| `--tags <text>` | 字符串 | search / api list | 按标签过滤（逗号分隔） |
| `--args <json>` | 字符串 | call / api call | 工具参数（JSON 字符串） |
| `--args-file <path>` | 字符串 | call / api call | 工具参数（JSON 文件） |
| `--save-images <dir>` | 字符串 | 调用族 | 保存返回图片到目录 |
| `--godot <path>` | 字符串 | start | 显式指定 Godot 可执行文件 |
| `--wait <sec>` | 字符串 | start/stop/restart | 就绪/停止等待秒数（0=不等待）；`restart` 的停止阶段固定 10 秒，不受该选项影响 |
| `--tail <n>` | 字符串 | logs | 日志末尾行数（默认 50） |
| `--headless` | 布尔 | start | 无头模式启动编辑器 |
| `--play` | 布尔 | start | 就绪后播放当前场景 |
| `--no-import` | 布尔 | start/restart | 跳过启动时资源导入 |
| `--force` | 布尔 | stop/restart | 强制停止进程 |
| `--bind <addr>` | 字符串 | serve | 控制面绑定地址，仅环回（`127.0.0.1` / `::1` / `localhost`；默认 `127.0.0.1`） |
| `--no-auth` | 布尔 | serve | 关闭控制面认证（任何本机进程都可调用工具） |
| `--query <text>` | 字符串 | api list | 关键字过滤（只对本子命令有效） |
| `--probe <level>` | 字符串 | api / endpoints | 存活校验级别：`light`（默认，TCP）或 `strict`（HTTP 探活） |
| `--help` / `--version` | 布尔 | — | 帮助 / 版本 |

`--quiet` 已被移除，不在 `gda --help` 中；传入会报用法错误（退出码 3）。诊断本就只走 stderr，该开关没有价值。

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

`--id`/`--port`/`--project` 互斥（同时指定报 `use only one of --id/--port/--project`）。解析是**七级优先链**，每一级都记录来源（`source`）与链位（`chain`）：

| 优先级 | 链位 `chain` | 来源 | `source` | 行为 |
|---|---|---|---|---|
| 1 | `port` | `--port` | `cli` | 直接端口（1–65535，非法报用法错误） |
| 2 | `id` | `--id` | `id` | CLI 本地注册表按实例 ID 查找，**须进程存活**，否则报 `unknown instance` |
| 3 | `env` | `GODOT_AUTOPILOT_PORT` 环境变量 | `env` | 环境变量端口 |
| 4 | `cli-registry` | CLI 本地注册表 | `project` / `cwd` | 按工程路径（`--project` 或当前工作目录）找**存活**实例 |
| 5 | `plugin-index` | 插件全局实例索引 | `registry` | 读 `<集中根>/instances.json`，按工程路径匹配（**不要求进程存活**） |
| 6 | `legacy` | 旧路径回退 | `trace` / `config` | 旧 `user://godot_autopilot/` 下的 `server_ready` trace → `config.json` 取端口 |
| 7 | `default` | 默认 | `default` | 9527 |

无显式选项时的环境链为 **env → cwd 登记 → 插件全局索引 → trace → config → 9527**。

**回溯**：`gda endpoints` 一次性打印命中的端口、`source`、`chain`、两个端点的完整视图（host / port / path / protocol / auth）与存活探针结果；`gda api` 在端点不可达时把「端口 + 来源 + 链位」写进错误信息。

**存活校验两级**（[`src/probe.ts`](../../../cli/src/probe.ts)）：轻量（默认）只做 TCP 连接探测（800ms 超时），端口能连上即视为存活；严格在轻量之上再对端点 URL 发一次 HTTP GET，能应答才算存活（REST 面的 401 也算应答成功）。严格级别能识别「端口被非 HTTP 服务占用」的假阳性。

**Godot userdata 目录解析**（trace/config 两级定位用，[`src/platform.ts`](../../../cli/src/platform.ts) `userDataBase()`）：`GODOT_AUTOPILOT_USERDATA` 显式覆写 > `%APPDATA%\Godot\app_userdata`（Windows）/ `~/Library/Application Support/Godot/app_userdata`（macOS）/ `~/.local/share/godot/app_userdata`（其他平台）。覆写变量供测试与非常规安装路径绕过平台推断，未设或为空白时走原生分支（既有解析语义不变）；其下再按 `<userdata>/<project_name>/godot_autopilot/` 读 `server_ready` trace 与 `config.json`。

## 集中根与两套注册表

**集中根解析**（[`src/platform.ts`](../../../cli/src/platform.ts) `gdaHome()` / `gdaRoot()`）：`GODOT_AUTOPILOT_HOME` 显式覆写 > `os.homedir()`，其下固定目录名 `.godot-autopilot`（`GDA_HOME_DIR_NAME`）。插件侧的解析是四级（另含平台环境变量与派生兜底，且候选必须是已存在目录），CLI 侧只有两级——差异见 [端口与实例登记](ports.md)「已知限制」。

| 注册表 | 位置 | 记录者 | 用途 |
|---|---|---|---|
| CLI 本地注册表 | `<集中根>/cli/instances.json`（state dir 可由 `GODOT_AUTOPILOT_CLI_HOME` 覆写） | `gda start` | CLI 自己拉起的编辑器实例 |
| 插件全局实例索引 | `<集中根>/instances.json` | 插件（编辑器进程内） | 所有在跑的插件实例，含端口、进程号与 REST 令牌 |

**令牌读取方式**：`gda api` 先按寻址链解析目标，再从插件全局实例索引里按端口 / 工程路径匹配记录，取其 `api_token` 字段作为 `Authorization: Bearer <令牌>` 头；取不到时报传输错误 `no API token is available for port N (source …)`，并提示从实例登记的 `api_token` 字段读取（索引路径由 `pluginIndexPath()` 打印在提示里）。

## 登记表（[`src/registry.ts`](../../../cli/src/registry.ts)）

**state dir 解析**：`GODOT_AUTOPILOT_CLI_HOME` 显式覆写 > `<集中根>/cli/`（即 `gdaRoot()` 下固定目录名 `cli`）。无法确定时报错并提示设 `GODOT_AUTOPILOT_CLI_HOME`。

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
- 加载时过滤死 pid 条目（`isAlive` 探测），有清理则写回。`isAlive` 在 Windows 上解析 `tasklist /FI "PID eq N" /NH /FO CSV` 输出的 pid 字段，**与系统语言无关**（旧实现按输出是否以 `INFO`/`消息` 开头判断，中文 Windows 实际输出「信息:」导致恒判存活）；POSIX 走 `process.kill(pid, 0)`。

## 生命周期语义

### start（[`src/commands/start.ts`](../../../cli/src/commands/start.ts) + [`src/process.ts`](../../../cli/src/process.ts)）

1. **同工程活跃实例预检**（10-22 新增，[`src/preflight.ts`](../../../cli/src/preflight.ts)）：**启动编辑器之前**先判该工程是否已有活跃实例，命中即**拒绝启动**（退出码 2），消息给出已有实例的端口与进程号，suggestion 给出三条可操作路径——`gda stop --project <工程>` 先停掉它、`--port <端口>` 改用其它端口、或设 `GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE=1` 豁免。
   - **活跃的判定**（两处来源，任一命中即算）：① CLI 本地注册表按工程路径找**进程存活**的条目（`src/registry.ts` 的 `findLiveByProject`）；② 插件全局实例索引（`<集中根>/instances.json`）里**工程路径相同**且**进程存活**的记录，再用两级存活校验确认端口真的在应答（`src/probe.ts`，级别取 `--probe`，默认轻量 TCP；严格级再发一次 HTTP GET）。陈旧登记（进程已死或端口不应答）不触发拒绝。
   - **显式 `--port` 时按用户意图处理**：用户已表明要另起一个实例，故**不拒绝**，改为在 stderr 给出冲突提示（已有实例的端口与进程号 + 「同一个工程开第二个编辑器可能静默崩溃」的风险说明）；请求端口与既有实例端口相同时，提示点明该端口已被占用。
   - **豁免开关**：`GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE`（与插件侧同名同语义，空串与 `0`/`false`/`off`/`no` 视为关闭）——置为开启时跳过拒绝并改为告警。
   - 提示只含端口与进程号，**不含令牌等敏感信息**。
2. **Godot 可执行文件链**：`--godot` > PATH 搜索（Windows: `godot.exe`/`godot4.exe`；其他: `godot`/`godot4`）。**不读 `GODOT_PATH` 环境变量、也不读 `<project>/.env`**——`GODOT_PATH` 是 L2 引擎测试的专属配置（`tests/runner/godot_process.cpp`），生产环境既不需要也不应被 CLI 嗅探；未显式指定且 PATH 未命中即报错并提示 `--godot`。
3. **端口**：显式 `--port` 或 9530–9599 自动选（`pickFreePort`，bind `127.0.0.1` 探测空闲）。
4. **导入预步**：默认执行 `--headless --import --path`（120s 超时，失败非致命仅告警）；`--no-import` 跳过。
5. **启动与脱父**：直连 `spawn` 拉起编辑器（stdin 忽略、stdout/stderr 重定向到日志文件，环境变量注入 `GODOT_AUTOPILOT_PORT`）。Windows 置 `detached: true` 让编辑器脱离父进程与控制台（Bun 的直系子进程会随 CLI 退出被作业对象连带杀掉），随后 `unref()` 释放引用；**pid 由 `spawn` 直接返回**，不做端口反查。非 Windows 不设 `detached`。引擎不可执行（路径不存在或不可运行）时抛 `failed to launch the editor executable: ENOENT: …` 并附 suggestion `check the --godot path points at a runnable Godot binary`，不再笼统报「failed to spawn」；该路径编辑器从未起来，故不进回滚流程，pending 日志直接删除。
6. **就绪探测**：TCP + MCP `ping`（`direct: true` 直连，500ms 间隔，默认 60s 超时，`--wait` 覆盖，0=不等待）。进程提前退出或超时均报传输错误并附日志路径。**失败回滚**（`rollbackFailedStart`）：报错前先终止已拉起的编辑器并删除 pending 日志，登记表中不写该实例；终止结果分三态，消息均带 pid——`the editor process (pid N) was terminated` / `the editor process (pid N) had already exited` / `the editor process (pid N) could not be terminated, stop it manually: taskkill /F /T /PID N`（POSIX 为 `kill -9 N`）；pending 日志的处置亦附在消息尾部（`its pending log was removed` / `its pending log could not be removed (…)`）。
7. **登记**：就绪后写入 registry（id = `p<pid>`），日志从 `.pending-<port>-<ts>.log` 重命名为 `<id>.log`。
8. **`--play`**：就绪后调 `play_editor_current_scene`（非致命，失败仅告警）。

### stop（[`src/commands/stop.ts`](../../../cli/src/commands/stop.ts)）

- **停止方法按窗口状态选择**（`stopMethod(force, headless)`）：显式 `--force` **或目标实例 `headless: true`** 时强制停止，否则优雅停止。无窗口实例收不到 `taskkill`（不带 `/F`）发出的关闭请求，因此必须直接强制；此时输出的 `method: "force"` 是**预期值**，不是优雅停止超时后的降级。
- **有窗口实例默认优雅**：Windows `taskkill /PID`，其他 `SIGTERM`；等待默认 10s（`--wait` 覆盖）。
- **失败只报告**：超时未退出时输出 `ok: false` + hint（`the process is still running; retry with --force`），退出码 2，**保留登记**；不会静默升级为强制停止。
- **`--force` 强杀**：Windows `taskkill /F /T /PID`，其他 `SIGKILL`；强杀后仍存活报 `the process survived a force stop`。
- 停止成功后从 registry 移除条目。

### restart（[`src/commands/restart.ts`](../../../cli/src/commands/restart.ts)）

- **定位规则与 `stop` 完全一致**：位置参数 `<id>`，或 `--id` / `--port` / `--project` 三选一（同时给出报用法错误）。只作用于 **CLI 本地注册表**里的实例（`gda list` 可见的那些），不会去操作插件索引里不由 CLI 拉起的编辑器。
- **流程**：① 定位条目；② 进程存活时先停止——按与 `stop` 相同的规则选择方法：**无窗口（`headless: true`）或显式 `--force` 直接强制停止**，有窗口实例默认优雅（Windows `taskkill /PID`、其他 `SIGTERM`）；③ **固定等待 10 秒**直到进程消失（该阶段**不受 `--wait` 影响**）；④ 从注册表移除旧条目；⑤ 以**原端口、原 headless、原 godot 路径**调用 `start`（`--wait` 在此阶段生效，默认 60 秒就绪超时；`--no-import` / `--play` / `--probe` 随本次调用传入）；⑥ 输出 `{ok: true, restarted: true, previous: {id, pid, port}, ...新实例视图}`。
- **失败语义**：有窗口实例优雅停止后 10 秒仍未退出 → **不启动替代实例**，退出码 2，建议改用 `--force` 或手工停止，并保留登记。理由与插件侧同工程守卫一致——同一工程同时开两个编辑器可能静默崩溃（见 [端口与实例登记](ports.md)「同一工程的多实例：默认拒绝」）。启动阶段失败时，错误消息会追加「原实例已被停止」，便于判断当前已无实例在跑。
- 目标实例本来就已退出时，跳停止与等待，直接按登记里的端口 / headless / godot 路径重新拉起。

### logs / list

- `logs <id>`：读 state dir `logs/<id>.log`，输出末尾 `--tail` 行（默认 50）。
- `list`：列 registry 全部实例（含 `live` 实时状态）。

## 控制面（`gda serve`）

### 定位

**纯代理**：控制面不复制任何工具语义、不解释 MCP 报文、不缓存响应，只回答「转给谁」并原样转发到插件实例的 `/mcp` 或 `/api`。工具 schema、授权判定、错误契约仍由插件与 [REST 接口面](api.md) 定义。启停实例是 CLI 的职责（`start` / `stop` / `restart`），控制面**不做实例启停**（见「已知限制」）。

典型用途：客户端只被允许连一个端口、或同一台机器上有多个编辑器实例需要按实例寻址时，由控制面提供唯一入口并按路由分派。

启动形式：`gda serve [--port <端口>] [--bind <地址>] [--no-auth] [--timeout <秒>]`。

### 监听与绑定

| 项 | 口径 |
|---|---|
| 默认监听 | `127.0.0.1:9700` |
| `--port <n>` | 1–65535；绑定失败退出码 2，提示改用其它端口（默认 9700） |
| `--bind <addr>` | 只接受环回地址：`127.0.0.1` / `::1` / `localhost`（大小写不敏感、前后空白忽略）；其它值**拒绝启动**（退出码 3），并提示远程访问改用 SSH 隧道或反向代理 |
| `--timeout <sec>` | 上游等待秒数，默认 60；必须为正数 |
| `--no-auth` | 显式关闭控制面认证；启动输出标 `authentication: disabled`，并在 stderr 打 WARNING（本机任何进程都可经该监听调用编辑器工具） |

端口选择约束见 [端口与实例登记](ports.md)「控制面端口」：9700 与插件首选 9527、CLI 自启 9530–9599、插件保留与自动分配区间（19900–29999）都不重叠。**控制面端口与实例端口是两套独立概念**——控制面不占用实例登记，也不写 `instances.json`。

### 路由表（[`src/serve/router.ts`](../../../cli/src/serve/router.ts)）

| 方法 | 路径 | 认证 | 行为 |
|---|---|---|---|
| GET | `/control/health` | 免 | `{ok, service: "gda-serve", version, uptimeSec}` |
| GET | `/control/instances` | 需 | 活跃实例清单：`id` / `port` / `pid` / `project` / `version` / `live` / `endpoints`；**不含 `apiToken`** |
| ANY | `/i/<ref>/api[/…]` | 需 | 按实例 ID / `p<pid>` / 端口精确转发到该实例的 `/api/...` |
| ANY | `/i/<ref>/mcp` | 需 | 按实例 ID / `p<pid>` / 端口精确转发到该实例的 `/mcp` |
| ANY | `/api/...` | 需 | 唯一活跃实例自动转发；多个返回 409，零个返回 503 |
| ANY | `/mcp` | 需 | 同上（MCP 面自动转发） |
| 其它 | 任意 | 需 | 404 `not_found` |

- `/i/<ref>` 接受三种寻址，按以下优先级匹配（[`resolveInstanceByRef`](../../../cli/src/serve/router.ts)）：
  1. **插件 `instance_id` 精确匹配**：形如 `gda-daa757f3-9672-1792697320437`；
  2. **CLI 标识 `p<pid>`**：形如 `p9672`，按插件实例记录里的进程号匹配，支持「存活优先、否则取首条」；该标识正是 `gda list` 显示的实例标识，可直接拼进 `/i/<ref>/...`；
  3. **纯数字端口**：形如 `9530`，须为 1–65535 的整数，匹配实例端口或声明的 MCP / REST 端点端口。
- 三种寻址均无匹配返回 404 `instance_not_found`。
- 路由**只按路径匹配**，请求方法不参与判定（表中按惯例记 `GET` / `ANY`）；方法是否被上游接受由目标实例的端点决定。
- 缺省路由的「活跃」判定 = 插件全局实例索引里进程存活，与 `/control/instances` 的 `live` 同口径；唯一活跃时自动转发，**多个**返回 409 `instance_ambiguous`（附 `candidates` 候选清单），**零个**返回 503 `no_active_instance`。
- 转发前对目标实例做进程与端点探活；实例已死或端点不应答返回 503 `instance_unavailable`，上游连接失败返回 502 `upstream_unreachable`，上游超过 `--timeout` 返回 504 `upstream_timeout`。

### 认证与令牌文件

| 项 | 口径 |
|---|---|
| 令牌文件 | `<集中根>/control.token`，默认 `~/.godot-autopilot/control.token`（集中根口径见上文「集中根与两套注册表」） |
| 生成 | `gda serve` 启动时若文件不存在，写入 **32 字节随机值的十六进制**（64 字符），权限 `0600`；**已存在则复用**，不轮换 |
| 客户端 | `Authorization: Bearer <token>`；比较为定长常量时间比较 |
| 免认证 | 仅 `GET /control/health`；`/control/instances` 与全部转发路由均需令牌 |
| `--no-auth` | 服务端不校验令牌；启动时明示认证已关闭 |

令牌本身是**本地凭据**，不改变环回绑定的安全边界；控制面无 TLS，不要把它当作可暴露到网络的远程入口。安全姿态见 [T0 安全边界与并发契约](../security_contract.md)。

### 转发与流式透传

- **请求头**：保留 `Content-Type` / `Accept` 等客户端头；丢弃客户端 `Authorization` / `Host` 与逐跳头（`Connection`、`Keep-Alive`、`TE`、`Trailer`、`Transfer-Encoding`、`Upgrade`、`Proxy-Authenticate`、`Proxy-Authorization`），再注入目标实例登记的 `apiToken` 作为 `Authorization: Bearer`——客户端只需持有控制面令牌，不必知道各实例的令牌。
- **响应**：状态码与上游头原样保留，删去 `Content-Length` / `Content-Encoding` / `Transfer-Encoding` 后由本地传输层重新分帧，并加 `X-GDA-Instance` 标识实际承接请求的实例（实例 ID 缺失时用 `port:<端口>`）。响应体**边到边流式透传**，不缓冲整包，MCP 的 SSE 分帧因此能直接穿过控制面。

### 日志口径

每个请求一行到 stdout：UTC ISO 时间、方法、路径、实例标签（实例 ID / `port:<端口>` / `control` / `-`）、状态码、耗时毫秒。**不记录令牌，也不记录请求体与响应体**。

### 已知限制

- **`--timeout` 只覆盖到响应头**：计时器在拿到上游响应头后即清除，响应体继续流式传输；因此 MCP / SSE 长连接不会被该超时切断。反过来说，上游发完头再长时间不吐数据也不会被它中止。
- **控制面不做实例启停**：HTTP 层不提供启动 / 停止端点，只做只读查询与转发；启停只在 CLI 侧（`start` / `stop` / `restart`）。这是明确的设计决定，不是遗漏——把进程启停暴露成 HTTP 端点会把本机进程控制变成任何能访问该端口的客户端都能触发的操作。
- **控制面无 TLS**：仅允许环回绑定，远程访问须自建 SSH 隧道或反向代理（`--bind` 会拒绝非环回地址）。

## 协议（[`src/mcp/client.ts`](../../../cli/src/mcp/client.ts)）

**标准 MCP 握手**：`initialize`（protocolVersion `2025-11-25`，clientInfo `gda-cli`）→ `notifications/initialized` → `tools/call`。懒握手：首次调用遇 400 + `-32600`（未初始化）时自动补握手并重试一次；握手状态按目标 URL 键控（同进程切换目标时各自握手，单目标进程行为不变）。

**协议版本对齐（2026-10-19）**：声明版本取 SDK 旧世代当前版本 `kLegacyProtocolVersion = "2025-11-25"`——GDA 未配置 `ServerOptions::protocol_version`，客户端声明受支持的旧版时服务端原样尊重（封顶 `2025-11-25`；modern `2026-07-28` 仅 `server/discover` 路径可达，与本客户端无关）。握手成功后从 initialize 响应 `result.protocolVersion` 学习协商版本，同一目标的后续 POST 携带 `MCP-Protocol-Version` 头；学习值为 modern 或不在已知五版本表（2024-11-05 / 2025-03-26 / 2025-06-18 / 2025-11-25 / 2026-07-28）内时不发头——modern 头会触发 stateless 传输 hold-open 长连接，与单响应客户端不兼容；握手失败不留下学习值。

**直连工具集**（`META_DIRECT_TOOLS`）：`ping`/`search_tools`/`list_categories`/`get_tool_detail`/`batch_execute`/`code_execute` 直接发 `tools/call`；其余工具经 `call_tool` 包装（`{name: "call_tool", arguments: {name, arguments}}`）。

**响应解析**：SSE（`text/event-stream`，`data:` 帧拼接）与纯 JSON 双通道。

**SDK 0.3.7 握手升级（历史问题）**：mcp-cpp-sdk 0.3.7 起严格要求完整握手，旧懒握手只发 `notifications/initialized` 被拒 `-32600`。CLI 升级为标准 `initialize` 流程（`840e648`）。`test_cli_contract.py` 的 mock 服务器严格模拟该语义（initialize 返回 200 SSE、notifications/initialized 返回 202、未初始化时 tools/call 被拒 400 + `-32600`）。

## 与技能书阶梯的关系

三级调用阶梯（[`godot-autopilot-tools/SKILL.md`](../../../skills/godot-autopilot-tools/SKILL.md)）：

1. **原生 MCP 工具**（客户端 harness 直连）
2. **GDA CLI**（本载体；`bunx`、全局安装、独立二进制是同一载体的三种分发方式）
3. **手动指南**（无 CLI 可用，[`manual-fallback.md`](../../../skills/godot-autopilot-tools/references/manual-fallback.md)）

技能书另有一份 **REST 接口参考**（[`rest-api.md`](../../../skills/godot-autopilot-tools/references/rest-api.md)）：REST 面与 MCP 面共用同一端口，REST 需 Bearer 令牌且无会话握手，面向「不做 MCP 握手的客户端」。该参考只示范 Python 与 JavaScript 两段最小调用，不新增脚本化通道——脚本化调用仍只有 GDA CLI 一条。

技能书钉版本 `github:jesspig/Godot-Autopilot#<VERSION>`；`version_sync_guard` 扫描 `skills/` 下全部文本文件与根 `README*.md`，断言其中每一处版本钉均与根 `VERSION` 同值（`VERSION` 与 `CLI_VERSION` 两个值源一并核对），非版本引用（如 `#develop`）只登记、不参与比对；覆盖范围以守卫脚本为准。

## 已知限制

- **`bun pm pack` 要求 `package.json` 有 `version`**：bunx 从 git 安装不需要该字段，故 `package.json` 未加 `version`（保持与根 `VERSION` 单一来源一致）。
- **`file:` 依赖 EPERM**：本机 `file:` 协议依赖有 EPERM 环境限制；git/tarball 路径（`github:...`）不受影响，故分发走 git 而非本地 pack。
- **集中根解析比插件简单**：CLI 侧为「`GODOT_AUTOPILOT_HOME` 覆写 → `os.homedir()`」两级，插件侧另有平台环境变量与派生兜底、且候选必须是已存在目录；覆写或异常 HOME 环境下两者可能分叉。**这是分工不同而非校验缺失**——CLI 没有「自行推断家目录」的派生层，插件侧那条存在性校验在 CLI 侧没有适用对象；判定依据见 [核心模块](core.md)「集中根解析」的两级校验口径结论。
- **REST 令牌只在插件全局索引里**：CLI 本地注册表不含令牌，因此 `gda api` 在第 4 级命中（只有 CLI 登记的实例）而第 5 级索引不可读时拿不到令牌，会报 `no API token is available`。

## 测试

| 层 | 位置 | 规模 | 说明 |
|---|---|---|---|
| TS 单测 | [`cli/tests/`](../../../cli/tests/) | 16 文件 259 项（257 pass + 2 skip，721 次 expect()） | `bun test cli/tests`；api_commands 43 / args 4 / client 17 / client_handshake 8 / commands 40 / detach 1 / lifecycle 34 / preflight 10 / process 8 / registry 8 / restart 4 / root_dir 7 / serve_auth 14（其中 2 项 Windows 跳过）/ serve_router 43 / stop_method 3 / target 15 |
| ctest `cli_unit` | [`tests/scripts/run_cli_unit.py`](../../../tests/scripts/run_cli_unit.py) | 封装 `bun test` | bun 缺失时 SKIP 并 exit 0 |
| ctest `cli_contract` | [`tests/scripts/test_cli_contract.py`](../../../tests/scripts/test_cli_contract.py) | CLI 载体 mock conformance | bun 驱动 gda CLI；bun 缺失时 SKIP |
| ctest `version_sync_guard` | [`tests/guard/version_sync_guard.py`](../../../tests/guard/version_sync_guard.py) | 全部版本钉与 `VERSION` 同值 | 扫描 `skills/` 全部文本文件 + 根 `README*.md` 的每一处版本钉，另含 `VERSION` 与 `CLI_VERSION` 两个值源 |

## 相关页面

- [REST 接口面](api.md) — `/api` 端点契约、Bearer 令牌与状态码
- [端口与实例登记](ports.md) — 端口模型、控制面端口分工、实例登记结构与七级发现链
- [支撑模块](support.md) — 技能书（9 册结构、渐进披露 references 与 MCP 资源注册）
- [安全边界与并发契约](../security_contract.md) — CLI 安全包络（§3.4）
- [测试体系](../tests.md) — ctest 注册点与 TS 单测计数
- [项目总览](../overview.md) — 目录树中的 `cli/`
