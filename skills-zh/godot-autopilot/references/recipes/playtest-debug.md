# playtest-debug

## 1. 触发条件与输入
当你需要运行游戏、驱动输入、截取证据并收集某次故障的调试错误时，使用本配方。输入：场景路径、输入类型与模式及事件载荷、截取目标与目录，以及报告标签与文件路径。

## 2. 前置检查
编辑器正在运行且要运行的场景已知。你接受启动与停止游戏属于运行时变更，需要确认。存在可写的目录用于保存截图和报告。

## 3. 可复制的 CLI 命令
```bash
gda game status
gda game play --path res://scenes/main.tscn --confirm
gda game input --type action --mode api --event '{"action": "jump"}' --confirm
gda game capture --target game --save --dir captures
gda debug status
gda debug errors --limit 50 --level warning
gda debug repro --path res://scenes/main.tscn --confirm
gda game stop --confirm
gda logs game --tail 100 --level error
gda debug report --label smoke --report-file reports/smoke.json
```

## 4. 每步观察字段
运行前读取游戏状态，避免重复启动。运行后确认当前场景与实例绑定。输入后检查事件是否被接受。截图时核对保存的图片路径与目标。从事件快照一起读取编辑器、游戏与追踪状态。从调试错误读取 code、message、retryable、request_sent，并与磁盘上的游戏日志对照。停止后确认游戏不再运行。报告应引用标签、图片路径与报告文件。

## 5. 验证与退出条件
目标场景已运行、输入被接受、截图已生成、调试错误连同其请求状态已收集、游戏已停止、报告文件已写入，即可退出。

## 6. 失败分支
运行返回 confirmation_required 时，审阅目标后用 --confirm 重试。游戏未启动时读取状态与编辑器日志。调试流中没有错误时放宽级别或上限并读取游戏日志。截图失败时检查目标与目录。结果未知的输入或运行时调用不要重发，应先读状态。

## 7. 安全与确认点
游戏运行、输入、求值、时间线与停止都属于运行时变更。返回 confirmation_required 时，审阅工具、目标、风险与参数后，用 --confirm 重试同一调用。非交互会话缺少确认即停止。结果未知的调用不要重发。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点，批量输入时间线尤其如此。

## 8. 明确不做什么
故障取证完成后不要让游戏继续运行。不猜测输入类型、事件结构或截图目标。不硬编码工具数量或版本窗口。
