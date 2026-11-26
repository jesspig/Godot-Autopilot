# Scripting

## 前置检查

```bash
gda script list
gda script read <path>
```

Inspect the project script set and read the target before editing. Never write
an unfamiliar engine API from memory: search the engine documentation first,
then confirm property names, method signatures and enum values.

## 操作：创建与修改

```bash
gda script create <path> --extends Node --source '<gdscript>'
gda script patch --path <path> --args-file <path>
gda script patch --path <path> --dry-run
gda script compile --path <path> --args-file <path>
```

Create with an explicit base class, or patch from a JSON argument file. Use the
dry-run option to validate and compile without writing. Compile-check before
attaching.

```json
{"path": "res://player.gd", "extends": "Node", "source_code": "extends Node"}
```

## 操作：挂载与执行

```bash
gda script attach --node <node> --script <path>
gda script exec --source '<gdscript>' --confirm
gda script reload --path <path>
```

Attach the compiled script to a scene node. In-editor execution is high risk and
requires the confirmation gate; review the source before confirming. Reload from
disk to pick up external edits.

## 操作：测试与删除

```bash
gda script test --directory <dir>
gda script delete <path> --confirm
```

Run inline or file-based GDScript tests. Deleting a script is high risk and
requires confirmation.

## 观察

```bash
gda script read <path>
gda logs game --tail <n>
```

Read the script back after a patch and watch for parse errors in the logs.

## 验证

Compile-check the patched source, run the relevant tests, then confirm the node
actually uses the new script.

## 失败分支

- Script does not run in the editor: it may need the tool-mode annotation or an
  editor execution channel.
- Parse error after a patch: read the file back and inspect the exact line.
- Confirmation gate on execution: review the source and re-run with `--confirm`.
