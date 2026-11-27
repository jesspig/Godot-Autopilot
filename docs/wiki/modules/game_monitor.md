---
type: 模块指南
title: 游戏运行时监视器
description: 编辑器启动的游戏运行逐帧采集 59 项 Godot Performance 监视器与自动发现的自定义监控，JSONL 落盘，按会话用 CLI 调取与分析
tags:
  - 监视器
  - 性能
  - 性能分析
  - JSONL
  - 自定义监控
timestamp: "2026-11-24T00:11:55+08:00"
resource: src/core/observability/
---

# 游戏运行时监视器

> 审计日期：2026-11-24。数值口径见 [tests.md](../tests.md)「数值核算总表」。

## 适用范围

仅对**从编辑器启动的游戏运行**（F5/F6、嵌入或独立窗口）生效：GDExtension 在游戏进程内加载，才能执行 `Performance::get_monitor` 与自定义监控读取。导出且不含 GDA 扩展的游戏**不支持**逐帧记录。

采集只在游戏进程进行；编辑器进程不录制。`game_monitor::ensure_connected()` 经 `GameBridgeListener` 的 `process_frame` 回调与 `gda` 消息通道接入，退出时 `stop()` 收尾落盘。

## 自定义监控

非侵入式发现：用户游戏只要用 Godot 标准 API `Performance.add_custom_monitor(id, callable, args, type)` 注册监控，GDA 即可读到，**无需在游戏里写任何 GDA 代码**。

- `get_monitor_modification_time()` 变化时重读 `get_custom_monitor_names()` / `get_custom_monitor_types()`，并写一条 `custom_change` 事件行。
- 每帧用 `get_custom_monitor(id)` 取值，类型为 `INT` / `FLOAT` 时记数值，其余记 `null`。
- 仅支持标准 API 注册的监控。autoload 普通变量、自定义 signal、用户自维护的统计字典没有全局注册表，非侵入手段无法发现。

## JSONL 数据格式

文件位于游戏工程的 `user://logs/monitor/gda-monitor-<session_id>.jsonl`。首行是会话头，后面每帧一行，`custom_change` 为事件行。

```json
{"type":"session","schema":1,"id":"123456-1","started_wall_ms":1763890212000,"builtin_monitors":[{"i":0,"n":"time/fps","t":"time"}],"custom_monitors":[{"i":0,"n":"Game/Enemies","t":0,"cat":"Game"}]}
```

```json
{"type":"frame","n":1,"f":1,"pf":1,"d":16.7,"t":0,"w":1763890212000,"v":[60.0,0.0167],"c":[3.0]}
```

```json
{"type":"custom_change","n":100,"custom_monitors":[{"i":0,"n":"Game/DPS","t":2,"cat":"Game"}]}
```

| 字段 | 含义 |
|---|---|
| `n` / `f` / `pf` | 会话帧序 / 进程帧计数 / 物理帧计数 |
| `d` | 本帧 delta，毫秒 |
| `t` / `w` | 相对会话单调毫秒 / wall-clock 毫秒 |
| `v` | 59 项内置监视值，下标即 `Performance::Monitor` 枚举 |
| `c` | 自定义监视值，下标对应头部 `custom_monitors` |

`schema` 为 1。写入按 120 帧或 1 秒缓冲刷盘，异常退出最多丢约 1 秒。轮转默认 24 文件 / 2 GB，可用 `GDA_MONITOR_MAX_FILES` / `GDA_MONITOR_MAX_BYTES` 覆盖。

## 调取与分析

CLI 命令 `gda monitor`：`status` / `sessions` / `report` / `export`。`report` 返回帧时间 avg/p95/p99/max、慢帧数、各监视器 min/max/avg/p50/p95/p99/first/last/delta；`export` 把会话复制到 GDA home 的 `monitor-sessions/`。

CLI：

```text
gda monitor status
gda monitor sessions
gda monitor report  <session_id> [--slow <ms>]
gda monitor export  <session_id>
gda monitor report  --file <path-to-jsonl> [--slow <ms>]
```

`--file` 走本地纯 TS 分析，不依赖插件，可用于离线复盘。`report` 与 `export` 的 `<session_id>` 来自 `sessions`。

## 实现落点

| 文件 | 职责 |
|---|---|
| `src/core/observability/game_monitor.{hpp,cpp}` | 逐帧采样、自定义监控发现、JSONL 写入与轮转 |
| `src/core/observability/monitor_analysis.{hpp,cpp}` | 纯函数解析与统计，零引擎依赖 |
| `src/runtime/game_bridge.cpp` | 游戏进程钩子：帧回调接入与退出收尾 |
| `src/tools/domains/monitor/` | CLI 命令 `gda monitor` |
| `cli/src/monitor.ts`、`cli/src/commands/monitor.ts` | CLI 离线分析与会话调取 |

相关：[core.md](core.md)、[tests.md](../tests.md)、[安全契约](../security_contract.md)。
