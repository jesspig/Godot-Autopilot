# 测试运行说明

## 1. 概述

测试体系分两层：

- **L1 纯单测**（`gsd_unit_tests`，59 个用例）：不启动引擎，不触碰 Godot API，验证核心逻辑与工具注册管线。
- **L2 引擎内集成**（`gsd_engine_tests`，42 个用例）：启动真实 headless 编辑器进程，经真实 MCP HTTP 全链路验证领域工具行为。

架构一句话：进程内 GDExtension（EditorPlugin），领域工具经 `call_tool` 元工具代理，由 `register_all.cpp` 的 `g_handlers` 映射分发。

## 2. 前置条件

| 依赖 | 说明 |
| ---- | ---- |
| CMake 3.28+ | 测试目标经根 `CMakeLists.txt` 的 `GSD_ENABLE_TESTS` 选项（默认 OFF）引入 |
| Ninja | 构建生成器 |
| clang-cl | 编译器（MSVC/GCC 自动回退，测试继承根配置） |
| Godot 可执行文件 | 仅 L2 需要，见第 3 节 |
| googletest | CMake FetchContent 自动拉取（v1.15.2），无需手动安装 |

## 3. 配置 Godot 路径

两种方式，**进程环境变量优先级高于 `.env` 文件**：

1. 仓库根 `.env` 文件：复制 `.env.template` 为 `.env` 并填写 `GODOT_PATH=`（`.env` 已被 .gitignore 忽略，不入库）
2. 进程环境变量（PowerShell）：

```powershell
$env:GODOT_PATH="C:\path\to\Godot.exe"
```

若二者皆无，L2 的 42 个用例将自动 Skipped（不失败），L1 不受影响。

## 4. 构建与运行

```powershell
# 1. 配置（启用测试）
cmake --preset debug -DGSD_ENABLE_TESTS=ON

# 2. 构建测试目标
cmake --build --preset debug --target gsd_unit_tests gsd_engine_tests

# 3. 运行全部测试
ctest --test-dir build/debug
```

注意事项：

- **L2 耗时**：`ToolSchemaContractTest` 每个用例独立编辑器实例，每用例约 16–25s，全量 L2 约 12–13 分钟。
- **L2 自动跳过**：未配置 `GODOT_PATH` 时 L2 用例 Skipped，无需额外开关。
- **首次运行**：编辑器首次扫描会生成 `.godot/extension_list.cfg`（fixture 自动执行 `--headless --editor --import`，幂等）。
- **端口隔离**：MCP 默认端口 9527，测试用随机端口 + `GODOT_SELF_DRIVING_PORT` 环境变量注入，并行运行互不干扰。

### 引擎副作用与工具污染防护

- **引擎自动保存副作用**：L2 每次运行后，`Example/project.godot` 可能被引擎自动追加 `[audio]` 段（`buses/default_bus_layout="uid://c6hb2nshs2igl"`），并生成 `Example/default_bus_layout.tres`。这是 headless 编辑器自身的自动保存行为，**非测试工具写入**，无害（不弹窗、不影响功能），但会弄脏 git 工作区。清理方式：

  ```powershell
  git checkout -- Example/project.godot
  Remove-Item Example/default_bus_layout.tres
  ```

- **持久副作用工具排除清单**：`ToolSchemaContractTest` 遍历调用工具时跳过 35 个有持久副作用的工具，避免污染 Example 项目或干扰用户桌面环境：
  - **持久项目/文件副作用 (13)**（写项目配置 / 写文件 / 保存场景）：`editor_set_main_scene`、`editor_save_scene`、`editor_save_scene_as`、`editor_save_all_scenes`、`editor_new_text_resource`、`editor_set_plugin_enabled`、`project_settings_save`、`input_map_action_add_event`、`input_map_persist`、`editor_settings_set`、`file_write`、`script_create`、`resource_save`
  - **用户可见副作用 (22)**（弹窗、改鼠标/窗口状态、启动进程、播放声音等，会干扰用户桌面环境）：
    - 弹窗与对话框 (2)：`os_alert`（弹窗）、`display_dialog_show`（系统对话框）
    - 进程与系统执行 (5)：`os_create_process`、`os_execute`、`os_kill`、`os_shell_open`、`os_move_to_trash`
    - 环境变量 (1)：`os_set_environment`
    - 音频/语音 (2)：`display_tts_speak`、`display_tts_stop`
    - 剪贴板/鼠标 (3)：`display_clipboard_set`、`display_mouse_set_mode`、`display_mouse_warp`
    - 窗口操作 (9)：`display_window_set_title`、`display_window_set_position`、`display_window_set_size`、`display_window_set_mode`、`display_window_set_flag`、`display_window_move_to_foreground`、`display_window_request_attention`、`display_window_create`、`display_window_delete`
- **历史污染事件**：曾发生工具调用写入 `main_scene="test"` 导致用户打开项目弹"主场景缺失"对话框、并生成 `NewNode.tscn` 的污染事故；另有 `os_alert` 弹窗（标题/内容为启发式参数 'test'）干扰用户桌面事故。均已通过上述排除清单修复，遍历不再产生工具级污染。

### 只跑单层

只跑 L1（排除 L2 四个套件）：

```powershell
ctest --test-dir build/debug -R "^(?!.*(MetaToolsTest|SceneToolsTest|PropertyToolsTest|ToolSchemaContractTest)).*"
```

或按套件名精确筛选，例如只跑 L1 的 `CommandQueueTest`：

```powershell
ctest --test-dir build/debug -R "CommandQueueTest"
```

## 5. 测试套件明细

### L1 纯单测（`gsd_unit_tests`，59 用例，不启动引擎）

| 套件 | 用例数 | 覆盖内容 |
| ---- | ------ | -------- |
| CommandQueueTest | 7 | 命令队列线程安全与主线程排空 |
| Bm25IndexTest | 14 | BM25 索引构建与检索 |
| LogSystemTest | 6 | 结构化日志系统 |
| ToolCatalogTest | 7 | 工具目录查询与过滤 |
| SchemaBuilderTest | 10 | JSON Schema 构建 |
| ErrorUtilTest | 2 | 错误工具函数 |
| RegisteredServerFixture | 13 | 用 mcp-cpp-sdk InMemoryTransport 验证 355 工具注册管线：7 元工具注册 + catalog 356 条目 + schema 统计（283 非空 / 73 空）+ Bm25 填充 356 + call_handler 错误路径 |

### L2 引擎内集成（`gsd_engine_tests`，42 用例，真实 headless 编辑器 + 真实 MCP HTTP 全链路）

| 套件 | 用例数 | 覆盖内容 |
| ---- | ------ | -------- |
| MetaToolsTest | 15 | 元工具（ping / search_tools / list_categories / get_tool_detail / call_tool / batch_execute / code_execute） |
| SceneToolsTest | 12 | 场景节点创建与操作 |
| PropertyToolsTest | 11 | 属性读写，含 readback MATCHED / CONVERTED / REJECTED 行为 |
| ToolSchemaContractTest | 4 | 对 348 个领域工具做空参数契约遍历 + 启发式合法参数冒烟 + 崩溃检测（遍历运行时解析 `src/tools/tool_defs.def`） |

架构要点：领域工具（348 个）不直接注册到 MCP server（`tools/list` 只返回 7 个元工具），经 `call_tool` 代理分发。

## 6. 已知发现

### 契约缺口（测试发现并验证，业务代码未改）

以下 3 个工具 schema 声明必填参数，但 handler 不校验：

| 工具 | 缺口 |
| ---- | ---- |
| `scene_node_create` | name / type 有默认值，不校验必填 |
| `resource_get_extensions` | 缺 type 时返回全类型列表 |
| `resource_reimport` | 空参时 count=0 静默成功 |

遍历套件（`ToolSchemaContractTest`）对这些输出**警告清单而非失败**，崩溃检测仍生效。

### 其他发现

- `Bm25Index` 的 `add_entry` 非幂等：重复添加同一条目会使检索得分失真，调用方需自行保证去重。
- `register_all.cpp` 存在死代码特例（保留引用全部 `handle_xxx` 符号的链接依赖，测试链接必需）。

## 7. 后续计划

- **L3 双进程 E2E**：`game_bridge` 通道（编辑器进程 ↔ 游戏运行进程）未纳入首期测试，规划中。
