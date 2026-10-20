---
type: 模块文档
title: 端口与实例登记
description: 单一监听端口承载 MCP 与 REST 两个端点、端口解析优先级与候选序列、显式与自动分配的失败差异、实例登记结构与全局索引、CLI 侧发现链与回溯
tags:
  - 模块
  - 端口
  - 实例登记
  - 发现链
timestamp: "2026-10-21T00:37:30+08:00"
resource:
  - src/core/port_resolver.cpp
  - src/core/server_context.cpp
  - src/core/instance_registry.cpp
  - src/core/project_store.cpp
  - src/core/gda_home.cpp
  - cli/src/target.ts
  - cli/src/instances.ts
---

# 端口与实例登记

> 审计日期：2026-10-21（随开放接口面落地批次首建——本批把「端口」从单一 MCP 端口扩为**一个端口承载两个端点**（`/mcp` 与 `/api`），端口解析改为「环境变量 → 工程级配置偏好 → 自动分配」三级，自动分配有确定的候选序列；实例登记迁到集中根下的工程级目录并汇总出全局索引，供 CLI 发现链使用。实现见 [`src/core/port_resolver.cpp`](../../../src/core/port_resolver.cpp) 与 [`src/core/server_context.cpp`](../../../src/core/server_context.cpp)，登记见 [`src/core/instance_registry.cpp`](../../../src/core/instance_registry.cpp) 与 [`src/core/project_store.cpp`](../../../src/core/project_store.cpp)。本轮校订补三处接线事实：① 探测上限 256 含首选 9527 在内，失败信息形如 `no free port candidate bound after N probes: <原因>`；② CLI 本地注册表的 state dir 组成补 `instances.lock` 与 `logs/`；③ 令牌字段名改为不加反引号的自然表述，避免与代码内 `api_token` 字段混淆）。
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
  "version": "0.2.7"
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
- **CLI 的集中根解析比插件简单**：CLI 侧为「`GODOT_AUTOPILOT_HOME` 覆写 → `os.homedir()`」两级，不做插件侧的存在性校验；两者在正常环境下指向同一目录，但覆写或异常 HOME 环境下可能分叉。

## 相关页面

- [REST 接口面](api.md) —— `/api` 端点契约、认证与状态码
- [核心模块](core.md) —— 集中根与工程级存储布局、根目录解析
- [CLI 载体](cli.md) —— `gda api` / `gda endpoints` 与寻址链
- [工具注册表](tools_registry.md) —— 统一执行入口与授权三档
- [T0 安全边界与并发契约](../security_contract.md) —— 环回绑定与端口解析
- [测试体系](../tests.md) —— 数值核算总表
