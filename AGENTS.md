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
- **领域工具（~306 个）**：通过 `call_tool` 代理，由 `register_all.cpp` 中的 `g_handlers` 映射分发。完整 schema 在 `ToolCatalog` 中
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
- **依赖**：godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.2.1 — 使用 FetchContent，不依赖子模块

## 知识局限

- **无 CI**（`.github/` 不存在）
- **无测试套件或 lint 配置**（googletest 在 `_deps/` 中但从未使用）
- **~299/313 工具 schema 为** `mcp::JsonValue(object_tag)`（空对象），仅 ~14 个工具拥有完整 JSON Schema。这是已知缺口
- **详细架构规划**见 `docs/plan/`，但那是计划而非当前实现
