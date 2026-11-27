# Workflows

## 前置检查

```bash
gda workflow list
```

List saved workflows before running one. Confirm the name and its arguments
from the manifest; do not guess a workflow name.

## 操作：运行工作流

```bash
gda workflow run <name> --args '<json>'
gda workflow status <id>
```

Run a saved multi-step workflow with explicit arguments, then read its status.
A workflow is high risk because it executes several steps in sequence; review
what it does before starting it.

```json
{"name": "bootstrap", "args_file": "res://wf.json"}
```

## 操作：批量与外壳

```bash
gda tool batch --args-file <path>
gda shell run '<command>' --timeout <sec>
```

A batch runs several tool calls in one process; shell runs a shell command and
is high risk. Confirm each high-risk item in a batch individually rather than
bulk-confirming.

## 操作：任务管理

```bash
gda job list
gda job get <id>
gda job wait <id> --timeout <sec>
gda job cancel <id> --confirm
gda job logs <id> --cursor <n>
gda job clean --force-clear
```

List and inspect jobs, wait for one with reconnect, and resume its events from a
cursor. Cancelling and cleaning are high risk and require explicit confirmation.

## 观察

```bash
gda job status <id>
```

Read the stage, binding generation and terminal state; a job that reconnects
should preserve its preserved terminal state.

## 验证

Confirm every step of the workflow reached its expected state and no job is
left running before finishing.

## 失败分支

- Job wait timed out: re-attach with the job commands instead of re-launching.
- Result unknown: read state back; do not resend a side-effecting step.
- Cancellation needs confirmation: review the target and re-run with `--confirm`.
