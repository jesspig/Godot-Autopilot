# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-09-29

- **B1–B7 韧性与工具语义修复批次**：B1 韧性子系统（插件 `PROCESS_MODE_ALWAYS`、CommandQueue 任务取消/心跳、dispatch 等待预算与 `main_thread_timeout`、8 个 SceneTree 工具改游戏通道转发）；B2 严格 JSON 形状（Vector3/4、Plane、Quaternion、Basis、Projection、Color）与 dry_run 真校验；B3 `resource_fs` 统一根路径与 theme `verified` 回读；B4 子场景实例继承节点分离计数；B5 斜杠属性全名与属性族展开；B6 编辑器/游戏三通道统一 GDScript 包装；B7 就绪窗口排队重放与 `wait_ready`。实测 L1 391 gtest（36 文件）全绿、全量 L2 37/37 全绿、`ctest -N` 431（394 + 37），`VERSION` 0.2.7。详见 [2026-09-29-log.md](2026-09-29-log.md)
- **知识库与模板同步**：`tests.md`/`tests/README.md` 数值口径与用例清单（L2 30→37、遍历 393 枚举/71 排除/322 候选）、模板 `scene-system--property-json-shapes.md`（Plane 等严格形状、斜杠属性与族展开）与 `runtime.md`/`runtime--runtime-inspection.md`（就绪窗口排队语义）、项目技能 `project-diagnosing-with-traces`/`project-adding-domain-tools` 随 B1/B7 同步；床工程 `[audio]` 副作用本轮复现并记录。详见 [2026-09-29-log.md](2026-09-29-log.md)
- **插件技能书源改标准目录布局**：`src/util/skill_templates/`（平铺 + registry.json）迁为仓库根 `skills/<name>/SKILL.md + references/*.md`（调用专册另有 `scripts/gda_mcp.mjs`），正文逐字节保留，description 入各册 frontmatter，registry.json 删除；`embed_skills.py`/`skill_gen.cmake`/单测脚本/项目技能/wiki 引用同步，394/394 全绿。详见 [2026-09-29-log.md](2026-09-29-log.md)

## 2026-09-26

- **构建入口 main.py 化与 scripts/ 拆分 + TUI 新增**：`build.py` 删除，拆为 `main.py`（build/package/clean/tui）与 `scripts/` 6 模块，部署目标改 `demo/×5`（`--demos` 可选子集），`questionary` 入依赖；`AGENTS.md`/构建技能/CI/wiki 引用同步。详见 [2026-09-26-log.md](2026-09-26-log.md)
- **修 ctest：L2 专用测试床 `tests/testbed/`**：执行器原把编辑器起在已随 `6c50b99` 删除的 `Example/`，30 份 `gda_runner_*` 全灭；现改用新床（入库工程定义三件，含 `14_tilemap_rect` 依赖的 `Apple.png`，产物忽略），床路径收为单一权威并新增缺失前置校验（秒级退出码 2），deploy/clean 纳入床而 `--demos`/打包不变。同批移除 `22_uid_guard` 两道靠 `%APPDATA%` 残留才通过的 `get_game_log_entries` 伪守卫，并定位 `add_input_map_action` 声明为 `SideEffect::None` 却落盘、被 03 遍历污染床的问题。实测 348/348 全绿（514.54s）。详见 [2026-09-26-log.md](2026-09-26-log.md)

## 2026-09-25

- **项目级技能层落地 + `AGENTS.md` 退为最小入口**：新增 `.agents/skills/project-*/SKILL.md` 共 8 个（构建登记、加工具、写 handler、跑测试、写埋点、读 trace 排障、安全授权门、三层文档维护），与交付给客户端的 `godot-autopilot-*` 技能书按前缀隔离；`AGENTS.md` 由 23448 字符精简到 4044（只留定位、命令、6 条硬约束、三层分工、技能索引、协作）。删除前用三个只读子代理逐条核对覆盖度，并把独有事实与错号补进 wiki。详见 [2026-09-25-log.md](2026-09-25-log.md)
- **纠正三处失实记载**：`raw_schema` 手写实际仅 3 个元工具（非"7 元全手写"）；`Args` 取参器"可用但未接线"（`src/` 下 0 个领域 handler include，现网仍是 `args.Find` + `util::error_json` 约 490 处）；Host 环回校验改按当前 SDK 行为记载为**绑定层 + 请求层两层**，并指认 08-28 changelog 的"可覆盖为 `0.0.0.0`"已被拒绝启动行为取代。另记：授权门只覆盖 `process`/`code_execute`/`game_runtime`/`user_tools`，`writes_file` 等枚举不进授权门。
- **wiki 同步 7 页**：`index.md` 新增"三层文档分工""编写规则"两节并把技能纳入维护入口与页面索引；`conventions.md` 新增"注释与提交"段与 Godot 4.7 API 口径三条；`security_contract.md` §2 两层环回；`modules/core.md` 协议埋点接线；`tool_base_design.md` `error_code` 口径 + `Args` 接入状态；`tests.md`/`build.md` 行号与守卫输出修正、L2 时长与 googletest 缓存兜底。
- **验证**：9 个改动文件相对链接 0 断链；`comment_guard` OK；`migration_guard` 输出 `-- [migration-guard] OK (domains: 30, strict: on)`；零 C++ 与测试配置改动，未触发构建与 ctest 全量。
- **技能脚本通道文档同步（T13）**：skill 8→9 册（新增 `godot-autopilot-tools` 调用专册）、模板 33 md + 1 mjs + registry、references 22→24；L1 312→315、ctest 注册点 344→348（过滤 314→318，G1 实测 318/318 全绿）；同步 `support.md`/`tests.md`/`security_contract.md`（新增 §3.3）/`overview.md`/`build.md` 与双语 README（"MCP 为主产品，skill/脚本是体验层"）；只改 `.md`，未编译/测试/提交。Example 生成验证待 E2E 验证。详见 [2026-09-25-log.md](2026-09-25-log.md)
