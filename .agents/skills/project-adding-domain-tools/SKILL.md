---
name: project-adding-domain-tools
description: 在 Godot Autopilot 新增一个领域 MCP 工具（ops handler + ToolSpec 注册），或调整工具的 side_effect/flags/命名/schema 时使用；写 handler 内部逻辑纪律看 project-writing-tool-handlers。
---

# 新增领域工具

## 适用范围

- 用于：新增 `<动词>_...` 领域工具、给既有工具改分类/参数表/flags/side_effect、补齐工具注册联动。
- 不适用：MCP 协议级 7 个元工具（`register_all.cpp` 内手写，改它们要同时看 catalog/BM25 派生）；用户脚本动态工具（GDScript 侧 `AutopilotTools` 注册，机制不同）；只补测试用例（`project-running-tests`）。

## 前置条件

- 先定位归属域（`src/tools/<域>_ops.cpp` / `<域>_tools.hpp`，共 30 域）。目标 Godot API 的 4.7 口径先在 `docs/wiki/conventions.md` 知识局限段核对，别按 3.x 或 4.8 记忆写。
- 确认是否已有近义工具：经 `search_tools`（空 query 可枚举全量）或 grep `<域>_tools.hpp` 里的工具名，避免新增重复能力。

## 流程

1. `src/tools/<域>_ops.hpp` 声明 `mcp::JsonValue handle_xxx(const mcp::JsonValue &args);`，`<域>_ops.cpp` 实现。
2. `<域>_tools.hpp` 匿名 namespace 里写参数表 `const std::vector<ParamSpec> kXxxParams = {{"name","string","描述",required_bool}, ...};`，在 `make_tools()` 里 `v.push_back(make_spec_tool(ToolSpec{name, description, category, {tags}, side_effect, flags, kXxxParams, <域>_ops::handle_xxx}));`，并把 `v.reserve()` 调到实际条数。
3. `register_all.cpp` 已经通过各域 `make_tools()` 自动注册，**不需要手改注册表**；只有新建了一个域文件才动这里。
4. 新增 `.cpp` 才需要登记构建（见 `project-building-gda`）；只加 `*_tools.hpp`/`*_ops.hpp` 与在既有 `.cpp` 里加实现都不用。
5. 走 `project-writing-tool-handlers` 的实现纪律，然后按下面"联动清单"逐项收尾。

## 规则与边界

- **命名**：`<动词>_<类别>_<维度>_<对象>_<修饰>` snake_case，动词置首（`create_scene_node`、`set_tilemap_cell`）。名词前置形态（`signal_connect`）是历史保留，新工具不要新增这种。
- **schema 一律由参数表派生**。`raw_schema` 只在确有嵌套结构、参数表表达不了时才手写；现状是全仓仅 3 个元工具（`search_tools`/`batch_execute`/`code_execute`）手写，"元工具都手写 raw_schema"是过时说法。参数描述写进表里第三字段——它是模型唯一能看到的说明。
- **`side_effect` 选值即声明风险**（`SideEffect`：`None`/`WritesFile`/`WritesConfig`/`ShowsAlert`/`ModifiesWindow`/`Process`/`CodeExecute`/`GameRuntime`）。只读工具用 `None` + `tool_flags::kNone`；有写入/可变动作用用对应值 + `kMutating`。声明正确才会自动进遍历排除，**不要去改 `tests/runner/traversal.cpp` 加白名单**。
- **flags 语义**（`tool_flags`，可 `|` 组合）：`kMeta` 归元工具表；`kDynamic` 动态工具；`kMutating` 可变态；`kObserve` 执行管线成功后合并编辑器截图（合成输入类工具用，别自己再实现一份截图）；`kCaptureImage` 图片附件判定单一来源；`kSceneTarget` `run_post` 在成功响应缺 `scene_path` 时幂等补全（不置脏）；`kUndoable` 纯标记，撤销仍由 handler 自管；`kHealthProbe` 健康探针（MCP 主线程等待改走 5s 短预算，仅 `ping`/`system_status` 用）；`kLongBlocking` 长阻塞例外（绕过等待预算、无限等待，只有确实可能长时间占主线程的工具才打，现状为 `code_execute`/`batch_execute`）。
- **迁移守卫会拒绝旧写法**：域文件里出现 `GDA_TOOL_CLASS`、新建 `schema_*_ops.cpp`、引用 `tool_input_schema`/`tool_decl.hpp` 都会让 `migration_guard` 失败。

## 联动清单（漏一项就是交付不完整）

- [ ] `ctest --preset debug -R migration_guard` 通过。
- [ ] `register_all_test` 的 `SchemaStatisticsBaseline` 会断言 catalog 总数与非空/空 schema 计数：新工具带参数即改变基线，跑一次 L1 按实测更新该测试基线，并在 `docs/wiki/tests.md` 数值核算总表同步。
- [ ] `03_tools_contract` 遍历候选数随 `side_effect`/`mutating` 声明变化：只读工具会新增两步空参+冒烟，跑 L2 确认不新增 warnings。
- [ ] `docs/wiki/modules/tools_ops_a.md` 或 `_b.md` 增补该工具的行为描述（行为事实归 wiki，AGENTS.md 不写）。
- [ ] 新增域工具必须同步 `src/util/skill_templates/tools--tool-catalog.md`：在对应域分节按既有 `- \`name\` - 描述` 格式增一条目，描述取 ToolSpec description 首句；漏同步则 L1 的 `ToolCatalogCoverage` 用例失败。格式示例：`- \`get_audio_bus_count\` - count audio buses including Master`。反引号词回验规则不变：模板正文反引号词须在 catalog∪schema 参数名∪白名单内可回验，否则 `skill_gen_test`（`ToolNamesExistInRegistry`）失败。
- [ ] 需要引擎内验证的新行为，用新增 `tests/config/*.json` 覆盖（零 C++）。

## 常见错误

- 只加了 handler 就宣布完成，注册表里没有这条工具（漏了第 2 步的 `make_tools()` 那一行）。
- 给只读工具错标 `kMutating`/`side_effect`，导致它被遍历排除、契约覆盖悄悄缩水。
- 参数描述写成 `"path"` 这类无信息量文本，模型只能猜。
- 在 `<域>_tools.hpp` 里写实现逻辑（该文件只放参数表与 spec 记录）。

## 协同与参考

- 设计依据与现状：[docs/wiki/tool_base_design.md](../../../docs/wiki/tool_base_design.md)、[docs/wiki/modules/tools_registry.md](../../../docs/wiki/modules/tools_registry.md)
- 实现纪律：`project-writing-tool-handlers`；授权与风险面：`project-enforcing-security-gates`
