# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-09-20

- **0.2.6 升版**：`VERSION` 0.2.5→0.2.6 对齐发布 tag（run 35502180800 的 validate 拦截属守卫按设计工作）；`0.2.6` tag 重打到新提交后重推，release 重新触发。详见 `changelog/2026-09-20-log.md`

- **代码-文档一致性审计**：`call_tool` 描述 385→391 域随源码修正（392 非元；overview 09-18"已修正"注记失实一并更正）；ctest 口径补计 `comment_guard`（270→271，全量 298→299，`tests.md` 新增守卫小节）；index/overview/build/roadmap 过时计数与 core/entry_runtime 行号重核同步。详见 `changelog/2026-09-20-log.md`
- **Release 触发修复**：`release.yml` 的 `on.push.tags` 仅 `v*`，而 7 个已发布 tag 全是无 `v` 格式，故一次都没触发过；改为双格式 `v*.*.*` / `[0-9]*.*.*` + `workflow_dispatch` 手动指定 tag，`validate` 剥离 `v` 后比对 `VERSION`；历史 7 个 tag 暂不补发。详见 `changelog/2026-09-20-log.md`

## 2026-09-19

- **ToolSpec 数据化重构 + 文档终态**：391 域工具 + `system_status` + 7 元工具全量改为 `ToolSpec` 数据记录（删除 `GDA_TOOL_CLASS` 宏/真类、`fn_tool.hpp`/`meta_tools.hpp`/`IMetaTool`、8 个 `schema_*_ops.cpp`/`schema_fills.hpp`/`tool_input_schema`）；新增 `tool_args`/`tool_pipeline`/`dynamic_spec_store`/`autopilot_tools`（用户脚本动态工具 + `user_tools` 授权门）与 `tests/guard/` 迁移守卫；L1 243→255、L2 26→27 份（`26_user_tools_code_mode`）、`ctest -E "^gda_runner_"` 256 项；遍历改为运行时枚举 392 条后按 `side_effect`/`mutating`/`dynamic` 排除（330 个进入两步骤，实测全过）；99 号诊断用例删除；wiki 11 页同步。详见 `changelog/2026-09-19-log.md`
- **收尾第二轮（traits/invoke/rescan/batch，零代码）**：traits 终态（`kCaptureImage` 9/`kSceneTarget` 25/`kUndoable` 10）+ `tool_invoke`（深度 8）+ `rescan` + `batch_execute` 变量串联；L1 255→269（3 文件 14 项）、L2 27→28 份（`27_user_tools_rescan`）、ctest 283→298（298/298 全绿）；`Example/.gitignore` 白名单放行 `user_tools_samples/echo_tool.gd(.cs)`（C# 文档级，未经 CI 验证）。详见 `changelog/2026-09-19-log.md`
- **示例目录迁移**：`Example/user_tools_samples/` → `samples/user-tools/`（`Example/` 为 L2 测试工程、示例属噪声；`Example/.gitignore` 白名单同步撤销；引用路径已全仓更新）。详见 `changelog/2026-09-19-log.md`
- **源码注释清理**：执行 2026-08-12"不写注释"约定，删除 `src/` 241 行整行 `//`（+ 约 5 处行内 `/*名字=*/`）与 `tests/` 236 行整行 `//`（+ 14 处行尾 `//`），`// namespace` 结尾标记 500 处保留；有信息量解释移植进 wiki（`entry_runtime` 22 条/`tools_ops_a` 15 条等）；新增 `tests/guard/comment_guard.py` 防回退守卫；构建 OK、L1 `ctest -E "^gda_runner_"` 271/271 全绿、`src`/`tests` 整行 `//` 归零。详见 `changelog/2026-09-19-log.md`

## 2026-09-18

- **CJK 传输链收敛批次**：14 处 latin1 构造改 UTF-8＋诊断透传＋L2-20 新用例；L1 单测方案纠偏回退；L1 243/243、19/20 与 live 25 步会话全绿。详见 `changelog/2026-09-18-log.md`
- **E2E 优化批次（计划驱动执行，8 分支/T00–T09，未提交）**：新增 6 域工具（域 385→391、SIDE 60→62、catalog 393→399）；保存回执+只读复核、脚本哈希自证、局部补丁、声明式建树试点、批量读/采样/断言/证据包/布局校验、错误码文本、绑定诊断、CJK 传输修复；L1 243/243、03 603/603、25 份 L2 干净态全绿。详见 `changelog/2026-09-18-log.md`
- **README 拆分与知识库一致性审计**：`README_zh.md` 删除，`README.md` 中文重写 + 新增 `README.en.md`（工具口径 badge `tools-385+` / 385+、前提 Godot 4.7+）；`register_all.cpp:363` 的 `call_tool` 描述 384→385 域对齐；wiki 同步 6 页（overview / build / tests / modules/core / modules/entry_runtime / example：README 引用、4.7 口径、行号重核）。详见 `changelog/2026-09-18-log.md`

## 2026-09-17

- **代码精简批次（纯删除零行为变化）**：27 文件 +104/−592，域工具 385 / SIDE 60 / catalog 393 不变；L1 267→243、ctest 注册点 292→268（构建 65/65、L1 243/243 全绿）；技能册计数 384→385 并同步 10 余页文档数字。详见 `changelog/2026-09-17-log.md`
- **客户端配置写入修复**：`write_file` 先递归创建父目录（修复 14 个含目录路径客户端首建失败）；pi 路径 `.pi/mcp.json`→`.mcp.json`（与 Command Code/CodeBuddy 共用，merge 幂等）。详见 `changelog/2026-09-17-log.md`
- **文档同步批次 T09+T12-docs（零代码）**：P2 释放 flush / P3 求值启发式 / P4 多签秒级失败 / P5 跨帧 undo / P7 文本点击与序号口径 / P8 改脚本工作流 / P12 登记状态如实返回等 7 项行为入 wiki；版本口径 godot-cpp rc1→rc2、mcp-cpp-sdk 0.3.3→0.3.4、compat 4.3→4.7；P12 回退恢复 fallback 与 `tab_rebuilt` 及 07/13 勘误。详见 `changelog/2026-09-17-log.md`

## 2026-09-16

- **坐标换算/键名统一/脚本新鲜度/UID 守卫/CJK 往返/open 幂等/属性两轮批次**：`click_game_ui_element` 窗口坐标换算（`window_position`/`viewport_position` 回显）与键名两侧统一判定、脚本 `fresh` 取用口径与 `cache_refreshed`、UID 按表成员选 `add_id`/`set_id`、文本链路全 `String::utf8`、`open_editor_scene` 幂等 `already_open`、`create_scene_node` 属性两轮应用；L1 246→267（新增 game_ui_coords 6 / keycode_alias 6 / script_freshness 7 / uid_guard 2）、L2 18→25 份（新增 18/19/22/23/24/25/28）、ctest 264→292；工具数 385/392/393、SIDE 60（325+60）；计数为静态核算、待构建运行复核；`window_position` 端到端断言待示例游戏恢复后补测。详见 `changelog/2026-09-16-log.md`
- **失败修复批次文档收尾（feature/failure-remediation）**：域工具 379→384（可达 391 / catalog 392，非 SIDE 324 / SIDE 60）、新增 5 工具（scene_tree_items / select_scene_tree_node / fill_tilemap_rect / start_game_job / get_game_job）、L1 172→229、L2 11→17 份、ctest 183→246；8 项行为变更已并入对应功能页（modules/tools_ops_a / modules/tools_ops_b / modules/entry_runtime / tests / modules/support）：eval 结构化错误、超时预算链与 late_results、batch_execute await_async、capture after_frames/when/scale、内联子资源、scene_path 回显、godot://skills 资源、get_game_status 无参；AGENTS.md / README 双语 / tests/README.md 与知识库 12 页同步。详见 `changelog/2026-09-16-log.md`
- **0.2.5 升版收尾同步**：根 `VERSION` 与 `AGENTS.md` 早在 09-14 已为 0.2.5，本次补齐遗漏页——`build.md` release validate 示例同步至 `v0.2.5` ↔ `0.2.5`；全库复核无其他滞留 0.2.4 现行版表述。详见 `changelog/2026-09-16-log.md`

## 2026-09-15

- **Computer Use grounding 增强（域工具 366→379）**：新增 13 个编辑器 UI 语义自动化 / 合成输入 / 坐标映射工具与游戏侧 `click_game_ui_element`，`capture_editor_viewport` 增 `region`/`max_dimension`/`space`/`annotate`/`diff_against_last`，游戏侧 wheel/`mouse_motion` 与 `op_capture` 增强；SIDE 49→57（`modifies_window` 12→19、`game_runtime` 5→6），MCP 可达 386 / catalog 387；L1 140→172（新增 EditorCoordsTest 32 项）、L2 9→11（09_editor_ui / 10_editor_input）、ctest 149→183；03 遍历 322 非 SIDE 工具 0 失败、warnings 4 条；GUI 验收 10/10。详见 `changelog/2026-09-15-log.md`

## 2026-09-14

- **CI Windows 构建编码修复**：`embed_skills.py` 中文 print 在 cp1252 控制台 `UnicodeEncodeError`（8/30 起 develop CI 连败根因），脚本 stdout/stderr 强制 UTF-8 + `ci.yml`/`release.yml` 增 `PYTHONUTF8=1`；本地 cp1252 复现环境验证通过，生成头与产物一致。详见 `changelog/2026-09-14-log.md`
- **0.2.5 修复批次（严格形状 / 异步语义 / 工具增强）**：Vector2/Vector2i 严格形状（数组输入明确报错、替代静默写 0）、`get_scene_tree` 属性摘要 String/StringName 判定修复、MCP Config 新增 Allow game_runtime 复选框、`batch_execute` 异步 game 工具返回 `status:"pending"`（不再假成功）、`capture_editor_viewport` 支持 `save`（编辑器/游戏两侧各保留最近 20 张）、`get_game_log_entries` 新增 `filter`/`matched_lines`；L1 126→140、L2 9 份、ctest 135→149。详见 `changelog/2026-09-14-log.md`

