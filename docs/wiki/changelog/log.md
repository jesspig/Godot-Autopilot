# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

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

## 2026-09-13

- **mcp-cpp-sdk 0.3.2 → 0.3.3**：`FetchDependencies.cmake` GIT_TAG 升级，重新 configure 拉取 0.3.3 并全量编译通过（无 API 适配），`uv run build.py` 部署通过，ctest L1 114/114 全绿；0.3.3 含 conformance/MRTR、Bearer 鉴权、FileEventStore 会话恢复、传输层死锁修复等（本仓库未启用新特性）。详见 `changelog/2026-09-13-log.md`
- **反馈修复批次全量交付**：property 数组/NodeType 转换与写失败 fail fast、资源 CoW/`reload_resource`/`copy_resource_file`/标签清理、`create_editor_scene` timeout_ms 诊断、调试器无 session 无回退描述、截图经 call_tool 以 image content 交付、技能 8 册（含 C# 专册与开发闭环）、`resolve_resource` path 注册；L1 114/114、L2 8/8，计数 365 域/49 副作用/316 遍历/373 全量。详见 `changelog/2026-09-13-log.md`
- **游戏通道根因修复与收口批次**：`call_tool` 改在 MCP 线程执行（修复主线程等待阻塞消息泵导致的 game_runtime 工具全超时）、新增 `get_plugin_log`（域工具 365→366、可达 373、catalog/index 374）、严格 JSON 形状与值类型 readback、授权门配置持久化 + MCP Config 复选框、运行时通道启动自检与超时诊断、`capture_editor_viewport target=game` 非阻塞化；L1 126、L2 9、ctest 135（L2 需 `GODOT_AUTOPILOT_ALLOW`）；遗留 mcp-cpp-sdk 客户端偶发等待缺陷待跟进。详见 `changelog/2026-09-13-log.md`
- **跨组一致性审计修复（A/B 两组 15 页）**：逐页核对并修复行号/计数/目录结构/过时引用（A 组含 `example.md` 整页重写）；跨组复核以工具 `category` 字段纠正类别分布（Physics 47/Debug 16/Debugger 6，27 类=366），同步 overview 类别表、README 系列（~365→~366、Debugger 5→6）与 McpConfigDock 技能 tooltip 7→8；清理 `Example/gda_tmp_rename/`、`~godot-autopilot_0.pdb` 残留；A/B 模块清单合计 180+186=366 复核通过，L1 126/L2 9/ctest 135。详见 `changelog/2026-09-13-log.md`
- **主 skill（GDA 使用指南）增强**：`godot-autopilot` description 改以"使用 GDA 插件前必读"开头（概括连接前置/搜索优先发现/版本文档查询/状态观测/任务路由/错误水印）；`autopilot.md` 新增 Search techniques、Consult the engine documentation、Choosing the right tool、Watch the engine state 四块内容，既有章节语义与顺序不变；support.md 同步、frontmatter timestamp 更新。详见 `changelog/2026-09-13-log.md`
- **skill 统一纯英文**：`godot-autopilot` description 改为英文必读定位（"Required reading before using the GDA (godot-autopilot) plugin"，其余 7 册不动）、`scene-system.md` 中文错误提示引用改英文转述；全库扫描确认 `src/util/skill_templates/` 其余文件无中文字符；support.md 同步。详见 `changelog/2026-09-13-log.md`
- **版本号 0.2.3 → 0.2.4**：根 `VERSION` 单一来源升版，AGENTS.md 与 build.md 示例同步；GDA_VERSION 注入与测试由主代理重新 configure/构建后验证。详见 `changelog/2026-09-13-log.md`
- **0.2.4 后知识库全量一致性审计（14 页）与源码文案同步**：5 组并行逐页核对/修复（工具页逐工具名集合复核 A 180 + B 186 = 366；overview/core/support/tools_registry/build/tests/example/roadmap 等行号与行为口径修正），同步源码侧旧文案（queue_game_input 描述与 skill 模板的 `ignored_params`、debug monitors 58→59）及 `tests/README.md` 行号；重建后 L1 126/126 全绿。详见 `changelog/2026-09-13-log.md`

