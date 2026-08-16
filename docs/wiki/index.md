# GDA (Godot Autopilot) 项目知识库

本知识库与源码同步维护，所有事实以当前代码为准；数值以运行时统计为准。维护记录见 [changelog/](../../changelog/log.md)。

## 页面索引

| 页面 | 内容 | 对应代码 |
|---|---|---|
| [overview.md](overview.md) | 项目定位、整体架构、技术栈、目录结构、命名体系 | 全仓库 |
| [build.md](build.md) | 构建/部署/打包流程、CMake 模块、产物清单、环境变量 | `CMakeLists.txt`、`cmake/`、`build.py` |
| [tests.md](tests.md) | L1/L2 测试体系、遍历排除清单、数值统计 | `tests/` |
| [conventions.md](conventions.md) | 工程约定：命名、日志、错误模式、添加工具流程 | 全仓库 |
| [example.md](example.md) | Example 示例项目与文档 | `Example/` |
| [modules/core.md](modules/core.md) | 核心层：线程模型、端口、日志、配置常量 | `src/core/` |
| [modules/entry_runtime.md](modules/entry_runtime.md) | 插件入口与运行时桥接、gda 协议 | `src/main.cpp`、`src/runtime/` |
| [modules/tools_registry.md](modules/tools_registry.md) | 工具注册管线、元工具、schema 统计、契约缺口 | `src/tools/`（register_all/dispatch/tool_catalog/schema_*） |
| [modules/tools_ops_a.md](modules/tools_ops_a.md) | 领域工具 A 组（场景/属性/输入/物理/导航/资源等 13 模块，180 工具） | `src/tools/*_ops.cpp` |
| [modules/tools_ops_b.md](modules/tools_ops_b.md) | 领域工具 B 组（调试/显示/OS/运行时/音频/渲染/瓦片等 18 模块） | `src/tools/*_ops.cpp` |
| [modules/support.md](modules/support.md) | 提示词、MCP 资源、UI、工具库 | `src/prompts/`、`src/resources/`、`src/ui/`、`src/util/` |

## 关键数值速查（以运行时统计为准）

- 工具总数 **339** = 7 元工具 + 332 领域工具；`ToolCatalog` 343 条目；领域工具分 **23 类**（InputMap 并入 Input）
- schema：非空/空数以运行时统计为准（def 静态：SCHEMA_NONE=208 / SCHEMA_BASIC=124）；**3 个契约缺口**（create_scene_node、get_resource_extensions、reimport_resource_files）
- 遍历排除 **34 个副作用工具**；L1 单元测试 **61 个 gtest**；L2 用例 **5 个文件**
- 工具命名规范：`<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，如 create_scene_node、intersect_physics_2d_ray）
- MCP 端口 **9527**（`/mcp`），`GODOT_AUTOPILOT_PORT` 可覆盖；产物名 `godot-autopilot`

## 维护入口

代码改动后：更新受影响页面 → 同步页头"审计日期"（带日期行的页面：overview / build / tests / example / modules/tools_registry / tools_ops_a / support / core；无日期头的页面不新增）→ 重核数值 → 追加 [changelog/](../../changelog/log.md)（按天分文件，每条记录精确到小时）→ 同步 [AGENTS.md](../../AGENTS.md)。
