# CLI

## 前置检查

```bash
gda commands --json
gda status --format json
```

The JSON command manifest is the single source of truth for groups, commands,
flags and exit codes. Use it before writing any command you have not seen.

## 调用形式

```bash
gda tool call <tool> --args '<json>'
gda tool call <tool> --args-file <path>
gda tool call <tool> --args-file -
gda shell run '<command>' --timeout <sec>
```

Global options may appear before the command: target flags such as `--id`,
`--project`, `--port` and `--socket`; output flags `--format json`, `--json`,
`--pretty` and `--raw`; and `--timeout`. Short JSON goes in `--args`, while long
or shell-quoted JSON goes in `--args-file`. A single `-` reads arguments from
stdin. The two argument flags are mutually exclusive.

## Envelope

```json
{
  "schema_version": 1,
  "ok": true,
  "command": "tool.call",
  "data": {},
  "error": null,
  "meta": {},
  "warnings": []
}
```

Every command prints this envelope: `ok`, `command`, `data`, `error` (with
`code`, `message`, `retryable`, `request_sent`), `meta` and `warnings`. Use the
raw option for the business payload alone. Exit codes: 0 success, 1 tool or
business failure, 2 transport/environment failure, 3 usage error, 4 confirmation
required, 5 user declined, 130 interrupted.

## 长任务

```bash
gda test run --detach
gda job wait <id>
gda job status <id>
gda job cancel <id> --confirm
gda completion bash
```

Long-running commands return a job id and stream progress to stderr; poll the
job commands rather than re-launching the work. Shell completion is generated
from the command spec.

## 观察

```bash
gda job status <id>
```

## 验证

Check `ok`, then the exit code, then the `error.code` before trusting `data`.
A zero `ok` with a non-empty `warnings` list still deserves a read-back.

## 失败分支

- Usage error (exit 3): verify the flag against the JSON command manifest.
- Exit 4 means nothing was sent; review the confirmation summary and re-run with
  `--confirm`.
- Exit 2: re-check the connection record and run doctor diagnostics.
