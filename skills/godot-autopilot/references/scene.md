# Scene

## 前置检查

```bash
gda scene list
gda scene tree --pattern <glob>
```

Observe the edited scene tree before changing it. Filter descendants by path
pattern, type or group when the tree is large; include properties only for the
filtered nodes you intend to edit.

## 操作：打开与建节点

```bash
gda scene open <path>
gda scene node --type <type> --name <name> --parent <path> --props '<json>'
gda scene node --args-file <path>
```

Create a node under an explicit parent path. Short property dictionaries go in
the inline flag; long ones go in the file flag (or `-` for stdin). Confirm node
type and property names against engine documentation before writing them.

```json
{"node_path": "Player", "type": "Sprite2D", "parent": "."}
```

## 操作：属性与信号

```bash
gda scene property --path <node> --property <prop> --value '<json>'
gda scene signal --from <a> --signal <sig> --to <b> --method <m>
```

Set one property at a time with a JSON value that matches the property's Variant
type. Connect or disconnect a signal between explicit node paths. Do not assume
a write landed just because it returned successfully.

## 操作：保存与校验

```bash
gda scene save --path <path>
gda scene verify
gda scene diff --args-file <path>
```

Save to an explicit path; overwriting an existing target needs explicit
confirmation. Verify compares the edited tree with the saved file, and diff
reports tree differences with path hashes.

## 观察

```bash
gda scene tree --pattern <glob>
```

Read back the node, its property and its parent after each write.

## 验证

Re-read the tree and run verify. A property write with no visible effect usually
means a wrong property name, a wrong value shape, or a missing owner.

## 失败分支

- Node missing after save: check the parent path, owner and save target.
- Property write has no effect: read it back and check the exact name and value
  shape.
- Reloading with unsaved changes requires `discard_unsaved=true` plus explicit
  confirmation; review the loss before proceeding.
