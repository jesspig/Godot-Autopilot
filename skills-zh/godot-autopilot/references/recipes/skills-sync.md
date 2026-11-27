# skills-sync

## 1. 触发条件与输入
当安装、更新或校验随附技能书及其引擎文档时，使用本配方。输入：技能书名、辅助文件路径、安装目标目录、要安装的产物，以及是否需要中文归档。

## 2. 前置检查
你知道本地技能应来自内嵌归档还是重新安装。你有权写入选定的安装目录。你已记录当前技能书列表，便于变更后比对。

## 3. 可复制的 CLI 命令
```bash
gda skill list
gda skill show godot-autopilot
gda skill show godot-autopilot --skill-file references/recipes/bootstrap.md
gda setup status
gda setup install --artifact skills --zh
gda skill install --path .agents/skills --prune
gda doctor run --offline
gda docs sync
gda docs search CharacterBody2D
gda docs class Node2D --section Methods
```

## 4. 每步观察字段
从技能书列表记录名称与本地性。从技能书展示确认入口卡，并确认辅助文件路径可解析。安装前用安装状态报告当前状态。安装或同步后，将新列表与记录比对并确认目标目录。诊断输出不得出现新的失败。文档状态、搜索与类查询确认缓存可离线使用。

## 5. 验证与退出条件
期望的技能书存在、辅助文件可读、安装目录包含技能书、诊断通过、且文档查询无需联网即可返回结果，即可退出。

## 6. 失败分支
安装返回 confirmation_required 时，审阅目标与风险后用 --confirm 重试。安装后技能书缺失时重新列出并检查安装状态。文档搜索无结果时刷新缓存后再次搜索。安装目标不对时停止并选择正确目录，不要就地覆盖文件。

## 7. 安全与确认点
安装技能或插件属于高风险操作。调用返回 confirmation_required 时，审阅工具、目标、风险与参数后，用 --confirm 重试同一调用。清理会删除目录，同意前先确认将移除的内容。非交互会话缺少确认即停止。结果未知的调用不要重发。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点。

## 8. 明确不做什么
不在技能书中硬编码工具数量、别名表、版本窗口或迁移历史。不手工编辑生成的技能资产。本配方不安装无关产物。
