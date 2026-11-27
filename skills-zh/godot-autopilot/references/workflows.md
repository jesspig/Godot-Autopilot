# Workflows

## 前置检查

```bash
gda workflow list
```

运行前列出已保存的工作流。从清单确认名字与参数；不要猜工作流名。

## 操作：运行工作流

```bash
gda workflow run <name> --args '<json>'
gda workflow status <id>
```

用显式参数运行保存的多步骤工作流，然后读取其状态。工作流会按顺序执行多步，属于高风险；启动前审查它要做什么。

```json
{"name": "bootstrap", "args_file": "res://wf.json"}
```

## 操作：批量与外壳

```bash
gda tool batch --args-file <path>
gda shell run '<command>' --timeout <sec>
```

批量在一个进程里执行多次工具调用；shell 运行外壳命令且属于高风险。批量中的高风险项逐项确认，不要批量确认。

## 操作：任务管理

```bash
gda job list
gda job get <id>
gda job wait <id> --timeout <sec>
gda job cancel <id> --confirm
gda job logs <id> --cursor <n>
gda job clean --force-clear
```

列出并检查 job，带重连地等待，并从游标恢复其事件。取消与清理是高风险操作，需要显式确认。

## 观察

```bash
gda job status <id>
```

读取阶段、绑定 generation 与终态；重连的 job 应保留其终态。

## 验证

确认工作流每一步都到达预期状态，且结束前没有 job 仍在运行。

## 失败分支

- job wait 超时：用 job 命令重新附着，不要重启任务。
- 结果未知：读回状态，不要重发有副作用的步骤。
- 取消需要确认：审查目标后用 `--confirm` 重跑。
