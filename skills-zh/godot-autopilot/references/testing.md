# Testing

## 前置检查

```bash
gda test list
```

运行前列出可用套件。对照命令清单确认名字；不要臆造套件名。

## 操作：运行套件

```bash
gda test run <suite> --label <label>
gda test run --shard <i/c> --retries <n> --junit <path> --coverage
gda test run --detach
```

运行整个套件或按标签过滤。大任务分片运行、失败重试，CI 消费时写出带覆盖率的 JUnit 报告。detach 运行返回 job id 并把进度流写到 stderr。

```json
{"label": "smoke", "shard": "1/4", "retries": 2}
```

## 操作：内联与场景测试

```bash
gda test script --source '<gdscript>'
gda test script --args-file <path>
gda test scenario --args-file <path> --confirm
```

运行内联 GDScript 用例或从文件加载。scenario 在运行中的游戏里触发输入时间线，属于高风险，需要确认。

## 操作：轮询

```bash
gda test watch --interval <sec> --detach
gda test status <id>
gda job wait <id>
```

watch 轮询游戏状态与测试结果直到运行结束。用 status 与 job 命令读取之前启动的运行。

## 观察

```bash
gda test status <id>
```

读取计数、失败项与报告文件，不要只信 stderr 上的进度行。

## 验证

确认预期用例都运行了、失败数为零，且 JUnit 报告在指定路径存在。

## 失败分支

- 套件找不到：重新列出套件并核对确切名字。
- 间歇性失败：带重试重跑并检查失败用例。
- detach 运行未完成：等待该 job，不要启动重复运行。
