# safe-refactor

## 1. 触发条件与输入
当重命名、移动或替换被其他文件依赖的脚本或资源时，使用本配方。输入：当前路径、目标路径、用于原地编辑的补丁载荷，以及验证行为所用的测试标签。

## 2. 前置检查
项目已打开，改动前你已查询该对象的全部引用。存在备份或受版本控制的副本。目标路径下没有你想保留的内容，或者你已确认可以覆盖。

## 3. 可复制的 CLI 命令
```bash
gda resource references --path res://scripts/player.gd
gda script patch --dry-run --args-file -
gda resource rename --path res://scripts/old.gd --to res://scripts/new.gd --confirm
gda script compile --path res://scripts/new.gd
gda script reload --path res://scripts/new.gd
gda scene tree --max-depth 4
gda scene verify --max-depth 4
gda test run --label refactor --junit reports/refactor.xml
```
干跑补丁的参数载荷：
```json
{
  "path": "res://scripts/player.gd",
  "action": "replace",
  "source_code": "extends Sprite2D\n"
}
```

## 4. 每步观察字段
引用查询记录每个依赖路径与数量。干跑展示拟议改动与编译结果但不写入。重命名后确认新路径以及引用是否已被改写。编译与重载后读取信封字段 ok、error、warnings。场景树与校验确认没有损坏的节点绑定。测试运行显示通过与失败数量以及 JUnit 报告路径。

## 5. 验证与退出条件
干跑编译干净、新路径可解析、无残留失效引用、场景校验与磁盘一致、且带标签的测试套件通过，即可退出。

## 6. 失败分支
干跑失败时修正载荷并在写入前重试。重命名返回 confirmation_required 时，审阅目标后用 --confirm 重试。重命名后编译失败时恢复备份并重新评估。测试失败时不继续后续重命名，先检查失败项。结果未知的重命名不要重发，应回读路径确认。

## 7. 安全与确认点
重命名与移动会改写引用，需要显式确认。调用返回 confirmation_required 时，审阅工具、目标、风险与参数后，用 --confirm 重试同一调用。非交互会话缺少确认即停止。结果未知的调用不要重发。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点，批量中的高风险项要逐项授权。

## 8. 明确不做什么
不硬编码工具数量、别名表或版本窗口。未经逐项审查不做批量重命名。本配方不执行游戏运行时变更。
