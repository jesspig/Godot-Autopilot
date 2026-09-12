# 测试体系说明

## 1. 概述

测试体系分两层：

- **L1 纯单测**（`gda_unit_tests`，114 个 gtest 用例）：不启动引擎，不触碰 Godot API，验证核心逻辑与工具注册管线。
- **L2 配置驱动引擎内测试**（`gda_test_runner` + `tests/config/*.json` 用例）：由 C++ 执行器自管 Godot headless 编辑器进程，经真实 MCP HTTP 全链路驱动领域工具，并按 JSON 用例中的断言语义（C++ 执行器侧）校验响应。**每份 config/*.json = 一次独立的编辑器生命周期最小闭环**（启动 → MCP 就绪 → 执行步骤 → 停止进程），文件间互不共享状态。

架构一句话：进程内 GDExtension（EditorPlugin），领域工具经 `call_tool` 元工具代理，由 `register_all.cpp` 的 `g_handlers` 映射分发。

## 2. 前置条件

| 依赖 | 说明 |
| ---- | ---- |
| CMake 3.28+ | 测试目标经根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 选项引入；该开关已固化在 `CMakePresets.json` debug/release 预设（默认 ON），清理 `build/` 后重新配置自动恢复（裸 `cmake` 不带 preset 时默认 OFF，`CMakeLists.txt:120`） |
| Ninja | 构建生成器（预设 `debug` / `release` 均为 Ninja） |
| clang-cl | 编译器（MSVC/GCC 自动回退，测试继承根配置） |
| Godot 可执行文件 | 仅 L2 需要，见第 3 节 |
| googletest | CMake FetchContent 自动拉取（v1.15.2，GIT_SHALLOW），无需手动安装；仅 L1 链接 |

## 3. 配置 Godot 路径

两种方式，**进程环境变量优先级高于 `.env` 文件**（`godot_process.cpp` 的 `resolve_godot_path()`：先读环境变量，为空才回退解析仓库根 `.env`）：

1. 仓库根 `.env` 文件：复制 `.env.template` 为 `.env` 并填写 `GODOT_PATH=`（`.env` 已被 .gitignore 忽略，不入库）
2. 进程环境变量（PowerShell）：

```powershell
$env:GODOT_PATH="C:\path\to\Godot.exe"
```

若二者皆无，`gda_test_runner` 退出码 2（"未找到 Godot 可执行文件"）。L1 不受影响。

## 4. 构建

```powershell
# 1. 配置（测试开关已固化在预设，无需传参）
cmake --preset debug

# 2. 构建测试目标
cmake --build --preset debug --target gda_unit_tests gda_test_runner
```

产物位于 `build/debug/tests/`：

- `gda_unit_tests.exe` — L1，gtest 可执行文件
- `gda_test_runner.exe` — L2，自驱动用例跑批器（CLI 入口 `tests/runner/main.cpp`）

## 5. 运行

### 5.1 全量（ctest）

```powershell
ctest --preset debug
```

注册方式（`tests/CMakeLists.txt:100-111`）：**每份 `config/*.json` 一条 `gda_runner_<文件名去后缀>` 用例**，命令为 `gda_test_runner --file <name> --report-dir <build>/tests/output`，`TIMEOUT 600`（单文件含遍历约 2-4 分钟，超时防挂死）。当前 8 个 config 文件 → 8 条 ctest 用例：`gda_runner_00_meta`、`gda_runner_01_scene`、`gda_runner_02_property`、`gda_runner_03_tools_contract`、`gda_runner_04_resources_scripts`、`gda_runner_05_rename_references`、`gda_runner_06_move_references`、`gda_runner_07_scene_tabs`。

- L1 经 `gtest_discover_tests` 注册，每用例一条（如 `CommandQueueTest.*`）。
- **耗时**：普通用例约 15s/文件（一次编辑器生命周期）；`03_tools_contract` 含两次全量遍历（316 领域工具 ×2），约 2-3 分钟。全量 ctest 约 3-4 分钟。
- 单跑一条：`ctest --preset debug -R gda_runner_00_meta` 或 `ctest --preset debug -R CommandQueueTest`。

### 5.2 单文件（直跑执行器）

```powershell
# --file 只跑指定用例文件，name 可含或不含 .json 后缀
build\debug\tests\gda_test_runner.exe --file 01_scene
```

### 5.3 CLI 参数全表（`tests/runner/main.cpp`）

| 参数 | 语义 |
| ---- | ---- |
| `--config-dir <dir>` | JSON 用例目录（默认 `PROJECT_ROOT/tests/config`） |
| `--file <name>` | 只跑指定用例文件，name 可含或不含 `.json` 后缀（默认跑目录下全部 `*.json`，按文件名排序；无匹配则退出码 2） |
| `--headless` | Godot 以 headless 模式启动（默认） |
| `--gui` | Godot 以窗口模式启动。`--headless` 与 `--gui` 互斥（同用报错退出码 2）；**用例 JSON 的 `headless` 字段优先于 CLI**，冲突时以用例为准并在 stderr 提示 |
| `--no-auto` | 不启动 Godot 进程；端口取自环境变量 `GODOT_AUTOPILOT_PORT`，TCP + MCP initialize 就绪后直连外部 MCP 服务跑用例（端口未设置或未就绪 → 退出码 2） |
| `--keep-open` | 全部文件跑完后不停止 Godot 进程（最后一个文件保留进程，便于人工排查） |
| `--report-dir <dir>` | 报告目录（默认 `PROJECT_ROOT/tests/output`，自动创建） |
| `--help` | 打印本说明并退出（退出码 0） |

**退出码语义**：

| 码 | 语义 |
| -- | ---- |
| 0 | 全部用例文件通过（无 FAIL / ERROR） |
| 1 | 存在失败（FAIL）或错误（ERROR，`fatal_error` 非空）的用例文件 |
| 2 | 参数或环境错误：未知参数、参数缺值、`--headless`/`--gui` 互斥、config-dir 不存在、`--file` 无匹配、目录无用例、`GODOT_PATH` 未配置、`--no-auto` 端口未设置/未就绪、未捕获异常 |

### 5.4 执行闭环与报告

每个文件的生命周期（`godot_process.cpp`）：随机空闲端口 → 首次 `--editor --import` 幂等同步执行（120s 超时，失败/超时不致命）→ 注入 `GODOT_AUTOPILOT_PORT` 后常驻启动 `--editor`（`--headless` 由用例决定）→ 就绪轮询（TCP 端口探测 + MCP initialize 握手）→ `before_all` → stages 步骤 → `after_all` → 停止（taskkill 软杀 → 5s 宽限 → TerminateProcess 兜底）。stdout/stderr 各接管道读线程持续消费（防 64KB 缓冲写满阻塞子进程），崩溃时截取最近 2000 字符日志。

输出：控制台表格（文件名称 / 通过步骤数 / 耗时（<10s 显毫秒，否则显秒）/ 状态 PASS|FAIL|ERROR）+ JSON 报告 `report-YYYYmmdd_HHMMSS.json`（字段：`generated_at` / `total_files` / `passed_files` / `files[].{name,passed,duration_ms,fatal_error,steps[]}`）。

## 6. JSON 用例 Schema（`tests/runner/config_loader.cpp` 冻结解析规则）

以下规则 1-8 为解析器强制校验，任何违规抛 `runtime_error`，错误消息含字段路径（如 `pipeline.stages[0].steps[1].expect.field_checks[0].key`）：

1. **顶层**：必填 `name`（string）、`pipeline`（object）；`description` / `headless` 可选，`headless` 默认 `true`
2. **`pipeline.on_failure`**：可选，默认 `"fail_fast"`，仅接受 `"fail_fast"`（失败即中止剩余步骤）| `"continue"`（失败继续）
3. **`pipeline.before_all` / `pipeline.after_all`**：steps 数组（可缺省），元素仅 `tool` 必填 + `args` 可选 + `id` 可选；**不支持 `traverse` 与 `expect`**（失败即整体 fatal_error，不判断言）
4. **`pipeline.stages`**：数组（可缺省），每项 `id` 可选 + `steps` 必填；所有 stages 的 steps **平铺进执行序列并保留顺序**
5. **步骤二选一**：`tool` 或 `traverse`（同含或缺省均报错）。`traverse` 步骤 `kind` 必填 `"domain_tools"`，`mode` 缺省 `"empty_args"`，仅接受 `"empty_args"` | `"heuristic_smoke"`（见第 7 节）
6. **`expect`**：`has_keys` 为 string 数组；`field_checks` 每项 `key` 必填（支持点路径）、`value` 可选任意 JSON、`not_empty` 可选 bool
7. **`args`**：值任意 JSON，嵌套原样保留
8. 数值字段类型严格校验

用例结构示例（节选自 `config/00_meta.json`）：

```json
{
  "name": "meta_tools",
  "description": "…",
  "headless": true,
  "pipeline": {
    "on_failure": "continue",
    "stages": [
      {
        "id": "main",
        "steps": [
          {
            "id": "ping_ok",
            "tool": "ping",
            "expect": {
              "field_checks": [ { "key": "result", "value": "pong" } ]
            }
          },
          {
            "id": "search_keyword",
            "tool": "search_tools",
            "args": { "query": "create_scene_node" },
            "expect": {
              "has_keys": ["results"],
              "field_checks": [ { "key": "results", "not_empty": true } ]
            }
          },
          {
            "id": "detail_existing",
            "tool": "get_tool_detail",
            "args": { "name": "create_scene_node" },
            "expect": {
              "field_checks": [
                { "key": "tool.name", "value": "create_scene_node" },
                { "key": "tool.input_schema.type", "value": "object" }
              ]
            }
          },
          {
            "id": "batch_sequential",
            "tool": "batch_execute",
            "args": {
              "operations": [
                { "tool": "ping", "args": {} },
                { "tool": "list_categories", "args": {} }
              ]
            },
            "expect": {
              "has_keys": ["results", "succeeded", "failed"],
              "field_checks": [
                { "key": "succeeded", "value": 2 },
                { "key": "failed", "value": 0 }
              ]
            }
          }
        ]
      }
    ]
  }
}
```

遍历步骤示例（`config/03_tools_contract.json`，两条步骤各一次跑完全部工具）：

```json
{
  "id": "empty_args_all",
  "traverse": { "kind": "domain_tools", "mode": "empty_args" }
},
{
  "id": "heuristic_smoke_all",
  "traverse": { "kind": "domain_tools", "mode": "heuristic_smoke" }
}
```

### 断言语义（`tests/runner/assert_engine.cpp`）

- **`has_keys`**：顶层键必须存在，缺失报 `missing top-level key(s): …`
- **`field_checks[].key`**：点路径分段解析（如 `tool.name`、`result.path`），任意段不存在即失败（`key '…' not found in response`）
- **`value` 数值兼容**：int / double 相互比较（统一转 double 判等，如期望 `2` 匹配实际 `2.0`）；string / bool 精确比较；其他类型按序列化字符串比较
- **`not_empty`**：string 非空；array/object `Size() > 0`；null 判空失败；其他标量视为非空
- 断言执行于 C++ 执行器侧，对 `call_tool` 的响应 JSON 校验；无 `expect` 时仅检查工具调用未返回 `error`

### 用例文件（`tests/config/`，8 个）

| 文件 | name | 内容 |
| ---- | ---- | ---- |
| `00_meta.json` | meta_tools | 15 步元工具语义（ping / search_tools / list_categories / get_tool_detail / call_tool / batch_execute / code_execute），全部无持久副作用 |
| `01_scene.json` | 01_scene | 场景节点创建/查询/删除/撤销，`before_all` 用 `create_editor_scene` 建干净根 Root |
| `02_property.json` | property_tools | 属性读写用例，含 readback MATCHED、int 字符串静默转 0、缺参报错；本轮新增 Node 引用（hint 34）转换、typed 数组元素转换与 fail fast、`property_get_list` 过滤参数 |
| `03_tools_contract.json` | tools_contract | 两个遍历步骤（empty_args + heuristic_smoke），全量 316 领域工具契约与冒烟；含 reload_resource 空参契约 |
| `04_resources_scripts.json` | resources_scripts | execute_script 四种行为（单表达式自返 / 多行显式 return / 语法错误 / 缺参报错）+ 资源只读查询；本轮新增 save_resource copy-on-write、reload_resource、duplicate_resource name、copy_resource_file 用例 |
| `05_rename_references.json` | 05_rename_references | 单文件 rename 引用重写 + 目录 rename fail fast（错误指引 move_resource_file） |
| `06_move_references.json` | 06_move_references | 单文件与目录级 move 的引用重写验证（find_in_files / get_resource_references 双向） |
| `07_scene_tabs.json` | 07_scene_tabs | 场景脏状态闭环：dirty 场景阻塞 open、reload 清脏后恢复、reload 未打开报错、create_editor_scene timeout_ms 参数校验 |

## 7. 遍历与排除清单（`tests/runner/traversal.cpp`）

遍历模式：

- **工具来源**：运行时枚举 `src/tools/*_tools.hpp` 中匹配 `GDA_TOOL_CLASS(` 的行（`parse_domain_tool_names`，按文件名遍历该目录）。域工具共 **365 个**；其中 49 个副作用工具使用前缀 `GDA_TOOL_CLASS_SIDE(`，解析器前缀匹配 `GDA_TOOL_CLASS(` 不将其纳入枚举（纳入枚举的工具另经 `side_effect` 字段兜底判定排除，见下）。解析失败（缺逗号/引号未闭合等）抛异常
- **内置前置校验**：每个工具先经 `get_tool_detail` 校验存在性与工具名一致性（响应非 JSON 对象或工具名不匹配 → FAIL）
- **`empty_args`**（空参契约）：空对象调用。响应非 JSON 对象 → FAIL；返回 `error` 字段算"有错误响应"（统计 error 数，不 FAIL）；schema 声明必填但空参未报错 → 记 **warnings**（不 FAIL）
- **`heuristic_smoke`**（启发式冒烟）：按 schema properties 类型生成启发式参数（`integer`→0、`number`→0.0、`boolean`→false、`array`→`[]`、`object`→`{}`、其余→`"test"`）；无 properties 的工具（SCHEMA_NONE）跳过
- **崩溃检测**：遍历中每步调用后检查编辑器进程存活，进程死亡即 fatal_error（附 stdout/stderr 日志截断 2000 字符）
- 统计输出：调用总数 / 通过 / 失败 / result 数 / error 数 / "missing required" 数 / 跳过 / 排除

### 副作用驱动排除（`get_tool_detail.side_effect`）

副作用工具在 empty_args 与 heuristic_smoke 两个遍历中一律跳过。解析器 `parse_domain_tool_names` 仅匹配前缀 `GDA_TOOL_CLASS(`，以 `GDA_TOOL_CLASS_SIDE(` 声明的副作用工具不进入枚举；对纳入枚举的工具还会先调 `get_tool_detail`，若返回的 `tool.side_effect` 字段非空即排除（计入 `excluded` 统计，见 `traversal.cpp`）。副作用工具在 `*_tools.hpp` 中用以 `GDA_TOOL_CLASS_SIDE(` 声明并实现 `ISideEffect::side_effects()`，共 **49 个**（历史事故：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`；`save_editor_scene` 空参生成 `Example/NewNode.tscn`；`show_os_alert` 弹系统模态对话框；`set_display_clipboard` 覆盖系统剪贴板；`speak_display_tts` 系统朗读）。

**副作用分类（49）**——按 `ISideEffect::side_effects()` 返回值分布（`writes_file`/`writes_config`/`process`/`game_runtime`/`code_execute` 为持久或运行时副作用，`shows_alert`/`modifies_window` 会直接干扰用户桌面）：

| side_effect | 数量 | 分布（`*_tools.hpp`）与工具 |
| ---- | ---- | ---- |
| `writes_file` | 15 | editor 3（`save_editor_scene` / `save_editor_scenes` / `save_editor_scene_as`）、resource 4（`save_resource` / `copy_resource_file` / `move_resource_file` / `create_directory`）、script 1（`create_script`）、theme 5（`create_theme_resource` / `set_theme_color` / `set_theme_constant` / `set_theme_font_size` / `set_theme_stylebox_flat`）、os 2（`write_file` / `move_os_file_to_trash`） |
| `writes_config` | 6 | config 2（`save_project_settings` / `set_editor_settings`）、editor 2（`set_editor_main_scene` / `set_editor_plugin_enabled`）、input_map 2（`save_input_map` / `add_input_map_action_event`） |
| `shows_alert` | 4 | os 1（`show_os_alert`）、display 3（`show_display_dialog` / `speak_display_tts` / `stop_display_tts`） |
| `modifies_window` | 12 | display 12（`create_display_window` / `delete_display_window` / `move_display_window_to_foreground` / `request_display_window_attention` / `set_display_clipboard` / `set_display_mouse_mode` / `set_display_window_flag` / `set_display_window_mode` / `set_display_window_position` / `set_display_window_size` / `set_display_window_title` / `warp_display_mouse`） |
| `process` | 6 | os 5（`create_os_process` / `execute_os_process` / `kill_os_process` / `open_os_path` / `set_os_environment`）、editor 1（`build_csharp_assembly`） |
| `game_runtime` | 5 | game 5（`execute_game_script` / `reload_game_scripts` / `queue_game_input` / `wait_game_input` / `sequence_game_inputs`） |
| `code_execute` | 1 | script 1（`execute_script`） |

### warnings 语义（3 个已知契约缺口，测试发现并验证，业务代码未改）

以下 3 个工具 schema 声明必填参数，但 handler 不校验，空参调用不报错——遍历记 warnings 而非失败：

| 工具 | 缺口 |
| ---- | ---- |
| `create_scene_node` | name / type 有默认值，不校验必填 |
| `get_resource_extensions` | 缺 type 时返回全类型列表 |
| `reimport_resource_files` | 空参时 count=0 静默成功 |

## 8. 已知引擎副作用

L2 每次运行后，`Example/project.godot` 可能被引擎自动追加 `[audio]` 段（`buses/default_bus_layout="uid://c6hb2nshs2igl"`），并生成 `Example/default_bus_layout.tres`。这是 headless 编辑器自身的自动保存行为，**非测试工具写入**，无害（不弹窗、不影响功能），但会弄脏 git 工作区。清理方式：

```powershell
git checkout -- Example/project.godot
Remove-Item Example/default_bus_layout.tres
```

## 9. 与参考项目（GodotMind-Archive）的差异

本体系为 GodotMind-Archive 测试方案（Python 编排 + `tests/yaml_tests/*.yaml` + 引擎内 `/run-tests` 端点）的 C++ 重实现，主要差异：

| 维度 | GodotMind-Archive | 本体系（GDA） |
| ---- | ---- | ---- |
| 用例格式 | YAML（`yaml_tests/*.yaml`） | JSON（`tests/config/*.json`） |
| 断言位置 | 引擎内 C++ `/run-tests` 端点执行断言 | **C++ 执行器侧**（`assert_engine.cpp`）解析响应校验 |
| 传输方式 | 整文件一次 `POST /run-tests`（`Content-Type: application/x-yaml`） | **每 step 一次 HTTP 往返**：经 `call_tool` 元工具调用（`integration/mcp_test_client.cpp`），独立响应逐条断言 |
| 进程管理 | Python 编排器（`test_orchestrator.py` + `godot_manager.py`）管理 Godot | 执行器自管：随机空闲端口、MCP initialize 握手就绪、taskkill→TerminateProcess 兜底、管道捕获崩溃日志 |
| 健康检查 | MCP ping + `/run-tests` 双端点 | TCP 端口探测 + MCP initialize 握手 |
| 闭环 | 文件间共享编辑器会话（分批/头less 分组复用） | **每 config 文件一次独立编辑器生命周期最小闭环** |
| 配置 | `tests/.env.example` | 根 `.env.template`（`GODOT_PATH`，进程环境变量优先级更高） |
| 报告 | Markdown 报告（`output/report-*.md`） | 控制台表格 + JSON 报告（`tests/output/report-*.json`） |

两者均为配置驱动、声明式用例（`pipeline.stages[].steps[].expect`），`headless`/`on_failure`/`before_all`/`after_all` 语义对齐。

## 10. 后续计划

- **L3 双进程 E2E**：`game_bridge` 通道（编辑器进程 ↔ 游戏运行进程）未纳入首期测试，规划中。
