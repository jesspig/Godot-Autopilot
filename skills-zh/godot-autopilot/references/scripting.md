# Scripting

## 前置检查

```bash
gda script list
gda script read <path>
```

先查看项目脚本集合并读取目标，再动手编辑。禁止凭记忆写不熟悉的引擎 API：先查引擎文档，再确认属性名、方法签名与枚举值。

## 操作：创建与修改

```bash
gda script create <path> --extends Node --source '<gdscript>'
gda script patch --path <path> --args-file <path>
gda script patch --path <path> --dry-run
gda script compile --path <path> --args-file <path>
```

用显式基类创建，或从 JSON 参数文件打补丁。用 dry-run 选项在不写入的情况下校验并编译。挂载前先编译检查。

```json
{"path": "res://player.gd", "extends": "Node", "source_code": "extends Node"}
```

## 操作：挂载与执行

```bash
gda script attach --node <node> --script <path>
gda script exec --source '<gdscript>' --confirm
gda script reload --path <path>
```

把编译好的脚本挂到场景节点。编辑器内执行属于高风险，需要确认门禁；确认前审查源码。从磁盘重载以接收外部修改。

## 操作：测试与删除

```bash
gda script test --directory <dir>
gda script delete <path> --confirm
```

运行内联或文件式 GDScript 测试。删除脚本是高风险操作，需要确认。

## 观察

```bash
gda script read <path>
gda logs game --tail <n>
```

补丁后读回脚本，并在日志里留意解析错误。

## 验证

对改后的源码做编译检查，运行相关测试，再确认节点确实使用了新脚本。

## 失败分支

- 脚本在编辑器里不执行：可能需要工具模式注解或编辑器执行通道。
- 补丁后解析错误：读回文件并检查确切行。
- 执行触发确认门禁：审查源码后用 `--confirm` 重跑。
