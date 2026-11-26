# error-recovery

## 1. 触发条件与输入
当调用失败、返回非零退出码、报告 confirmation_required，或结果未知时，使用本配方。输入：失败的调用及其退出码、受影响的实例或项目，以及你已掌握的任务 id 或追踪标识。

## 2. 前置检查
你尚未重发失败的调用。你知道这次 CLI 失败属于业务错误、环境错误、用法错误还是确认闸门。尚未启动任何破坏性清理。

## 3. 可复制的 CLI 命令
```bash
gda doctor run --offline
gda doctor diagnose --ci
gda status
gda logs cli --tail 100 --level error
gda logs plugin --limit 50 --level error
gda trace failures --limit 20
gda trace summary
gda job list
gda tool call resource_property --args-file - --confirm
```
带确认的通用调用参数载荷：
```json
{
  "action": "set",
  "path": "res://data/config.tres",
  "property": "enabled",
  "value": true,
  "confirm": true
}
```

## 4. 每步观察字段
读取信封的 ok、error、warnings，并在 error 内读取 code、message、retryable、request_sent。不可重试的错误不能盲目重发。若 request_sent 不清楚，就把结果视为未知，应检查状态而不是重发。诊断列出失败的检查及修复代码。状态、日志、追踪失败与任务列表共同说明副作用是否已经发生。

## 5. 验证与退出条件
根因已定位、修复已应用、同一调用成功一次、且通过读回确认状态，即可退出。退出码：零为成功，一为业务失败，二为环境失败，三为用法错误，四为需要确认，五为拒绝确认，一百三十为被中断。

## 6. 失败分支
诊断报告环境失败时先修复连通性再重试。用法错误时修正 flag 与参数。返回 confirmation_required 时，读完摘要后用 --confirm 重试同一调用；若用户拒绝则停止。结果未知时检查任务列表并读取目标状态，不要重发。

## 7. 安全与确认点
确认是闸门，不是一揽子授权。调用返回 confirmation_required 时，审阅工具、目标、风险与参数，再用 --confirm 重试。非交互会话缺少确认即停止。结果未知的调用不要重发。把破坏性、进程、代码执行或游戏运行时信号视为停下审查点。

## 8. 明确不做什么
不硬编码工具数量、别名表或版本窗口。不把清理缓存、取消任务或删除文件当作反射式恢复动作。未描述前不猜测替代工具名。
