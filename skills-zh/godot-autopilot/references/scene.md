# Scene

## 前置检查

```bash
gda scene list
gda scene tree --pattern <glob>
```

改动前先观测正在编辑的场景树。树很大时按路径模式、类型或分组过滤后代；只为打算编辑的过滤节点包含属性。

## 操作：打开与建节点

```bash
gda scene open <path>
gda scene node --type <type> --name <name> --parent <path> --props '<json>'
gda scene node --args-file <path>
```

在显式父路径下创建节点。短属性字典用行内 flag，长的用文件 flag（或 `-` 从 stdin 读取）。写入前对照引擎文档确认节点类型与属性名。

```json
{"node_path": "Player", "type": "Sprite2D", "parent": "."}
```

## 操作：属性与信号

```bash
gda scene property --path <node> --property <prop> --value '<json>'
gda scene signal --from <a> --signal <sig> --to <b> --method <m>
```

每次只设一个属性，传与该属性 Variant 类型匹配的 JSON 值。在显式节点路径之间连接或断开信号。不要因为返回成功就假定写入已落地。

## 操作：保存与校验

```bash
gda scene save --path <path>
gda scene verify
gda scene diff --args-file <path>
```

保存到显式路径；覆盖已有目标需要显式确认。verify 把编辑后的树与保存文件比对，diff 用路径哈希报告树差异。

## 观察

```bash
gda scene tree --pattern <glob>
```

每次写入后读回节点、其属性与父节点。

## 验证

重新读取树并运行 verify。属性写入看不出效果，通常是属性名错误、值形状错误或缺少 owner。

## 失败分支

- 保存后节点丢失：检查父路径、owner 与保存目标。
- 属性写不进去：读回并检查确切名称与值形状。
- 带未保存改动重载需要 `discard_unsaved=true` 加显式确认；继续前先审查丢失内容。
