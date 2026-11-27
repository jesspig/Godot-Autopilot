# Debugging

## 前置检查

```bash
gda debug status
gda debug errors --limit <n> --level error
```

Assemble one incident snapshot: editor state, game state, trace context and
debugger errors. Read the error watermark before changing anything.

## 操作：读取调试流

```bash
gda debug output --limit <n>
gda debug stack
gda debug inspect --action object --path <path>
gda debug trace
```

Read captured standard output and the debug stack, inspect a live object or
node, then read the trace context. While a game runs, the debugger stream
carries its errors and prints; without one, the log actions read editor output.

## 操作：复现与报告

```bash
gda debug repro --path <scene> --confirm --detach
gda job wait <id>
gda debug report --label <label> --report-file <path>
```

Repro orchestrates play, input, sampling, capture and error collection. It is a
high-risk game-runtime change and returns a job id when detached; poll the job
before reading the report.

```json
{"label": "crash", "report_file": "res://report.json"}
```

## 观察

```bash
gda logs editor --level error --tail <n>
gda logs plugin --level error
gda logs cli --source <s>
gda trace summary
gda trace slow --slow <ms>
gda monitor report <id>
```

Cross-check the debugger stream against the on-disk game log. Use plugin and CLI
logs when the problem is in the tooling itself; use trace and monitor reports
for latency and frame issues.

## 验证

Confirm the error watermark is back at zero, the stack no longer shows the
fault, and a fresh report contains no new errors.

## 失败分支

- Error count non-zero after a fix: pull the real log lines and explain them.
- Repro detached and unfinished: wait on the job, do not start a second repro.
- Plugin-side failure: read the plugin log and report it, do not retry blindly.
