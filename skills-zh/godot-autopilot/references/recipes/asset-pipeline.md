# asset-pipeline

## 1. 触发条件与输入
当任务需要导入、检查、编辑或重新保存项目资源与素材时，使用本配方。输入：资源路径、要设置的属性名与新值、要导入的素材路径，以及复制或保存的覆盖目标。

## 2. 前置检查
项目已打开，资源或素材路径存在。你已读过打算通用调用的工具描述。你知道目标位置是否已存在文件，因为覆盖需要显式确认。

## 3. 可复制的 CLI 命令
```bash
gda resource list
gda resource inspect --path res://data/enemy.tres
gda resource get --path res://data/enemy.tres --property health
gda resource set --path res://data/enemy.tres --property health --value 20
gda resource save --path res://data/enemy.tres --dest res://data/enemy.tres
gda resource import --path res://art/hero.png --args-file -
gda resource references --path res://data/enemy.tres
gda tool call resource_property --args-file - --confirm
```
通用调用的参数载荷：
```json
{
  "action": "save",
  "path": "res://data/enemy.tres",
  "target": "res://data/enemy.tres",
  "overwrite": true,
  "confirm": true
}
```

## 4. 每步观察字段
从列表与检查中读取资源类型、元数据与注册名。从属性读取确认当前值以及信封的 ok 与 error。写入后再次读取该属性。保存后确认目标路径，并检查是否意外产生副本。导入后检查 warnings 中是否有导入错误。引用查询记录所有依赖路径，之后再重命名或替换。

## 5. 验证与退出条件
属性回读显示新值、保存报告目标路径正确、导入无错误、且所有依赖引用仍可解析，即可退出。

## 6. 失败分支
属性写入被拒绝时检查 error，并重新核对属性名与类型。调用返回 confirmation_required 时，审阅摘要后用 --confirm 提供确认。导入失败时核实素材路径后重试导入；结果未知的调用不要反复重发。覆盖被拒绝时检查目标并显式确认。

## 7. 安全与确认点
资源与场景覆盖需要显式确认。高风险调用返回 confirmation_required 时，审阅工具、目标、风险与参数后，用 --confirm 重试同一调用。非交互会话缺少确认即停止。结果未知的调用不要重发，应先回读资源。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点。

## 8. 明确不做什么
不猜测资源类型、属性名或工具名。不硬编码工具数量或版本窗口。本配方不执行批量破坏性清理，也不做游戏运行时变更。
