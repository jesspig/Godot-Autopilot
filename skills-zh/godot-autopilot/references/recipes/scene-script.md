# scene-script

## 1. 触发条件与输入
当任务需要编辑已有场景、添加节点，或创建并挂载脚本时，使用本配方。输入：场景路径、节点类型与名称、父节点路径、脚本路径，以及要应用的补丁载荷。

## 2. 前置检查
编辑器正在运行且目标场景已知。对需要通用调用的工具，你已先读过其描述。脚本路径下没有你想保留的既有内容，或者你已确认可以覆盖。

## 3. 可复制的 CLI 命令
```bash
gda scene list
gda scene open res://scenes/main.tscn
gda scene node --type Sprite2D --name Player --parent . --props '{"visible": true}'
gda scene property --path Player --property position --value '[0, 0]'
gda scene tree --max-depth 4
gda script create res://scripts/player.gd --extends Sprite2D
gda script attach --node Player --script res://scripts/player.gd
gda script patch --args-file -
gda script compile --path res://scripts/player.gd
gda scene save --path res://scenes/main.tscn
gda scene verify --max-depth 4
```
补丁步骤的参数载荷：
```json
{
  "path": "res://scripts/player.gd",
  "action": "replace",
  "source_code": "extends Sprite2D\nfunc _ready():\n\tpass\n"
}
```

## 4. 每步观察字段
打开场景后读取树深度与节点路径。创建节点和写入属性后回读编辑中的树，确认节点名称、类型、父节点与属性值。创建并挂载脚本后，确认脚本路径及其绑定的节点。对补丁和编译，读取信封字段 ok、error、data、warnings；保存前必须修掉编译错误。保存后用场景校验比较编辑中的树与磁盘文件。

## 5. 验证与退出条件
补丁编译无错误、脚本已挂到目标节点、且场景校验报告编辑中的树与保存文件无差异，即可退出。

## 6. 失败分支
补丁被拒绝时检查错误字段，并在写入前用 --dry-run 选项重试。编译失败时修正源码后重新编译。节点路径不对时重新读取树并使用真实路径。保存失败时重新检查目标路径，以及是否需要显式的覆盖确认。

## 7. 安全与确认点
场景与脚本写入虽在本地，也必须读回。覆盖已有场景或脚本需要显式确认：若调用返回 confirmation_required，审阅摘要后用 --confirm 重试。非交互会话缺少确认即停止。结果未知的写入不要重发，应通过读回验证。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点。

## 8. 明确不做什么
未读取之前不假设节点路径、工具名或脚本类名。本配方不启动游戏，也不运行输入时间线。不硬编码工具数量或版本窗口。
