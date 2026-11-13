---
type: 测试体系指南
title: 测试体系
description: 程序化四层测试体系（单元/集成/回归属性/守卫）+ Agent e2e、集成层进程内边界、回归属性基线与数值统计
tags:
  - 测试
  - 单元测试
  - 集成
  - 守卫
timestamp: "2026-11-13T23:15:35+08:00"
resource: tests/
---

# 测试体系（tests/）

> 审计日期：2026-11-14（T-03：search_tools 新增 limit 的 5 组集成用例；T-06：技能书发现契约断言改为 gda search/describe/tools list；注册点 935）2026-11-13（T-01 依赖升级：`register_all_test.cpp` 的重注册用例改按 SDK 0.3.11 语义断言「同 server 重注册抛 `McpError`」，数量不变；完整数值重基线与总表更新由 T-02 执行）2026-11-09（**随测试体系四层化重构逐项重基线**——L2 引擎轨（`gda_test_runner` / `gda_pool_ctl` / `tests/config` / `tests/testbed` / `tests/pool`）整体删除（提交 `c023a5c`，2026-11-09），口径由「L1/L2 双层」改为「程序化四层（单元 / 集成 / 回归属性 / 守卫）+ Agent e2e」；本轮实测：单元测试 **875** 不变（75 文件）、集成层 **109**（12 文件）、守卫与脚本项 **10**、ctest 注册总数 **994 = 875 + 109 + 10**（`ctest -N` 实测，证据 `.handoff/evidence/T-08/ctest-N-after.txt`），同提交独立 worktree 全量 ctest **100% 通过**（`.handoff/evidence/T-08/manifest.md`）；CLI TS 单测 **296** 项 / 18 文件不变；脚本编排收敛为默认门语义（提交 `bf31e8a` / `8997db9`）。被删章节的历史结论以一行迁移说明指回 changelog 或 git 历史，见「历史口径迁移」）

## 架构总览

测试由根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 引入（`CMakeLists.txt:100`）。该开关已固化在 `CMakePresets.json` 的 `base-debug`/`base-release` 基座（cacheVariables 默认 `ON`），并经 `inherits` 下发给全部 4 个可执行 configure 预设（`debug`/`debug-portable`/`release`/`release-portable`），**清理/重建 `build/` 后重新配置（`bun run gda-dev-tools build` 或 `cmake --preset debug`）自动恢复测试目标，无需手动传参**；直接以裸 `cmake`（不带 preset）配置时仍为默认 OFF。

**三层 + 守卫与脚本项**（全部零引擎、零 Godot 进程；注册总数以 `ctest --preset debug -N` 实测为准——2026-11-13 新增 `no_python_guard` 与自测两条，本页未重跑）：

| 层 | ctest 载体 | 数量 | 标签 | 判定方式 | 验证对象 |
|---|---|---|---|---|---|
| 单元 | `gda_unit_tests`（gtest，`gtest_discover_tests` 逐条注册） | 636（53 文件） | `unit` | gtest 断言 | 合法输入下的模块行为：纯逻辑、编解码、注册与查询 |
| 边界 | `gda_boundary_tests`（gtest，同上） | 239（22 文件） | `boundary` | gtest 断言 | 非法 / 极端输入与拒绝路径：参数校验、错误映射、预算超时、门控守卫 |
| 集成 | `gda_integration_tests`（gtest，同上） | 109（12 文件） | `integration` | gtest 断言 | 进程内 inline 派发的 handler 契约（参数校验 / 授权门 / 错误串 / 注册面），零引擎零项目绑定 |
| 守卫与脚本项 | `tests/CMakeLists.txt` 的 `add_test`（含 `no_python_guard`、源码守卫、守卫自测、`cli_contract` / `cli_unit`） | 以 `ctest -N` 为准 | `guard` / `cli` | 脚本退出码 | 源码纪律、无 Python 回退与 CLI wire 形状 |
| 合计 | `ctest -N` | 以实测为准 | — | — | — |

**回归属性**不是独立 ctest 目标，而是分布在单元 / 集成 / 脚本项内的基线断言组，钉住注册面、错误面、wire 形状与技能面，防止工具集与协议漂移；基线清单与更新 SOP 见「回归属性清单」。

**Agent e2e**：依赖真实编辑器 / Godot 进程的行为验证（GDScript 编译执行、游戏进程通道、端到端 HTTP 往返、编辑器态错误文案）不再有仓库内引擎轨，移交 `gda-e2e-testing` 工作流执行、`gda-e2e-analysis` 分析闭环（根 `.agents/skills/`）；仓库测试基建（configure + build + ctest）不含任何 Godot 进程路径。

## 构建与运行

```powershell
# 配置（测试开关已固化在预设，无需传参；下述命令为显式写法）
cmake --preset debug

# 构建
cmake --build --preset debug --target gda_unit_tests gda_integration_tests

# 全量运行（默认门：994 条注册点全跑，无过滤、无引擎前置）
ctest --preset debug

# 集成层单跑（ctest 正则匹配「套件.用例」名）
ctest --preset debug -R "MetaToolsSuite|EditorInputSuite|AuthorizationGate"

# gtest 直跑（不经 ctest，支持 --gtest_filter / --gtest_list_tests）
build\debug\tests\gda_integration_tests.exe --gtest_filter=MetaToolsSuite.*

# devtools 封装（Bun/TS）
bun run gda-dev-tools test                 # 全量默认门（ctest --preset <resolved>，无过滤）
bun run gda-dev-tools test --single migration_guard   # 单跑（ctest -R）
```

上列 `debug` / `release` 预设**固定 Ninja 生成器**，本机 PATH 无 ninja 时须换成 `debug-portable` / `release-portable`（无 ninja 回退路径见 [build.md](./build.md)「预设」）。走 `bun run gda-dev-tools test` 则由 `resolvePreset()`（`devtools/lib/config.ts`）自动选路，`--preset` 选项仍只有 `debug` / `release` 两个。四个 preset 共用同一 `binaryDir`，测试目标与可执行文件位置均为 `build/debug/` 或 `build/release/`，不受生成器选择影响。

- **依赖**：googletest 为 `third_party/googletest` 子模块（tag `v1.15.2`，commit `b514bdc898e2951020cbdca1304b75f5950d1f59`），随仓库检出（`git clone --recurse-submodules` 或 `git submodule update --init --recursive`），由 `tests/CMakeLists.txt:9-15` 复用 `gda_require_submodule` + `add_subdirectory` 引入，缺失即 configure 报错、不做网络回退；godot-cpp 与 mcp-cpp-sdk 同为子模块，单元与集成目标均链接（集成目标另链接 `mcp-http`，`tests/CMakeLists.txt:107-108`）。**不需要 Godot 可执行文件、不需要 `.env` / `GODOT_PATH`、不需要部署插件**——仓库测试链路无任何引擎拉起路径。
- **测试侧构建目标**：`gda_unit_tests` / `gda_boundary_tests` / `gda_integration_tests`（ctest 常驻，分别打 `unit` / `boundary` / `integration` 标签）与 `gda_mcp_transport_probe`（`tests/probe/mcp_transport_probe.cpp`，传输自检程序，**不注册为 ctest 用例**、不计入 994，由主代理验收阶段按需执行）。

### tests/ 目录布局

| 目录 | 内容 |
|---|---|
| `tests/unit/` | 单元测试层（53 个 `*_test.cpp`，glob 自动收集，标签 `unit`） |
| `tests/boundary/` | 边界测试层（22 个 `*_test.cpp`，参数校验 / 拒绝路径 / 预算超时 / 错误映射，标签 `boundary`） |
| `tests/integration/` | 集成测试层（`harness.hpp` 夹具 + 12 个 `*_test.cpp`，标签 `integration`） |
| `tests/support/` | `config_loader.{cpp,hpp}`（单元与边界共用的测试支撑源；原 `tests/runner/` 目录其余文件已随引擎轨删除） |
| `tests/guard/` | 源码守卫（`migration_guard.cmake` + Bun/TS 守卫）与 `migrated_domains.txt` |
| `tests/probe/` | `mcp_transport_probe.cpp`（传输探针，非 ctest） |
| `tests/CMakeLists.txt` | 三个 gtest 目标 + `add_test` 守卫/脚本项（含 `no_python_guard`） |
| `tests/README.md` | 使用说明（数值一律以本页为准） |

## 三层划分

按**测试性质**而非执行等级划分三层，三层的标签可直接用 `ctest -L <label>` 单独执行：

| 层 | 标签 | 目标 | 判据 |
|---|---|---|---|
| 单元测试 | `unit` | `gda_unit_tests` | 合法输入下的模块行为：纯逻辑、数据结构、编解码、注册与查询 |
| 边界测试 | `boundary` | `gda_boundary_tests` | 非法 / 极端输入与拒绝路径：参数校验、错误映射、预算与超时、门控与守卫、可容忍降级 |
| 集成测试 | `integration` | `gda_integration_tests` | 跨模块调用链：注入 `CommandQueue` 与处理器表后经 `dispatch` 驱动元工具与域工具 |

划分依据是**文件主旨**（文件名与断言主体共同表达的意图），不是单条用例的输入形态——一个单元测试文件里出现个别边界断言属正常，不因此改判。新增测试先按判据选目录，再用对应标签验证能被 `ctest -L` 筛到。

## 单元测试层

**53 个测试文件，636 个用例**（`ctest -L unit -N` 实测；源码 `TEST`/`TEST_F` 宏口径见数值总表；历史批次计数演变见本页 git 历史与 [changelog/](changelog/) 各日记录）：

| 文件 | 数量 | 主题 |
|---|---|---|
| `bm25_index_test.cpp` | 20 | BM25 检索：命中排序、分类/标签过滤、空查询、上限截断 |
| `register_all_test.cpp` | 26 | 注册管线：7 元工具 ListTools 往返、catalog 覆盖、schema 基线、未知工具错误、同 server 重注册拒绝（SDK 0.3.11 语义）、双服务器一致性；`tool.side_effect` 可读性断言与「全量工具详情 `side_effect` 非空」断言 |
| `client_config_gen_test.cpp` | 20 | 20 客户端配置文件渲染（URL/type/enabled/顶层键/ZCode 双层嵌套/Crush `$schema`/共享 `.mcp.json` 三客户端同构/Reasonix `[[plugins]]`）、JSON 合并三态（新建/保留其他键/非法）、TOML 追加与跳过（Codex 形态与 Reasonix 形态） |
| `command_queue_test.cpp` | 16 | 跨线程 submit/drain、异常经 future 传播、主线程记录；`submit_tracked`/`cancel`（命中/已执行/未知 id）与 drain 心跳 `last_drain_age_ms`；带预算 submit 在预算内完成、超时取消待执行任务、取消计数递增，owner 线程内联执行、预算非正时仍等完成 |
| `tool_catalog_test.cpp` | 6 | ToolCatalog：add/get、重复覆盖、并发安全 |
| `log_system_test.cpp` | 9 | 单例、级别/分类/文本过滤、环形缓冲覆盖、值拷贝快照与 recent 上限；`trace_id`/`span_id` 关联字段与 `filter_text` 匹配 detail/`trace_id`/`span_id` |
| `tool_registry_test.cpp` | 6 | `SpecTool`/ToolRegistry：add/find/categories、execute echo、`side_effect_of`、`kMeta` flags 归类、flags 路由与 `make_tool_info` 的 dynamic/mutating |
| `schema_builder_test.cpp` | 5 | schema 构造形状、required 位置、未知类型不崩溃 |
| `runtime_ops_test.cpp` | 12 | 编辑器队列注入往返、`payload_is_pending` 识别（非对象/非整数拒绝）、`discard_pending` 未命中返回 false |
| `security_parallel_hardening_test.cpp` | 21 | 安全与并行硬化：队列关闭/满/线程校验、路径/鉴权、配置与截断、日志并发 |
| `skill_gen_test.cpp` | 10 | 技能生成器：10 册清单（含 setup）、name/description 规范、自包含 SKILL.md 布局、frontmatter 渲染、CLI-first 发现契约（`SkillBooksDescribeDiscovery` / `CliChannelContract`）、脚本桥防复活守卫（`SkillBooksCarryNoScriptBridge`）、反引号词回验（详见下节） |
| `mcp_image_content_test.cpp` | 11 | 截图 MCP image content（header-only `src/util/mcp_image_content.hpp`）：3 个 capture_* 白名单、顶层/嵌套 data 提取与优先级、data 替换为 `<attached-as-image-content>`、非白名单/非对象/缺 data/非 png 不注入 |
| `log_ops_test.cpp` | 4 | 游戏日志过滤（`log_ops::filter_log_lines`）：空 filter 返回全部、子串命中、保留末尾 limit 条、limit ≤0 返回全部命中 |
| `editor_coords_test.cpp` | 15 | 编辑器坐标纯逻辑（`src/core/editor_coords.hpp/cpp`）：`fit_within` 缩放、`scale_size` 缩放链、截图 `diff_sample` 采样比较与 bbox |
| `editor_output_decode_test.cpp` | 8 | 控制台输出解码纯逻辑（`decode_console_output`）：UTF-8 中文与 ASCII/空串原样通过、GBK 字节转为可读 UTF-8、Windows 下 ACP 预解码乱码回环还原（CP936「姝ｅ湪」→「正在」）、纯 CJK 文本不被 ACP 回环改写、非法与截断字节替换 `?` 不崩溃、严格 UTF-8 拒绝超长/代理/越界编码（非 Windows 或 ACP 非 CP936 时 `GTEST_SKIP`） |
| `editor_ui_tree_test.cpp` | 6 | 编辑器场景树行纯逻辑（`editor_ui_ops` 侧）：filter 匹配（空过滤/大小写不敏感跨 path 与 name/selected_only）、`max_items` 钳制上限 1000、ASCII 折叠仅限 ASCII 字母、相对路径分段拼接 |
| `inline_resource_json_test.cpp` | 12 | 资源属性内联描述解析：`{"type": ..., "properties": {...}}` 一步创建 `[sub_resource]` 的解析/校验与错误路径 |
| `skill_resources_test.cpp` | 13 | `godot://skills` MCP 资源解析：目录/单册/单文件 URI 解析与未知 URI 错误；`ScriptMimeType`（`.md`→`text/markdown`；实现侧 `skill_resources.hpp` 仍保留 `.mjs`→`text/javascript` 映射，当前无技能使用该扩展名） |
| `tilemap_ops_test.cpp` | 13 | `fill_tilemap_rect` 矩形铺砖纯逻辑：角点归一/格数上限（>100000 报错）/擦除分支与 set_count 统计 |
| `vision_assist_test.cpp` | 7 | 视觉辅助截图参数纯逻辑：`scale`/`max_dimension` 组合封顶与小图不放大、`scale` 越界拒绝、`region` 左上交集裁剪、`diff_image` bbox 与阈值噪声忽略、`annotate_nodes_max` 先于绘制强制 |
| `game_response_envelope_test.cpp` | 6 | 游戏回包信封纯逻辑（`runtime/game_response_envelope.hpp`）：标量负载顶层抬升 `wrapped`、数组负载留在 `result` 下并顶层带 `next_seq`、对象负载合并传输字段不改写、负载自有值优先于传输字段、缺传输字段不改形状、非对象游戏回包原样返回 |
| `game_ui_coords_test.cpp` | 6 | `click_game_ui_element` 注入坐标换算纯逻辑（`core/editor_coords.hpp`）：4x 拉伸、letterbox 边距、画布平移复合、rect 中心映射 |
| `keycode_alias_test.cpp` | 6 | 键名→键码统一判定纯逻辑（`input_map_ops::resolve_key_name_code`）：裸名/`KEY_` 前缀/大小写不敏感/数字码等价、未知名判无效 |
| `script_freshness_test.cpp` | 7 | 脚本取用口径纯逻辑（`script_ops.cpp`）：`script_load_cache_mode` 缓存模式选择、`wants_fresh_load` 只认字面 true |
| `gdscript_wrap_test.cpp` | 29 | GDScript 包装层纯逻辑（`src/util/gdscript_wrap.hpp`）：裸语句包装（多行/注释与 `extends` 剥离/tab 缩进保持/混用缩进拒绝/顶层函数定义拒绝）、单表达式自动 return 与入口名、`wrap_whole_script` 入口校验、`map_error_line_numbers` 偏移映射与多命中、`strip_extends_lines` 行数保持 |
| `tool_pipeline_traits_test.cpp` | 4 | traits 图片附件判定纯逻辑：顶层 data 附加 image content、非 png/缺 data 拒绝、无 flag 透传（有 flag 的场景补全需编辑器，单元测试未覆盖） |
| `tool_invoke_test.cpp` | 4 | 工具间组合调用纯逻辑（`tools::invoke_tool`）：未知/成功透传、异步 pending 嵌套拦截、深度守卫（导出分支无 setter，单元测试未覆盖，注释已声明） |
| `batch_refs_test.cpp` | 7 | `batch_execute` 最小变量串联纯逻辑：`$prev` 链、`$steps[N].result` 嵌套替换与字面量保持原样、前向引用/首操作 `$prev`/状态错配报错前缀、stop_on_error 截断 |
| `trace_recorder_test.cpp` | 18 | `TraceRecorder` 纯逻辑：id 前缀/唯一性、seq 严格递增、`query_recent` 尾取与 0/超量边界、`query_by_trace` 过滤、`query_since` 边界与 `next_seq`、`clear_for_test` 重置、脱敏剥离敏感键值保留其他键、脱敏/原始两种截断口径、FNV-1a 稳定值与区分、base64 解码、`to_json_line` 核心键与转义；`TraceKind`/`trace_kind_name`、schema v2 新字段与敏感键通用集合、`query_by_request` |
| `log_persist_test.cpp` | 10 | `LogPersist` 纯函数：`format_human_line` 三态、`session_stamp_from_ticks` UTC 转换、`should_prune` 边界（>20 文件或 >50MB）、`level_name`/`category_name` 全枚举与未知值；健康计数访问器与 `health_summary`、`trace_dir`；`health_summary` 可观测键与非主线程 trace 图片缓冲落盘、脱敏时返回空 |
| `monitor_test.cpp` | 9 | 统一埋点门面 `monitor` 纯逻辑：`trace_kind_name` 全枚举、`RequestRegistry` 的 put/get/take/size/clear 与 `expire` 边界、`emit` 严格递增 seq 与默认填充（`thread_id`/`monotonic_ns`）、`tool_call` 失败态与人类日志按 trace_id 可检索、`begin_request`/`end_request` 经 traceparent 关联、`push_request`/`pop_request`/`RequestScope`、`build_attrs`/`json_escape` 转义、`sanitize_field` 截断与大小写不敏感剥离 |
| `perf_sampler_test.cpp` | 6 | 性能采样纯函数（仅 header 内联部分）：`accumulate_frame`（负 delta 归零与峰值跟踪）、`average_frame_ms`（空样本为 0）、`compute_rates`（`seconds<=0` 全 0 与手算窗口的吞吐/错误率/平均延迟）、零响应/零样本时比率归 0、`milliseconds_to_nanoseconds` 换算；`tick`/`set_queue`/超时看门狗依赖引擎与 `perf_sampler.cpp`，单元测试未覆盖 |
| `array_script_branch_test.cpp` | 10 | 类型化数组元素的分支判定纯逻辑：空类不判型、原生类走原生分支、全局脚本类走脚本分支、未知类报不可解析、原生/脚本不匹配的双类名报错；`parse_hint_class` 裸名保持、去类型前缀、逗号截断与空白裁剪、空串产出空 |
| `coords_inverse_test.cpp` | 6 | 坐标反解往返纯逻辑：恒等与拉伸往返还原视口点、letterbox 边距先减后反缩、矩形中心往返、singular 与近 singular 变换回退输入点 |
| `metadata_key_test.cpp` | 8 | 元数据键路由与校验纯逻辑：前缀名路由 metadata、裸名保持 property、键合法性校验（空键/`..`/禁用字符拒绝）、detail 四要素文案与契约字面量 |
| `text_ops_csharp_test.cpp` | 13 | C# 类声明解析纯逻辑（`text_ops.hpp` 导出的 `parse_csharp_classes`）：空格与紧贴冒号基类、泛型列表前空格与泛型后基类、tab/换行空白、缺基类留空、限定名取尾段、注释与字符串噪声、多类返回、partial 检测 |
| `api_server_test.cpp` | 34 | REST 端点面纯逻辑（`src/core/api_surface.{hpp,cpp}`）：路由匹配（8 条端点 + 未知路由 + 方法不匹配 + 非法工具名）、工具名字符集校验、清单过滤（`query`/`category`/`tags`）与分页窗口（`limit`/`offset`）、批量请求体解析（`operations`/`stop_on_error`/上限）、工具清单信封形状、错误码→状态码映射、OpenAPI 文档、认证（缺令牌 401 / health 免认证）、工具调用（参数对象即请求体、非对象体 400、工具失败映射状态码）、批量逐项执行与早停 |
| `api_endpoints_test.cpp` | 3 | 端点表纯逻辑（`src/core/api_endpoints.{hpp,cpp}`）：REST 路由 `GET /api/endpoints` 与 MCP 侧资源 `godot://endpoints` 共用同一份端点表文档（实例 id / 版本 / 进程号 / 工程路径 / 端点条目逐项一致）、缺令牌 401 `unauthorized`、端点路由登记与 `GET /api` 索引公示 |
| `api_execute_chain_test.cpp` | 4 | REST 调用统一执行链纯逻辑（`src/core/api_surface.{hpp,cpp}` + `src/tools/tool_execute.{hpp,cpp}`）：导出期短路 503 `export_in_progress`、授权拒绝短路 403 `authorization_required`、未知工具 404 `tool_not_found`、缺令牌 401 |
| `api_envelope_test.cpp` | 15 | 错误信封归一纯逻辑（`src/core/api_envelope.{hpp,cpp}`）：`normalize_code` 的取值优先级（顶层 `code` → `authorization_required` → `structured_error.code` → 文案特征推断）与 `status_for` 的码→状态码映射；信封追加 `code`/`warnings`/`tool`/`trace_id`/`request_id`；无 `error` 字段的结果不加信封 |
| `api_auth_test.cpp` | 10 | REST 认证纯逻辑（`src/core/transport/api_auth.cpp` + `src/core/transport/api_token.cpp`）：Bearer 头解析（大小写、缺头、空令牌）、常量时间比较（相等/不等/长度不同/空串）、令牌来源优先级（环境变量覆盖令牌文件）、签发失败时的降级路径 |
| `authorization_dock_toggle_test.cpp` | 29 | 面板授权开关纯逻辑（`src/tools/base/authorization.hpp` + `src/ui/authorization_switches.hpp`）：`write` / `destructive` / `process` 属有面板开关的能力且开关集合恰为**六项**（`read` 无开关）、`process` 开关文案说明外部进程执行、勾选与取消后的允许列表内容（取消一项不影响其它项、`all` 先展开再逐项删）、允许列表每次调用实时读取不缓存且环境变量优先、拒绝文案对 `write` / `destructive` / `process` 给出面板指引 |
| `http_transport_test.cpp` | 47 | 自建 HTTP 传输纯逻辑（`src/core/http_transport.{hpp,cpp}` + `http_parse.hpp`）：请求行/头块解析上限、查询串解析、`Content-Length` 与请求体上限（4 MiB → 413）、路由表匹配、百分号解码；传输轮转公平性与流式收口用例 |
| `mcp_http_shim_test.cpp` | 36 | MCP HTTP 适配层纯逻辑（`src/core/mcp_http_shim.{hpp,cpp}`）：请求/响应与 MCP 消息的映射、SSE 与 JSON 双通道、错误帧转换 |
| `mcp_http_shim_dispatch_test.cpp` | 8 | MCP 适配层与派发的接线纯逻辑：请求分类与派发入口的对应关系、初始化通知转发 |
| `storage_layout_test.cpp` | 7 | 存储布局路径纯逻辑（`src/core/storage/storage_paths.cpp` + `project_store.cpp`）：全局配置路径、工程目录名（名称替换 + 截断 + 路径哈希）、工程级配置路径、日志/轨迹/截图目录路径 |
| `legacy_compat_test.cpp` | 15 | 遗留数据兼容纯逻辑（`src/core/storage/legacy_store.cpp`）：旧目录定位、配置项回退选取（新路径优先、缺键回退、解析失败回退）、相对路径分类（全局配置/日志/轨迹/截图/不支持）、复制式导入规划与执行（不覆盖、不删源、跳过计数）、导入摘要文本 |
| `gda_home_test.cpp` | 13 | 家目录集中根解析纯逻辑（`src/core/storage/gda_home.cpp`）：四级来源的候选顺序、形状校验与存在性校验的差异、覆写值的直返语义、平台分支（Windows / POSIX） |
| `project_store_test.cpp` | 8 | 存储根解析纯逻辑（`src/core/storage/project_store.cpp`）：集中根目录名、工程目录名生成、路径哈希稳定性与截断、全局/工程配置与实例索引路径 |
| `instance_registry_test.cpp` | 6 | 实例登记纯逻辑（`src/core/storage/instance_registry.cpp`）：单实例记录读写、记录结构（端口/进程号/端点/令牌/版本）、全局索引重建与条目跳过 |
| `dotnet_env_test.cpp` | 6 | .NET 环境判定纯逻辑（`src/util/dotnet_env.hpp`）：特性名版本切换（4.4 起 `dotnet` / 4.4 之前 `mono`）、`CSharpScript` 类注册即 .NET 构建（不受特性名影响）、4.4+ 只认 `dotnet` 特性名、4.4 之前只认 `mono` 特性名、版本未知取二者或、普通编辑器判否 |
| `http_stream_registry_test.cpp` | 3 | HTTP 流式注册表纯逻辑（`src/core/http_stream_registry.{hpp,cpp}`）：Sink 帧数超上限拒绝写入、close 后不再接收帧、多连接广播的拒绝计数 |
| `godot_transport_pending_test.cpp` | 3 | Godot 传输 pending 键纯逻辑（`src/core/godot_transport.{hpp,cpp}`）：复合键区分连接与请求 id、同输入键稳定（幂等）、int 与 string 请求 id 前后缀区分不串扰 |
| **合计** | **875** | |

**链接来源**（`tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`，源清单经 `cmake/sources/` 下 7 份清单聚合：`gda_unit_sources.cmake` 30 个业务源（含 `tests/runner/config_loader.cpp`——tests/ 侧唯一业务源）+ 3 个登记的测试文件本体 + 1 组 `GDA_TOOLS_OPS_SOURCES` glob；`api_unit_transport.cmake` 11 / `api_unit_contract.cmake` 3 / `api_unit_storage.cmake` 2 / `api_unit_instance_guard.cmake` 1，另在同名清单登记 18 个测试文件本体便于评审核对；`api_unit_cli.cmake` / `api_unit_docs.cmake` 现无登记项。合计显式业务源 47 + 1 组 glob）：`src/core/` 的 `log_system`、`log_persist`、`resource_registry`、`scene_dirty_tracker`、`export_guard`、`editor_readiness`、`editor_coords`、`trace_recorder`、`monitor`、`gda_home`、`project_store`、`instance_registry`、`storage_paths`、`legacy_store`、`instance_guard`；`src/util/` 的 `bm25_index`、`error_util`、`readback_util`、`scene_verify`、`variant_json`、`client_config_gen`、`skill_gen`、`skill_content_generated`（构建期嵌入薄胶水）；`src/tools/` 的 `tool_catalog`、`schema_builder`、`register_all`、`dispatch`、`debugger_access`、`editor_ui_actions`、`tool_args`、`tool_pipeline`、`tool_invoke`、`tool_execute`、`argument_validator`；传输与 API 面的 `src/core/port_resolver`、`http_route_table`、`http_stream_registry`、`godot_transport`、`mcp_http_shim`、`api_surface`、`api_endpoints`、`api_server_routes`、`api_auth`、`api_token`、`api_envelope`；`src/resources/resource_error_mapping`；`src/tools/domains/*/*_ops.cpp`（glob，`register_all.cpp` 引用全部 `handle_xxx` 符号故必须链接）。`gdscript_wrap`/`ready_policy` 等 header-only 头由对应测试直接 include，无需源登记。

**约束**：单元测试禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃；handler 契约由集成层在受控边界内覆盖，见「集成层」）；`register_all_test` 仅测注册/分发/未知工具错误路径。`SchemaStatisticsBaseline`（`register_all_test.cpp:136-152`）为运行时统计：当前基线断言 `tools.size()==459`（catalog 459 = 451 域 + `system_status` + 7 元）= 非空 393 + 空 66（`:147-149` 三条硬断言；随工具集变化，以运行时统计为准）。`client_config_gen` 为纯 C++（仅 std + `mcp::JsonValue`），不触碰 Godot API，可安全纳入 单元测试。`sanitize_policy.cpp` 未列入 单元与边界测试业务源——`enabled()`/`set_enabled()` 为 header 内联原子缓存（单元测试 经 `TraceRecorder::sanitize_args(text, bool)` 直接传参断言脱敏口径），`initialize()` 仅由入口调用、不参与 单元测试 链接。`monitor.cpp` 显式列入 单元与边界测试业务源（`monitor_test` 覆盖）；`monitor_env.cpp` 仅主线程调用、`perf_sampler.cpp` 依赖引擎 tick，二者不进 单元测试。

### skill_gen 技能生成器测试（`skill_gen_test.cpp`）

生成器 `src/util/skill_gen.cpp`；10 册正文存于仓库根 `skills/` 标准目录（10 个自包含 `SKILL.md`，name/description 存 frontmatter；聚合顺序为目录名字序），经 `tools/embed_skills.py` 构建期嵌入为生成头 `skill_content_embedded.h`（入 `build/<preset>/generated/`，gitignore 覆盖），渲染目标 `.agents/skills/<name>/SKILL.md`。纯 C++ 不触碰 Godot API，单元测试 可跑无需引擎。10 个用例：

| 用例 | 内容 |
|---|---|
| `AllTenSkillsPresent` | 10 册 name 定稿清单逐一核对（数量 = 10，双向集合比对：多出与缺失均 FAIL） |
| `SkillNamesFollowSpecRules` | name 匹配 `^[a-z0-9]+(-[a-z0-9]+)*$`、≤64 字符、路径 `.agents/skills/<name>/SKILL.md` |
| `DescriptionsValid` | description 非空、≤1024 字符 |
| `FilesLayout` | `files[0]` 必为 SKILL.md，其余须 `references/` 或 `scripts/` 前缀（`scripts/` 仅直属 `.mjs`）；body 非空、无 PLACEHOLDER 残留 |
| `CliChannelContract` | CLI-first 通道契约：全部 SKILL.md 禁 `bunx`/`9527`/`gda serve`/`gda api`/`Allow write`/`curl -`/`Invoke-WebRequest`；调用专册须含 `gda search`、`gda describe`、`gda tools list`、`gda call` 与 `confirmation_required` |
| `RenderedFrontmatterWellFormed` | 渲染产物 frontmatter 完整：name/author/`version`（取 `GDA_VERSION`）/description 含 `: ` 必加引号/`metadata:` 与收尾 `---` |
| `SkillRegistryFixture.ToolNamesExistInRegistry` | 工具名回验（机制见下） |
| `SkillRegistryFixture.SkillBooksDescribeDiscovery` | CLI-first 发现契约：`godot-autopilot-tools` 正文须含 `gda search`、`gda describe`、`gda tools list`、`gda call`、`confirmation_required` 与 `new_errors_since_last_call`，防技能书退回旧调用阶梯 |
| `EverySkillHasSkillMd` | 每册恰 1 个自包含 `SKILL.md`（files.size() == 1，files[0] 必为 `SKILL.md`） |

**工具名回验机制**：`SkillRegistryFixture` 照抄 `register_all_test.cpp` 的服务端构造方式（`register_all_tools` 填充 catalog，仅注册不调用任何 handler），允许集 = 运行时 catalog 全部工具名 ∪ 各工具 `input_schema.properties` 参数名 ∪ 253 项手动白名单（`kManualWhitelist`，`skill_gen_test.cpp:40-109` 逐项统计，均为明确非工具名的词：Godot API 方法、Godot 属性名、内部机制/返回字段、枚举值等）；随后提取各册正文反引号包裹的全小写含下划线词逐一比对（``` 围栏代码块整体跳过），未命中即 FAIL。保证技能文档中出现的工具名/参数名与运行时注册始终一致。

## 边界测试层

**22 个测试文件，239 个用例**（`ctest -L boundary -N` 实测）。判据见「三层划分」：断言主体是非法 / 极端输入下的确定性行为。

| 文件 | 主题 |
|---|---|
| `argument_validator_test.cpp` | 参数校验器：类型/必填/未知键的拒绝路径与错误消息 |
| `authorization_mode_test.cpp` | 授权模式（strict/legacy）下的放行与拒绝差异 |
| `authorization_policy_test.cpp` | 能力门判定：读/写/危险/破坏四档的拒绝路径 |
| `config_loader_strict_test.cpp` | 严格配置加载：缺键、非法值、类型错误的拒绝 |
| `destructive_tier_guard_test.cpp` | 破坏类档位判定标准的防漂移扫描 |
| `dispatch_budget_test.cpp` | 主线程等待预算与队列不可用返回 |
| `dispatch_queue_unavailable_test.cpp` | 无编辑器队列时的兜底错误 |
| `dispatch_semantics_test.cpp` | 派发语义：导出拦截、工具存在性、参数校验的失败序 |
| `error_cursor_test.cpp` | 调试器错误游标的排他与越界行为 |
| `error_path_log_test.cpp` | 错误路径日志的字段与分类 |
| `error_util_test.cpp` | 错误 JSON 形状与结果包装 |
| `exec_budget_test.cpp` | 执行预算与超时上限的钳制 |
| `export_guard_test.cpp` | 导出期拒绝三类入口 |
| `instance_guard_test.cpp` | 同工程实例冲突判定 |
| `ready_policy_test.cpp` | 就绪门控与未就绪失败路径 |
| `resource_error_mapping_test.cpp` | 资源错误到错误码的映射 |
| `resource_gate_test.cpp` | 资源门控与拒绝条件 |
| `tool_args_test.cpp` | 取参器：缺失/类型错误/null 语义/未知键拒绝 |
| `tool_execute_test.cpp` | 统一执行入口：导出拦截 → 存在性 → 校验 → 授权的短路 |
| `transport_lifecycle_test.cpp` | 线程退出的阶段判定与预算比较 |
| `uid_guard_test.cpp` | UID 写选择守卫的表成员判定 |
| `variant_json_strict_test.cpp` | 严格 JSON 形状回读：别名冲突、缺字段、越界的拒绝 |

## 集成层（进程内契约测试）

**`gda_integration_tests`，12 个 `*_test.cpp`，109 个 TEST**。由旧 L2 引擎轨的 48 份 config JSON 蒸馏而来：错误串逐字等价、结构键与值一致、env 注入等价复刻旧 runner `requires` 语义；不可进程内复现的引擎触达断言划回 Agent e2e（逐条台账见各对照表的 R-01 条目）。

### 目标与边界

- **验证对象**：handler 层契约——参数校验错误串（逐字）、授权门两态（默认拒绝 / 放行）、元工具语义、注册面与排除划分契约、运行期通道的非引擎前置路径。
- **调用路径**：harness 经 `queue.drain()` 把测试线程登记为主线程，`call(name, args)` 直达 `dispatch::call_handler`（owner 线程 inline 旁路）。与生产 MCP 流量的 handler 层等价：`call_tool` 元工具 → `execute_tool_core` = `dispatch::call_handler` 纯转发（核实见 `.handoff/evidence/T-01/callpath-finding.md`；中央参数校验器与授权门属 REST/`tools::execute_tool` 链，MCP 流量不经此链）。
- **零引擎零项目绑定**：不启动编辑器与 Godot 进程、不读写工程、无端口监听（MCP 传输为 `InMemoryTransport` 配对；`register_all_tools` 的 9527 仅作注册参数）。
- **不覆盖（划回 Agent e2e）**：真实 GDScript 编译执行、游戏进程行为、编辑器态错误文案（`game not running: …` 全串）、HTTP socket 往返——判据见下「进程内边界」。

### 夹具结构（`tests/integration/harness.hpp`）

- **`IntegrationHarness`**：`SetUp` 构造 `InMemoryTransport::CreatePair()` → `McpServer::Create(pair.server)` → `register_all_tools(server, queue, catalog, index, 9527)` → `queue.drain()`（登记 owner 线程，此后 handler 全部 inline 执行）→ `runtime_ops::set_editor_queue(&queue)`；`TearDown` 摘除 editor 队列 → `dispatch::clear_handlers()` → 关闭 server，用例间零残留。
- **`IntegrationHarnessWithAllow(allow_value)`**：`SetUp` 注入 `GODOT_AUTOPILOT_ALLOW`（保存并在 `TearDown` 恢复原值），等价旧 runner 的 `requires` 授权语义；默认 `IntegrationHarness`（不注入）= 严格模式默认拒绝态（read 恒放行）。
- **业务源与 单元测试 同源**：集成目标 = `integration/*_test.cpp` glob + `GDA_UNIT_BUSINESS_SOURCES`（剔除 `tests/unit/` 测试文件），另链接 `mcp-http`（`tests/CMakeLists.txt:93-120`）。
- **注册**：`file(GLOB integration/*_test.cpp CONFIGURE_DEPENDS)` + `gtest_discover_tests`——新增 `*_test.cpp` 后重新 configure 即逐 TEST 注册为 ctest 用例，无需改 CMake。

### 套件覆盖清单（109 = 试点 20 + 校验类 43 + 通道契约 30 + 工具面 16）

| 文件 | fixture（授权 env） | 数量 | 覆盖内容 |
|---|---|---|---|
| `harness_smoke_test.cpp` | `IntegrationHarness`（默认拒绝） | 1 | 冒烟：`search_tools` 空 query 枚举非空 |
| `meta_tools_test.cpp` | `MetaToolsSuite`（`code_execute`） | 12 | 7 元工具语义：ping / search_tools（关键词、分类、标签三查）/ list_categories / get_tool_detail（命中、缺失、缺参）/ call_tool（未知、缺参）/ batch_execute（成功、含失败）；code_execute 三步不迁（真实脚本执行，划回 e2e） |
| `editor_input_validation_test.cpp` | `EditorInputSuite`（`write`） | 6 | 编辑器输入/UI 动作参数校验错误串：click 缺 position、scroll 非法 direction、drag 缺 to、type 缺 text、click_editor_element 缺 path、run_editor_shortcut 未知键——全部在注入前返回 error，无副作用 |
| `authorization_gate_test.cpp` | `AuthorizationGate`（`none`） | 1 | 授权门默认拒绝态：write 档 `create_scene_node` 在授权门（先于 handler 参数校验）被拒——结构化拒绝响应全串 + `code=authorization_required` + `authorization_required=write` |
| `capture_params_validation_test.cpp` | `CaptureParamsSuite`（`write`） | 13 | capture 参数校验：`after_frames` 负数/非整数、`when` 表达式错误码、`scale` 越界、`save`/`save_dir` 类型与 `res://../` 逃逸、`user://` 拒绝；工程外绝对路径一步不迁（`ProjectSettings::get_singleton()` 边界，见 R-04-01） |
| `game_jobs_validation_test.cpp` | `GameJobsSuite`（`game_runtime`） | 9 | `start_game_job`/`get_game_job` 参数校验：op 白名单、缺 params、timeout 上限 25000（`GDA_MAX_GAME_OP_TIMEOUT_MS`）、未知 job 四段式错误、cancel 类型；真实 job 生命周期划回 e2e |
| `vision_assist_validation_test.cpp` | `VisionAssistSuite`（`write`） | 21 | 视觉辅助参数校验：`annotate_nodes` 类型/空/越界、`diff_image` 组合、`review_*` 全参数负例；`diff_image requires diff_against_last` 一步不迁（`EditorInterface::get_singleton()` 边界，见 R-04-02） |
| `click_ui_coords_validation_test.cpp` | `ClickUiCoordsSuite`（`game_runtime`） | 11 | `click_game_ui_element` 缺参/越界/未知参数精确全串（含 em dash 四段式）、`queue_game_input`/`sequence_game_inputs` 的 `coord_space` 校验（先于通道）；无游戏通道三步不迁（编辑器态文案，见 R-01-01） |
| `queue_game_input_keycode_alias_test.cpp` | `KeycodeAliasSuite`（`game_runtime`） | 2 | `queue_game_input` 键名入参接受路径（error+hint 非空）；编辑器侧 `add_input_map_action(_event)` 九步不迁（`InputMap::get_singleton()` 边界，见 R-01-02；键名判决由 单元测试 `keycode_alias_test` 纯函数覆盖） |
| `scene_tree_gate_test.cpp` | `SceneTreeGateDenied`（`none`）/ `SceneTreeGateAllowed`（`game_runtime`） | 16 | 8 个 SceneTree 工具 × 两态：授权拒绝（`authorization required` 全串）与运行期通道错误（进程内恒为 `debugger capture plugin not initialized`，非空断言满足 JSON 声明；辅助断言区分两态）；场景搭建与编辑器树回读五步不迁（真实引擎，见 R-01-03） |
| `tool_detail_contract_test.cpp` | `ContractAssertSuite`（`code_execute`） | 1 | `get_tool_detail` 契约：`code_execute` 的 `tool.side_effect` 序列化值 `"code_execute"`；执行步不迁（真实脚本执行，见 R-01-05） |
| `toolface_contract_test.cpp` | `IntegrationHarness`（默认拒绝） | 16 | 工具面动态枚举契约：契约 4 + 排除划分自洽 1 + 空参实调 11 组（见「回归属性清单」） |

> 步骤级对照表（源 JSON expect ↔ gtest 断言 ↔ 等价结论）与 R-01 台账（引擎触达剔除记录）：`.handoff/evidence/T-02/mapping.md`（试点）、`T-04/mapping-12-16-17-39.md`、`T-05/mapping-23-24-35-37-40.md`、`T-06/mapping-toolface.md`。合并定稿「A 类迁移对照表」与引擎行为验收清单已落盘 wiki（[engine-behavior-checklist.md](engine-behavior-checklist.md) 的「A 类迁移对照表」小节，含已迁逐例对照与未迁用例划回 e2e 汇总），本页不复制对照表全文。

### 运行与排查

- **ctest**：`ctest --preset debug -R <正则>`（正则匹配「套件.用例」名，如 `-R "MetaToolsSuite"`、`-R "capture_params|CaptureParams"`）；`bun run gda-dev-tools test --single <name>` 等价走 `ctest -R`。
- **gtest 直跑**：`build\<preset>\tests\gda_integration_tests.exe`，配 `--gtest_filter=<Suite>.<Test>`（支持通配）与 `--gtest_list_tests` 枚举；单文件定位时先 `--gtest_filter=IntegrationHarness.*` 类推。
- **排查**：进程内 SEH（0xc0000005）或挂死几乎必属「进程内边界」清单所列形态，先对照该节与 `.handoff/evidence/` 各 mapping 的 R-01 台账再定位；一组 TEST 崩溃不影响其他组（`gtest_discover_tests` 逐 TEST 独立进程）。

### 进程内边界（硬边界）

1. **栈上 godot 对象构造**：handler 在参数校验**前**构造含 `godot::Variant` / `godot::Ref<…>` 的栈对象 → 测试进程 SEH——本仓 godot-cpp 的 `Variant()`/`~Variant()` 无条件调用 gdextension 接口指针，测试进程 GDExtension 接口未初始化即空函数指针调用（实证：`handle_code_execute` 的 `ExecContext`，`.handoff/evidence/T-02/mapping.md` R-01-03）。2026-11-10 起，`VariantJsonStrictTest.*Accepts*` 的 12 个用例按此边界从程序化测试中移除（由 e2e 覆盖）。
2. **引擎单例**：`godot::<Class>::get_singleton()` 同为硬边界（`ProjectSettings`/`EditorInterface`/`InputMap` 等，生成 getter 走同一接口指针；`.handoff/evidence/T-04/mapping-12-16-17-39.md` R-04-01、`T-05/mapping-23-24-35-37-40.md` R-01-02）。凡依赖上述两条路径的断言不可迁集成层，已划回 e2e（对照表各 R-01 条目）。
3. **运行期通道错误文案**：通道错误依赖编辑器插件实例（`DebugCapturePlugin::s_instance` 仅编辑器入口置位），进程内恒为 `debugger capture plugin not initialized`——凡以 `game not running: …` 等编辑器态精确全串为 expect 的断言不可迁（进程内无法复现该文案）；JSON 仅声明 error 非空的步骤可迁（进程内通道错误满足声明）。
4. **HTTP socket 层**：传输栈绑定 Godot 引擎 `TCPServer`/`StreamPeerTCP`，进程内无法监听（判定记录 `.handoff/evidence/T-03/verdict.md`：degrade；FR-03 降级决议草案待用户确认）——集成层无真实 HTTP 监听试点，HTTP 解析/shim 头校验/路由的纯逻辑面由 单元测试 与集成层分担，端到端 HTTP 往返随 `gda-e2e-testing` 自然发生。
5. **`tests/runner/` 残留口径**：仅保留 `config_loader.{cpp,hpp}`（单元与边界测试业务源，`gda_unit_sources.cmake:41` 登记、`config_loader_strict_test` 8 项测之；偏离登记见 `.handoff/evidence/T-08/manifest.md`），其余 runner 文件已随引擎轨删除。

## 回归属性清单

「回归属性」是分布在单元 / 集成 / 脚本项内的基线断言组（非独立 ctest 目标），每条基线钉住一个防漂移面：

| 基线 | 载体 | 当前值 | 断言内容 |
|---|---|---|---|
| 单元测试 schema 基线 | `register_all_test.cpp` 的 `SchemaStatisticsBaseline` | 459 = 393 非空 + 66 空 | `tools.size()==459` / `non_empty==393` / `empty==66` 三条硬断言（`register_all_test.cpp:147-149`） |
| 工具面动态枚举契约 | `toolface_contract_test.cpp`（集成层） | 全动态（无硬编码工具名单） | ① detail 契约：对运行时枚举的全部工具逐一断言响应为对象、`tool.name` 与枚举名一致、`input_schema` 为对象、`side_effect`/`mutating`/`dynamic` 存在且类型可读；② 排除划分自洽：候选∩排除=∅、计数和与全覆盖三重复算；③ 空参实调 11 组（`side_effect` 9 档位 + `mutating` + `dynamic`）：档位组断言授权拒绝三形态（error 含 `authorization required`、`code=authorization_required`、`authorization_required=<capability>`），Mutating/Dynamic 组断言缺参语义。枚举/排除/候选计数为观察值（`GTEST_SUCCEED` 输出），**不作断言钉**；未知名 detail 返回 `tool not found`（合成探针名） |
| 错误全串契约 | `tests/integration/case_*_test.cpp` 各套件（集成层） | 逐字等价（错误串与 handler 源码字符串字面量一致，含 em dash 与引号转义） | 各 case TEST 对错误串的逐字断言；步骤级对照表在 `.handoff/evidence/` 各 mapping |
| CLI wire 形状 | `cli_contract`（脚本项） | — | 进程内 mock MCP 服务器（严格握手：`initialize` 200 SSE、`notifications/initialized` 202、未初始化 `tools/call` 400 + `-32600`）驱动 gda CLI，断言 stdout JSON、退出码（0 成功 / 1 工具错 / 2 传输错 / 3 用法错）、直连 vs `call_tool` 包装 wire、`--save-images` 落盘；PATH 无 `bun` 时 SKIP 退出 0 |
| skill 白名单 | `skill_gen_test.cpp` 的 `SkillRegistryFixture`（单元测试层） | `kManualWhitelist` 覆盖状态/CLI 字段与 Godot API 词 | 工具名回验（正文反引号词 ∪ catalog 工具名 ∪ 参数名逐一比对）；发现契约（`SkillBooksDescribeDiscovery`）替代静态工具目录 |

### 基线更新 SOP

| 变更 | 动作 | 改后必跑 |
|---|---|---|
| 新增 / 删除 / 改参域工具 | 同步 schema 基线三条硬断言为新值；toolface 契约与 skill 回验无需改（动态枚举 / 运行时 catalog 驱动）；技能书用发现契约（`SkillBooksDescribeDiscovery`）而非静态工具目录 | `ctest -R "SchemaStatisticsBaseline|SkillRegistryFixture"`，收口跑 `bun run gda-dev-tools test` |
| 新增 `SideEffect` 档位枚举值 | toolface 契约新增对应档位分组 TEST（9 档位名单来自枚举运行时值全集，非名单硬编码） | `ctest -R "EmptyArgCallRejects"` |
| 改任何域工具错误文案 | 先查 `.handoff/evidence/` 各 mapping 对照表定位钉住该串的 case TEST，同步修改断言 | `ctest -R "case_"`（或对应套件） |
| 改 CLI 协议 / 握手 / 输出形状 | 同步 `cli/tests/contract.test.ts` 的 mock server 断言 | `ctest -R cli_contract` |
| 技能书正文新增词汇误伤回验 | 确认非工具名/参数名后加入 `kManualWhitelist` 并归入语义类别 | `ctest -R "SkillGenTest|SkillRegistryFixture"` |

## 守卫与脚本项（tests/guard/，ctest 常驻）

10 条 `add_test`（`tests/CMakeLists.txt`），全部零引擎、判定时机为 ctest 而非编译：

- `migration_guard`（`migration_guard.cmake` + `migrated_domains.txt`，30 域 + `strict`）：零依赖源码断言，防旧宏/`schema_*_ops.cpp`/`tool_input_schema` 回退；实测输出 `-- [migration-guard] OK (domains: 30, strict: on)`；迁移规则见 [ToolSpec 数据层与执行管线](tool_base_design.md)。
- `comment_guard`（`comment_guard.ts`）：递归扫描 `src/` 与 `tests/` 下 `*.cpp/*.hpp`（`SCAN_DIRS`，只认这两棵目录的 C++ 文件），除 `// namespace` 结尾标记外禁止任何 `//`/`/*…*/` 注释（正确处理字符串/字符字面量与 `R"TAG(…)TAG"` raw string）；无违规打印 `comment-guard OK` 并退出 0，违规退出 1，文件无法解码退出 2。
- `no_python_guard`（`no_python_guard.ts`）：**无 Python 守卫**，扫描全仓 `*.py`、`uv run`、`python main.py`、`find_package(Python3`、`GDA_PYTHON_EXECUTABLE`、CI 的 `setup-python`/`PYTHONUTF8` 与 `pyproject.toml`/`uv.lock`/`.python-version`；历史 changelog、运维日志与设计文档按「文件 + 规则 + 行内特征」白名单豁免并附理由。退出码：0 无残留、1 有违规（逐条打印 `文件:行号: [规则]`）、2 读失败或参数错误。`--self-test` 以临时目录注入违规样本验证守卫本身能报错。
- `main_thread_guard`（`main_thread_guard.ts`）：**主线程契约守卫**，静态扫描 `src/` 下 `*.cpp/*.hpp` 里的 **Godot API 直接调用**。判定依据是「线程上下文分类」——每个源文件被显式归入主线程（`main`）、受管（`managed`）或外线程（`foreign`）三类（`THREAD_CONTEXT_RULES`），外线程文件里的 Godot API 调用默认违规，只有落在该文件显式登记的主线程作用域里才放行（`FOREIGN_MAIN_THREAD_SCOPES`，当前只登记 `src/core/observability/log_persist.cpp` 的 6 个主线程作用域）。词法顺序：先剥离字符串字面量、字符字面量与 raw string，再剥离注释，最后做标识符匹配（`GODOT_API_PATTERNS` 覆盖 `godot::FileAccess` / `DirAccess` / `EditorInterface` / `Engine` / `SceneTree` / `ClassDB` / `ProjectSettings` / `Input` / `DisplayServer` / `OS` / `Performance` / `Resource` / `ResourceLoader` / `ResourceSaver` 等）。退出码：0 无违规（`main-thread-guard OK`）、1 有违规（逐行打印 `文件:行号: 片段`）、2 文件读或解码失败；可选参数为待扫描目录，缺省仓库 `src/`；只用标准库。
- `version_sync_guard`（`guard/version_sync_guard.ts`）：版本号一致性——根 `README*.md` 的版本钉必须与根 `VERSION` 同值，另含 `VERSION` 与 `CLI_VERSION` 两个值源；`skills/` 与 `skills-zh/` 是交付技能书，**禁止**出现版本钉或 Git ref（技能书不钉版本）。非版本引用（如 `#develop`）只登记、不参与比对。退出码：0 全部一致、1 存在版本漂移、2 关键文件缺钉 / 技能书出现版本钉 / 未知引用形态 / 文件读失败；覆盖范围以守卫脚本为准。
- `source_manifest_guard`（`source_manifest_guard.ts`）：**源清单守卫**——断言 测试源清单引用的 `src/` 源 ⊆ 插件源并集，防「漏登根 `CMakeLists.txt` / 清单文件导致编译单元不进产物」的静默缺失（解析规则见该脚本模块 docstring）。
- `skills_guard`（`guard/skills_guard.ts`，Bun 可用时注册；不可用时由 `cli_unit` 的 `bun test cli/tests` 覆盖同一实现）：扫描 `skills/` 与 `skills-zh/` 全部文本，失败逐条输出 `文件:行号: [规则] 说明`。规则包括旧版本钉、`bunx`、`9527`、`gda serve`、`gda api`、`Allow write`、REST、配置面板与 allow-list；提取 `gda` 命令与参数并对照 `cli/src/commands/index.ts`、`cli/src/args.ts`；校验 `gda status --json` 示例字段来自 `cli/src/commands/status.ts`；校验 EN/ZH 相对路径、frontmatter 与 10 册数量一致。
- `skills_guard_self_test`（同一脚本 `--self-test`）：注入未知册、旧关键词、未知命令、未知 status 字段、多文件与缺册反例，验证护栏本身能报错；Bun 可用时与 `skills_guard` 一起注册。
- 守卫自测项：`migration_guard_self_test` / `comment_guard_self_test` / `no_python_guard_self_test` / `main_thread_guard_self_test` / `tool_domain_guard_self_test` / `skills_guard_self_test`——以注入违规样本的方式验证源码/技能守卫自身能报出违规（守卫的守卫）；`migration_guard_self_test` 带 `RESOURCE_LOCK` 防与其他守卫并发。
- `cli_contract`（`cli/tests/contract.test.ts`）：CLI 契约 mock conformance，详见「回归属性清单」的 CLI wire 形状行；仓库内不写盘（图片落临时目录）。
- `cli_unit`（`tests/CMakeLists.txt` 直接执行 `bun test cli/tests`，cwd 为仓库根，TIMEOUT 300）：`cmake -E env` 注入 `GDA_TEST_TMPDIR` 与 `TMP`/`TEMP`/`TMPDIR`；PATH 无 bun 时注册 `cmake -E echo` 的 SKIP 用例并 exit 0（`cli_contract` 同此 SKIP 语义）。TS 用例 18 文件 296 项（2026-10-30 实测 294 pass / 2 skip / 0 fail；文件清单见「数值核算总表」CLI 行；参数化循环在运行期展开，运行期项数以 `bun test cli/tests` 实测为准）。
- 上述守卫与脚本项与两层 gtest 目标合为默认门：`ctest --preset debug`（或 `bun run gda-dev-tools test`）**无过滤全跑**——引擎轨删除后不再需要 `-E "^gda_runner_|^pool_"` 之类的排除模式。

## 数值核算总表

本页是全仓数值的**唯一权威落点**。表中「命令与证据位置」给出可复核的复算命令或证据文件；`AGENTS.md`、技能层、其它 wiki 页与 `tests/README.md` 只引用本页，不复制数值。基线日 2026-11-09（develop @ `0dad560`）。

| 条目 | 权威口径（2026-11-09 重基线） | 命令与证据位置 | 结论 |
|---|---|---|---|
| 域工具 | 451（33 个域） | 命令：逐文件统计 `src/tools/domains/*/*_tools.hpp` 的 `make_spec_tool(`——33 个域头（34 个匹配文件中 `autopilot_tools.hpp` 不含工具）合计 450 处 + `property_tools.hpp:102` 的 `make_set_node_transform_spec()` 特例 1 处 = 451；`src/` 与 develop 零差异（`.handoff/evidence/T-09/src-diff-empty.txt`） | 一致 |
| 工具类别 | 27 | 451 条 `ToolSpec` 聚合初始化的第 3 个字段（category）去重后 27 个取值 | 一致 |
| catalog 条目 | 459 = 451 域 + `system_status` + 7 元 | 命令：构建后 `ctest -R SchemaStatisticsBaseline`；源码：`register_all.cpp:329` 单独注册 `system_status`，`ToolRegistry::all()` = 452、`all_meta()` = 7、`all_any()` = 459；单元测试 硬断言 `register_all_test.cpp:147` | 一致 |
| MCP 可达工具 | 458 = 7 元 + 451 域 | MCP 顶层只注册 7 个元工具（单元测试断言 `register_all_test.cpp:84-86`：`ListTools` 恰 7 个），域工具只能经 `call_tool` 元工具代理调用；`system_status` 只计入 catalog 459 | 一致 |
| schema 基线 | 459 = 393 非空 + 66 空 | 单元测试三条硬断言 `register_all_test.cpp:147-149`；命令：构建后 `ctest -R SchemaStatisticsBaseline` | 一致 |
| 单元 | 636（53 个 unit 文件） | 命令：`ctest --preset debug -N -L unit`（2026-11-09 三层划分后实测 636）；源码口径 `grep -cE "^TEST(_F)?\(" tests/unit/*.cpp` | 一致 |
| 边界 | 239（22 个 boundary 文件） | 命令：`ctest --preset debug -N -L boundary`（实测 239）；源码口径 `grep -cE "^TEST(_F)?\(" tests/boundary/*.cpp` | 一致 |
| 集成 | 109（12 个 `*_test.cpp`） | 命令：`grep -cE "^TEST(_F)?\(" tests/integration/*_test.cpp`（2026-11-09 实测 109 = 冒烟 1 + MetaToolsSuite 12 + EditorInputSuite 6 + AuthorizationGate 1 + 校验类 43 + 通道契约 30 + 工具面 16）；分支 gtest 实测：试点 20/20（`.handoff/evidence/T-02/mapping.md`）、63 = 20 + 43（`T-04/gtest-output.txt`）、50 = 20 + 30（`T-05/gtest-output.txt`）、36 = 20 + 16（`T-06/gtest-output.txt`） | 一致 |
| 守卫与脚本项 | 以 `ctest -N` 为准（2026-11-13 新增 `no_python_guard` 与自测两条，本页未重跑） | 命令：`grep -c "^add_test(" tests/CMakeLists.txt`；证据：`tests/CMakeLists.txt` 的 `add_test` 注册 | 待复核 |
| ctest 注册总数 | 996 = 636 + 239 + 109 + 10 + 2（单元 + 边界 + 集成 + 守卫 + CLI） | 命令：`ctest --preset debug -N`；证据：`.handoff/evidence/T-08/ctest-N-after.txt`（`Total Tests: 994`，`gda_runner_*`/`pool_*` 零命中）；全量 ctest **100% tests passed out of 994**（`.handoff/evidence/T-08/manifest.md`） | 实测已复核 |
| CLI TS 单测 | 25 个文件 / 255 项（237 pass + 18 skip + 0 fail，2026-11-13 实测口径） | 命令：`bun test cli/tests`；静态：`cli/tests/*.test.ts` 25 个文件；参数化循环在运行期展开，运行期项数以实测为准 | 一致 |
| 技能书 | EN 嵌入 10 册 / 10 文件；EN+ZH 源共 20 文件（各 10 SKILL.md） | 命令：`find skills skills-zh -type f | wc -l`（当前 20）；各 10 个册目录 | 一致 |
| MCP skills 资源 | 21（1 目录 + 10 单册 + 10 单文件） | `register_skill_resources` 按 `skills/` 内嵌文件动态注册 | 一致 |
| add_library 源文件 | 115 个 `.cpp`（main 1 / core 34 / tools 54 / util 8 / prompts 9 / runtime 4 / resources 3 / ui 2） | 命令：`find src -name "*.cpp" | wc -l`（2026-11-09 实测 115）；根 `CMakeLists.txt` 经 `cmake/sources/*.cmake` 聚合 | 一致 |
| 传输探针 | 1 个构建目标（`gda_mcp_transport_probe`），非 ctest、不计入 994 | `cmake/sources/api_probe.cmake`；诊断工具，验收阶段按需执行 | 一致 |
| 客户端配置 | 20 个 | `src/util/client_config_gen.hpp` 的 `enum class ClientId` 20 个具名项（`COUNT` 为哨兵） | 一致 |
| 端口 | 无默认端口；首次在 20000–29999 随机分配并持久化，探测上限 256 | `src/core/config.hpp`：`GDA_PORT_CANDIDATE_BASE/LIMIT = 20000/29999`、`GDA_PORT_PROBE_LIMIT = 256`、`GDA_CONTROL_PORT_BASE/LIMIT = 19900/19999`、`GDA_PRIVATE_PORT_BASE = 32768`（四条 `static_assert` 约束）；候选序列为 `[20000, 29999]` 随机排列（`src/core/transport/port_resolver.cpp`）；显式指定绑定失败不自动更换 | 一致 |
| REST 端点数 | 0（0.3.2 已移除） | `src/core/transport/api_surface.cpp` 仍留在构建中但入口无调用点，属待清理死代码 | 已移除 |
| 请求体上限 | 4 MiB | `src/core/transport/http_parse.hpp` 的 `kMaxBodyBytes`（超限由传输层直接 413） | 一致 |
| 授权档位与能力名 | 0（0.3.2 已移除授权门） | `authorization.hpp` 与 `GODOT_AUTOPILOT_ALLOW` 已删除；高风险工具改由 `confirm=true` 控制 | 已移除 |
| 破坏类档位 | 6 个声明者 + 4 档判据 | `src/tools/base/side_effect_policy.hpp` 的 `kTierCriteria` 四档判据与两条边界规则、`kDestructiveExemptions` 9 条豁免白名单（每条带理由）；声明 `destructive` 的工具 6 个；0.3.2 起判据保留但不再驱动授权门，高风险工具由 `confirm` 控制；单元测试 用例 `destructive_tier_guard_test` 防漂移 | 判据保留 |
| 授权模式 | 0（0.3.2 已移除） | `AuthMode` / `GODOT_AUTOPILOT_AUTH_MODE` 已删除 | 已移除 |
| 面板可勾选能力 | 6 项（`read` 无开关） | `src/tools/base/authorization.hpp` 的 `kDockToggleCapabilities`；面板控件由 `src/ui/authorization_switches.hpp` 的 `kSwitchLabels` 生成；单元测试断言见 `authorization_dock_toggle_test` | 一致 |
| 主线程预算 | 默认 27000 ms / 健康探针 5000 ms / 长阻塞 29000 ms | `src/core/config.hpp:16-20`（常量即生效值）；预算选择逻辑 `src/tools/base/dispatch.cpp:288-298`；长阻塞预算经 `env_int_clamped` 钳制在 `[1000, 29000]`，未设 `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 时生效 29000 ms | 一致 |
| `code_execute` 超时上界 | 静态 27000 ms，实际生效 = `min(27000, 生效长阻塞派发预算)` | `src/core/config.hpp:45` `GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000`；唯一实现点 `code_exec_ops.cpp:464-470` 的 `max_timeout_ms()`；派发侧余量 2000 ms、传输侧余量 2000 ms，由 `config.hpp:48-62` 三条 `static_assert` 约束 | 一致 |

> 对照说明：本表为逐文件/逐宏统计与实测证据的重基线值；`AGENTS.md` 与技能层只引用本页、不复制数值，`tests/README.md` 若出现旧值（其 L2 内容为 T-10 重基线对象）以本页为准。旧「L1/L2 双层」口径的行（L2 用例数、池化注册、池床、遍历枚举/排除/候选/步数/warnings）已随引擎轨删除移除，去向见「历史口径迁移」。

## 历史口径迁移（L2 引擎轨删除，2026-11-09）

「L1/L2 双层」时代的下列章节随引擎轨删除（提交 `c023a5c`；脚本编排收敛 `bf31e8a` / `8997db9`）整体移除，历史结论去向如下（下述 L2 均为已删除资产的历史指称，不作现存事实）：

- **L2 载体行、构建与运行、执行闭环（GodotProcess / GDA_FORCE_HEADLESS / 就绪轮询）、报告格式、CLI 与退出码**：随 `gda_test_runner` 删除；测试床迁移与无头实测的历史记载见 [changelog/2026-09-26-log.md](changelog/2026-09-26-log.md)、[changelog/2026-09-29-log.md](changelog/2026-09-29-log.md)。
- **L2 池化编排**（五池常驻、`pools.json` 分区、`gen_partition.py`/`gen_pool_map.py`、`pool_setup`/`pool_cleanup` fixtures 与 `RESOURCE_LOCK`、逐例会话复位、`isolated` 名单、池床布局与磁盘占用、游戏默认无头）：无 changelog 落点，设计与实测记录仅存于 git 历史（本页 2026-10-28 至 2026-10-30 版本与 `feature/l2-pool-test` 分支）。
- **48 份 L2 用例表、JSON schema 冻结规则、断言语义、软失败语义**：用例语义已由集成层逐条蒸馏（对照表见 `.handoff/evidence/` 各 mapping，R-01 台账登记划回 e2e 的条目；合并定稿「A 类迁移对照表」与引擎行为验收清单见 [engine-behavior-checklist.md](engine-behavior-checklist.md)）；配置加载器以 `config_loader.{cpp,hpp}` 形式保留为 单元与边界测试业务源。
- **全量遍历与排除工具清单**（`traversal.cpp`、预期错误豁免表、`side_effect` 逐档名单与历史事故、warnings 基线）：判定逻辑由集成层 toolface 契约承接（动态枚举 + 授权拒绝形态断言 + 计数观察值口径，见「回归属性清单」）；历史基线演变（枚举 452 / 排除 182 / 候选 270 / 489 步 / warnings 0 等）见 [changelog/2026-10-19-log.md](changelog/2026-10-19-log.md) 至 [changelog/2026-10-22-log.md](changelog/2026-10-22-log.md)；Example 污染事故（`set_editor_main_scene` / `save_editor_scene`）的原始记载见 [changelog/2026-09-18-log.md](changelog/2026-09-18-log.md) 与 git 历史（排除清单注释考古，提交 `ca1461c`）。
- **已知引擎副作用、L2 环境漂移（编辑器场景恢复）、审计不一致点清单**：对象均为已删除的测试床与引擎轨，历史记载见 [changelog/2026-09-26-log.md](changelog/2026-09-26-log.md)、[changelog/2026-09-29-log.md](changelog/2026-09-29-log.md)。
- **已知遗留（待跟进）**：mcp-cpp-sdk 客户端响应等待缺陷随测试客户端（`tests/integration/mcp_test_client.cpp`）删除而不再适用于测试轨，生产服务端不受影响；「编辑器进程的引擎错误不可经 MCP 观测」属产品能力边界，历史记载见本页 git 历史（2026-10-30 版「已知遗留」）与 `skills/godot-autopilot-runtime/references/debug-paths.md`；游戏侧双进程 e2e 覆盖缺口与编辑器 UI 动作自动化缺口移交 Agent e2e 闭环（`gda-e2e-testing` 工作流 + `gda-e2e-analysis` 分析闭环）。
- **页头批次级数值演变（2026-08 至 2026-10-30 的 L1/L2/ctest 计数史）**：见本页 git 历史（2026-10-30 版页头）与 [changelog/2026-10-23-log.md](changelog/2026-10-23-log.md)（现有最新一篇，其余各日按文件名检索）。

## 相关页面

- [引擎行为验收清单（engine-behavior-checklist.md）](engine-behavior-checklist.md)
- [工具注册表（modules/tools_registry.md）](modules/tools_registry.md)
- [ToolSpec 数据层与执行管线（tool_base_design.md）](tool_base_design.md)
- [构建说明（build.md）](build.md)
- [核心模块（modules/core.md）](modules/core.md)
