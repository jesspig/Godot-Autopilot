# T-08 CLI 生命周期与发现一级命令 —— 实现证据（静态记录，未运行测试）

本文件只记录静态实现事实。按任务约束，子代理未运行 `bun test` / `tsc` / 任何构建命令，
不声称任何测试通过；全部测试均由主代理统一执行。

## 一、改动文件

新增（CLI 命令层）：
- `cli/src/commands/editor.ts`
- `cli/src/commands/project.ts`
- `cli/src/commands/tool.ts`
- `cli/src/commands/doctor.ts`
- `cli/src/commands/shell.ts`

修改（CLI 核心与入口）：
- `cli/src/discovery.ts`：保留 MCP 协议工具过滤；新增原生 GCP envelope 构造/解析、
  4 字节长度前缀 IPC 会话（字符串 id，不发送 initialize、不访问 HTTP /mcp）、
  hello/capabilities/catalog.list 原生调用、离线 catalog 读写与 FNV-1a hash 校验、本地过滤与 categories 汇总。
- `cli/src/target.ts`：新增 `socketText` selector、`instanceId/pinned/stale` 解析字段、
  `AmbiguousTargetError`（带 candidates）、`resolveNativeTarget`（显式优先级、歧义候选、
  陈旧实例与 PID 复用拒绝、实例固定）。
- `cli/src/command-spec.ts`：仅为新命令/新 flag 补 spec —— `editor.ping`、`doctor.diagnose`、
  `editor.status/restart` 与 `tool.*` 的 target/offline flags、`doctor --ci`、`shell --protocol`；
  `ping` alias 目标改为 `editor.ping`，新增 `diagnose` → `doctor.diagnose` alias。
- `cli/bin/gda.ts`：新增一级领域命令拦截与 spec 严格解析，把 `--ci` / `--protocol`
  等新 flag 映射到处理器；保留 `gda commands`、帮助、旧顶层 alias 路径。
- `cli/src/commands/index.ts`：未改（顶层 `COMMANDS` 必须保持 24 项快照，
  新领域命令由 `gda.ts` 直接分发，避免破坏 `command-manifest/surface.test.ts`）。

修改（测试）：
- `cli/tests/lifecycle.test.ts`：追加 T-08 命令树/alias 端到端用例。
- `cli/tests/target.test.ts`：追加 `resolveNativeTarget` 用例。
- `cli/tests/tools.test.ts`：追加 tool 子命令、离线 catalog、hash 校验、live catalog 用例。
- `cli/tests/status.test.ts`（新增）：`editor status/ping` 原生输出与无 MCP initialize 用例。
- `cli/tests/doctor.test.ts`（新增）：`doctor --ci` 与 `diagnose` 稳定 code/remediation 用例。
- `cli/tests/shell-ndjson.test.ts`（新增）：NDJSON `id/argv/command/exitCode/envelope/shutdown` 用例。

未触碰：`src/` C++、`tests/` C++、CMake、Skills、VERSION、`third_party`、
`.handoff`、其他工作树、主工作树。

## 二、命令树变更

新增一级可调用入口（经 `gda.ts` 分发，不进入顶层 `COMMANDS` 快照）：
- `gda editor status|list|start|stop|restart|ping`
- `gda project list|info|open`
- `gda tool list|search|describe|categories`（`call`/`batch` 在本阶段明确拒绝并指向旧命令）
- `gda doctor run|diagnose`（`gda doctor` 默认 run；`gda diagnose` 为 alias）
- `gda shell run`（`gda shell` 默认 run；仅接受 `--protocol ndjson`）

原生状态输出字段（`editor status`）：`schema_version, ok, instance{id,pid}, project, version,
ipc_kind, ready, tool_count, capabilities`。已移除 `mcp_url`、`mcp`、`port`、
`port_source`、`transport`、`server` 等旧字段；错误分支输出相同字段骨架且 `ok:false`。

## 三、alias 策略

旧顶层命令继续走 `main()` → `COMMANDS` 原处理器，保持既有 MCP parity 测试不变；
一级领域命令为新的原生实现，二者互不覆盖：
- `list → editor.list`、`start → editor.start`、`stop → editor.stop`、`restart → editor.restart`
- `status → editor.status`（新原生输出；旧顶层 `status` 仍输出旧结构）
- `ping → editor.ping`（新原生 hello；旧顶层 `ping` 仍走 MCP）
- `projects → project.list`、`categories → tool.categories`、`search → tool.search`
- `describe → tool.describe`、`tools → tool.list`
- `diagnose → doctor.diagnose`

所有 alias 均为隐藏兼容入口，帮助文本指向新子命令路径；spec/alias 单源仍为 `command-spec.ts`。

## 四、目标解析规则

`resolveNativeTarget` 的显式优先级：`--id > --socket > --port > --project`；
显式 selector 全部优先于 `GODOT_AUTOPILOT_PORT` 环境变量与 cwd/plugin-index 隐式发现。
- `--id`：按实例 id 固定；进程已死或（POSIX 可测时）启动时间不匹配 → 拒绝陈旧实例。
- `--socket`：按 IPC socket 路径匹配实例记录；多实例命中 → `AmbiguousTargetError`。
- `--port`：按端口匹配插件实例记录；多实例命中 → `AmbiguousTargetError`。
- `--project`：多实例命中 → `AmbiguousTargetError`，candidates 含 `id/project/port/pid`。
- 解析结果携带 `instanceId/pinned/stale`，供 status/ping/tool/diagnose 复用。
- 原生通道只使用实例记录中的 IPC endpoint（`unix`/`tcp`/`pipe`），无 endpoint 时直接报错，
  不回退 HTTP /mcp，也不发送 MCP initialize。

## 五、离线 catalog 与 hash 校验

- `catalogPath()` 落在 CLI home 下 `catalog.json`（可用 `GODOT_AUTOPILOT_CLI_HOME` 覆写）。
- 文档 `{schema_version:1, hash, tools[]}`；hash = `fnv1a32` 对按 name 排序的
  `{name,description,category,tags[],aliases[],mutating,high_risk}` 规范化 JSON 计算。
- live `catalog.list` 成功后原子写入缓存；`--offline` 只读缓存；live 失败时若缓存存在则降级并标注 `source:"offline"`。
- 读取时重算 hash，不匹配即拒绝并提示重建，避免返回被篡改/截断的 catalog。

## 六、doctor / diagnose

稳定 code 集合（检查项 `id/code/status/detail/remediation`）：
`GDA_REGISTRY_OK/UNREADABLE`、`GDA_INSTANCE_OK/NO_INSTANCE/STALE_INSTANCE/UNKNOWN`、
`GDA_IPC_ENDPOINT_OK/MISSING`、`GDA_NATIVE_UNKNOWN`、`GDA_CATALOG_OK/MISSING/HASH_MISMATCH`、
`GDA_TARGET_OK/FAILED`、`GDA_INSTANCE_FRESH`、`GDA_NATIVE_OK/FAILED`。
`--ci` 模式固定字段顺序、去除绝对路径文本，只输出稳定 code 与 remediation；
有 error 级检查退出 2，仅 warn 退出 0。

## 七、shell NDJSON

`gda shell --protocol ndjson` 从 stdin 逐行读取 JSON，向 stdout 逐行输出：
- `{type:"argv", id, argv, command?}` —— 启动确认
- `{type:"envelope", id, envelope}` —— 子进程 stdout（合法 JSON 原样，否则 `{text}`）与显式 envelope 请求
- `{type:"exitCode", id, exitCode}` —— 子进程退出码
- `{type:"shutdown", id, ok:true}` —— 关闭确认（EOF 时 id 为 `eof`）
- `{type:"error", id, error}` —— 未知请求类型
`argv` 与 `command` 二选一；`shutdown` 终止全部子进程。

## 八、未运行验证原因

按任务约束，子代理不运行 `bun test` / `tsc` / `cmake` / 构建，也不提交。
以上全部用例为「已编写、未执行」。

## 九、风险与待主代理核对

1. `gda.ts` 对领域命令的拦截发生在 `main()` 之前；顶层 `COMMANDS` 保持 24 项快照，
   需确认 `command-manifest/surface.test.ts` 仍通过。
2. 新增 spec 会改变 `buildManifest()` 的 command_count/aliases；现有 manifest 测试只断言
   与 `COMMAND_SPECS`/`COMMAND_ALIASES` 一致，理论无硬编码计数，但需实跑确认。
3. 原生 GCP IPC 会话自行实现长度前缀与字符串 id 匹配，未复用 `ipc/client.ts`（不在授权文件内）；
   需主代理统一 typecheck 与 fake-server 用例验证帧边界、超时与错误水位。
4. `editor status` 依赖插件实例记录中的 IPC endpoint；若真实插件仍只声明 `mcp-jsonrpc`
   而未提供可用 socket/tcp endpoint，命令会报「no native IPC endpoint」——这是刻意不回退 HTTP 的结果。
5. `tool call/batch` 在本阶段拒绝并指向旧命令，避免新命令面发送 MCP initialize；
   若验收要求 `tool call` 可用，需在 T-09+ 接入原生 `call.start/confirm`。
6. `diagnose` 把 `port` 放在 `target` 诊断对象内（非 `status` 输出面），不违反 status 去 port 语义。
7. `editor list/start/stop/restart` 复用既有进程级实现；其中 `start` 的就绪探测与 `--play`
   仍走旧版 ping/`play_editor_current_scene` 通道（进程生命周期不属于原生控制面发现命令）。
   若验收把「不得发送 MCP initialize / HTTP /mcp」应用到 start，需要在后续任务改造
   preflight/play 的协议适配，本阶段未触碰 `start.ts`（不在授权文件列表内）。
