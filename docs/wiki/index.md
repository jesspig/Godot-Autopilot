# GDA (Godot Autopilot) 项目知识库

本知识库与源码同步维护，所有事实以当前代码为准；数值以运行时统计为准。维护记录见 [changelog/log.md](changelog/log.md)。

## 页面索引

| 页面 | 内容 | 对应代码 |
|---|---|---|
| [overview.md](overview.md) | 项目定位、整体架构、技术栈、目录结构、命名体系 | 全仓库 |
| [build.md](build.md) | 构建/部署/打包流程、CMake 模块、产物清单、环境变量 | `CMakeLists.txt`、`cmake/`、`build.py` |
| [tests.md](tests.md) | L1/L2 测试体系、遍历排除清单、数值统计 | `tests/` |
| [conventions.md](conventions.md) | 工程约定：命名、日志、错误模式、添加工具流程 | 全仓库 |
| [security_contract.md](security_contract.md) | T0 安全边界与并发契约：可信客户端、监听、风险工具、主线程、生命周期与边界原则 | `src/core/`、`src/tools/`、`src/main.cpp` |
| [example.md](example.md) | Example 示例项目与文档 | `Example/` |
| [tool_base_design.md](tool_base_design.md) | ToolBase 工具统一标准化（接口 + 组合 + 真类化）：385 域工具全为独立 ToolBase 子类、ToolRegistry 唯一来源、tool_defs.def 删除 | `tool_base.hpp`、`tool_registry.hpp`、`fn_tool.hpp`、`tool_decl.hpp`、30 个域 `*_tools.hpp`、`register_all.cpp` |
| [modules/core.md](modules/core.md) | 核心层：线程模型、端口、日志、配置常量 | `src/core/` |
| [modules/entry_runtime.md](modules/entry_runtime.md) | 插件入口与运行时桥接、gda 协议 | `src/main.cpp`、`src/runtime/` |
| [modules/tools_registry.md](modules/tools_registry.md) | 工具注册管线、元工具、schema 统计、契约缺口 | `src/tools/`（register_all/dispatch/tool_catalog/schema_*） |
| [modules/tools_ops_a.md](modules/tools_ops_a.md) | 领域工具 A 组（场景/属性/输入/物理/导航/资源/脚本/配置/文档/编辑器 16 模块，193 工具） | `src/tools/*_ops.cpp` |
| [modules/tools_ops_b.md](modules/tools_ops_b.md) | 领域工具 B 组（调试/显示/OS/运行时/音频/渲染/瓦片等 18 模块，192 工具；A 193 + B 192 = 385） | `src/tools/*_ops.cpp` |
| [modules/support.md](modules/support.md) | 提示词、MCP 资源、UI、工具库 | `src/prompts/`、`src/resources/`、`src/ui/`、`src/util/` |
| [plans/roadmap.md](plans/roadmap.md) | 竞品对齐路线图：竞品定位速览与 P0-P3 批次交付状态 | 全仓库 |

## 关键数值速查（以运行时统计为准）

- 工具注册总入口 `ToolRegistry`（单一来源）：**393 条目** = 385 域工具 + `system_status` + 7 元工具；域工具分 **27 类**（InputMap 并入 Input）；MCP 可达工具总数 **392** = 7 元 + 385 域
- schema：非空/空数以运行时统计为准；`03_tools_contract` 遍历 warnings 实测 **4 条**（start/stop_input_gamepad_vibration、get_resource_extensions、reimport_resource_files；create_scene_node 是否告警视运行期场景状态）
- 遍历排除 **60 个 SIDE 宏声明工具**（域 385 个中解析器仅按 `GDA_TOOL_CLASS(` 枚举 325 个，`GDA_TOOL_CLASS_SIDE` 60 个不进入枚举，其中 `fill_tilemap_rect` 的 side_effect 为 None，side_effect 字段兜底判定保留）；L1 单元测试 **267 个 gtest**（25 个 unit 文件）；L2 引擎用例 **25 个文件**（00_meta-10_editor_input 11 份 + 11_editor_tree/12_capture_params/13_inline_subresource/14_tilemap_rect/15_scene_path/16_game_jobs/17_vision_assist + 18_script_freshness/19_cjk_roundtrip/22_uid_guard/23_click_ui_coords/24_keycode_alias/25_open_scene_idempotent/28_sprite_frames_animation）；当前 ctest 注册点为 **292**（267 L1 + 25 L2，L2 需引擎环境与 `GODOT_AUTOPILOT_ALLOW`）
- 工具命名规范：`<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，如 create_scene_node、intersect_physics_2d_ray）
- MCP 端口 **9527**（`/mcp`），`GODOT_AUTOPILOT_PORT` 可覆盖；产物名 `godot-autopilot`

## 维护入口

代码改动后：更新受影响页面 → 同步页头"审计日期"（带日期行的页面：overview / build / tests / example / modules/tools_registry / tools_ops_a / support / core；无日期头的页面不新增）→ 更新 frontmatter 的 `timestamp`（真实系统时间，ISO 8601）→ 重核数值 → 追加 `changelog/<YYYY-MM-DD>-log.md`（按小时记录，[log.md](changelog/log.md) 仅留最近 7 天）→ 同步 [AGENTS.md](../../AGENTS.md)。
