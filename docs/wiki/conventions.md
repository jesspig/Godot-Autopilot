---
type: 工程约定
title: 工程约定
description: 命名、日志、错误模式、线程纪律与添加工具流程等全仓库编码约定
tags:
  - 约定
  - 命名
  - 规范
timestamp: "2026-10-20T00:57:31+08:00"
resource: src/
---

# 工程约定

本文汇总代码中实际体现的工程约定（与 [overview.md](overview.md)、[AGENTS.md](../../AGENTS.md) 互为参照）。安全边界与并发生命周期以 [T0 安全边界与并发契约](security_contract.md) 为准。

## 命名体系

- **命名空间**：`godot_autopilot`（工具子命名空间 `<模块>_ops`，如 `godot_autopilot::scene_ops`）
- **前缀**：核心常量 `GDA_*`（如 `GDA_DEFAULT_PORT`）；运行时协议 `gda:*`；导出宏 `GDA_EXPORT`
- **产物与目录**：CMake 目标 `godot-autopilot`，部署到 `demo/*/addons/godot-autopilot/` 与 L2 测试床 `tests/testbed/addons/godot-autopilot/`（口径见 [构建与部署](build.md)）
- **工具命名**：约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（snake_case，**以动词置首为主导**，如 `create_scene_node`、`intersect_physics_2d_ray`、`set_input_map_action_deadzone`）；451 个工具名段数随粒度自然变化（2-8 段，如 `read_file` / `add_physics_3d_body_collision_exception`），`signal_connect`/`property_set` 等少量短名为规范内保留的名词前置形态
- **头文件 include guard**：`GODOT_AUTOPILOT_<MODULE>_HPP`

## 日志

- 类别（仅 5 个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`（`LogCategory` 枚举）
- 级别：`Debug`、`Info`、`Warning`、`Error`（`LogLevel` 枚举）
- `LogSystem` 单例：环形缓冲上限 10000 条，缓冲本体仅内存（`McpLogDock` 轮询消费）；磁盘落盘经 `LogPersist` 增量拉取到 `user://godot_autopilot/logs/gda-<stamp>.log`（人类可读，含 `log_detailed` 的 detail）与 `traces/trace-<stamp>.jsonl`（结构化事件），两目录各保留最近 20 文件 / 50MB
- `log_detailed(level, category, summary, detail, trace_id = {}, span_id = {})` 用于带诊断上下文的日志（detail 上限 8192 字符，超出截断）；09-21 起扩展为 6 参重载（`trace_id`/`span_id` 关联字段，旧 4 参委托），`LogEntry` 追加 `trace_id`/`span_id`；`query` 的 `filter_text` 同时匹配 message、detail、`trace_id` 与 `span_id`
- 日志/轨迹分离与关联（09-21 起）：人类日志（`logs/gda-*.log`）与结构化事件（`traces/trace-*.jsonl`）分离落盘，统一经 `src/core/monitor.{hpp,cpp}` 门面写入（`tool_call`/`lifecycle`/`data_flow`/`perf`/`persist_health`/`security` 等便捷函数）；trace 事件以 `kind`（`enum class TraceKind`）区分类型，并用 `request_id`/`correlation_id`/`parent_span` 串起「协议请求 → call_tool → 域工具」链路，`RequestRegistry` 维护 `request_id → trace/span` 关联（上限 4096、FIFO 淘汰）；jsonl 为追加式 schema v2（原键序后追加 `kind`/`request_id` 等新键，旧键保留）
- 禁止记录密钥与 PII；参数/trace 数据默认经 `sanitize_policy` 脱敏（`GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize` > 默认 true），关闭后原始参数与截图会落盘 `user://`；脱敏键为大小写不敏感的通用集合（`token`/`secret`/`password`/`key`/`api_key`/`authorization`/`cookie`/`session_token` 及 `*_token`/`_key`/`_secret`/`_password` 后缀等）

## 错误模式

- 领域工具失败返回 `{"error": "消息"}` JSON（构造经 `util::error_json`，成功结果经 `util::ok_result` 包 `result`，见 `src/util/error_util.hpp`；个别执行类响应另带 `error_details`/`structured_error` 诊断字段），`call_tool` 元工具检查 `error` 字段并设 `is_error = true`
- 命名契约：`get/fetch/load` 目标必存在否则抛异常；`find/search/query` 允许为空；`validate/check` 失败即抛；`apply/compute` 为纯函数
- **C++ 抛动态消息（`std::string` 拼接结果或 `std::string` 局部量）一律用 `godot_autopilot::RuntimeError` / `InvalidArgument**（`src/util/owned_exception.hpp`：以成员 `std::string` 自持消息并覆写 `what()`，仍派生自 `std::runtime_error`／`std::invalid_argument`，既有 `catch (const std::runtime_error&)` 捕获点与 `EXPECT_THROW(..., std::runtime_error)` 断言无需改动）。原因：MSVC STL 的 `runtime_error(const std::string&)` 只把 `c_str()` 转交 `exception(const char*, int)`，后者原样保存指针、不做拷贝（`<stdexcept>`/`<exception>` 头文件实证），消息临时量在 throw 完整表达式结束即销毁、`std::string` 局部量在栈展开离开函数帧时销毁，`what()` 随之野悬，释放块被复用即读到乱码；libstdc++/libc++ 会拷贝消息，故仅 Windows 复现。10-19 CI `VariantJsonStrictTest.Rect2MissingSizeMessage` 实测复现（本地亦确定性复现），根因为我方代码长期潜伏的 UB，SDK 0.3.7 升级只改变堆布局使其显性化；纯字面量消息为静态存储期、无此风险，可继续用 `std::` 原类型（`src/core/command_queue.hpp` 即如此）

## 线程纪律

- **所有 Godot API 调用必须通过 `CommandQueue` 在主线程执行**（HTTP 线程直接调用会崩溃）；`CommandQueue::execute_sync()` **已无调用点**且是无界等待，**不得新增调用**——跨线程进主线程一律走 `submit()` 或带预算的等待入口（`dispatch::run_on_main_thread_with_budget` / `dispatch::run_bounded_on_main_thread`）。例外：`debugger_prompts` 为纯静态模板文本、不触碰 Godot API 亦不经队列；`debugger_resources` 与 `resource_handlers` 读取捕获缓冲经 `dispatch::run_bounded_on_main_thread`（默认预算 `GDA_DISPATCH_WAIT_DEFAULT_MS`）提交到主线程执行；`call_tool` 元工具编排回调在 MCP 线程执行但自身不触碰 Godot API（领域工具经 dispatch 路由回主线程，2026-09-13 起）
- **主线程契约由静态守卫强制**：`tests/guard/main_thread_guard.py` 扫描 `src/` 下全部 `*.cpp/*.hpp`，把每个源文件按「线程上下文分类」归入 `main`（主线程）/ `managed`（受管，游戏进程侧经 SceneTreeTimer 与 awaiter 排到游戏主线程）/ `foreign`（外线程）三类；`foreign` 类文件里的 Godot API 直接调用默认违规，只有落在该文件显式登记的主线程作用域（`FOREIGN_MAIN_THREAD_SCOPES`）里才放行。词法层面先剥离字符串字面量（含 raw string）与注释再匹配标识符，避免把字面量/注释里的 API 名误判为真实调用。判定在 ctest 阶段（`ctest --preset debug -R main_thread_guard`），无违规打印 `main-thread-guard OK` 退出 0，有违规退出 1，文件读/解码失败退出 2。
- `CommandQueue`：header-only，`submit()` 返回 `std::future`（异常经 `set_exception` 传播），主线程 `_process()` 每帧 `drain()`
- 导出期间（`ExportGuard` **实例级**导出标志——每个实例各持一份 `ExportState`，`is_exporting()` 遍历实例注册表、任一实例在导出期即返回 true）`dispatch` 拒绝工具调用

## 添加工具流程

`ToolRegistry` 单一来源 + 全量 `ToolSpec` 数据记录（无 `tool_defs.def`、无宏/真类；设计见 [tool_base_design.md](tool_base_design.md)）：

1. `<域>_ops.hpp` 声明 `mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`
2. `<域>_ops.cpp` 实现（返回 `{"error": ...}` 或结果对象）。取参现状必须知道：`Args(raw, kXxxParams)` 取参器（`tool_args.hpp`：opt_/require_/get_ + `reject_unknown()`）**已实现且有 L1 覆盖，但 `src/` 下没有任何领域 handler 接入**（除 `tool_args.cpp` 自身，全仓 0 处 include）；在架 handler 一律 `args.Find("name")` 手工判类型 + `util::error_json("missing required parameter: <name>")`（`util::error_json` 在 `src/tools/*.cpp` 实测 759 行、`util::ok_result` 46 行）。33 个领域的 handler 全部走 `args.Find(...)`。新增工具跟随所在域邻居的写法，不要在已用 `args.Find` 的文件里混入 `Args`，否则同一文件两种取参风格并存
3. 该域 `<域>_tools.hpp`：定义 `const std::vector<ParamSpec> kXxxParams = {...}` 参数表，并在 `make_tools()` 中以 `v.push_back(make_spec_tool(ToolSpec{name, description, category, {tags}, side_effect, flags, kXxxParams, handler}))` 一行注册——`register_all.cpp` 经各域 `make_tools()` 自动注册，无需手改；schema 从参数表自动派生（仅嵌套结构才用 `raw_schema` 手写 JSON）
4. `side_effect`（`SideEffect` 枚举选值）与 `flags`（如 `kMutating`）按工具实际影响声明即自动进入遍历排除，无需手改 `tests/runner/traversal.cpp`；合成输入类可加 `kObserve` 复用执行管线的截图后处理
5. 仅新增 `.cpp` 才需动 `CMakeLists.txt:65` 的 `add_library()`（header-only 无需；**所有**新增 `.cpp` 都要逐项登记，Unity 构建只编译列出的文件）；迁移守卫 `tests/guard/` 拒绝旧宏（`GDA_TOOL_CLASS` 等）与 `schema_*_ops.cpp` 残留

## 测试纪律

- L1（`gda_unit_tests`）禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃），只能测注册/分发/错误路径
- 子代理迭代：只写代码不编译，主代理统一 configure + build 再分批跑测试（并行编译会锁）
- **新增 `src/*.cpp` 的登记分两处、口径不同**：
  - 根 `CMakeLists.txt:65` 的 `add_library()`：**所有**新增 `.cpp` 都要逐项登记（含 `src/tools/*_ops.cpp`），漏登记即漏编译；
  - `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`：**只有非 `*_ops.cpp` 的源文件才需登记**——`src/tools/*_ops.cpp` 已由 `file(GLOB GDA_TOOLS_OPS_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/tools/*_ops.cpp")`（`tests/CMakeLists.txt:21-22`）自动纳入 L1 业务源，新加的 `*_ops.cpp` 无需手改清单（`CONFIGURE_DEPENDS` 保证 reconfigure 时重新收集）；其余目录（`src/core/`、`src/util/`，以及 `src/tools/` 下的非 ops 文件如 `register_all.cpp`/`dispatch.cpp`/`tool_catalog.cpp`/`schema_builder.cpp`/`tool_args.cpp`/`tool_pipeline.cpp`/`tool_invoke.cpp`/`debugger_access.cpp`/`editor_ui_actions.cpp`）仍需显式登记，漏登记只在 L1 链接期报错。
- 计数口径（L1 gtest 数、L2 `tests/config/*.json` 份数、ctest 注册点、遍历候选/步骤数）以运行时统计为准；迁移守卫经 `ctest --preset debug -R migration_guard` 零依赖运行（该预设固定 Ninja 生成器，无 ninja 时改用 `debug-portable`，见 [build.md](build.md)「预设」）

## 守卫（guards）

守卫是「不靠人自觉」的静态门禁，全部在 ctest 阶段判定（**编译通过不等于守住**），当前 6 个：

| 守卫 | 载体 | 守住什么 |
|---|---|---|
| `migration_guard` | `tests/guard/migration_guard.cmake` | 旧宏 / 旧 schema 文件零残留 |
| `comment_guard` | `tests/guard/comment_guard.py` | `src/` 与 `tests/` 下源码无注释 |
| `main_thread_guard` | `tests/guard/main_thread_guard.py` | 外线程文件不直接调用 Godot API |
| `version_sync_guard` | `tests/guard/version_sync_guard.py` | 版本号单一来源（根 `VERSION`）在各载体间一致 |
| `cli_contract` | `tests/scripts/test_cli_contract.py` | CLI 载体与 MCP 服务端的调用契约一致 |
| `cli_unit` | `tests/scripts/run_cli_unit.py` | CLI 单测（封装 `bun test cli/tests`） |

**新增守卫的命名与登记约定**：

1. **命名**：`<主题>_guard`（`.py` 或 `.cmake`），落在 `tests/guard/`；`tests/CMakeLists.txt` 里的 ctest 名字与文件名一致（`add_test(NAME <主题>_guard COMMAND ...)`），便于 `ctest -R <主题>_guard` 单独跑。
2. **豁免清单**：守卫若需豁免，**豁免必须显式登记在守卫文件自己的清单常量里**，不允许靠「文件不在扫描范围」或注释绕过。已登记的两处清单：
   - `main_thread_guard.py` 的 `THREAD_CONTEXT_RULES`（逐文件标注 `main`/`managed`/`foreign` 三类线程上下文 + 判定理由）与 `FOREIGN_MAIN_THREAD_SCOPES`（外线程文件里允许触碰 Godot API 的主线程作用域白名单，当前仅 `src/core/log_persist.cpp` 的 6 个函数）；
   - `migration_guard.cmake` 的 `migrated_domains.txt`（已迁移域清单 + `strict` 行）。
3. **零依赖**：守卫只允许用标准库或 CMake 自带能力，不引入第三方依赖、不启动引擎、不联网。
4. **退出码约定**：0 = 通过（打印一行可辨识的 OK 文案），1 = 有违规（逐条打印 `文件:行号: 片段`），2 = 用法/环境错误（参数错误、文件读不到）。CI 靠退出码判定。
5. **豁免要写进知识库**：新增或调整豁免清单后，同步更新本页与对应模块页，避免「代码里放行了、文档里没记」。

## 注释与提交

- **源码禁止注释**：`src/` 与 `tests/` 下 `*.cpp/*.hpp` 除 `// namespace` 结尾标记外不得出现任何 `//` 或 `/*…*/`；需要解释的内容写进本知识库对应页，不留在代码里。门禁是 `comment_guard`（见 [tests.md](tests.md)「守卫」），判定时机在 ctest 而非编译，所以"编译过了"不等于"守住了"。
- **提交信息**：`<type>(<scope>): <中文描述>`，type 取 `feat`/`fix`/`docs`/`test`/`chore` 等，scope 用中文功能域（实际历史例：`feat(编辑器界面): 新增 Trace 视图并补齐界面操作埋点`、`docs(知识库): 同步可观测性批次 wiki 页面与维护日志`）。一次交付按功能域拆成多个提交，不用 `--no-verify` 绕过守卫。
- **行尾伪变更**：仓库无 `.gitattributes`，Windows 上 `git status` 会因 CRLF 显示大量 `M`；核查实际变更一律以 `git diff HEAD` 为准，不要据 `git status` 判断改动范围，更不要在提交前 `git checkout --` 批量还原（会连带回掉真实修改）。

## 能力边界（08-20 确立）

gda 聚焦「编辑器内的引擎操作」，以下能力**明确不做**，避免重复造轮子，交由外部编辑器/CLI 工具承担：

- **文本级增量编辑**：gda 不做文件内精确 diff/apply 式编辑；对场景/资源内容的语义级读写走 `scene_*`/`resource_*` 工具，纯文本查看/搜索用 `read_file`/`find_in_files`，深度编辑交给外部编辑器。
- **glob 风格文件匹配**：文件列举/目录遍历由 `get_resource_dir_files`、`get_editor_file_system_tree`、`find_in_files` 覆盖，不单独提供 glob 匹配工具。
- **C# 程序集热重载**：`build_csharp_assembly` 仅能触发 `dotnet build`（进程副作用，已入遍历排除清单）；GDExtension(C++) **无公共 API** 触发编辑器内 .NET 程序集重载（该能力位于引擎 `modules/mono` 内部）。因此 C# 类/签名变更后，编辑器内 `load()`/`get_script()`/校验渲染仍必须由用户在编辑器点 Build 或重启编辑器；gda 侧的 `code_execute`/GDScript 通道不受影响。

## 知识局限（文档提示）

- **C# 只读/私有 setter 属性对 GDScript 不可见**（Godot .NET 绑定既有行为，非 gda 缺陷）：`get("MaxHealth")` 对无 getter 的属性返回 null。AI 用 GDScript/`property_get` 校验 C# 派生值时易误判——应改为检查可读导出引用而非派生值。
- **目标引擎按 Godot 4.7 口径写 API**（本项目 `GODOTCPP_API_VERSION "4.7"` 锁定，见 [build.md](build.md)）：`TileSetAtlasSource.get_tile_data()` 的入参是 `atlas_coords`+`source_id`+`alternative_tile`（不是 3.x 的 `source_id` 打头）；`AnimatedSprite2D` 的帧集合属性名是 `sprite_frames`（不是 `frames`）；`CharacterBody2D/3D.motion_mode` 枚举值 `GROUNDED=0`、`FLOATING=1`。这三条在工具描述与 skill 模板里都是踩点写死的，改动前先在引擎头文件核对。

## 其他

- CI/Release 工作流见 [build.md](./build.md)「CI 与 Release」；CI 仅跑 L1，L2 引擎用例仍仅本地执行（需 `GODOT_PATH`）
- 依赖版本固定：godot-cpp `10.0.0-stable`（commit `507ed9d`）、mcp-cpp-sdk `0.3.8`（commit `d79f9b1`），二者与 googletest `v1.15.2`（commit `b514bdc`）均为 `third_party/` 下的 git submodule，版本由 gitlink 锁定的 commit 唯一确定，检出方式为 `git clone --recurse-submodules` 或 `git submodule update --init --recursive`（另以 `GODOTCPP_API_VERSION "4.7"` FORCE 锁定目标 API，OpenSSL 集成由 `MCP_WITH_OPENSSL "OFF"` 显式关闭，见 [build.md](./build.md)）
