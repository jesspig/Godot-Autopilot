---
type: 模块文档
title: 端口与实例登记
description: 单一监听端口承载 MCP 与 REST 两个端点、端口解析优先级与候选序列、显式与自动分配的失败差异、实例登记结构与全局索引、CLI 侧发现链与回溯、异常退出后的端口与登记自愈、传输层工作线程的退出契约
tags:
  - 模块
  - 端口
  - 实例登记
  - 发现链
timestamp: "2026-10-22T03:59:53+08:00"
resource:
  - src/core/port_resolver.cpp
  - src/core/port_recovery.hpp
  - src/core/server_context.cpp
  - src/core/instance_registry.cpp
  - src/core/project_store.cpp
  - src/core/transport_lifecycle.hpp
  - src/core/gda_home.cpp
  - cli/src/target.ts
  - cli/src/instances.ts
---

# 端口与实例登记

> 审计日期：2026-10-22（**10-22 随本地收口批次增补「运行时边界」小节**——退出时若有未响应「队列关闭」的处理请求，收割会等它，最坏把退出拖到约 30 秒（崩溃换成慢退出）；进程被强杀时监听端口不立即释放、实例登记会残留，下次启动自动清理同工程残留登记并给出提示，但端口要等占用它的进程真正退出才彻底释放；并明确「同一工程不要同时打开两个编辑器」——第二个编辑器会在扩展加载阶段因 Godot 的扩展库临时副本被占用而可能静默崩溃，插件代码此时尚未运行、给不出提示，插件侧的同工程实例检测是第二道防线）2026-10-21（随开放接口面落地批次首建——本批把「端口」从单一 MCP 端口扩为**一个端口承载两个端点**（`/mcp` 与 `/api`），端口解析改为「环境变量 → 工程级配置偏好 → 自动分配」三级，自动分配有确定的候选序列；实例登记迁到集中根下的工程级目录并汇总出全局索引，供 CLI 发现链使用。实现见 [`src/core/port_resolver.cpp`](../../../src/core/port_resolver.cpp) 与 [`src/core/server_context.cpp`](../../../src/core/server_context.cpp)，登记见 [`src/core/instance_registry.cpp`](../../../src/core/instance_registry.cpp) 与 [`src/core/project_store.cpp`](../../../src/core/project_store.cpp)。本轮校订补三处接线事实：① 探测上限 256 含首选 9527 在内，失败信息形如 `no free port candidate bound after N probes: <原因>`；② CLI 本地注册表的 state dir 组成补 `instances.lock` 与 `logs/`；③ 令牌字段名改为不加反引号的自然表述，避免与代码内 `api_token` 字段混淆）。
> 统计口径：端口号与候选区间以 [`src/core/config.hpp`](../../../src/core/config.hpp) 的常量为事实标准；数值以运行时统计为准，见 [测试体系](../tests.md)「数值核算总表」。

## 端口模型：一个端口，两个端点

插件**只监听一个 TCP 端口**，该端口上同时挂两套路由：

| 端点 | 路径 | 常量 | 协议 |
|---|---|---|---|
| MCP | `/mcp` | `GDA_MCP_PATH` | Streamable HTTP（JSON-RPC / SSE） |
| REST | `/api` | `GDA_API_PATH` | 普通 HTTP（JSON） |

- 端点路径是常量，不可配置；端口是唯一可配项。
- 两个端点共用同一个自建 HTTP 传输层（[`src/core/http_transport.cpp`](../../../src/core/http_transport.cpp)）与同一把 `CommandQueue`——不存在「REST 另起一个端口」或「REST 绕过主线程」的第二条路径。
- 监听地址仍**只允许环回**（`127.0.0.1` / `::1`），非环回地址在 `ServerContext::start()` 被拒绝；见 [T0 安全边界与并发契约](../security_contract.md) §2。
- REST 端点的契约见 [REST 接口面](api.md)。

## 端口解析优先级

| 优先级 | 来源 | `PortSource` | 说明 |
|---|---|---|---|
| 1 | 环境变量 `GODOT_AUTOPILOT_PORT` | `kEnvExplicit` | 测试与 CI 场景的显式指定；`resolve_explicit_port()` 读取后经 `atoi` 转换 |
| 2 | 工程级配置偏好（`config.json` 的 `port` 键） | `kConfigExplicit` | 控制面板「Apply」写入工程级配置；`PluginConfig::load_port()` 读取，需 > 0 |
| 3 | 自动分配 | `kAutoAssigned` | 前两级都没有时启用候选序列顺延 |

另有一条**运行期改端口**的通道：控制面板 Apply 触发的 `ServerContext::restart(port)`，其 `requested_port_ > 0` 走 `kUserApplied` 分支——**优先级高于上面三级**，直接绑定该端口（绑定失败即报错，不顺延）。

**环境变量优先于配置**，保证 CI/测试场景不受控制面板的持久化值影响。

## 自动分配的候选序列

`PortResolver::candidate_sequence(pid)` 生成的顺序是：

1. **先试默认端口 9527**（`GDA_DEFAULT_PORT`）；
2. 随后从 `20000 + (进程号 mod 10000)` 起，**连续递增到 29999 并回绕**（候选区间 `[GDA_PORT_CANDIDATE_BASE, GDA_PORT_CANDIDATE_LIMIT]` = `[20000, 29999]`，共 10000 个）。

举例：进程号 12345 → `20000 + (12345 mod 10000)` = 22345，序列为 `9527, 22345, 22346, …, 29999, 20000, 20001, …`。

- 候选区间上界 29999 **低于**私有动态端口区间起点 32768（`GDA_PRIVATE_PORT_BASE`），由 `static_assert` 约束；控制面端口区间 `[19900, 19999]` 与自动分配区间不相交，同样由 `static_assert` 保证。
- 实际探测时**最多探 256 个候选**（`GDA_PORT_PROBE_LIMIT`，含首选的 9527 在内）；探完仍未绑定成功即启动失败，错误信息带实际探测次数与最后一次失败原因（`no free port candidate bound after N probes: <原因>`）。
- 回绕保证「每个进程都从自己的偏移处开始」，避免多实例总是挤在候选序列头部。

## 显式与自动的失败差异

这是本模型最要紧的一条：**显式指定的端口绑定失败时不自动更换**。

| 场景 | 行为 |
|---|---|
| 显式（环境变量 / 配置偏好 / 面板 Apply） | 只尝试该**一个**端口；绑定失败即启动失败，错误信息形如 `the requested port N is unavailable: <原因>`。**不顺延、不换端口**——用户手动改，避免「配置了 9527 却连到了别处」这种难以察觉的错位 |
| 自动分配 | 按候选序列顺延到下一个可用端口，并把实际绑定到的端口写回分配说明（`auto assigned: preferred 9527, fallback candidates 20000-29999; bound port N`） |

端口冲突因此有两种完全不同的可观测表现：显式冲突表现为**启动失败并报端口不可用**；自动冲突表现为**静默换端口**——所以「客户端连不上」时，先确认实例登记里记录的端口，而不是假定仍是 9527。

## 实例登记

插件每次成功绑定端口后写一条实例登记，供 CLI 发现链与 REST 令牌分发使用。

### 存储位置

实例登记采用「工程级隔离 + 全局汇总」两段式（目录布局见 [核心模块](core.md)）：

| 内容 | 路径 |
|---|---|
| 单实例记录 | `<集中根>/projects/<项目名>-<路径哈希前 8 位>/instances/<实例 ID>.json` |
| 全局索引 | `<集中根>/instances.json` |

- `<项目名>` 取工程路径的目录名（经非法字符替换并截断到 40 字符）；`<路径哈希前 8 位>` 是归一化工程路径的 FNV-1a 十六进制摘要前 8 位——**同名工程靠哈希区分**，两处同名工程不会互相覆盖。
- 实例 ID 形如 `gda-<哈希 8 位>-<进程号>-<启动毫秒时间戳>`，同一工程多次启动互不覆盖。
- 写单实例记录后**自动重建全局索引**（`update_global_index()` 遍历全部工程目录下的实例文件汇总）；删除实例同样会刷新索引。
- 条目读取失败或格式非法时**跳过该条并记 Warning**，不阻断其余条目。

### 单实例记录结构

```json
{
  "instance_id": "gda-1a2b3c4d-12345-1760000000000",
  "port": 9527,
  "pid": 12345,
  "project_path": "C:/path/to/project",
  "api_token": "<64 位十六进制令牌>",
  "endpoints": {
    "mcp": {"host": "127.0.0.1", "port": 9527, "path": "/mcp", "protocol": "mcp", "auth": "none"},
    "api": {"host": "127.0.0.1", "port": 9527, "path": "/api", "protocol": "http", "auth": "bearer"}
  },
  "gda_home": "C:/Users/<user>",
  "started_at": 1760000000000,
  "version": "0.3.0"
}
```

关键字段语义：

- `port` 与 `endpoints.*.port` 是**同一个端口**——两个端点不各自占端口。
- `endpoints.mcp.auth` 为 `"none"`（MCP 面仍无认证，沿用可信客户端模型），`endpoints.api.auth` 为 `"bearer"`（REST 面要求令牌）。
- `api_token` 是 REST 面的 Bearer 令牌；令牌文件也在工程级目录内，签发来源优先级为环境变量 `GODOT_AUTOPILOT_API_TOKEN` 优先。
- `gda_home` 记录本次实例解析出的集中根，便于排查「多用户 / 多 HOME」造成的分叉。

### 全局索引结构

```json
{"instances": [ { ...单实例记录... } ]}
```

索引是**全量快照**（每次写/删实例后重建），CLI 侧按「端口匹配」或「工程路径匹配」检索；同一工程有多条记录时优先取**进程存活**的那条，仍有多条则取 `started_at` 最大者（最新启动的那个）。

## 同一工程的多实例：默认拒绝

插件启动早期（**绑定端口之前**）会检查「同一工程是否已有存活实例」。判据是实例登记里存在 `project_path` 与当前工程相同、且同时满足两个条件的记录：

| 校验 | 手段 | 判定 |
|---|---|---|
| 进程存活 | Windows 走 `OpenProcess` + `GetExitCodeProcess`（退出码为 `STILL_ACTIVE` 视为存活；因权限不足查不到时**保守判存活**）；POSIX 走 `kill(pid, 0)`（返回 `EPERM` 视为存活） | 该进程号仍被系统持有 |
| 端口占用 | 用一个临时套接字去 `bind` 该记录声明的端口（**不设** `SO_REUSEADDR`，否则会与已有监听者共享端口） | 返回 `EADDRINUSE` / `WSAEADDRINUSE` 即端口仍被占用 |

两条**同时成立**才算「确有实例在跑」。这样「上次运行残留的旧记录」（进程已退出、端口已释放）不会被误判成冲突；进程号被系统复用的情况则被端口占用这一条挡住。

命中冲突时插件**拒绝启动**：不监听端口、不写实例登记，并在 `last_error_` 与日志里给出明确提示——已有实例的端口与进程号、关闭另一个编辑器的建议、以及豁免开关。提示同时出现在三处：日志（`LogSystem`，Error 级）、面板状态区（`McpConfigDock` 的状态行，未运行时显示 `last_error_`）、以及 `last_error_` 本身；提示里不含令牌等敏感信息。提示原文形如 `another Godot Autopilot instance is already running for this project on port 9527 (pid 12345); a second instance on the same project is refused because Godot loads the plugin through a shared temporary copy and the editor can die without any message; close the other editor for this project, or set GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE=1 to start it anyway on another port (GODOT_AUTOPILOT_PORT=<port>); the editor may still crash while the other instance is running`（生成点为 `src/core/instance_guard.cpp` 的 `conflict_message`）。

检查发生在**写自己的实例登记之前**，且跳过与自身进程号相同的记录，因此不会把自己刚写的记录算成冲突。运行期改端口触发的 `restart()` 属于「改绑」而不是「新开实例」，不再重复该检查。

### 豁免：允许同工程多实例

调试等场景需要在同一工程开第二个实例时，用环境变量 `GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE=1` 显式豁免（**默认关闭**；`0` / `false` / `off` 视为关闭，空值同样视为关闭）。豁免后照常启动，端口仍按既有规则解析（显式指定优先，否则自动分配到候选序列里的下一个可用端口）。

豁免只跳过插件侧的拒绝，**不消除 Godot 侧的加载机制**：编辑器加载 GDExtension 时会先把扩展库复制成同目录下的临时副本，第二个编辑器进程可能因此崩溃，而插件代码在扩展加载之后才运行，无法在复制发生前介入。插件侧检查的价值是把「同工程重复开编辑器」从静默崩溃变成一条明确提示，并避免第二个实例悄悄占走另一个端口。

## CLI 侧的发现链

CLI 的目标解析是一条**七级优先链**，每一级都记录来源（`source`）与链位（`chain`），便于回溯「我为什么连到了这个端口」：

| 优先级 | 来源 | `chain` | `source` | 行为 |
|---|---|---|---|---|
| 1 | `--port` | `port` | `cli` | 直接端口（1–65535，非法报用法错误） |
| 2 | `--id` | `id` | `id` | CLI 本地注册表按实例 ID 查找，**须进程存活**，否则报 `unknown instance` |
| 3 | 环境变量 `GODOT_AUTOPILOT_PORT` | `env` | `env` | 环境变量端口 |
| 4 | CLI 本地注册表 | `cli-registry` | `project` / `cwd` | 按工程路径（`--project` 或当前工作目录）找**存活**实例 |
| 5 | 插件全局实例索引 | `plugin-index` | `registry` | 读 `<集中根>/instances.json`，按工程路径匹配（不要求进程存活） |
| 6 | 旧路径回退 | `legacy` | `trace` / `config` | 旧 `user://godot_autopilot/` 下的 `server_ready` trace → `config.json` 取端口 |
| 7 | 默认 | `default` | `default` | 9527 |

**回溯能力**：`gda endpoints` 会把命中的端口、来源（`source`）、链位（`chain`）、两个端点的完整视图（host / port / path / protocol / auth）与存活探针结果一次性打印出来——端口从哪一级来、当前是否活着，看这一条命令即可。`gda api` 在端点不可达时同样会把「端口 + 来源 + 链位」写进错误信息，并提示改用 `--port` 或检查实例登记。

### 存活校验的两级

| 级别 | 手段 | 判定 |
|---|---|---|
| 轻量（默认，`--probe light`） | TCP 连接探测（默认 800ms 超时） | 端口能连上即视为存活 |
| 严格（`--probe strict`） | 轻量 + 对端点 URL 发一次 HTTP GET | 端口能连上**且**该地址上确实有 HTTP 服务应答才视为存活 |

严格级别能识别「端口被非 HTTP 服务占用」这类假阳性（TCP 通但 HTTP 不通）；代价是多一次 HTTP 往返。注意它只确认「那儿有个 HTTP 服务」，不校验对方是不是 GDA——REST 面的 401 同样算应答成功。

### 两套注册表的分工

| 注册表 | 位置 | 记录者 | 用途 |
|---|---|---|---|
| CLI 本地注册表 | `<集中根>/cli/instances.json`（state dir 可由 `GODOT_AUTOPILOT_CLI_HOME` 覆写；同目录下还有 `instances.lock` 与 `logs/`） | `gda start` | CLI 自己拉起的编辑器实例 |
| 插件全局实例索引 | `<集中根>/instances.json` | 插件（编辑器进程内） | 所有在跑的插件实例，含端口、进程号与 REST 令牌 |

两者在发现链上**分处第 4 级与第 5 级**：CLI 先看自己拉起的实例，再退到插件写的全局索引。REST 令牌只出现在后者里，因此 `gda api` 需要从插件全局索引取令牌；取不到时报 `no API token is available for port N (source ...)`，并提示从实例登记的 api token 字段读取。

## 已知限制

- **端口是全局资源，多实例必然分散**：多个编辑器同时开着时，除了第一个占用 9527 的实例，其余走候选序列落在 20000–29999。客户端若假定 9527，必须先发现再连接。
- **全局索引是快照**：实例登记的写入失败（目录不可写、磁盘满）会让该实例对 CLI 不可见，但**不影响 REST 端点本身**——插件照常监听，CLI 只是找不到它。
- **令牌只在实例登记里**：没有第二个可读位置（不写环境变量、不写工程文件），因此丢了实例登记就等于丢了令牌，需重启插件实例重新签发。
- **旧路径回退只取端口**：第 6 级仅从旧 `user://godot_autopilot/` 读 `server_ready` trace 或 `config.json` 中的端口，不迁移数据本身；数据迁移是控制面板的复制式导入，见 [核心模块](core.md)。
- **CLI 的集中根解析比插件简单**：CLI 侧为「`GODOT_AUTOPILOT_HOME` 覆写 → `os.homedir()`」两级，不做插件侧的存在性校验；两者在正常环境下指向同一目录，但覆写或异常 HOME 环境下可能分叉。**这不是校验缺失而是分工不同**——CLI 没有「自行推断家目录」的派生层，插件侧那条存在性校验在 CLI 侧没有适用对象；判定与依据见 [核心模块](core.md)「集中根解析」的两级校验口径结论。

### 异常退出后的端口与登记残留

编辑器崩溃、被强杀或插件未走完退出流程时，**监听端口与实例登记都可能残留**：登记文件仍在（CLI 发现链会指向一个已经不存在的实例），端口在编辑器进程仍存活时也可能被占住。

**下一次启动会自愈并提示**（实现见 [`src/core/port_recovery.hpp`](../../../src/core/port_recovery.hpp)）：

| 步骤 | 行为 |
|---|---|
| 启动首次绑定端口 | 扫描全局实例索引，挑出**同一工程**（路径按分隔符归一后比较）的登记，并按进程号判活 |
| 进程已死的登记 | 视为**残留**，直接删除登记文件并重建全局索引，日志记 `removed the stale instance registration <实例 ID> (pid …, port …)` |
| 进程仍存活的登记 | 保留，并在**绑定失败**时随错误信息一起给出可操作提示：点名实例 ID、进程号与端口，并说明登记文件在集中根的 `instances` 目录下、可手工删除 |
| 提示内容 | 只含实例 ID / 进程号 / 端口 / 文件位置，**不含 REST 令牌** |

因此「崩溃过一次后再启动」不会一直被占：残留登记在下一次启动时自动清理，端口被占用时错误信息会直接指出是哪个实例占着。

**残余窗口**：进程仍存活时端口不会被系统释放（此时自愈只清理登记、不夺端口），需要先结束该进程或改用其它端口；这与「显式端口不自动顺延」的既有约定一致。

### 传输层工作线程的退出契约

自建 HTTP 传输层为每个连接开一个工作线程（见 [`src/core/http_transport.cpp`](../../../src/core/http_transport.cpp)）。线程退出契约由 [`src/core/transport_lifecycle.hpp`](../../../src/core/transport_lifecycle.hpp) 的纯逻辑判定：

- **停止标志**：连接通道的 `closed` 置位后，工作线程不再取新任务，取到任务也直接丢弃退出。
- **等待预算**：正常停止按 `GDA_RESPONSE_GRACE_MS`（2 秒）有界等待；超时未结束的任务线程被**挂起**（移入堆上容器暂不 join）。
- **收割时机**：插件退出流程在停服之后做一次短预算收割（同样用 `GDA_RESPONSE_GRACE_MS`），把挂起容器清空——挂起线程在**卸载前**一定已 join，DLL 卸载时不会有本模块的线程在跑。
- **上界与退出时长**：收割在预算内未结束时改为**阻塞 join**（该路径的等待上界 = 路由处理器自身的剩余时长，MCP 侧 30 秒分发超时、REST 侧主线程派发预算是其上界）；正常退出因为「关队列 → 关 MCP 传输 → 停传输」的顺序会先把处理器唤醒，实测退出耗时在百毫秒量级。

## 运行时边界（退出拖慢与强杀残留）

本节汇总两条**无法靠配置绕开**的运行时边界。它们的共同点是：异常没有消失，而是换了一种可观测的表现（慢退出 / 残留登记），并且都有对应的自愈或提示路径。

### 退出时的最坏耗时：约 30 秒

插件退出时若还有**未响应「队列关闭」的处理请求**在途，传输层的收割会等它，而不是直接放弃：

- 收割按 `GDA_RESPONSE_GRACE_MS`（2 秒）有界等待；超时的线程被挂起，退出流程在停服后做一次短预算收割，预算内仍未结束就改为**阻塞 join**。
- 阻塞 join 的等待上界 = 该请求处理器的剩余时长：MCP 侧分发超时为 **30 秒**（`src/core/mcp_http_shim.cpp` 的 `kDispatchTimeout`），REST 侧为主线程派发预算（上界 **29 秒**，`GDA_DISPATCH_WAIT_MAX_MS`）。
- 因此**退出最坏可拖到约 30 秒**——把「崩溃」换成了「慢退出」。正常路径不受影响：「关队列 → 关 MCP 传输 → 停传输」的顺序会先把在途处理器唤醒，实测退出耗时在**百毫秒量级**；只有「处理器不响应关闭」这条路径才会吃满上界。
- 观察口径：编辑器进程迟迟不退出、而任务管理器里 CPU 与内存已归零时，多半是这条路径（线程在等超时，不是在干活）。

### 强杀后的端口与登记残留

编辑器被**强杀**（崩溃 / `taskkill /F` / 断电 / 插件未走完退出流程）时，插件来不及执行退出清理：

| 残留 | 表现 | 何时释放 |
|---|---|---|
| 实例登记 | `<集中根>/projects/<项目名>-<路径哈希前 8 位>/instances/<实例 ID>.json` 与全局索引里仍留着该实例，CLI 的发现链会指向一个已经不存在的实例 | 下次启动自动清理（见下） |
| 监听端口 | 端口**不会立即释放**：占用它的进程真正退出前，端口一直被占住 | 由系统在该进程真正退出时释放 |

**下次启动的自愈与提示**（实现见 [`src/core/port_recovery.hpp`](../../../src/core/port_recovery.hpp) 与 `src/core/http_transport.cpp` 的 `transport_residual_plan()`）：

- 启动时扫描全局实例索引，挑出**同一工程**（路径按分隔符归一后比较）的登记并按进程号判活；**进程已死的**视为残留，直接删除登记文件并重建全局索引，日志记 `removed the stale instance registration <实例 ID> (pid …, port …)`。
- **进程仍存活的**登记予以保留，并在**绑定失败**时随错误信息一起给出可操作提示：点名实例 ID / 进程号 / 端口，说明登记文件在集中根的 `instances` 目录下、可手工删除，并提示 `stop that process or choose another port`。
- 提示只含实例 ID / 进程号 / 端口 / 文件位置，**不含 REST 令牌**。

**端口的彻底释放有前提**：自愈只清登记、**不夺端口**（与「显式端口不自动顺延」一致）。若旧编辑器进程仍存活（卡死、任务管理器里还在），端口要等**那个进程真正退出**才彻底释放——新起的编辑器不会替它释放；此时只有两条路：先结束旧进程，或改用其它端口。

**同一工程不要同时打开两个编辑器。** 第二个编辑器会在**扩展加载阶段**就撞上 Godot 的扩展库临时副本占用问题：Godot 加载 GDExtension 时先把扩展库复制成同目录下的临时副本，副本被第一个编辑器占用时第二个编辑器**可能直接静默崩溃**（无任何消息）；插件代码此时尚未运行，来不及给出提示，也来不及做同工程实例检测。插件侧「同工程已有实例则拒绝启动并提示」是**第二道防线**（见上文「同一工程的多实例：默认拒绝」），它只能挡住「插件已加载、实例检查已跑完」之后的情形；扩展加载期的崩溃发生在更早的位置，只能靠「不要同时开两个编辑器」这条纪律避免。

## 相关页面

- [REST 接口面](api.md) —— `/api` 端点契约、认证与状态码
- [核心模块](core.md) —— 集中根与工程级存储布局、根目录解析
- [CLI 载体](cli.md) —— `gda api` / `gda endpoints` 与寻址链
- [工具注册表](tools_registry.md) —— 统一执行入口与授权三档
- [T0 安全边界与并发契约](../security_contract.md) —— 环回绑定与端口解析
- [测试体系](../tests.md) —— 数值核算总表
