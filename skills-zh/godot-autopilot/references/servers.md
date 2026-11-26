# Servers

## 前置检查

```bash
gda tool search "physics"
gda tool describe <tool>
gda debug status
```

服务器工作指 RID、物理、导航与渲染。先发现该服务器调用对应的确切工具，调用前验证 RID 生命周期与线程上下文；过期 RID 或后台线程是常见的静默失败来源。

## 操作：发现并调用服务器工具

```bash
gda tool search "navigation"
gda tool describe <tool>
gda tool call <tool> --args '<json>'
```

按服务器概念搜索（“physics query”“navigation”“rendering server”），从描述里读取必填 action 与参数 schema，然后调用工具。短参数用行内 flag，长 JSON 用文件 flag。

```json
{"path": "res://main.tscn", "wait": 1}
```

## 操作：检查与执行

```bash
gda debug inspect --action object --path <path>
gda script exec --source '<gdscript>' --confirm
gda game capture --target game --save
```

检查 live 对象，服务器 API 需要时在主线程执行 GDScript，并截取视觉效果。编辑器内执行属于高风险，需要确认门禁。

## 观察

```bash
gda debug status
gda game capture --target game --save
```

服务器调用后读引擎状态与截图，不要相信静默的成功。

## 验证

确认调用时 RID 有效、操作在预期线程执行，且视觉或物理结果符合意图。

## 失败分支

- 服务器调用静默失败：验证 RID 有效性与主线程执行。
- RID 无效：重新获取后重试调用。
- 执行触发确认门禁：审查源码后用 `--confirm` 重跑。
