# Tools

## 前置检查

```bash
gda commands --json
gda status --format json
```

命令清单是 group、命令、flag 与退出码的权威来源。写命令前用它确认；工具目录则单独发现。

## 发现：列出与分类

```bash
gda tool list
gda tool list --category <cat> --tags <tags>
gda tool categories
```

工具目录随 CLI 变化。禁止硬编码名字或数量，禁止凭记忆断言工具存在。先列出当前 canonical 目录，结果太宽再按分类或标签收窄。

## 发现：搜索与描述

```bash
gda tool search <query>
gda tool describe <tool>
```

先按对象搜索（“tile cell”“animation track”“import sidecar”），结果太宽再补动词。下结论说工具缺失前换同义词重试：tile/cell、sprite/texture、log/output、delete/remove/erase、create/add/new。首次调用前描述所选工具：读它的 schema、必填的 `action` 或 `mode` 值以及副作用注解。禁止推断参数名。

许多工具用一个必填 `action` 枚举收拢相关操作。从描述里读枚举，不要记独立的一次性工具名。

## 调用：单次与批量

```bash
gda tool call <tool> --args '<json>'
gda tool call <tool> --args-file <path>
gda tool batch --args-file <path>
```

短参数用行内 flag；长 JSON 用文件 flag，路径写单独的 `-` 表示从 stdin 读取。批量在一个进程里执行多次调用。高风险调用需要安全参考里描述的确认门禁。

```json
{"node_path": "Level", "type": "Sprite2D", "parent": "."}
```

## 别名规则

旧工具名是 legacy alias。记忆中的名字可能仍可解析，也可能不行，因此用上面的搜索与描述命令确认，并优先使用带显式 action 的 canonical 工具。没有可依赖的固定迁移表，也没有关于别名何时消失的版本承诺：每次都用 CLI 发现当前关系。

## 验证

调用后检查 `ok`、`error.code` 与观测结果。成功的 envelope 只表示调用通过了，不表示预期改动已落地。

## 失败分支

- 工具未知：用同义词重跑发现；不要猜名字。
- 缺少必填 `action`：读描述里的枚举并补上。
- 触发确认门禁：审查摘要后用 `--confirm` 重跑。
