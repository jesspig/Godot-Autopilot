# Resources

## 前置检查

```bash
gda resource list
gda resource inspect --path <path>
```

List project resources and inspect the target type and metadata before editing.
Find references before a rename or move so you know what will break.

## 操作：读取与写入

```bash
gda resource get --path <path> --property <prop>
gda resource set --path <path> --property <prop> --value '<json>'
gda resource set --args-file <path>
gda resource save --path <path> --dest <dir>
```

Read one property, then write it back with a JSON value of the right type. Use
the file flag for long argument objects. Save the resource to an explicit
destination; overwriting an existing target needs explicit confirmation.

```json
{"path": "res://art.tres", "property": "resource_name", "value": "Hero"}
```

## 操作：导入、重命名、移动

```bash
gda resource import --path <path>
gda resource rename --path <from> --to <dest> --confirm
gda resource move --path <from> --to <dir> --confirm
gda resource references --path <path>
```

Reimport a changed asset. Rename and move rewrite references and are high risk,
so each requires confirmation. Re-check references afterwards.

## 操作：读取与监听

```bash
gda resource read <uri> --save <dir>
gda resource watch <uri> --interval <sec>
```

Read a resource and optionally write its blobs to a directory. Watch polls for
changes at a fixed interval.

## 观察

```bash
gda resource inspect --path <path>
gda resource get --path <path> --property <prop>
```

Read back every property you wrote and confirm UID and dependency state after a
move.

## 验证

Re-open the resource, check its references, and verify dependent scenes still
load.

## 失败分支

- Property write has no effect: check the exact property name and value shape.
- A path breaks after rename or move: check UID, dependencies and remaining
  references.
- Overwrite refused: review the destination, then confirm explicitly.
