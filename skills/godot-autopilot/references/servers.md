# Servers

## 前置检查

```bash
gda tool search "physics"
gda tool describe <tool>
gda debug status
```

Server work means RIDs, physics, navigation and rendering. Discover the exact
tool for the server call, then verify the RID lifecycle and the thread context
before calling it; a stale RID or a background thread is a common silent
failure.

## 操作：发现并调用服务器工具

```bash
gda tool search "navigation"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

Search by the server concept ("physics query", "navigation", "rendering
server"), read the required action and argument schema from the description,
then call the tool. Short arguments go inline; long JSON goes in the file flag.

```json
{"path": "res://main.tscn", "wait": 1}
```

## 操作：检查与执行

```bash
gda debug inspect --action object --path <path>
gda script exec --source '<gdscript>' --confirm
gda game capture --target game --save
```

Inspect a live object, run GDScript on the main thread when a server API needs
it, and capture the visual result. In-editor execution is high risk and needs
the confirmation gate.

## 观察

```bash
gda debug status
gda game capture --target game --save
```

Read engine state and the capture after a server call rather than trusting a
silent success.

## 验证

Confirm the RID is valid for the call, the operation ran on the expected thread,
and the visual or physics result matches intent.

## 失败分支

- Server call fails silently: verify RID validity and main-thread execution.
- RID invalid: re-acquire it and retry the call.
- Confirmation gate on execution: review the source and re-run with `--confirm`.
