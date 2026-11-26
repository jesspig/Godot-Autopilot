---
type: 验收门禁
title: 真实 Godot 引擎 Smoke 门禁
description: T-15 固定 Godot 版本的加载与行为门禁：夹具结构、orchestrator 步骤、CI 分层、报告判读与已知边界
tags:
  - 测试
  - e2e
  - CI
  - 引擎门禁
timestamp: "2026-11-26T18:10:15+08:00"
resource:
  - tests/engine/
  - .github/workflows/engine-smoke.yml
---

# 真实 Godot 引擎 Smoke 门禁

> 本页记录 T-15 夹具的事实与边界。夹具是否通过以实跑报告为准，本页不写运行结果与计数口径；测试数值仍以 [tests.md](tests.md)「数值核算总表」为唯一权威。

## 目标与边界

- 建立固定 Godot 版本（`4.7.2.stable.mono.official.ed1daf0bf`）的真实加载与行为门禁，证明 CLI / GCP / GDExtension 在引擎中可用，而非只有 C++/TS 单测通过。
- 夹具位于 `tests/engine/`：最小 Godot 4.x 工程（`project.godot` / `main.tscn` / `probe.gd` / `assertions.json`）、`smoke.ts` orchestrator、`failure-injection.md`、`engine-report.template.json`。
- addon 路径按仓库既有约定为 `addons/godot-autopilot/`，描述文件 `godot-autopilot.gdextension` 的内容与 [gdextension.ts](../../devtools/lib/gdextension.ts) 的生成物一致，`compatibility_minimum = "4.7"`。
- 不修改 `src/`、`cli/`、`tests/unit/`、`tests/guard/`、CMake、`skills/` 与 `VERSION`；不写 `.handoff/`。

## orchestrator 步骤

`smoke.ts` 接收 `--godot` / `--cli` / `--project`，可选 `--lib` / `--report` / `--mode` / `--export-preset` / `--timeout`。每步记录 `id` / `expected` / `observed` / `ok` / `status` / `error` / `stdout` / `stderr`，失败返回非 0 并写报告。

1. `preflight`：路径与断言模板可解析。
2. `godot_version`：`godot --version` 等于固定版本；版本漂移直接判失败，CI 不自动升级。
3. `prepare_addon`：部署平台库与 gdextension。
4. `editor_start`：`gda start --headless` 等待插件注册。
5. `gcp_hello` / `gcp_ping` / `gcp_status`：走 native GCP 握手、健康探针与 `system_status`。
6. `scene_open` / `scene_create` / `scene_save` / `gdscript_exec`：场景创建落盘与 GDScript 编译执行；编辑器的 `AutopilotTools` 单例在此验证。
7. `game_bridge` / `game_probe` / `input_injection` / `frame_advance`：启动游戏、探针快照、输入与帧推进。
8. `screenshot`：GUI 模式产出 PNG；headless 跳过（`status=skip`）。
9. `error_delta`：受控 `push_error` 后用增量游标观测错误，不依赖绝对计数（Godot 4.7.2 启动期 root certificate store 错误会污染绝对值）。
10. `export_guard`：三类等价入口的 baseline；导出期拒绝在自动化中保持 `status=pending`。
11. `ipc_auth` / `ipc_no_resend`：伪造端点在发送前失败；已发送请求超时后不重发，随后 ping 可用。
12. `cleanup`：`gda stop`。

## CI 分层

[engine-smoke.yml](../../.github/workflows/engine-smoke.yml) 两个作业共用同一固定版本与缓存 key：

- `logic`（Linux headless）：完整逻辑链、版本断言、原生库与 CLI 构建、日志/trace/报告 artifact。
- `gui`（Windows）：只承担截图必需项；逻辑正确性由 `logic` 作业负责。
- Godot 缓存 key 含完整版本串；版本不符即作业红，失败不自动升级版本。

## 已知边界

- CLI 无进程内导出入口，无法让已连接实例进入导出期；`export_guard` 只记录 baseline 并标 `pending`，真实拒绝须按 `failure-injection.md` 第 5 节在编辑器导出中人工验证。
- `AutopilotTools` 单例在编辑器级别注册，游戏进程内不可用；探针脚本不依赖该单例，改由 `gdscript_exec` 在编辑器上下文验证。
- 详细运行方式与判读规则见 `tests/engine/README.md`；引擎行为点的来源与历史回归点见 [engine-behavior-checklist.md](engine-behavior-checklist.md)。
