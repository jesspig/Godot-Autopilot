# 测试体系说明

## 1. 概述

测试体系分两层：

- **L1 纯单测**（`gda_unit_tests`，269 个 gtest 用例 / 29 个测试文件；另加迁移守卫 `migration_guard` 1 项，`ctest --preset debug -E "^gda_runner_"` 共 270 项）：不启动引擎，不触碰 Godot API，验证核心逻辑与工具注册管线。
- **L2 配置驱动引擎内测试**（`gda_test_runner` + `tests/config/*.json` 用例）：由 C++ 执行器自管 Godot headless 编辑器进程，经真实 MCP HTTP 全链路驱动领域工具，并按 JSON 用例中的断言语义（C++ 执行器侧）校验响应。**每份 config/*.json = 一次独立的编辑器生命周期最小闭环**（启动 → MCP 就绪 → 执行步骤 → 停止进程），文件间互不共享状态。

架构一句话：进程内 GDExtension（EditorPlugin），领域工具经 `call_tool` 元工具代理，由 `register_all.cpp` 的 registry（`build_registry`，经 `refresh_derived` 派生 dispatch 映射）分发。

## 2. 前置条件

| 依赖 | 说明 |
| ---- | ---- |
| CMake 3.28+ | 测试目标经根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 选项引入；该开关已固化在 `CMakePresets.json` debug/release 预设（默认 ON），清理 `build/` 后重新配置自动恢复（裸 `cmake` 不带 preset 时默认 OFF，`CMakeLists.txt:168`） |
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

另有 L2 授权前置：涉及任意脚本/游戏运行时的用例（如 `00_meta` 的 code_execute、`04_resources_scripts` 的 execute_script、`16_game_jobs` 的 start_game_job、`23`/`24` 的游戏侧路径需 `game_runtime`，`18`/`22`/`26`/`27` 需 `code_execute`）需要 capability 授权——由 `GODOT_AUTOPILOT_ALLOW` 环境变量（如 `game_runtime`、`code_execute` 或 `all`）或 `user://godot_autopilot/config.json` 的 `allow` 字段提供，环境变量优先；缺失时相应用例被授权门拒绝而 FAIL。其中 `26_user_tools_code_mode` 与 `27_user_tools_rescan` 的用户工具注册/调用另受 `user_tools` 能力门保护：用例内 `AutopilotTools.set_enabled(true)` 写配置可自包含，但若 `GODOT_AUTOPILOT_ALLOW` 已设置且不含 `user_tools`/`all`，env 优先使 `set_enabled` 无效、用例失败（此时需在 env 中补 `user_tools` 或取消该 env）。

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

注册方式（`tests/CMakeLists.txt:118-130`）：**每份 `config/*.json` 一条 `gda_runner_<文件名去后缀>` 用例**，命令为 `gda_test_runner --file <name> --report-dir <build>/tests/output`，`TIMEOUT 600`（单文件含遍历约 2-4 分钟，超时防挂死）。当前 28 个 config 文件 → 28 条 ctest 用例（21 号段空缺，99 号诊断用例已删）：`gda_runner_00_meta`、`gda_runner_01_scene`、`gda_runner_02_property`、`gda_runner_03_tools_contract`、`gda_runner_04_resources_scripts`、`gda_runner_05_rename_references`、`gda_runner_06_move_references`、`gda_runner_07_scene_tabs`、`gda_runner_08_property_readback`、`gda_runner_09_editor_ui`、`gda_runner_10_editor_input`、`gda_runner_11_editor_tree`、`gda_runner_12_capture_params`、`gda_runner_13_inline_subresource`、`gda_runner_14_tilemap_rect`、`gda_runner_15_scene_path`、`gda_runner_16_game_jobs`、`gda_runner_17_vision_assist`、`gda_runner_18_script_freshness`、`gda_runner_19_cjk_roundtrip`、`gda_runner_20_cjk_text_roundtrip`、`gda_runner_22_uid_guard`、`gda_runner_23_click_ui_coords`、`gda_runner_24_keycode_alias`、`gda_runner_25_open_scene_idempotent`、`gda_runner_26_user_tools_code_mode`、`gda_runner_27_user_tools_rescan`、`gda_runner_28_sprite_frames_animation`。

- L1 经 `gtest_discover_tests` 注册，每用例一条（如 `CommandQueueTest.*`，269 条），迁移守卫 `migration_guard` 为独立 ctest 用例 1 条（`tests/CMakeLists.txt:132-136`，零依赖读源码断言，不启动引擎也无需构建产物）——L1 过滤后共 **270** 条；ctest 总注册点 **298** = 270（L1）+ 28（L2）。
- **耗时**：普通用例约 15s/文件（一次编辑器生命周期）；`03_tools_contract` 含两次全量遍历（330 个候选工具 ×2），为最慢单文件（约 2-3 分钟）。全量 ctest 总耗时随用例数与机器波动，以运行时统计为准。
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

### 5.5 迁移守卫（migration_guard）

- 位置：`tests/guard/migration_guard.cmake` + 清单 `tests/guard/migrated_domains.txt`（30 个域 + `strict` 行）。
- 内容：零依赖读源码断言，`strict` 模式下禁止旧机制回退——已迁移域与全部 `src/tools/*_tools.hpp` 不得含旧宏 `GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE`；不得残留 `src/tools/schema_*_ops.cpp`；`src/tools/*.hpp|*.cpp` 不得引用 `tool_input_schema` 或 `tools/tool_decl.hpp`。
- 运行：`ctest --preset debug -R migration_guard`（configure 后即可运行，不依赖构建产物），或直接 `cmake -DSOURCE_DIR=<仓库根> -P tests/guard/migration_guard.cmake`；实测输出 `OK (domains: 30, strict: on)`。

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

### 用例文件（`tests/config/`，28 个）

| 文件 | name | 内容 |
| ---- | ---- | ---- |
| `00_meta.json` | meta_tools | 15 步元工具语义（ping / search_tools / list_categories / get_tool_detail / call_tool / batch_execute / code_execute），全部无持久副作用 |
| `01_scene.json` | 01_scene | 场景节点创建/查询/删除/撤销，`before_all` 用 `create_editor_scene` 建干净根 Root |
| `02_property.json` | property_tools | 属性读写用例，含 readback MATCHED、int 字符串静默转 0、缺参报错；本轮新增 Node 引用（hint 34）转换、typed 数组元素转换与 fail fast、`property_get_list` 过滤参数 |
| `03_tools_contract.json` | tools_contract | 两个遍历步骤（empty_args + heuristic_smoke），运行时枚举 392 条（391 域 + system_status）、排除 62 条后对 330 个候选工具做契约与冒烟；含 reload_resource 空参契约 |
| `04_resources_scripts.json` | resources_scripts | execute_script 四种行为（单表达式自返 / 多行显式 return / 语法错误 / 缺参报错）+ 资源只读查询；本轮新增 save_resource copy-on-write、reload_resource、duplicate_resource name、copy_resource_file 用例 |
| `05_rename_references.json` | 05_rename_references | 单文件 rename 引用重写 + 目录 rename fail fast（错误指引 move_resource_file） |
| `06_move_references.json` | 06_move_references | 单文件与目录级 move 的引用重写验证（find_in_files / get_resource_references 双向） |
| `07_scene_tabs.json` | 07_scene_tabs | 场景脏状态闭环：dirty 场景阻塞 open、reload 清脏后恢复、reload 未打开报错、create_editor_scene timeout_ms 参数校验 |
| `08_property_readback.json` | property_readback | property_set 严格 JSON 形状回读与错误路径（Rect2 文档/别名形状、size 冲突/缺失、Transform2D 缺 columns）+ create_scene_node 属性应用与严格错误路径 |
| `09_editor_ui.json` | editor_ui | 编辑器 UI 只读工具契约与几何字段断言：get_editor_ui_elements 列表与 window 空间标记、get_editor_viewport_geometry 映射、hit_test_editor_point 命中链、get_display_window_rect 窗口矩形、get_scene_node_screen_rect 缺 paths 报错（headless 下不断言元素非空） |
| `10_editor_input.json` | editor_input | 复合输入工具的参数校验路径：click_input_mouse 缺 position、scroll_input_mouse 非法 direction、drag_input_mouse 缺 to、type_input_text 缺 text、click_editor_element 缺 path、run_editor_shortcut 未知键，均在注入事件前报 error |
| `11_editor_tree.json` | editor_tree | 编辑器场景树行枚举与选中：`scene_tree_items` 行结构（path/name/type/selected/rect）与 filter/selected_only/max_items 参数、`select_scene_tree_node` 按 path 选中并联动检查器（inspect/focus/add 追加选中）、缺参与无场景报错（before_all 建树 fixture，含选中副作用） |
| `12_capture_params.json` | capture_params | capture_editor_viewport / capture_game_viewport 新增参数的参数校验路径：after_frames 负数与非整数、when 目标帧表达式错误码、scale 越界等，错误即报不产生截图副作用 |
| `13_inline_subresource.json` | 13_inline_subresource | 资源属性内联描述一步创建 `[sub_resource]`（`{"type": ..., "properties": {...}}`）：property_set 与 create_scene_node 的 properties 内联子资源、响应回显 inline_resources、嵌套与错误路径 |
| `14_tilemap_rect.json` | tilemap_rect | `fill_tilemap_rect` 矩形铺砖：from/to 角点（含逆序）成功铺砖与擦除、alternative/layer 参数、>100000 格上限报错、单次 undo；fixture 经 create_tilemap_tileset + add_tilemap_atlas_source 构建 |
| `15_scene_path.json` | 15_scene_path | 写工具（create_scene_node/delete_scene_node/property_set/rename_scene_node/attach_script_to_node）响应回显 `scene_path`/`scene_unsaved`：未保存场景误建防护与错误路径 |
| `16_game_jobs.json` | game_jobs | `start_game_job` / `get_game_job` 的参数校验与 headless 运行时通道路径（op 仅 eval、job 表上限 16、过期宽限 2s）；**前置条件：需 `game_runtime` capability 授权**（`GODOT_AUTOPILOT_ALLOW` env 或 `user://godot_autopilot/config.json` 的 `allow` 字段，env 优先），未授权时用例 FAIL |
| `17_vision_assist.json` | vision_assist | 视觉辅助截图参数校验：`annotate_nodes`/`annotate_nodes_max` 范围、`diff_image` 前置条件（须配 `diff_against_last`）、`review_scene_visually` 参数校验与只读组合 |
| `18_script_freshness.json` | script_freshness | 脚本取用口径回归：`create_script` 后 `cache_refreshed`、覆写探针版本后 `attach_script_to_node`/`execute_script`/`get_script_property {fresh:true}` 均取磁盘新版；需 `code_execute` 授权 |
| `19_cjk_roundtrip.json` | cjk_roundtrip | CJK 文本落盘往返：含中文注释脚本 `create_script`（verified/readback/cache_refreshed）+ `read_file` 全量内容比对 + `find_in_files` 中文 query 命中；after_all 回收 `res://tests_tmp` |
| `20_cjk_text_roundtrip.json` | 20_cjk_text_roundtrip | 编辑器侧中文文本往返：`property_set` 写中文到 Label.text、`property_get` 读回逐字相等（`String::utf8` 链路回归）+ 缺失属性/缺失节点错误分支；`save_editor_scene_as` 落盘后 `close_editor_scene`，after_all 回收 `res://tests_tmp` |
| `22_uid_guard.json` | uid_guard | 资源 UID 守卫：保存资源与 `set_resource_uid` 显式/省略 uid 分支回验 + `get_game_log_entries` 配 filter 断言 `matched_lines`；需 `code_execute` 授权 |
| `23_click_ui_coords.json` | click_ui_coords | `click_game_ui_element` 参数校验与无游戏通道错误（error+hint）；坐标换算由 L1 `game_ui_coords_test` 覆盖，端到端断言待示例游戏恢复后补测；需 `game_runtime` 授权 |
| `24_keycode_alias.json` | keycode_alias | 键名→键码统一判定：编辑器侧裸名/`KEY_` 前缀/大小写归一/裸数字、未知名精确报错；游戏侧 `queue_game_input` 入参；探针动作前后清理；需 `game_runtime` 授权 |
| `25_open_scene_idempotent.json` | 25_open_scene_idempotent | `open_editor_scene` 幂等：首次打开 `ok` → `get_scene_tree` 确认 → 同路径二次打开 `already_open=true` → 无效路径仍报错 |
| `26_user_tools_code_mode.json` | user_tools_code_mode | AutopilotTools 用户脚本工具端到端：`code_execute` 内注册 `user_echo_probe` → MCP `call_tool` 回显 → `search_tools`/`get_tool_detail` 标记 `dynamic=true` → code-mode 直连 → `unregister_tool` 后 not found；需 `code_execute` + `user_tools` 授权（见第 3 节） |
| `27_user_tools_rescan.json` | user_tools_rescan | 目录扫描 rescan 端到端：`code_execute` 写 probe（`register_autopilot_tools(api)` 约定）→ `api.rescan` 注册非空 → MCP `call_tool` 回显 → 二次 rescan 幂等（`registered` 空、`failed` 存在）→ 清理；夹具目录 `res://gda_tmp_user_tools/`；需 `code_execute` + `user_tools` 授权（见第 3 节） |
| `28_sprite_frames_animation.json` | sprite_frames_animation | `create_scene_node` 属性两轮应用回归：同传 `sprite_frames`+`animation`，断言 `applied_properties==["sprite_frames","animation"]` 且读回 `animation=="idle"` |

## 7. 遍历与排除清单（`tests/runner/traversal.cpp`）

遍历模式：

- **工具来源**：运行时经 `search_tools` 空 query 枚举全部 catalog 工具名（`list_tool_names`，响应形如 `{"results":[{"name","score"},...]}`，跳过 7 个协议级元工具）。域工具 391 个（30 个 `<域>_tools.hpp` 全部经 ToolSpec 声明）+ system_status = **392 个**；枚举纳入的工具再经 `mutating`/`dynamic`/`side_effect` 字段判定排除（见下）。解析失败（响应非 JSON 对象/缺 results 数组/清单为空）抛异常
- **内置前置校验**：每个工具先经 `get_tool_detail` 校验存在性与工具名一致性（响应非 JSON 对象或工具名不匹配 → FAIL）
- **`empty_args`**（空参契约）：空对象调用。响应非 JSON 对象 → FAIL；返回 `error` 字段算"有错误响应"（统计 error 数，不 FAIL）；schema 声明必填但空参未报错 → 记 **warnings**（不 FAIL）
- **`heuristic_smoke`**（启发式冒烟）：按 schema properties 类型生成启发式参数（`integer`→0、`number`→0.0、`boolean`→false、`array`→`[]`、`object`→`{}`、其余→`"test"`）；无 properties 的工具（SCHEMA_NONE）跳过
- **崩溃检测**：遍历中每步调用后检查编辑器进程存活，进程死亡即 fatal_error（附 stdout/stderr 日志截断 2000 字符）
- 统计输出：调用总数 / 通过 / 失败 / result 数 / error 数 / "missing required" 数 / 跳过 / 排除

### 副作用驱动排除（`get_tool_detail.side_effect`）

副作用工具在 empty_args 与 heuristic_smoke 两个遍历中一律跳过。运行时枚举不区分工具来源：对每个工具先调 `get_tool_detail`，若返回的 `tool.side_effect` 非空、`tool.mutating` 为 true 或 `tool.dynamic` 为 true 即排除（计入 `excluded` 统计，见 `traversal.cpp`）。排除项共 **62 个**，全部由 `ToolSpec.side_effect` 与 `flags`（`tool_flags::kMutating`）承载：60 个 `side_effect` 非空，另有 `fill_tilemap_rect`、`build_nodes_from_spec` 两个工具 `side_effect` 为 None、仅以 `mutating` 排除（历史事故：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`；`save_editor_scene` 空参生成 `Example/NewNode.tscn`；`show_os_alert` 弹系统模态对话框；`set_display_clipboard` 覆盖系统剪贴板；`speak_display_tts` 系统朗读）。

**副作用分类（62 个排除项 = 60 个非空 `side_effect` + 2 个仅 mutating）**——按 `ToolSpec.side_effect` 返回值分布（`writes_file`/`writes_config`/`process`/`game_runtime`/`code_execute` 为持久或运行时副作用，`shows_alert`/`modifies_window` 会直接干扰用户桌面）：

| side_effect | 数量 | 分布（`*_tools.hpp`）与工具 |
| ---- | ---- | ---- |
| `writes_file` | 16 | editor 3（`save_editor_scene` / `save_editor_scenes` / `save_editor_scene_as`）、resource 4（`save_resource` / `copy_resource_file` / `move_resource_file` / `create_directory`）、script 2（`create_script` / `patch_script`）、theme 5（`create_theme_resource` / `set_theme_color` / `set_theme_constant` / `set_theme_font_size` / `set_theme_stylebox_flat`）、os 2（`write_file` / `move_os_file_to_trash`） |
| `writes_config` | 6 | config 2（`save_project_settings` / `set_editor_settings`）、editor 2（`set_editor_main_scene` / `set_editor_plugin_enabled`）、input_map 2（`save_input_map` / `add_input_map_action_event`） |
| `shows_alert` | 4 | os 1（`show_os_alert`）、display 3（`show_display_dialog` / `speak_display_tts` / `stop_display_tts`） |
| `modifies_window` | 20 | display 12（`create_display_window` / `delete_display_window` / `move_display_window_to_foreground` / `request_display_window_attention` / `set_display_clipboard` / `set_display_mouse_mode` / `set_display_window_flag` / `set_display_window_mode` / `set_display_window_position` / `set_display_window_size` / `set_display_window_title` / `warp_display_mouse`）、editor 4（`click_editor_element` / `type_editor_element_text` / `run_editor_shortcut` / `select_scene_tree_node`）、input 4（`click_input_mouse` / `scroll_input_mouse` / `drag_input_mouse` / `type_input_text`） |
| `process` | 6 | os 5（`create_os_process` / `execute_os_process` / `kill_os_process` / `open_os_path` / `set_os_environment`）、editor 1（`build_csharp_assembly`） |
| `game_runtime` | 7 | game 7（`execute_game_script` / `start_game_job` / `reload_game_scripts` / `queue_game_input` / `wait_game_input` / `sequence_game_inputs` / `click_game_ui_element`；`start_game_job` 为 09-16 批次新增） |
| `code_execute` | 1 | script 1（`execute_script`） |
| `None`（仅 mutating） | 2 | tilemap 1（`fill_tilemap_rect`，09-16 批次新增：flags 含 `tool_flags::kMutating` 使遍历跳过，`side_effect` 为 None——批量铺砖虽可撤销，仍不进入自动冒烟）、scene 1（`build_nodes_from_spec`，flags 含 `tool_flags::kMutating`，`side_effect` 为 None——声明式建树整体可回滚，仍不进入自动冒烟） |

### warnings 语义（4 条实测契约缺口，测试发现并验证，业务代码未改）

以下 4 个工具 schema 声明必填参数，但 handler 不校验，空参调用不报错——遍历记 warnings 而非失败：

| 工具 | 缺口 |
| ---- | ---- |
| `start_input_gamepad_vibration` | device/weak/strong 声明必填，handler 缺参时回退默认值 0 / 0.5 |
| `stop_input_gamepad_vibration` | device 声明必填，handler 缺参时回退默认值 0 |
| `get_resource_extensions` | 缺 type 时返回全类型列表 |
| `reimport_resource_files` | 空参时 count=0 静默成功 |

`create_scene_node`（name/type 有默认值）是否告警取决于运行期场景状态（场景是否已有根节点），本轮实测未触发。

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
- **编辑器 UI/输入自动化端到端**：L2 用例 schema 无跨步骤变量引用（无法把 `get_editor_ui_elements` 返回的 path 传给 `click_editor_element`），故点击/输入链路当前仅覆盖只读查询与参数校验路径；游戏侧新增输入能力（wheel 注入、单步 mouse_motion、`click_game_ui_element`）仅经编译与代码级验证，待 L3 落地后补测。
