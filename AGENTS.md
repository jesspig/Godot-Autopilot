# AGENTS.md

## 构建

- **`uv run build.py`** — Debug: 配置 + 构建 + 部署到 `Example/addons/godot-self-driving/`
- **`uv run build.py --release`** — Release: 先清理，再配置 + 构建 + 部署
- 手动: `cmake --preset debug && cmake --build --preset debug`
- 预设 (`CMakePresets.json`): `debug`, `release`（均为 Ninja）
- **切勿删除 `build/<preset>/_deps/`** — 缓存已获取的依赖项（godot-cpp、mcp-cpp-sdk、libhv、simdjson、googletest）
- **添加新 .cpp 时必须在 `CMakeLists.txt` 的 `add_library()` 中加入**，否则 Unity 构建也不会包含

## 架构

- **进程内 GDExtension**，在 `MODULE_INITIALIZATION_LEVEL_EDITOR` 阶段加载到 Godot 编辑器
- **线程模型**：HTTP 线程（libhv）→ `CommandQueue::submit()` → Godot 主线程（在 `_process()` 中排空）
- **所有 Godot API 调用必须通过 `queue.submit()`** — 从 HTTP 线程直接调用会崩溃
- **MCP 端口**：9527，端点 `/mcp`。通过 `GODOT_SELF_DRIVING_PORT` 环境变量覆盖
- **元工具（7 个）**：直接在 `server.RegisterTool()` 注册，不经过 `call_tool`：
  `ping`、`search_tools`、`list_categories`、`get_tool_detail`、`call_tool`、`batch_execute`、`code_execute`
- **领域工具（348 个，数量随插件版本变化，以 MCP search_tools 返回为准）**：通过 `call_tool` 代理，由 `register_all.cpp` 中的 `g_handlers` 映射分发。完整 schema 在 `ToolCatalog` 中
- **工具总数**：355 = 7 元 + 348 领域（数量随插件版本变化，以 MCP search_tools 返回为准）；`ToolCatalog` 条目 356（另含 `system_status` 与 3 个 meta 快照）
- **入口点**：`src/main.cpp` → `GDExtensionEntryPoint` → 注册 `GodotSelfDrivingPlugin(EditorPlugin)` 并启动 `ServerContext`
- **关键工具函数**：`VariantJson::serialize/deserialize` (`util/variant_json.hpp`) 用于 `godot::Variant` ↔ `mcp::JsonValue` 互转
- **错误模式**：领域工具返回 `{"error": "消息"}` JSON；`call_tool` 元工具检查 `error` 字段并设 `is_error = true`

## 添加工具

需要四处修改：

1. 在 `src/tools/<category>_ops.hpp` 声明：`mcp::JsonValue handle_xxx(const mcp::JsonValue& args);`，命名空间 `godot_self_driving::<category>_ops`
2. 在 `src/tools/<category>_ops.cpp` 实现处理函数
3. 在 `register_all.cpp` 中注册：`g_handlers["tool_name"] = <category>_ops::handle_func;` + `catalog.add_tool({"name", "desc", "Category", {"tags..."}, schema});`
4. 在 `CMakeLists.txt` 的 `add_library()` 中添加新的 `.cpp`

**命名约定**：`<category>_<action>_<subaction>`（snake_case，如 `physics_2d_ray_cast`、`scene_node_create`）

## 关键约定

- **命名空间**：`godot_self_driving`
- **日志类别**（仅此几个）：`System`、`Transport`、`Tools`、`Resources`、`Prompts`
- **编译器**：优先 Clang/clang-cl，自动检测；MSVC/GCC 回退
- **优化**：自动 sccache/ccache、LTO（Release 使用 ThinLTO/LTCG/IPO）、Unity 构建、Ninja 作业池 — 均根据硬件自适应，可通过 `GSD_COMPILE_JOBS` / `GSD_LINK_JOBS` 等环境变量覆盖
- **依赖**：godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.2.2 — 使用 FetchContent，不依赖子模块

## 测试

- **启用**：`cmake --preset debug -DGSD_ENABLE_TESTS=ON`（默认 OFF；release 同）
- **运行**：`ctest --preset debug`（L1 秒级；L2 全量约 3-4 分钟，需 Godot 路径）；单文件：`build/debug/tests/gsd_test_runner.exe --file 01_scene`
- **结构**：L1 = `gsd_unit_tests`（59 个 gtest，不启动引擎，含 355 工具注册管线断言，数量随插件版本变化）；L2 = `gsd_test_runner` + `tests/config/*.json`（5 个用例文件，每文件一次 headless 编辑器生命周期最小闭环，经真实 MCP HTTP）
- **新增 JSON 用例 = 新增 `tests/config/*.json`，零 C++ 改动**；schema / CLI / 排除清单全量文档在 `tests/README.md`
- **Godot 路径**：环境变量 `GODOT_PATH` 或仓库根 `.env`（复制 `.env.template`）；缺失时 L2 全部失败/跳过
- **全工具遍历**：`03_tools_contract.json` 对 348 领域工具做空参契约 + 启发式冒烟（数量随插件版本变化，以 MCP search_tools 返回为准；560 步，约 15s）
- **35 个副作用工具被遍历排除**（`os_alert` / `file_write` / `editor_save_scene` / `display_*` 等，清单在 `tests/runner/traversal.cpp`）——新增工具若写配置/文件/弹窗/改窗口，必须同步加入排除清单，否则遍历会污染 Example 项目或干扰桌面
- **引擎副作用**：L2 运行后 `Example/project.godot` 会被追加 `[audio]` 段并生成 `default_bus_layout.tres`（headless 编辑器自动保存，无害；`git checkout -- Example/project.godot` 清理）

## 知识局限

- **无 CI**（`.github/` 不存在）
- **schema 覆盖**：283 非空 / 73 空（数量随插件版本变化，以 MCP search_tools 返回为准；由 `tests/unit/register_all_test.cpp` 的 `SchemaStatisticsBaseline` 运行时统计断言，不硬编码数量）；**3 个契约缺口**：`scene_node_create`（name/type 有默认值不校验必填）、`resource_get_extensions`（缺 type 返回全类型）、`resource_reimport`（空参 count=0 静默成功）——遍历测试记 warnings 不 FAIL
- **目标引擎版本**：Godot 4.7；常见 API 迁移事实：`TileSet.get_tile_data` 属 `TileSetAtlasSource.get_tile_data(source_id→atlas_coords, alternative)`；`AnimatedSprite2D` 属性名为 `sprite_frames`（`frames` 自 4.0 起更名）；`motion_mode` 枚举 GROUNDED=0/FLOATING=1
- **详细架构规划**见 `docs/plan/`，但那是计划而非当前实现

## 协作约定

- **多代理迭代时：子代理只写代码不编译，主代理统一 configure + build 再分批运行测试**（并行编译会锁）
- 新增 `src/*.cpp` 时同步更新三处：业务 `add_library()`（根 `CMakeLists.txt`）、`tests/CMakeLists.txt` 的 `GSD_UNIT_BUSINESS_SOURCES`（L1 链接需要，漏了会 undefined symbol）
- **L1 测试禁止调用任何已注册工具 handler**（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃）；`register_all_test` 只能测注册/分发/未知工具错误路径
- 编译选项：`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<path>` 可在网络不可用时复用本地已拉取的 googletest（`build/debug/_deps/googletest-src`）
