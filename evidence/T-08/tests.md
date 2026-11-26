# T-08 测试编写记录（未执行）

按任务约束未运行 `bun test`，以下为已写入的用例清单，不声称通过。

## lifecycle.test.ts（追加 describe「T-08 命令树与 alias」）
1. `gda editor list` 空登记输出 `[]`
2. `gda editor status` 无实例输出原生错误且无 `mcp_url/mcp/port/port_source`
3. `gda editor stop nope` exit 2
4. `projects` 旧顶层 alias 仍可用
5. `ping --help` 指向 `gda editor ping`
6. `gda tool categories --offline` 无缓存报传输错误
7. `gda shell` 缺少 `--protocol ndjson` 报用法错误
8. `gda doctor --ci` 输出稳定 `checks[].code/remediation`

## target.test.ts（追加 describe `resolveNativeTarget`）
1. 显式 selector 优先级 `--id > --socket > --port > --project`（互斥错误文案含 socket）
2. `--socket` 返回 socket 链与 IPC endpoint
3. `--port` 返回 port 链并保留原生 endpoint
4. `--project` 多实例产生 `AmbiguousTargetError.candidates`
5. `--id` 陈旧实例拒绝固定
6. `--id` 活实例固定 instance id

## tools.test.ts（追加 describe「tool 子命令与离线 catalog」）
1. 离线 list 输出 `source=offline/hash`，按 category/tags 过滤
2. 离线 search/describe/categories 共用同一 catalog（含别名解析）
3. hash 被篡改时拒绝离线读取
4. live `catalog.list` 不发送 MCP initialize 并写入离线缓存
5. 未知 tool 子命令报用法错误

## status.test.ts（新增）
1. `editor status` 输出 `instance/project/version/ipc_kind/ready/tool_count/capabilities`，
   无 `mcp_url/mcp/port/port_source/transport/server`；所有原生请求 `v=1/kind=request` 且无 initialize
2. `editor status` 经 `run` 分发，无实例时结构化错误
3. `editor ping` 走原生 hello，不发送 MCP initialize
4. 未知 editor 子命令报用法错误

## doctor.test.ts（新增）
1. 无实例/无缓存时 `doctor --ci` 给出 `GDA_NO_INSTANCE` / `GDA_CATALOG_MISSING` 与 remediation，无绝对路径
2. 陈旧实例报 `GDA_STALE_INSTANCE` 且 exit 2
3. catalog hash 不匹配报 `GDA_CATALOG_HASH_MISMATCH`
4. `doctor run` 子命令路由
5. `diagnose` 无目标报 `GDA_TARGET_FAILED`
6. `diagnose` 活实例经原生 hello 成功且不发送 MCP initialize

## shell-ndjson.test.ts（新增）
1. `argv` 回显 id/argv，子进程退出产生 `exitCode`，stdout 产生 `envelope`
2. `envelope` 请求按 id 转发
3. `shutdown` 终止子进程并确认 id
4. 未知请求类型产生 `error` frame
5. 非 ndjson protocol 报用法错误
