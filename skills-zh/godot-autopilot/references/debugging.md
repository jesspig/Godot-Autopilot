# Debugging

## 前置检查

```bash
gda debug status
gda debug errors --limit <n> --level error
```

汇总一份事件快照：编辑器状态、游戏状态、trace 上下文与调试器错误。改动任何东西前先读错误水位线。

## 操作：读取调试流

```bash
gda debug output --limit <n>
gda debug stack
gda debug inspect --action object --path <path>
gda debug trace
```

读取捕获的标准输出与调试栈，检查 live 对象或节点，再读 trace 上下文。游戏运行时调试流承载其错误与打印；没有游戏时由日志动作读取编辑器输出。

## 操作：复现与报告

```bash
gda debug repro --path <scene> --confirm --detach
gda job wait <id>
gda debug report --label <label> --report-file <path>
```

repro 编排播放、输入、采样、截图与错误收集。它是高风险游戏运行时变更，detach 时返回 job id；读报告前先轮询该 job。

```json
{"label": "crash", "report_file": "res://report.json"}
```

## 观察

```bash
gda logs editor --level error --tail <n>
gda logs plugin --level error
gda logs cli --source <s>
gda trace summary
gda trace slow --slow <ms>
gda monitor report <id>
```

把调试流与磁盘上的游戏日志交叉核对。问题出在工具本身时看插件与 CLI 日志；延迟与帧问题用 trace 与 monitor 报告。

## 验证

确认错误水位线回到零、调试栈不再显示故障，且新报告没有新错误。

## 失败分支

- 修复后错误数仍非零：拉取真实日志行并解释它们。
- repro 已 detach 但未完成：等待该 job，不要启动第二个复现。
- 插件侧失败：读插件日志并上报，不要盲目重试。
