# Resources

## 前置检查

```bash
gda resource list
gda resource inspect --path <path>
```

编辑前列出项目资源并检查目标类型与元数据。重命名或移动前先找引用，弄清楚会破坏什么。

## 操作：读取与写入

```bash
gda resource get --path <path> --property <prop>
gda resource set --path <path> --property <prop> --value '<json>'
gda resource set --args-file <path>
gda resource save --path <path> --dest <dir>
```

先读一个属性，再用类型正确的 JSON 值写回。长参数对象用文件 flag。把资源保存到显式目标；覆盖已有目标需要显式确认。

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

变更过的资源可重新导入。重命名与移动会重写引用且属于高风险，因此各需确认。之后重新检查引用。

## 操作：读取与监听

```bash
gda resource read <uri> --save <dir>
gda resource watch <uri> --interval <sec>
```

读取资源并可把其 blob 写入目录。watch 按固定间隔轮询变化。

## 观察

```bash
gda resource inspect --path <path>
gda resource get --path <path> --property <prop>
```

每个写入的属性都读回，移动后确认 UID 与依赖状态。

## 验证

重新打开资源，检查其引用，并确认依赖场景仍能加载。

## 失败分支

- 属性写不进去：检查确切属性名与值形状。
- 重命名或移动后路径断裂：检查 UID、依赖与残留引用。
- 覆盖被拒：审查目标后显式确认。
