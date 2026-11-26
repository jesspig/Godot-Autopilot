# Testing

## 前置检查

```bash
gda test list
```

List available suites before running anything. Confirm names against the
command manifest; do not invent a suite name.

## 操作：运行套件

```bash
gda test run <suite> --label <label>
gda test run --shard <i/c> --retries <n> --junit <path> --coverage
gda test run --detach
```

Run a whole suite or filter by label. Shard large runs, retry failed cases, and
write a JUnit report with coverage when a CI system consumes it. Detached runs
return a job id and stream progress to stderr.

```json
{"label": "smoke", "shard": "1/4", "retries": 2}
```

## 操作：内联与场景测试

```bash
gda test script --source '<gdscript>'
gda test script --args-file <path>
gda test scenario --args-file <path> --confirm
```

Run inline GDScript cases or load them from a file. A scenario fires an input
timeline in the running game and is high risk, so it needs confirmation.

## 操作：轮询

```bash
gda test watch --interval <sec> --detach
gda test status <id>
gda job wait <id>
```

Watch polls game state and test results until a run finishes. Use the status and
job commands to read a run you started earlier.

## 观察

```bash
gda test status <id>
```

Read counters, failures and the report file rather than trusting a progress
line on stderr.

## 验证

Confirm all expected cases ran, the failure count is zero, and the JUnit report
exists at the requested path.

## 失败分支

- Suite not found: re-list suites and check the exact name.
- Intermittent failures: re-run with retries and inspect the failing case.
- Detached run not finished: wait on the job; do not start a duplicate run.
