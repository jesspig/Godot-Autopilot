# 环境与恢复协议

当准备环境、执行前置探针、处理引擎崩溃、保存增量或调用模态工具时读取本文件。

## 环境准备（全程自主，默认完整编辑器）

- 构建、部署、装插件、启动引擎都自行完成，默认启动**完整编辑器**（非 headless）；仅无显示环境才回退 headless。不向命令传业务参数，也不需要用户打开工程、启用插件或关闭编辑器。
- 固定沙盒 `examples/e2e`：新轮次先执行 `bun run gda-dev-tools jam e2e --reset --json` 重置，读取返回的 `path` 作为 `<project-dir>`；随后用 `assets/fixed-task-brief.md` 覆盖 `<project-dir>/GAMEJAM.md` 作为本轮固定任务书（忽略 `jam e2e` 内部随机主题）。仅用户明确要求续跑时跳过重置。
- 收到用户指定工程时直接使用该工程，不重新抽题；若路径就是 `examples/e2e`，仍按新轮次先重置（续跑除外）。
- 创建/重置后用 `bun run gda-dev-tools harness setup --project <project-dir>` 完成环境准备（它按序执行 build、deploy、`gda start`、list、status、ping、tools list）；必要时可单独 `build` / `deploy`。读取命令输出，不读实现源码。
- 已知缺陷：若 setup 因末尾 `gda ping` 缺 `--project` 失败并终止刚启动的编辑器，先记录上下文（见下），再用 `gda start --project <project-dir>` 恢复并确认 `gda status` ok=true、`gda list` live=true；不反复重试 setup。
- 可接收本轮聚焦工具域（如「重点验证脚本工具」），它调整验证侧重，但不降低覆盖全部 252 工具的硬门槛。

## 先记录上下文再自重启

任何重启（含重置沙盒、中毒兜底、崩溃恢复）都先记录：当前工具调用、工程未保存增量清单、CLI 与插件 trace/log 尾部、`GOAL.md` 末态，再执行重启。进程已消失时从 `<集中根>/projects/.../logs/gda-*.log` 与 `~/.godot-autopilot/cli-traces/trace-YYYYMMDD.jsonl` 取最后记录。未记录不得重启，也不再请用户关闭或打开编辑器。

## 前置检查 CLI 探针（只读，在仓库根执行）

| 顺序 | 命令 | 通过判据 |
| --- | --- | --- |
| 1 | `gda status --project <project-dir> --json` | 退出码 0，`ok=true`，`project` 与 `<project-dir>` 一致（Windows 大小写不敏感） |
| 2 | `gda call --project <project-dir> get_engine_state --args '{"action":"version"}'` | 退出码 0，返回引擎版本 |
| 3 | `gda call --project <project-dir> system_status`（连续两次） | 两次均退出码 0 |

- 退出码语义：0 成功 / 1 工具失败 / 2 传输环境失败 / 3 用法错误。
- 失败时记录命令、退出码与 `gda status` / `gda list` 的实例、端口、`mcp_url`、`live`。按「先记录上下文再自重启」重试环境准备一次再复测；`status.project` 不匹配则自重启到目标工程后重跑；CLI 成功而 MCP 调用失败记为客户端路径问题；端口无监听则查 `%LOCALAPPDATA%\CrashDumps` 后自重启一次，仍失败转报告记阻断。
- `system_status` 要求 `running=true`；不可达按上条重试一次，仍不可达转报告记阻断。
- 用 `execute_script`（confirm=true）读取 `GODOT_AUTOPILOT_TRACE_DIR` 与 `GODOT_AUTOPILOT_LOG_DIR`；被拒时记录原因，不推测。
- 以 `search_tools` / `gda describe` 确认工具名与参数，不凭记忆调用。

## 保存优先与中毒兜底

- 每完成一个可保存增量（场景调整、脚本挂载、资源生成、属性配置）立即 `save_editor_scene` 或对应资源保存工具，不要攒批。
- 仅当日志命中 `task store: failed to persist` 签名时启用恢复：先读文件确认是否落盘；已落盘直接恢复，未落盘重做后立即保存；记录恢复次数与丢失增量。整轮未命中写「未触发」。
- 不得因预期可能中毒而预防性重启；确需重启按「先记录上下文再自重启」执行（崩溃恢复例外见下）。

## 模态工具受限

- `show_display_dialog`、`open_os_path`、`show_os_alert` 列为受限工具：放在每轮最后、仅人工值守时调用、调用前先保存全部工作。
- `show_os_alert` 已有无人值守保护（返回 notice 不弹窗），为正面范例。无人值守会话不调用前两者；调用超时后须记录后续工具是否连锁超时。这类工具在覆盖报告中逐项写明「需人工值守」理由，不记缺口。

## 崩溃恢复与定位

引擎崩溃不直接终止本轮；允许记录上下文与初步定位后重启恢复。

1. **先记录上下文再重启**：崩溃前最后工具调用（工具名、参数摘要、`request_id`、`trace`）、CLI 与插件日志尾部、未保存增量清单。未记录不得重启。
2. **初步归因**（三选一，禁止臆断）：工具导致（崩溃前调用时间紧邻或属高风险/主线程类，写明可疑工具）／偶发（无明确邻近调用或该调用此前多次成功，标注待观察）／未定（证据不足）。
3. **可复现性**：仅在安全前提下重试可疑调用；再次崩溃记「可复现」，否则「不可复现」，未尝试记原因。不得为复现反复崩溃。
4. **恢复**：重启 Godot，按保存优先协议补存增量、核对工程状态后继续；记录恢复次数与丢失增量。
5. **登记**：写成 Q 条目（类型=失败），五要素 + 归因 + 可复现性 + 恢复次数，证据用轨迹与日志路径。
6. **停止条件**：同一崩溃重复 3 次无法定位，或恢复后仍崩溃、工程无法还原 → 停止本轮转报告。

## 证据来源

| 证据 | 来源 | 用途 |
| --- | --- | --- |
| CLI 轨迹 `trace-YYYYMMDD.jsonl` | `~/.godot-autopilot/cli-traces/` | 覆盖统计（按本轮起始时间过滤）、调用序列、耗时、错误码 |
| GDA 日志 `gda-*.log` | `GODOT_AUTOPILOT_LOG_DIR` | 失败上下文、中毒签名检索 |
| 截图 | 本轮目录 `img/` | 结构、资源、画面、菜单与行为 |
| 读回值 | 工具返回 | 逻辑与资源断言 |
| 对照证据 | 基线 vs 改动后成对截图/读回值/日志 | 证明行为差异，区分修复与回归 |

- 报告注明轨迹默认脱敏：参数截断 4000 字符、敏感键替换；截图不写入轨迹。
- 插件轨迹保留最近 20 文件 / 50MB。结束前把本轮关键事件摘录到 `evidence.md`：工具名、参数摘要、ok、error_code、duration_ms、request_id、对应截图、run 标识。不复制完整轨迹与日志。
- 报告与 `evidence.md` 用 `<集中根>/projects/...` 占位路径，不写真实用户名或本机绝对路径；截图用相对路径引用；原始轨迹/日志只作可追溯来源，不要求轮目录内有副本。
- 涉及画面效果时看截图验收，不用逻辑断言替代截图。
