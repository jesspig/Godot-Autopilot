# T-15 引擎门禁失败注入

本文件记录四类注入及预期失败信号。注入只用于证明门禁会失败，不做于常规 CI；每次注入后须恢复夹具再跑一次正向门禁。

前提：按 `README.md` 准备 `--godot`、`--cli`、`--project`、`--lib`。报告路径建议指向临时文件，避免覆盖正向证据。

## 1. 损坏库

操作：先把可用的平台库复制到安全位置，再用随机字节覆盖 `tests/engine/project/addons/godot-autopilot/` 下的平台库（`godot-autopilot.dll` / `libgodot-autopilot.so`）。

```bash
bun tests/engine/smoke.ts --godot "$GODOT" --cli "$CLI" --project tests/engine/project \
  --lib "$GOOD_LIB" --report tests/engine/out/inject-corrupt-lib.json
```

预期：`editor_start` 失败（插件未在 60s 内注册实例，或进程退出），`status=fail`，退出码非 0，下游步骤标为 skip。Godot 日志出现 `gdextension` 加载/符号错误。恢复：把安全副本复制回去。

## 2. 错误 Godot 版本

操作：把 `--godot` 指向非 4.7.2 的编辑器（如 4.6 或 4.8 stable），其余不变。

预期：`godot_version` 步骤失败（版本字符串不含 `4.7.2.stable.mono.official.ed1daf0bf`）；若加载 `.gdextension`，`compatibility_minimum = "4.7"` 会使低版本拒绝加载并连带 `editor_start` 失败。退出码非 0。CI 不自动升级版本：版本不符即作业红。

## 3. 主线程停摆

操作：保持编辑器在线，在注入运行期间用另一个 CLI 调用占住主线程：

```bash
gda tool call execute_script --args '{"source_code":"while true: pass"}' --timeout 2 --confirm --project tests/engine/project
```

预期：健康探针（`gcp_ping` / `ipc_no_resend` 后续 ping）在预算内返回 `main_thread_timeout` 或 `exec_timeout`，而非无限等待；`ipc_no_resend` 记录 `request_sent=true` 且 `not retried`。恢复：等待协作式截止触发或重启编辑器。

## 4. 确认缺失

操作：调用高风险工具但不带 `--confirm`：

```bash
gda tool call execute_script --args '{"source_code":"return 1"}' --project tests/engine/project
```

预期：退出码 4（`confirmation_required`），`error.request_sent !== true`，业务未执行；`error_delta` / `frame_advance` 不应把这次调用算作通过。带 `--confirm` 后同一调用应成功，作为对照。

## 5. 导出期三类入口拒绝（人工补充）

CLI 无进程内导出入口，故 orchestrator 的 `export_guard` 步骤只记录三类等价入口可达的 baseline，并标为 `pending`，不声称通过。真实验收需在编辑器里进入导出后，用同一实例执行：

1. `gda tool call ping --args '{}'` —— 领域工具入口。
2. `gda resources read <uri>` —— MCP 资源入口（uri 取自 `gda resources list`）。
3. `gda prompts get <name>` —— 提示词入口（name 取自 `gda prompts list`）。

预期三者均返回含 `editor is exporting; retry after export completes` 的拒绝。导出结束后三者恢复可用。此步在自动化报告里保持 `pending`，由人工证据回填。
