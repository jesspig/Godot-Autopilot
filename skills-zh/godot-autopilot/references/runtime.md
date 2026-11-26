# Runtime

## 前置检查

```bash
gda game status
gda monitor status
```

注入输入或改变游戏前先观测运行状态。启动、停止与驱动游戏都是高风险副作用：每一项都需要确认门禁。

## 操作：启动与停止

```bash
gda game start --headless
gda game play --path <scene> --confirm
gda game status
gda game stop --confirm
```

启动 headless 运行，或打开某个场景后播放。play 与 stop 都要确认游戏运行时变更；缺少确认时什么都不会发送。

## 操作：输入与截图

```bash
gda game input --type key --mode event --event '<json>' --confirm
gda game capture --target game --after-frames <n> --save --dir <dir>
```

用显式类型与模式排队一个输入事件。时序敏感时在固定渲染帧数后截图，并把 PNG 保存到目录。

```json
{"type": "key", "mode": "event", "args_file": "-"}
```

## 操作：运行脚本与时间线

```bash
gda game eval --source '<gdscript>' --confirm
gda game scenario --args-file <path> --confirm
```

eval 在运行中的游戏里执行 GDScript；scenario 触发输入时间线。两者都是高风险，需要确认。

## 观察

```bash
gda logs game --tail <n>
gda debug errors --level error
```

输入后读游戏日志与调试器错误，不要假定事件已被消费。游戏看似忽略输入时，把注入对齐到渲染帧。

## 验证

截图预期状态并读取运行状态。宣布 playtest 成功前确认错误水位线干净。

## 失败分支

- 游戏忽略输入：把事件对齐到帧并重新读取状态。
- 触发确认门禁：审查运行时目标后用 `--confirm` 重跑。
- 没有运行中的游戏：先启动，再重试观测。
