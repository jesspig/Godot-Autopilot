# T-15 真实引擎 Smoke 门禁

固定 Godot 版本的加载与行为门禁：证明 CLI / GCP / GDExtension 在真实引擎里可用，而不是只靠 C++/TS 单测。

## 目录

- `project/`：最小 Godot 4.x 工程，主场景 `main.tscn` 挂 `probe.gd`，探针经 GDScript 暴露 `probe_snapshot()`。
- `project/assertions.json`：结构化断言模板（`step` / `expected` / `observed` / `ok`）。
- `smoke.ts`：orchestrator，真实调用 CLI 与 Godot，写机器可读报告。
- `failure-injection.md`：四类失败注入及预期信号。
- `engine-report.template.json`：报告结构模板，观测值留空、`ok=null`。

注意：仓库的 addon 目录是 `addons/godot-autopilot/`（连字符），gdextension 文件名为 `godot-autopilot.gdextension`，与 `devtools/lib/gdextension.ts` 的生成物一致。

## 准备

1. 构建本机 GDExtension 产物：`cmake --preset debug && cmake --build --preset debug`（产物如 `build/godot-autopilot.dll` / `build/libgodot-autopilot.so`）。
2. 准备 CLI：可用编译后的独立二进制（`bun build cli/bin/gda.ts --compile --outfile dist/gda`），或 `bun` + `cli/bin/gda.ts`。
3. Godot 固定版本：`4.7.2.stable.mono.official.ed1daf0bf`。版本不符直接失败，不自动升级。

## 运行

```bash
bun tests/engine/smoke.ts \
  --godot /path/to/Godot.exe \
  --cli /path/to/gda \
  --project tests/engine/project \
  --lib /path/to/build/godot-autopilot.dll \
  --mode headless \
  --report tests/engine/out/engine-report.json
```

参数：

- `--godot`、`--cli`、`--project`：必需，分别指 Godot 可执行文件、gda CLI、测试工程目录。
- `--lib`：可选，把本机构建的平台库复制进工程 addon 目录；不传则要求库已就位。
- `--mode`：`headless`（默认，逻辑链）或 `gui`（额外执行截图步骤）。
- `--report`：报告输出路径，默认 `tests/engine/out/engine-report.json`。
- `--export-preset`：可选，导出期验收用；当前 CLI 无进程内导出入口，该步骤保持 `pending`。
- `--timeout`：单步超时秒数，默认 90。

脚本用 `node:child_process` + `path.join`，不硬编码平台分隔符；Windows 与 Linux 均可运行。退出码：0 表示无失败步骤；1 表示至少一步失败或发生异常；3 表示参数错误。

## 步骤清单

| id | 内容 |
| --- | --- |
| `preflight` | 路径与断言模板存在性 |
| `godot_version` | `godot --version` 等于固定版本 |
| `prepare_addon` | 部署 gdextension 与平台库 |
| `editor_start` | `gda start --headless` 等待插件注册 |
| `gcp_hello` | `gda editor status` 的 hello/capabilities |
| `gcp_ping` | `gda editor ping` |
| `gcp_status` | `system_status` 工具 |
| `scene_open` / `scene_create` / `scene_save` | 打开、建节点、落盘场景 |
| `gdscript_exec` | `execute_script` 编译执行并验证 AutopilotTools 单例 |
| `game_bridge` / `game_probe` | 启动游戏、探针快照 |
| `input_injection` / `frame_advance` | 输入注入与帧推进 |
| `screenshot` | GUI 截图；headless 跳过 |
| `error_delta` | 受控 `push_error` 后错误增量游标 |
| `export_guard` | 三类等价入口 baseline；导出期拒绝标 `pending` |
| `ipc_auth` / `ipc_no_resend` | 伪造端点失败、已发送请求不重发 |
| `cleanup` | `gda stop` |

## 判读报告

报告 `status` 取值：`pass`（无失败/待定）、`fail`（至少一步失败）、`pending`（含待定步骤，常见为导出期拒绝）。每个 step 含 `id` / `name` / `expected` / `observed` / `ok` / `status` / `error` / `stdout` / `stderr`。

- `observed` 为真实 CLI envelope 或提取值；失败时看 `error` 与 `stderr`。
- `ok: null` + `status: "skip"`：headless 下的截图，属预期跳过。
- `ok: null` + `status: "pending"`：导出期拒绝需人工在真实导出中验证，见 `failure-injection.md` 第 5 节；不得当作通过。
- 游戏侧错误计数一律看增量（watermark / count 变化），不依赖绝对计数：Godot 4.7.2 启动期的 root certificate store 错误会使绝对值漂移。

CI 分层：`logic` 作业在 Linux headless 跑逻辑链；`gui` 作业在 Windows 只保留截图必需项。两者都固定同一 Godot 版本与缓存 key，保存日志、trace 与断言报告为 artifact。
