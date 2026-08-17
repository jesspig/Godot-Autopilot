# AGENTS.md

## 构建

- **`uv run build.py`** — Debug: 配置 + 构建 + 部署到 `Example/addons/godot-autopilot/`
- **`uv run build.py --release`** — Release: 先清理，再配置 + 构建 + 部署
- **`uv run build.py --package`** — 打包已部署的 addons 为 `dist/godot-autopilot-<version>.zip`
- 手动: `cmake --preset debug && cmake --build --preset debug`
- 预设 (`CMakePresets.json`): `debug`, `release`（均为 Ninja）
- **切勿删除 `build/<preset>/_deps/`** — 缓存已获取的依赖项（godot-cpp、mcp-cpp-sdk、googletest）
- **添加新 .cpp 时必须在 `CMakeLists.txt` 的 `add_library()` 中加入**，否则 Unity 构建也不会包含

## 架构

- **进程内 GDExtension**，在 `MODULE_INITIALIZATION_LEVEL_EDITOR` 阶段加载到 Godot 编辑器
- **线程模型**：HTTP 线程（mcp-cpp-sdk 0.3.x 自研网络栈）→ `CommandQueue::submit()` → Godot 主线程（在 `_process()` 中排空）
- **所有 Godot API 调用必须通过 `queue.submit()`** — 从 HTTP 线程直接调用会崩溃
- **MCP 端口**：9527，端点 `/mcp`。解析优先级：环境变量 `GODOT_AUTOPILOT_PORT` > `user://godot_autopilot/config.json`（`PluginConfig`，配置面板 Apply 后持久化）> 默认 9527——环境变量优先保证测试/CI 不受面板配置影响
- **元工具（7 个）**：直接在 `server.RegisterTool()` 注册，不经过 `call_tool`：
  `ping`、`search_tools`、`list_categories`、`get_tool_detail`、`call_tool`、`batch_execute`、`code_execute`
- **领域工具（332 个，数量随插件版本变化，以 MCP search_tools 返回为准）**：通过 `call_tool` 代理，由 `register_all.cpp` 中的 `g_handlers` 映射分发。完整 schema 在 `ToolCatalog` 中
- **工具总数**：339 = 7 元 + 332 领域（数量随插件版本变化，以 MCP search_tools 返回为准）；`ToolCatalog` 条目 343（另含 `system_status` 与 3 个 meta 快照）
- **入口点**：`src/main.cpp` → `GDExtensionEntryPoint` 注册类与 `GodotAutopilotPlugin(EditorPlugin)`；`ServerContext` 由插件 `_enter_tree()` 启动、`_exit_tree()` 停止
- **关键工具函数**：`VariantJson::serialize/deserialize` (`util/variant_json.hpp`) 用于 `godot::Variant` ↔ `mcp::JsonValue` 互转
- **错误模式**：领域工具返回 `{"error": "消息"}` JSON；`call_tool` 元工具检查 `error` 字段并设 `is_error = true`

## 添加工具

需要四处修改：

1. 在 `src/tools/<category>_ops.hpp` 声明：`mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`，命名空间 `godot_autopilot::<category>_ops`
2. 在 `src/tools/<category>_ops.cpp` 实现处理函数
3. 在 `src/tools/tool_defs.def` 加一行 `TOOL_ENTRY(id, "tool_name", "desc", "Category", "tags", <category>_ops::handle_xxx, SCHEMA_BASIC|SCHEMA_NONE)`——`register_all.cpp` 把 def include 两次，宏自动写入 `g_handlers` 与 `ToolCatalog`，**register_all.cpp 无需手改**；SCHEMA_BASIC 时还需在 `src/tools/schema_*_ops.cpp` 的 fill 表加 `schema::build_schema({ParamDef...})` 条目（SCHEMA_NONE 走回退：scene_tree_* 自动补 `group`、tilemap_* 补 `node_path`，否则空表）
4. 在 `CMakeLists.txt` 的 `add_library()` 中添加新的 `.cpp`

元工具（`ping`/`search_tools`/`list_categories`/`get_tool_detail`/`call_tool`/`batch_execute`/`code_execute`）例外：直接 `server.RegisterTool()` 注册，不进 def。

**命名约定**：`<动词>_<类别>_<维度>_<对象>_<修饰>`（snake_case，动词置首，如 `create_scene_node`、`intersect_physics_2d_ray`、`set_input_map_action_deadzone`；动词 get/set/create/add/remove/apply/play/stop 等置首，段数随粒度变化）

## 关键约定

- **命名空间**：`godot_autopilot`
- **日志类别**（仅此几个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`
- **编译器**：优先 Clang/clang-cl，自动检测；MSVC/GCC 回退
- **优化**：自动 sccache/ccache、LTO（Release 使用 ThinLTO/LTCG/IPO）、Unity 构建、Ninja 作业池 — 均根据硬件自适应，可通过 `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` 等环境变量覆盖
- **依赖**：godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.3.1（0.3.x 起移除 libhv 与 simdjson，改为 SDK 自研网络栈/JSON 解析器） — 使用 FetchContent，不依赖子模块

## 测试

- **启用**：`GDA_ENABLE_TESTS` 已固化在 `CMakePresets.json` 的 debug/release 预设（默认 ON）——清理或重建 `build/` 后 `uv run build.py` / `cmake --preset debug` 自动恢复测试，无需手动传参（裸 `cmake` 不带 preset 时默认 OFF）
- **运行**：`ctest --preset debug`（L1 秒级；L2 全量约 2 分钟，需 Godot 路径）；单文件：`build/debug/tests/gda_test_runner.exe --file 01_scene`
- **结构**：L1 = `gda_unit_tests`（72 个 gtest，不启动引擎，含 339 工具注册管线断言，数量随插件版本变化）；L2 = `gda_test_runner` + `tests/config/*.json`（5 个用例文件，每文件一次 headless 编辑器生命周期最小闭环，经真实 MCP HTTP）
- **新增 JSON 用例 = 新增 `tests/config/*.json`，零 C++ 改动**；schema / CLI / 排除清单全量文档在 `tests/README.md`
- **Godot 路径**：环境变量 `GODOT_PATH` 或仓库根 `.env`（复制 `.env.template`）；缺失时 L2 全部失败/跳过
- **全工具遍历**：`03_tools_contract.json` 对 332 领域工具做空参契约 + 启发式冒烟（数量随插件版本变化，以 MCP search_tools 返回为准；约 410 步为静态推算，以运行时统计为准，约 2-3 分钟）
- **34 个副作用工具被遍历排除**（`show_os_alert` / `write_file` / `save_editor_scene` / `display_*` 等，清单在 `tests/runner/traversal.cpp`）——新增工具若写配置/文件/弹窗/改窗口，必须同步加入排除清单，否则遍历会污染 Example 项目或干扰桌面
- **引擎副作用**：L2 运行后 `Example/project.godot` 会被 headless 编辑器自动追加 `[audio]` 段（并生成 `default_bus_layout.tres`）或 `[input]` 段（输入映射）等自动保存内容，无害但弄脏工作区；`git checkout -- Example/project.godot` 清理（`default_bus_layout.tres` 如生成需手动删除）

## 知识局限

- **无 CI**（`.github/` 不存在）
- **schema 覆盖**：非空/空数以运行时统计为准（由 `tests/unit/register_all_test.cpp` 的 `SchemaStatisticsBaseline` 运行时统计断言，不硬编码数量；def 层面静态：SCHEMA_NONE=208 / SCHEMA_BASIC=124）；**3 个契约缺口**：`create_scene_node`（name/type 有默认值不校验必填）、`get_resource_extensions`（缺 type 返回全类型）、`reimport_resource_files`（空参 count=0 静默成功）——遍历测试记 warnings 不 FAIL
- **目标引擎版本**：Godot 4.7；常见 API 迁移事实：`TileSet.get_tile_data` 属 `TileSetAtlasSource.get_tile_data(source_id→atlas_coords, alternative)`；`AnimatedSprite2D` 属性名为 `sprite_frames`（`frames` 自 4.0 起更名）；`motion_mode` 枚举 GROUNDED=0/FLOATING=1
- **领域工具分 23 类**（`tool_defs.def` 实测：Render 49、Physics 47、Display 24、Editor 22、Resources 21、Audio 20、Input 19、OS 16、Debug 16、Navigation 15、Config 13、Scene 12、Scripts 10、Text 10、Debugger 7、TileMap 7、Game 7、Properties 5、Docs 4、Group 3、SpriteFrames 3、System 1、Capture 1；InputMap 并入 Input）——README.md 的类别表已同步
- **3 个模块无独立 .hpp**：`environment_ops.cpp`、`display_window_ops.cpp`、`runtime_game_ops.cpp` 分别复用 `render_ops.hpp`/`display_ops.hpp`/`runtime_ops.hpp`
- **`code_execute.timeout_ms` 无上限钳制**：schema 声称 max 30000，实现仅 `static_cast<int>`；对比 `runtime_game_ops` 有 `GDA_MAX_TIMEOUT_MS` 钳制
- **DNS rebinding 保护**（mcp-cpp-sdk 0.3.1 起）：HttpServer 默认只允许 Host 为 `localhost`/`127.0.0.1`/`::1` 的请求，其余返回 403（`StreamableHttpServerTransport` 未开放 `allowed_hosts` 配置）——MCP 客户端必须连 `127.0.0.1:9527`，局域网 IP 直连会被拒
- **详细架构规划**见 `docs/wiki/`（当前实现的权威文档，与源码同步维护）

## 协作约定

- **CRLF 伪变更**：`git status` 常显示大量 `M`（LF→CRLF 行尾规范化差异），但 `git diff HEAD` 为空——改动前务必用 `git diff` 确认真实变更，不要凭 status 判断工作区状态
- **提交信息**：`<type>(<scope>): <中文描述>`，沿用仓库历史风格（如 `fix(运行时调试): eval 脚本错误回传`、`docs(知识库): 全量数字同步`）
- **多代理迭代时：子代理只写代码不编译，主代理统一 configure + build 再分批运行测试**（并行编译会锁）
- 新增 `src/*.cpp` 时同步更新三处：业务 `add_library()`（根 `CMakeLists.txt`）、`tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`（L1 链接需要，漏了会 undefined symbol）
- **L1 测试禁止调用任何已注册工具 handler**（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃）；`register_all_test` 只能测注册/分发/未知工具错误路径
- 编译选项：`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>` 可在网络不可用时复用本地已拉取的 googletest（`build/debug/_deps/googletest-src`）

## 工程原则

- **不写注释、代码即文档**：意图靠清晰命名与结构表达。
- **先搜索后动手**：以实际代码/文档为事实，禁止臆测不存在的 API、节点、资源路径。

## 项目知识库

- **知识库位置**：`docs/wiki/`，入口 `docs/wiki/index.md`（全量目录）
- **按需更新**：完成功能 / 交付操作指南 / 提交前统一更新；微调与小改动不更新。彻底清除过时描述，不留废弃标记
- **基于事实更新**：更新前用 `git status` / `git diff HEAD` 核查工作区全部实际变更（含用户在编辑器中手动修改的代码、配置等），以实际文件为准，禁止仅凭对话记忆撰写；无法核实的标注 `> [!todo] 待补充`
- **维护日志**：`docs/wiki/changelog/` 按天分文件（`<YYYY-MM-DD>-log.md`），每条记录 `<YYYY-MM-DD-HH>` 精确到小时；`log.md` 为摘要，仅留最近 7 条
- **页面结构**：`overview.md`（总览/架构/技术栈）、`build.md`（构建/部署/打包）、`tests.md`（L1/L2 测试体系与数值）、`conventions.md`（工程约定）、`modules/`（core、entry_runtime、tools_registry、tools_ops_a/b、support）、`example.md`（Example 项目）
- **frontmatter 规范**：概念页面必须含 YAML frontmatter——`type`（必填，自解释描述性短语，同类概念 type 值一致，如 modules/ 下统一"模块文档"）、`title`、`description`（一句话）、`tags`（列表）、`timestamp`（最后显著更新时刻，ISO 8601，必须取真实系统时间）、`resource`（对应具体源码资产时填相对路径 URI，抽象概念不填）；`index.md` / `log.md` / `changelog/*` 为保留文件，不加 frontmatter
- **维护规则**：修改代码后只更新受影响页面；删除过时描述而非留废弃标记；每页至少 1 条出站/入站相对路径链接；数值（工具数、gtest 数等）以运行时统计为准，改动后需重新核算
- **审计日期同步**：带页头"审计日期"行的页面（overview / build / tests / example / modules/tools_registry / tools_ops_a / support / core），内容修改后必须同步更新该日期为当天并标注变更原因（格式：`2026-08-16（2026-08-12 初稿；08-16 随 <变更> 同步）`）；无审计日期头的页面（conventions / entry_runtime / index 等）不新增该行
- **日志联动**：修改 wiki 页面或代码后，须在当天 `changelog/<YYYY-MM-DD>-log.md` 追加记录（含变更内容与验证结果）；漏记会在下次 `log.md` 摘要整理时发现
- **配对审计**：页面与代码的对应关系——core.md↔`src/core/`、entry_runtime.md↔`src/main.cpp`+`src/runtime/`、tools_registry.md↔`src/tools/`（register_all/dispatch/tool_catalog/schema_*）、tools_ops_a/b↔`src/tools/*_ops.cpp`、support.md↔`src/prompts/`+`src/resources/`+`src/ui/`+`src/util/`、build.md↔`CMakeLists.txt`+`cmake/`+`build.py`、tests.md↔`tests/`、example.md↔`Example/`
