# Runtime

## 前置检查

```bash
gda game status
gda monitor status
```

Observe the run state before injecting input or changing the game. Starting,
stopping and driving the game are high-risk effects: each requires the
confirmation gate.

## 操作：启动与停止

```bash
gda game start --headless
gda game play --path <scene> --confirm
gda game status
gda game stop --confirm
```

Start a headless run, or play a specific scene after opening it. Confirm the
game runtime change on play and stop; a missing confirmation sends nothing.

## 操作：输入与截图

```bash
gda game input --type key --mode event --event '<json>' --confirm
gda game capture --target game --after-frames <n> --save --dir <dir>
```

Queue one input event with an explicit type and mode. Capture after a fixed
number of rendered frames when timing matters, and save the PNG to a directory.

```json
{"type": "key", "mode": "event", "args_file": "-"}
```

## 操作：运行脚本与时间线

```bash
gda game eval --source '<gdscript>' --confirm
gda game scenario --args-file <path> --confirm
```

Evaluation runs GDScript inside the running game; a scenario fires an input
timeline. Both are high risk and need confirmation.

## 观察

```bash
gda logs game --tail <n>
gda debug errors --level error
```

After input, read the game log and debugger errors rather than assuming the
event was consumed. Align injected input to rendered frames when the game seems
to ignore it.

## 验证

Capture a screenshot of the expected state and read the run status. Confirm the
error watermark is clean before declaring a playtest successful.

## 失败分支

- Game ignores input: align the event to frames and re-read the status.
- Confirmation gate: review the runtime target and re-run with `--confirm`.
- No game running: start a run, then retry the observation.
