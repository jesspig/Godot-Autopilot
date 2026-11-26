# Tools

## 前置检查

```bash
gda commands --json
gda status --format json
```

The command manifest is authoritative for groups, commands, flags and exit
codes. Use it to confirm a command before writing it; the tool catalog itself is
discovered separately.

## 发现：列出与分类

```bash
gda tool list
gda tool list --category <cat> --tags <tags>
gda tool categories
```

The catalog changes with the CLI. Never hard-code a name or a count, and never
assume a tool exists from memory. List the current canonical catalog, then
narrow by category or tags when the result is broad.

## 发现：搜索与描述

```bash
gda tool search <query>
gda tool describe <tool>
```

Search by object first ("tile cell", "animation track", "import sidecar") and
add a verb only when results are too broad. Try synonyms before concluding a
tool is missing: tile/cell, sprite/texture, log/output, delete/remove/erase,
create/add/new. Describe the chosen tool before the first call: read its
schema, required `action` or `mode` values, and its side-effect annotation.
Never infer a parameter name.

Many tools group related operations behind one required `action` enum. Read the
enum from the description rather than remembering separate one-operation names.

## 调用：单次与批量

```bash
gda tool call <tool> --args '<json>'
gda tool call <tool> --args-file <path>
gda tool batch --args-file <path>
```

Short arguments go in the inline flag; use the file flag for long JSON, and a
single `-` as the path to read from stdin. Batch runs several calls in one
process. High-risk calls require the confirmation gate described in the safety
reference.

```json
{"node_path": "Level", "type": "Sprite2D", "parent": "."}
```

## 别名规则

Old tool names are legacy aliases. A name you recall may or may not still
resolve, so confirm it with the search and describe commands above, and prefer
the canonical tool with its explicit action. There is no fixed migration
table to rely on, and no version-based promise about when an alias disappears:
discover the current relationship with the CLI each time.

## 验证

After a call, check `ok`, `error.code` and the observed result. A successful
envelope only means the call went through, not that the intended change landed.

## 失败分支

- Unknown tool: re-run discovery with synonyms; do not guess a name.
- Missing required `action`: read the description enum and supply it.
- Confirmation gate: review the summary and re-run with `--confirm`.
