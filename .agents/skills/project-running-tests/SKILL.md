---
name: project-running-tests
description: 运行、诊断或新增本项目 L1/L2 测试时使用：ctest 过滤口径、单跑引擎用例、GODOT_PATH 与授权前置、遍历契约、守卫失败和测试后的仓库脏变更清理。
---

# 运行与新增测试

## 适用范围

- 用于：改完代码要验证、测试失败要看懂、新增/挑选 L2 用例、判断某失败是回归还是环境漂移。
- 不适用：构建与源文件登记（`project-building-gda`）；测试之外的运行时排障（`project-diagnosing-with-traces`）。

## 前置条件

- 已 `cmake --preset debug && cmake --build --preset debug`（预设里 `GDA_ENABLE_TESTS=ON`，裸 `cmake` 配置没有测试目标）。
- L2 需要引擎：`GODOT_PATH` 环境变量 > 仓库根 `.env`（从 `.env.template` 复制）。缺失时执行器**以退出码 2 报错**，不会静默跳过——看到 exit 2 先查路径，别当成用例失败。
- 需要授权的 L2 用例由 `GODOT_AUTOPILOT_ALLOW` 环境变量或 `user://godot_autopilot/config.json` 的 `allow` 提供（env 优先）：游戏侧路径要 `game_runtime`、脚本类要 `code_execute`、用户工具注册/执行要 `user_tools`。

## 流程

1. 默认验证闭环：先 L1 快筛 `ctest --preset debug -E "^gda_runner_"`（秒级），再按改动挑对应 L2 单跑，最后才考虑全量。
2. 单跑 L1 用例：`ctest --preset debug -R <用例名>`；单跑守卫：`-R migration_guard` / `-R comment_guard`。
3. 单跑 L2：`ctest --preset debug -R gda_runner_01_scene`，或直接 `build/debug/tests/gda_test_runner.exe --file 01_scene`（看逐步骤输出更快）。
4. 全量 L2 约 12 分钟（每份 `tests/config/*.json` 是一次独立的编辑器启动→MCP 就绪→before_all→stages→after_all 生命周期，文件间不共享状态）。CI 只跑 L1。
5. 新增用例优先零 C++：新建一份 `tests/config/NN_name.json`，由 GLOB 自动发现；只有在引擎内无法表达断言时才动 runner。

## 规则与边界

- **数值口径以运行时统计为准**：L1 gtest 数、L2 份数、ctest 注册点、遍历候选/步骤数一律以 `ctest -N` 与执行器输出为准，权威表在 `docs/wiki/tests.md` 的"数值核算总表"。不要在代码注释、AGENTS.md 或技能里另写一份数字副本。
- **L1 严禁调用已注册工具的 handler**：无引擎时 godot-cpp 接口指针为 `nullptr`，直接崩溃。L1 只能测注册/分发/取参/错误路径/纯 std 组件。
- **遍历契约靠声明驱动**：`03_tools_contract` 运行时经 `search_tools` 枚举后按 `get_tool_detail` 的 `side_effect`/`mutating`/`dynamic` 排除。要让某工具进/出遍历，改它的 `side_effect`/`flags` 声明，**不要改 `tests/runner/traversal.cpp` 的清单**。warnings 数量以运行时报告为准，历史基线是少数几个固有告警，别把新增 warning 当噪声忽略。
- **两个守卫常驻 ctest**：`migration_guard` 断言 30 域无旧宏/`schema_*_ops.cpp`/`tool_input_schema` 残留（实测输出 `-- [migration-guard] OK (domains: 30, strict: on)`）；`comment_guard` 扫 `src/` 与 `tests/` 的 `*.cpp/*.hpp`，除 `// namespace` 结尾标记外禁止任何注释。它们的判定时机是 ctest，所以"编译过了"不是结论。
- **L2 之后的仓库脏变更**：引擎会自行给 `Example/project.godot` 追加 `[audio]`/`[input]` 段并生成 `Example/default_bus_layout.tres`。收尾用 `git checkout -- Example/project.godot` 并删掉该 tres；提交前用 `git diff HEAD` 复核，避免把测试残留一起提交。
- **环境漂移不是回归**：`Example/` 若残留 `scenes/main.tscn` + `run/main_scene`，无头编辑器会在首次文件扫描后约 0.5–0.8s 自动打开主场景，与用例 `before_all` 的干净场景竞态，表现为多份用例同时失败。跑 L2 前确认 `Example/.godot/editor/editor_layout.cfg` 无残留 `open_scenes`；同批失败先判污染，再谈代码。

## 常见错误

- 用 `ctest --preset debug` 一把梭跑全量 L2（12 分钟）来验证一个纯 std 改动。
- `GODOT_PATH` 没配就跑 L2，把 exit 2 记成"用例失败"。
- 需要授权的门禁工具报 `authorization required`，就去放宽 `deny_if_unauthorized` 而不是给测试进程加 `GODOT_AUTOPILOT_ALLOW`。
- 为让遍历通过而往 runner 的排除清单里加工具名。

## 检查清单

- [ ] L1 过滤集全绿，两个守卫 OK。
- [ ] 受影响域的 L2 单跑通过（不是只看全量汇总）。
- [ ] 数值若变化，已按 `ctest -N` 实测并同步 `docs/wiki/tests.md` 总表。
- [ ] `git diff HEAD` 里没有测试残留的 `Example/` 变更。

## 协同与参考

- [docs/wiki/tests.md](../../../docs/wiki/tests.md) · [docs/wiki/build.md](../../../docs/wiki/build.md) · `tests/README.md`
