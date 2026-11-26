# Safety

## 前置检查

```bash
gda tool describe <tool>
```

Read side effects and required arguments before any high-risk call. The
high-risk classes are `destructive`, `process`, `code_execute` and
`game_runtime`; each is a stop-and-review signal, not a formality.

## 确认门禁

```bash
gda tool call <tool> --args '<json>' --confirm
```

A high-risk command does not execute without confirmation. It returns this
envelope instead:

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

`request_sent` is false: nothing reached the editor. Review the printed summary
of tool, target, risk and arguments, then re-run the same command with
`--confirm`. In a non-interactive session a missing `--confirm` stops the
command. Never bypass the gate by switching to another tool or an alias.

## 覆盖与破坏性动作

```bash
gda resource rename --path <from> --to <dest> --confirm
gda resource move --path <from> --to <dir> --confirm
gda script delete <path> --confirm
gda game stop --confirm
gda job cancel <id> --confirm
```

Rename, move, delete, stopping the game and cancelling a job are high risk and
require `--confirm`. Reloading a scene with unsaved changes needs
`discard_unsaved=true` plus `confirm=true` in the tool arguments; overwriting a
save target needs `overwrite=true` plus `confirm=true`; resource copy overwrite
needs `overwrite=true` plus `confirm=true`. Editing a node with `action=replace`
and trashing a file with `action=trash` both require `--confirm` on the tool
call.

## 观察

```bash
gda scene save --path <path>
gda resource save --path <path> --dest <dir>
gda scene tree
gda resource inspect --path <path>
```

## 验证

After every write, read the target back and compare it with the intent.
Confirm each high-risk item one by one inside a batch; never bulk-confirm a list
you have not reviewed.

## 失败分支

- Result unknown: do not resend the side-effecting call. Read the state back
  first.
- Confirmation declined (exit 5): the user said no; do not retry without a new
  decision.
- Non-interactive without `--confirm`: stop and report; do not switch tools.
