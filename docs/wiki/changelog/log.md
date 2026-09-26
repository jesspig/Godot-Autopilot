# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-09-26

- **构建入口 main.py 化与 scripts/ 拆分 + TUI 新增**：`build.py` 删除，拆为 `main.py`（build/package/clean/tui）与 `scripts/` 6 模块，部署目标改 `demo/×5`（`--demos` 可选子集），`questionary` 入依赖；`AGENTS.md`/构建技能/CI/wiki 引用同步。详见 [2026-09-26-log.md](2026-09-26-log.md)

## 2026-09-25

- **项目级技能层落地 + `AGENTS.md` 退为最小入口**：新增 `.agents/skills/project-*/SKILL.md` 共 8 个（构建登记、加工具、写 handler、跑测试、写埋点、读 trace 排障、安全授权门、三层文档维护），与交付给客户端的 `godot-autopilot-*` 技能书按前缀隔离；`AGENTS.md` 由 23448 字符精简到 4044（只留定位、命令、6 条硬约束、三层分工、技能索引、协作）。删除前用三个只读子代理逐条核对覆盖度，并把独有事实与错号补进 wiki。详见 [2026-09-25-log.md](2026-09-25-log.md)
- **纠正三处失实记载**：`raw_schema` 手写实际仅 3 个元工具（非"7 元全手写"）；`Args` 取参器"可用但未接线"（`src/` 下 0 个领域 handler include，现网仍是 `args.Find` + `util::error_json` 约 490 处）；Host 环回校验改按当前 SDK 行为记载为**绑定层 + 请求层两层**，并指认 08-28 changelog 的"可覆盖为 `0.0.0.0`"已被拒绝启动行为取代。另记：授权门只覆盖 `process`/`code_execute`/`game_runtime`/`user_tools`，`writes_file` 等枚举不进授权门。
- **wiki 同步 7 页**：`index.md` 新增"三层文档分工""编写规则"两节并把技能纳入维护入口与页面索引；`conventions.md` 新增"注释与提交"段与 Godot 4.7 API 口径三条；`security_contract.md` §2 两层环回；`modules/core.md` 协议埋点接线；`tool_base_design.md` `error_code` 口径 + `Args` 接入状态；`tests.md`/`build.md` 行号与守卫输出修正、L2 时长与 googletest 缓存兜底。
- **验证**：9 个改动文件相对链接 0 断链；`comment_guard` OK；`migration_guard` 输出 `-- [migration-guard] OK (domains: 30, strict: on)`；零 C++ 与测试配置改动，未触发构建与 ctest 全量。
- **技能脚本通道文档同步（T13）**：skill 8→9 册（新增 `godot-autopilot-tools` 调用专册）、模板 33 md + 1 mjs + registry、references 22→24；L1 312→315、ctest 注册点 344→348（过滤 314→318，G1 实测 318/318 全绿）；同步 `support.md`/`tests.md`/`security_contract.md`（新增 §3.3）/`overview.md`/`build.md` 与双语 README（"MCP 为主产品，skill/脚本是体验层"）；只改 `.md`，未编译/测试/提交。Example 生成验证待 E2E 验证。详见 [2026-09-25-log.md](2026-09-25-log.md)

## 2026-09-21

- **可重放监控与日志系统**：新增 `src/core/` 的 monitor（统一埋点门面，纯 std，可被 MCP worker 线程调用）、monitor_env（`environment_snapshot()` 仅主线程，写 `kind=snapshot`）、perf_sampler（主线程周期采样 `kind=perf` + 请求超时看门狗 `kind=execution_state, state=timed_out`）；`TraceRecorder` 增 `TraceKind`（15 值）与 schema v2 新字段、`LogEntry` 增 `trace_id`/`span_id`、`LogPersist` 增健康计数与 `trace_dir`、`CommandQueue` 增 `Stats`；`server_context` 经 `opts.on_request`/`opts.outgoing_filters` 关联协议请求（`begin_request`/`end_request`），元工具经 `RequestScope`+`RequestSpanGuard` 端到端关联；McpLogDock 增 Open Traces 按钮与 Trace 视图（可点击 `[trace]`、搜索兼匹配 detail）。构建 add_library 86→89（core 12→15）、L1 285→312（33 文件）、L2 29→30（`30_observability`）、ctest 316→344（L1 过滤 287→314）；实测 jsonl 观测 `lifecycle`/`protocol_request`/`protocol_response`/`tool_call`/`persist_health`/`perf`/`snapshot`/`concurrency`，`request_id` 贯通、`protocol_response` 回填 `duration_ms`；工具总数 399 不变。详见 `changelog/2026-09-21-log.md`

## 2026-09-20

- **依赖升级**：godot-cpp 升 10.0.0-stable（rc2→stable，`_deps` checkout `10.0.0-stable`/507ed9d）；mcp-cpp-sdk 维持 0.3.4（0.3.5 尝试后回滚，`cmake/FetchDependencies.cmake` 仅余 godot-cpp 一处变更；configure 实测 `[mcp] SDK version: 0.3.4`）；`AGENTS.md` 与 wiki 三页（build/conventions/overview）同步。详见 `changelog/2026-09-20-log.md`
- **可重放监控 + 双目录持久化**：新增 LogPersist / TraceRecorder / sanitize_policy 三 core 模块与 `LogSystem::log_detailed`，工具调用 trace（图片只记 `image_ref`/hash，默认脱敏）落盘 `user://godot_autopilot/logs|traces`（各保留 20 文件 / 50MB），`_process` 先 flush 后 drain；McpLogDock 增 Detail/Open Logs、McpConfigDock 增 Desensitize data；修 `FileAccess::READ_WRITE` 不建文件导致的首次追加失败（log_persist + text_ops）。L1 269→285（31 文件）、L2 28→29（`29_trace_persistence`，实测 PASS）、ctest 299→316；终验 L1 287/287、L2 全量 29/29（707.95s），脱敏开关两端、GUI 图片落盘、跨线程 trace 与 Dock 新控件均有端到端探针实测。详见 `changelog/2026-09-20-log.md`
- **0.2.6 升版**：`VERSION` 0.2.5→0.2.6 对齐发布 tag（run 35502180800 的 validate 拦截属守卫按设计工作）；`0.2.6` tag 重打到新提交后重推，release 重新触发。详见 `changelog/2026-09-20-log.md`

- **代码-文档一致性审计**：`call_tool` 描述 385→391 域随源码修正（392 非元；overview 09-18"已修正"注记失实一并更正）；ctest 口径补计 `comment_guard`（270→271，全量 298→299，`tests.md` 新增守卫小节）；index/overview/build/roadmap 过时计数与 core/entry_runtime 行号重核同步。详见 `changelog/2026-09-20-log.md`
- **Release 触发修复**：`release.yml` 的 `on.push.tags` 仅 `v*`，而 7 个已发布 tag 全是无 `v` 格式，故一次都没触发过；改为双格式 `v*.*.*` / `[0-9]*.*.*` + `workflow_dispatch` 手动指定 tag，`validate` 剥离 `v` 后比对 `VERSION`；历史 7 个 tag 暂不补发。详见 `changelog/2026-09-20-log.md`
- **项目知识库二轮一致性审计**：`docs/wiki/` 15 个内容页 + changelog 3 页对照 `git diff HEAD`（42 修改 + 9 未跟踪）与源码逐项核实——消解 3 处语义冲突（LogPersist 消费方、图片收集门槛、`image_width`/`image_height`）、补录 5 处缺失事实（22 处 `ScopedTraceContext` 包装点、`get_plugin_log` detail 口径、PluginConfig/AutopilotTools `log_detailed`）、修正 28 处行号漂移与 5 项附加一致性（断链、try/catch 组数、dock 布局、`src/tools/` 48 cpp、editor_ops 标题口径）、11 页 frontmatter `timestamp` 同步；`AGENTS.md` 同步构建/测试/架构/知识库四段，数值口径以 [tests.md](../tests.md) 核算总表为准。详见 `changelog/2026-09-20-log.md`

## 2026-09-19

- **ToolSpec 数据化重构 + 文档终态**：391 域工具 + `system_status` + 7 元工具全量改为 `ToolSpec` 数据记录（删除 `GDA_TOOL_CLASS` 宏/真类、`fn_tool.hpp`/`meta_tools.hpp`/`IMetaTool`、8 个 `schema_*_ops.cpp`/`schema_fills.hpp`/`tool_input_schema`）；新增 `tool_args`/`tool_pipeline`/`dynamic_spec_store`/`autopilot_tools`（用户脚本动态工具 + `user_tools` 授权门）与 `tests/guard/` 迁移守卫；L1 243→255、L2 26→27 份（`26_user_tools_code_mode`）、`ctest -E "^gda_runner_"` 256 项；遍历改为运行时枚举 392 条后按 `side_effect`/`mutating`/`dynamic` 排除（330 个进入两步骤，实测全过）；99 号诊断用例删除；wiki 11 页同步。详见 `changelog/2026-09-19-log.md`
- **收尾第二轮（traits/invoke/rescan/batch，零代码）**：traits 终态（`kCaptureImage` 9/`kSceneTarget` 25/`kUndoable` 10）+ `tool_invoke`（深度 8）+ `rescan` + `batch_execute` 变量串联；L1 255→269（3 文件 14 项）、L2 27→28 份（`27_user_tools_rescan`）、ctest 283→298（298/298 全绿）；`Example/.gitignore` 白名单放行 `user_tools_samples/echo_tool.gd(.cs)`（C# 文档级，未经 CI 验证）。详见 `changelog/2026-09-19-log.md`
- **示例目录迁移**：`Example/user_tools_samples/` → `samples/user-tools/`（`Example/` 为 L2 测试工程、示例属噪声；`Example/.gitignore` 白名单同步撤销；引用路径已全仓更新）。详见 `changelog/2026-09-19-log.md`
- **源码注释清理**：执行 2026-08-12"不写注释"约定，删除 `src/` 241 行整行 `//`（+ 约 5 处行内 `/*名字=*/`）与 `tests/` 236 行整行 `//`（+ 14 处行尾 `//`），`// namespace` 结尾标记 500 处保留；有信息量解释移植进 wiki（`entry_runtime` 22 条/`tools_ops_a` 15 条等）；新增 `tests/guard/comment_guard.py` 防回退守卫；构建 OK、L1 `ctest -E "^gda_runner_"` 271/271 全绿、`src`/`tests` 整行 `//` 归零。详见 `changelog/2026-09-19-log.md`
