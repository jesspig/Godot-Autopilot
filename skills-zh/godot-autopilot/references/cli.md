# CLI

## 前置检查

```bash
gda commands --json
gda status --format json
```

JSON 命令清单是 group、命令、flag 与退出码的唯一事实来源。写任何没见过的命令前先查它。

## 调用形式

```bash
gda tool call <tool> --args '<json>'
gda tool call <tool> --args-file <path>
gda tool call <tool> --args-file -
gda shell run '<command>' --timeout <sec>
```

全局选项可放在命令之前：目标类如 `--id`、`--project`、`--port`、`--socket`；输出类 `--format json`、`--json`、`--pretty`、`--raw`；以及 `--timeout`。短 JSON 放 `--args`，长 JSON 或含 shell 引号的 JSON 放 `--args-file`。单独的 `-` 表示从 stdin 读取参数。两个参数 flag 互斥。

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

每条命令都输出此 envelope：`ok`、`command`、`data`、`error`（含 `code`、`message`、`retryable`、`request_sent`）、`meta` 与 `warnings`。只看业务载荷用 raw 选项。退出码：0 成功，1 工具或业务失败，2 传输/环境失败，3 用法错误，4 需要确认，5 用户拒绝，130 中断。

## 长任务

```bash
gda test run --detach
gda job wait <id>
gda job status <id>
gda job cancel <id> --confirm
gda completion bash
```

长任务会返回 job id 并把进度流写到 stderr；用 job 命令轮询，不要重启任务。Shell 补全由命令 spec 生成。

## 观察

```bash
gda job status <id>
```

## 验证

先查 `ok`，再查退出码，最后在信任 `data` 前查 `error.code`。`ok` 为真但 `warnings` 非空时仍需读回。

## 失败分支

- 用法错误（退出 3）：对照 JSON 命令清单核实 flag。
- 退出 4 表示什么都没发送；审查确认摘要后用 `--confirm` 重跑。
- 退出 2：重新检查连接记录并运行 doctor 诊断。
