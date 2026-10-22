---
type: 路线图文档
title: 竞品对齐路线图
description: 面向主流 Godot MCP/AI 插件的竞品定位速览与本仓 P0-P3 批次交付状态
tags:
  - 规划
  - 竞品对齐
  - 工具扩展
timestamp: "2026-10-23T02:39:34+08:00"
resource: src/
---

# 竞品对齐路线图

> 记录 2026-08 功能分支 `feature/engine-aware-fs-and-tool-expansion` 的竞品对齐批次（P0/P1/P2 已交付）与后续 P3 方向，工具侧实现细节见 [工具注册表](../modules/tools_registry.md)，运行时协议见 [入口与运行时](../modules/entry_runtime.md)。**当前口径（2026-10-23）**：域工具 **451**（27 类 / 33 个源模块）、catalog 条目 459、MCP 可达 458、schema 基线 459 = 393 非空 + 66 空、遍历枚举 452 / 排除 151 / 候选 301、L1 **759** 个 gtest（67 个 unit 文件）、L2 **41** 份用例、守卫与脚本 6 项、**ctest 注册点 806**（759 + 41 + 6，`ctest -N` 实测）、CLI 命令 **14**、CLI TS 单测 **257** 项（16 个文件，Windows 跳过 2 项）、技能书 9 册 / 35 文件、根 `CMakeLists.txt` 的 `add_library` 收录 **113** 个 `.cpp`；各项的权威核算口径见 [测试体系](../tests.md) 的数值核算总表。下列各节的批次内计数是**该批次交付当时的增量**，不代表当前值。

## 竞品全景（一行定位速记）

| 项目 | 形态 | 一行定位 |
|---|---|---|
| [hi-godot/godot-ai](https://github.com/hi-godot/godot-ai) | Python FastMCP(HTTP) + WebSocket → 编辑器插件 | 约 43 工具/120+ op，主打客户端一键 attach 自动配置与 AssetLib/Snap 分发 |
| Godot MCP Pro | 商业 MCP 服务器 | 宣称 162 工具/23 类别，开箱即用面最广的付费方案 |
| GodotPilot | 社区开源 MCP（Node.js） | 以自然语言驱动编辑器启动/运行、场景节点与 GDScript 增删改查 |
| [Coding-Solo/godot-mcp](https://github.com/Coding-Solo/godot-mcp) | stdio MCP 服务器（npx 分发） | 社区使用最广的轻量方案：launch/run/调试输出捕获 + 打包 GDScript 操作脚本 |
| [IvanMurzak/Godot-MCP](https://github.com/IvanMurzak/Godot-MCP) | C# 编辑器插件 + 共享 MCP 服务端 | Unity-MCP 同源栈：42 工具/12 族，云托管（ai-game.dev）或自托管 GameDev-MCP-Server |

> 定位以各仓库 README 自述为准，功能数字随对方版本演进，仅作对齐参照不构成评测结论。

## 已交付：P0 文件操作收编

资源文件 CRUD 从"裸磁盘写"收编为引擎感知事务（`resource_ops.cpp`）：

- **新增** `move_resource_file` / `create_directory`（Resources 类 22→24；后续 09-13 批次增至 26）：目录级递归移动保布局，`.uid` sidecar 随行
- **`remove_resource_file` 两段式**：默认 dry-run 返回影响预检；`force=true` 才删，优先 OS 回收站（trashed/permanent 标注）
- **rename/move 补全 remap**：project.godot 的 main_scene/autoload 命中自动改写（保留 `*` 前缀），依赖 `.tscn/.tres` 内 path=/uid= token 重写，残留引用以结构化 `stale_references: {file, token}` 回传
- **`write_file` 引擎感知化**：res:// 内写入自动判定 reimport/update_file 并附脚本写入诊断（布尔级）；user:// 维持裸写并标 `engine_managed:false`

## 已交付：P1 AI 易用性

- **错误水印 doorbell**：所有 MCP 响应顶层附 `new_errors_since_last_call`（一次性消费）；编辑器侧计 error 结果、游戏侧计 runtime_error（`src/core/error_watermark.hpp`）
- **undo 接入**：create/delete/rename/reparent 场景节点全走 `EditorUndoRedoManager`（响应带 `undoable` 字段）；`property_set` 可撤销（Object 型值降级 `undoable:false`）；`batch_execute` 新增 `rollback_on_error`（按 EditorUndoRedo version 差值整批撤销）
- **readiness 门控**（`src/core/editor_readiness.{hpp,cpp}`）：导入/扫描进行中时 reimport 类调用返回 `retryable:true` 软错误（retry_after_ms=500），scan 幂等跳过
- **`code_execute` timeout 上界与派发预算同源**（越界值直接报错而非静默接受；此前 schema 声称上限但实现未校验）——上界取 `min(GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000, 生效长阻塞派发预算)`，默认环境下 27000ms，由 `code_exec_ops::max_timeout_ms()` 单一来源给出
- **BM25 中文检索**：分词器支持 CJK bigram（`src/util/bm25_index.cpp`），`search_tools` 中文查询可用
- **`capture_editor_viewport` target='game' 接通**：08-24 初始接通；09-13 下午改为非阻塞 pending 协议——返回 `{"__gda_pending": id, "timeout_ms": ...}`，由 `call_tool` 等待并经 `finalize_capture_response` 定型（阻塞式 `runtime_ops::game_capture_blocking` 已删除），`timeout_ms` 参数保留（默认 5000、钳制 30000）

## 已交付：P2 覆盖扩展

领域工具 336→**363**（本批净 +27，类别 23→27；后续 09-13 批次新增至 366，10-11 / 10-12 / 10-16 又经三批扩充至 **451**——当前值见[工具注册表](../modules/tools_registry.md)）：

- **Animation 域新设 10**：create_scene_animation_player / create_animation / remove_animation / get_animation_list / create_animation_track / insert_animation_keyframe / remove_animation_track / create_scene_animation_tree / add_animation_machine_state / connect_animation_states
- **Theme 域新设 8**：create_theme_resource / set_theme_color / set_theme_constant / set_theme_font_size / set_theme_stylebox_flat / get_theme_info / apply_theme_to_control / set_control_anchor_preset
- **Testing 域新设 2**：run_gdscript_tests / run_gdscript_test_files（内置断言框架 check/check_equal/check_almost_equal/fail/fatal）
- **Analysis 域新设 3**：validate_scene_file / find_unused_resources / trace_signal_flow
- **Scene 域 +2**：rename_scene_node / reparent_node；`create_scene_node` 支持 `properties` 参数
- **Game 域 +2**：sequence_game_inputs（逐物理帧时间线注入）/ get_game_ui_elements（运行中 Control 树枚举）
- **删除 2 死工具**：get_debugger_stack_dump / get_debugger_monitors（ScriptDebugger 未暴露给 GDExtension，数据源不可达）

## 已交付：Agent Skills 生成

- **skill_gen 一键生成 Agent Skills**（2026-09-08 交付首版 7 册，09-13 扩至 8 册，09-25 随技能脚本通道落地增至 **9 册**（`skills/` 36 文件 = 9 份 `SKILL.md` + 26 份 references + 1 个 `mjs` 桥），10-19 随独立二进制分发再扩为五级调用阶梯，10-20 随技能书自带的脚本桥删除收敛为**三级**（原生 MCP → GDA CLI → 手动兜底，`bunx`、全局安装与独立二进制合为 GDA CLI 一档的三种分发方式；`skills/` 36→**34** 文件 = 9 份 `SKILL.md` + 25 份 references）；生成器 + UI 按钮 + 7 用例 L1）：写入项目根 `.agents/skills/`，覆盖写仅限自有命名空间

## 已交付：Computer Use grounding 增强（09-15）

领域工具 366→**379**（净 +13；Editor 23→29、Input 11→15、Display 24→25、Scene 6→7、Game 9→10），MCP 可达 386 / catalog-index 387（**当前口径**：域工具 451 / catalog 459 / MCP 可达 458）：

- **编辑器 UI 语义自动化**：`get_editor_ui_elements`（元素枚举：语义 path/类型/文本/客户区矩形）与 `hit_test_editor_point`（客户区点命中链）负责定位，`click_editor_element` / `type_editor_element_text` / `run_editor_shortcut` 执行动作；原始坐标回退 `click_input_mouse` / `scroll_input_mouse` / `drag_input_mouse` / `type_input_text`（合成点击/滚轮/拖拽/文本）
- **坐标映射**：`get_editor_viewport_geometry`（截图像素↔客户区 `window = offset + image * scale`）、`get_scene_node_screen_rect`（编辑场景节点→客户区，2D/3D）、`get_display_window_rect`（窗口客户区矩形与屏幕几何）
- **截图增强**：`capture_editor_viewport` 新增 `region` / `max_dimension` / `space` / `annotate` / `diff_against_last`（与上次编辑器截图对比；尺寸不同按公共左上区域比较并标 `size_mismatch`；不可比较时 `reason` 为 `no_baseline`/`unsupported_format`/`baseline_too_large`）；游戏侧 `capture_game_viewport` 的 region/max_dimension/annotate 在游戏进程内执行
- **游戏侧输入与点击**：`queue_game_input` / `sequence_game_inputs` 增 wheel 与单步 mouse_motion；`click_game_ui_element` 按节点 path 在游戏进程内完成枚举 + 坐标注入（单次协议往返）
- **安全分级**：新增 8 个副作用工具全部标记（编辑器侧 7 个 `ModifiesWindow`、游戏侧 1 个 `GameRuntime`），未新增授权能力门；详见 [T0 安全边界与并发契约](../security_contract.md)

## 已交付：失败修复批次（09-16，feature/failure-remediation）

领域工具 379→**384**（净 +5；Editor 29→31、TileMap 7→8、Game 10→12），MCP 可达 391 / catalog-index 392；L1 172→229、L2 11→17 份、ctest 183→246（**当前口径**：域工具 451 / L1 458 / L2 41 份 / ctest 505）。行为变更（eval 编译错误结构化返回、game 工具超时预算链、batch_execute await_async、capture after_frames/when/scale、内联子资源、scene_path 回显、godot://skills 资源等 8 项）详见 [changelog/2026-09-16-log.md](../changelog/2026-09-16-log.md)：

- **编辑器场景树**：`scene_tree_items`（Scene dock 行枚举）/ `select_scene_tree_node`（按 path 选中并联动 Inspector）
- **瓦片地图**：`fill_tilemap_rect`（矩形铺砖/擦除，>100000 格报错、单次 undo）
- **长任务异步**：`start_game_job` / `get_game_job`（游戏脚本异步提交+轮询，job 表上限 16、过期宽限 2s，需 `game_runtime` 授权）
- **MCP 资源**：新增 `godot://skills` 目录/单册/单文件资源组（技能经协议层可发现）

## 已交付：批量工具扩充 + 构建生成器双路径（10-11）

- **域工具 392→443（净 +51）**：新增两个新域 `gridmap`（5 个，TileMap 类）与 `tween`（4 个，Game 类），`file_dir_ops` 新实现挂 `os` 域（源模块 30→32）；存量域同步扩容（animation +1 / audio +4 / config +4 / os +4 / game +4 / input_map +1 / physics +3 / property +2 / render +3 / resource +7 / scene +7 / theme +2）。
- **工具重构**：存量工具拆分为 `*_ops` 处理层与 `*_tools` 描述层（`ToolSpec` 数据化管线的延续）。
- **构建适配**：CMake 预设引入**生成器双路径**（有 ninja 用 `debug`/`release`，无则用 `debug-portable`/`release-portable`，共用 `build/{debug,release}`），并适配多配置生成器（MSVC `/Zc:lambda`、产物多一层配置目录、ctest 需 `-C`）。
- 详见 [changelog/2026-10-11-log.md](../changelog/2026-10-11-log.md)。

## 已交付：C# 桥补完（10-12）

- **C# 反射桥从起步到补完**：10-11 手写 C# 反射桥并新增三个薄工具（域工具 443→446，新域 `csharp`）；10-12 拆分绑定文件、补齐缺口（域工具 446→448，`csharp` 域 3→5，新增 `csharp_set_property` / `csharp_unsubscribe_event`），并完成构建同步、创建/发现/绑定与注册封装（ABIv2）。
- **运行时兜底与测试床**：桥兜底与数组类型修复、L2 38 号用例自包含化（去跟踪夹具）、首触排队与兜底命中日志。
- 10-12 未单独产出 changelog 文件，其事实记录在 [changelog/2026-10-16-log.md](../changelog/2026-10-16-log.md) 的 C# 桥根因段与 [工具注册表](../modules/tools_registry.md) 的审计日期头。

## 已交付：依赖升级 + C# 解析修复 + 计数复核（10-16）

- **mcp-cpp-sdk 0.3.4 → 0.3.7**（仅安全迁移，不启用新特性）：SDK 自 0.3.5 起删除 `JsonValue::FromObject` / `FromArray`，8 处调用点就地替换为 `JsonValue(JsonValue::Object{...})`，全仓复核归零。
- **`parse_csharp_classes` 基类解析缺陷修复**：类名与 `:` 之间未跳过空白（`Foo <T>` 泛型形态同样漏判），先红 6 失败后全绿。
- **计数复核**：域工具 448→**451**（+3：`csharp_list_members` / `create_csharp_script` / `dotnet_environment`），catalog 459、MCP 可达 458、schema 基线 459 = 393 非空 + 66 空、遍历枚举 452 / 排除 147 / 候选 305。
- 详见 [changelog/2026-10-16-log.md](../changelog/2026-10-16-log.md)。

## 已交付：CLI 载体落地（10-18）

- **新增 `cli/`**（Bun + TS 源码直跑，零运行时依赖、无构建步骤）+ 根 `package.json` / `bun.lock`；命令族 `gda`（调用族 6 + 生命周期 4），寻址链五级（`GODOT_AUTOPILOT_PORT` → cwd 登记 → trace → config → 9527），登记表 `instances.json` + 锁 + 死 pid 清理，生命周期 `start`（Godot 链 / 端口 9530–9599 / 导入预步 / 就绪探测 / Windows 蹦床脱父）/ `stop`（优雅优先，`--force` 强杀）。
- **协议握手升级**：CLI 改为标准 MCP `initialize` 流程（mcp-cpp-sdk 0.3.7 起严格要求完整握手）。
- **新增 `version_sync_guard` 守卫**，ctest 注册点 476→478。
- 详见 [changelog/2026-10-18-log.md](../changelog/2026-10-18-log.md)。

## 已交付：协议版本协商 + 独立二进制分发 + CI 三平台修复（10-19）

- **四条子分支合并**（基线 `d9b72c0`，HEAD `1285d8d`）：① SDK 0.3.7→0.3.8 + OpenSSL 三态开关取显式 `OFF`（必须带 `FORCE`）；② MCP 协议版本协商对齐（客户端声明 `2025-03-26`→`2025-11-25`，握手状态按目标 URL 键控，新增 `initialize` 响应版本学习）；③ CLI 跨平台（`GODOT_AUTOPILOT_USERDATA` 覆写、优雅停止用例去 Windows 专属语义）；④ MSVC `std::exception` 不拷贝消息导致的 `what()` 野悬根因定位与修复（`src/util/owned_exception.hpp`）。
- **独立二进制分发**：`release.yml` 新增 `cli` 作业，产出 5 个平台目标（`windows-x64` / `linux-x64` / `linux-x64-musl` / `macos-arm64` / `macos-x64`）+ `sha256sums.txt`；技能书调用阶梯扩为五级（新增「独立二进制」与「插件已装但未配 MCP client」触发条件），`skills/` 35→36 文件。
- **CLI 引擎定位收窄**：`gda start` 不再读 `GODOT_PATH` 与 `<project>/.env`，可执行文件链收敛为 `--godot` > PATH。
- 详见 [changelog/2026-10-19-log.md](../changelog/2026-10-19-log.md)。

## 已交付：执行预算与契约治理（10-20）

- **执行预算与超时治理**：`kLongBlocking` 由「无限等待」改为长阻塞预算档（常量 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS` = 29000，env `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 可下调至 `[1000, 29000]`）；`code_execute` 的 `timeout_ms` 上界取 `min(27000, 生效长阻塞派发预算)`（`code_exec_ops::max_timeout_ms()` 单一来源，schema `maximum` 与提示词同源）；`CommandQueue::execute_sync` 在 `src/` 内已无调用点，资源 / 提示词 / 动态工具 / 游戏通道的等待全部改走带预算的 `submit_tracked_bounded`；新增 `code_execute` 协作式执行期超时（`code="exec_timeout"`）与游戏侧 eval 超时 / 取消（`eval_timeout` / `eval_cancelled`）；无编辑器队列时返回 `queue_unavailable` 结构化错误。
- **工具契约强制**：`code_execute` 与 `batch_execute` 的 `side_effect` 由 `None` 改为 `CodeExecute`——**`batch_execute` 由此受 `code_execute` 能力门保护，属对外行为变更**；能力门判定去掉工具名特判，只按 `side_effect` 统一判定；`side_effect_name(None)` 由空串改为 `"none"`，任何工具的 `side_effect` 都非空（L1 + L2 双层断言锁定）。
- **主线程契约守卫**：新增 `tests/guard/main_thread_guard.py` 并注册 ctest 用例；trace 截图落盘与轨迹写盘统一到主线程持锁通道；`ExportGuard` 改实例级标志并补齐异常安全复位，导出期拒绝覆盖工具调用 / MCP 资源 / MCP 提示词三类入口。
- **发布产物完整性**：新增 `scripts/test_package_integrity.py`（三平台库 / C# 桥源码 / gdextension 条目 / 无 `.pdb`）并接入 CI 构建后与发布前；`addons.zip` 纳入 C# 桥源码并统一两条打包路径口径。
- **CLI 中危修复**：启动就绪探测失败终止进程并清理 pending 日志；登记表比较统一 `path.resolve` 且握手状态可失效；顶层 `error` 信封映射为工具失败；移除未实现的 `--quiet`；`stop` 支持 `--id` / `--port` 并复用统一选择器校验。
- **知识库收敛**：安全契约页两条与代码冲突的断言（`FileAccess` 落盘线程、`call_tool` 是否写 trace）按修复后事实重写；退役整页指向已删除目录的示例工程页；本页补记 10-11 / 10-12 / 10-16 / 10-18 / 10-19 五个缺失批次。
- 详见 [changelog/2026-10-20-log.md](../changelog/2026-10-20-log.md) 与 [T0 安全边界与并发契约](../security_contract.md)。

## 已交付：控制面二期（0.3.0 内完成）

控制面二期随提交 `4aeee67` 进入 CLI，命令总数 12→**14**；E-1a / E-1b / E-1c 三项均落地：

- **E-1a `gda restart`**：定位规则与 `stop` 完全一致（位置参数 `<id>`，或 `--id` / `--port` / `--project` 三选一），只作用于 CLI 本地注册表里的实例；流程为「优雅停止（`--force` 才强杀）→ 固定等待 10 秒（**不受 `--wait` 影响**）→ 从注册表移除旧条目 → 按原端口 / 原 headless / 原 godot 路径重新拉起 → 输出 `previous` 与新实例视图」。停止阶段超时**不启动替代实例**，避免同一工程同时开两个编辑器（扩展加载期可能静默崩溃）。
- **E-1b 多实例路由**：`/i/<实例 ID 或端口>/api[/…]` 与 `/i/<ref>/mcp` 精确转发；缺省 `/api/...` 与 `/mcp` 在唯一活跃实例时自动转发，多个活跃返回 **409 `instance_ambiguous`**（附 `candidates`），零个返回 **503 `no_active_instance`**；目标不可达 **503 `instance_unavailable`**，上游连接失败 **502 `upstream_unreachable`**，上游超时 **504 `upstream_timeout`**。
- **E-1c 双面转发**：控制面默认 `127.0.0.1:9700`，`--bind` 只接受环回地址；`GET /control/health` 免认证，`GET /control/instances` 需 Bearer 且**不返回 `apiToken`**，其余路由全部需认证；令牌文件 `<集中根>/control.token`（32 字节随机值的十六进制、`0600`，已存在则复用）；转发时丢弃客户端 `Authorization` 与逐跳头并注入目标实例令牌，响应加 `X-GDA-Instance` 并边到边流式透传（MCP SSE 可直接穿过）。
- **不做项（明确决定）**：不提供 HTTP 启停端点，控制面只做只读查询与转发；启停仍只在 CLI 侧（`start` / `stop` / `restart`）。理由是把本机进程控制暴露成 HTTP 端点，会让任何能访问该端口的客户端获得启停编辑器的能力，风险大于收益。
- **验证与口径**：`bun test cli/tests` 实测 **255 pass / 2 skip / 0 fail**（257 项 / 16 文件，Windows 跳过 2 项，193→257）；路由与认证细节见 [CLI 载体](../modules/cli.md)「控制面（`gda serve`）」，端口分工见 [端口与实例登记](../modules/ports.md)「控制面端口」。

## 遗留：P3 待规划

- **客户端桥接深化**：一键配置已交付——配置面板现支持 20 个客户端（09-16 由 8 扩至 20，见[支撑模块](../modules/support.md)），对标 godot-ai 的 attach 桥与"Configure all"全家桶仍余 stdio bridge、状态点探测差距
- **AssetLib 上架**：当前经 GitHub Release zip 分发（`uv run main.py package`），未进 Godot Asset Library

## 出站链接

- [工具注册表](../modules/tools_registry.md)（类别分布与数值核算总表）
- [支撑模块](../modules/support.md)（客户端配置生成现状）
- [入口与运行时桥接](../modules/entry_runtime.md)（input_sequence/ui_elements 协议 op）
- [T0 安全边界与并发契约](../security_contract.md)（监听、风险工具、能力门、主线程与生命周期）
- [测试体系](../tests.md)（数值核算总表，L1/L2/守卫与 ctest 注册点口径）
