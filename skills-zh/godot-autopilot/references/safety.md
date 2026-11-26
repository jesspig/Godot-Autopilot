# Safety

## 前置检查

```bash
gda tool describe <tool>
```

任何高风险调用前先读副作用与必填参数。高风险类别为 `destructive`、`process`、`code_execute` 与 `game_runtime`；每一类都是停下审查的信号，不是形式。

## 确认门禁

```bash
gda tool call <tool> --args '<json>' --confirm
```

高风险命令未经确认不会执行，而是返回此 envelope：

```json
{
  "schema_version": 1,
  "ok": false,
  "command": "tool.call",
  "data": null,
  "error": {"code": "confirmation_required", "message": "requires --confirm", "retryable": false, "request_sent": false},
  "meta": {},
  "warnings": []
}
```

`request_sent` 为 false：没有任何内容到达编辑器。审查打印出的工具、目标、风险与参数摘要，然后用 `--confirm` 重跑同一条命令。非交互会话缺少 `--confirm` 会停止命令。禁止换另一个工具或别名绕过门禁。

## 覆盖与破坏性动作

```bash
gda resource rename --path <from> --to <dest> --confirm
gda resource move --path <from> --to <dir> --confirm
gda script delete <path> --confirm
gda game stop --confirm
gda job cancel <id> --confirm
```

重命名、移动、删除、停止游戏与取消 job 都是高风险，需要 `--confirm`。重载未保存的场景需在工具参数里加 `discard_unsaved=true` 与 `confirm=true`；覆盖保存目标需 `overwrite=true` 与 `confirm=true`；资源复制覆盖需 `overwrite=true` 与 `confirm=true`。用 `action=replace` 编辑节点、用 `action=trash` 丢弃文件，都需要在工具调用上加 `--confirm`。

## 观察

```bash
gda scene save --path <path>
gda resource save --path <path> --dest <dir>
gda scene tree
gda resource inspect --path <path>
```

## 验证

每次写入后读回目标并与意图比对。批量任务中的高风险项逐项确认；绝不批量确认未经审查的列表。

## 失败分支

- 结果未知：不要重发有副作用的调用，先读回状态。
- 确认被拒（退出 5）：用户已拒绝；没有新决定就不要重试。
- 非交互且缺少 `--confirm`：停止并报告，不要换工具。
