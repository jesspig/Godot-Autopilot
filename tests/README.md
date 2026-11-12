# 测试体系说明

> **本文件的定位**：程序化测试体系的**使用说明**（怎么构建、怎么跑、每层覆盖什么）。**全套数值的权威落点是 [docs/wiki/tests.md](../docs/wiki/tests.md) 的「数值核算总表」**——本文件不另立数值口径，出现不一致时以该页为准。

## 1. 概述

程序化测试按**测试性质**分三层 + 守卫与脚本项，全部**零引擎、零项目绑定**——`ctest --preset debug` 全绿不产生任何 Godot 进程。三层各带 ctest 标签，可用 `ctest --preset debug -L <label>` 单独执行：

- **单元层**（`gda_unit_tests`，标签 `unit`，53 文件 / 636 用例）：合法输入下的模块行为——纯逻辑、编解码、注册与查询，不触碰 Godot API。
- **边界层**（`gda_boundary_tests`，标签 `boundary`，22 文件 / 239 用例）：非法与极端输入下的确定性行为——参数校验、错误映射、预算与超时、门控与守卫。
- **集成层**（`gda_integration_tests`，标签 `integration`，12 文件 / 109 用例）：进程内契约测试——注入 `CommandQueue`（owner 为测试线程）与处理器表，经 `dispatch::call_handler` inline 路径驱动元工具与域工具，断言错误全串与响应形状。夹具与覆盖清单见 [docs/wiki/tests.md](../docs/wiki/tests.md) 的「集成层」章节。
- **回归属性**（跨载体）：各层基线断言承载——schema 基线、工具面动态枚举契约、错误全串契约、CLI wire 形状、skill 白名单；清单与更新 SOP 见 wiki「回归属性清单」小节。
- **守卫与脚本项**（ctest 常驻）：源码守卫（注释 / 主线程契约 / 版本同步 / 迁移 / 源清单 / 工具域 / skills 契约）+ 守卫自测 + 2 个 CLI 脚本项（标签 `guard` / `cli`），全部零构建依赖（读源码断言）。

真实引擎行为的端到端验证**不在仓库基建内**：由 `gda-e2e-testing` Agent 工作流在 `examples/<名称>` 场地自然发生（工具调用随实际操作驱动），问题经 `gda-e2e-analysis` 闭环分析。历史 L2 引擎轨（`gda_test_runner` / `tests/config/*.json` / `tests/testbed/` / 池化编排）已于 2026-11 删除，语义蒸馏落点见 wiki「引擎行为验收清单」。

架构一句话：进程内 GDExtension（EditorPlugin），领域工具经 `call_tool` 元工具代理，由 `register_all.cpp` 的 registry（`build_registry`，经 `refresh_derived` 派生 dispatch 映射）分发。

## 2. 前置条件

| 依赖 | 说明 |
| ---- | ---- |
| CMake 3.28+ | 测试目标经根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 选项引入；该开关已固化在 `CMakePresets.json` 的 `base-debug`/`base-release` 基座（默认 ON）并经 `inherits` 下发给全部 4 个可执行 configure 预设，清理 `build/` 后重新配置自动恢复（裸 `cmake` 不带 preset 时默认 OFF） |
| Ninja（可选） | 构建生成器。有 ninja 时用 `debug`/`release`（更快）；无 ninja 时自动落到 `debug-portable`/`release-portable`（省略 generator，由 CMake 发现平台默认生成器），共用同一 `build/{debug,release}` 目录。`uv run main.py` 各子命令自动选路，手动 `cmake --preset` / `ctest --preset` 须自己选对预设名 |
| clang-cl | 编译器（MSVC/GCC 自动回退，测试继承根配置） |
| googletest | `third_party/googletest` 子模块（commit `b514bdc898e2951020cbdca1304b75f5950d1f59`），随仓库检出：`git clone --recurse-submodules <repo>`，或 `git submodule update --init --recursive`；缺失即 configure 报错，不做网络回退 |
| Bun | 仅 CLI 脚本项需要（`bun test cli/tests`）；缺失时 `cli_contract`/`cli_unit` 打印 `SKIP` 并退出 0，其余测试不受影响 |

## 3. 构建

```powershell
# 1. 配置（测试开关已固化在预设，无需传参；submodule 需已检出）
cmake --preset debug

# 2. 构建测试目标
cmake --build --preset debug --target gda_unit_tests gda_integration_tests
```

产物位于 `build/debug/tests/`（portable 预设多一层配置子目录）：

- `gda_unit_tests.exe` — 单元层，gtest 可执行文件
- `gda_integration_tests.exe` — 集成层，gtest 可执行文件（同时是单跑与 `--gtest_filter` 排查入口）

## 4. 运行

```powershell
# 全量默认门（单元 + 边界 + 集成 + 守卫 + CLI，ctest 注册口径见 wiki 数值总表）
ctest --preset debug

# 单跑（按用例名正则，如守卫或某个套件）
ctest --preset debug -R migration_guard
ctest --preset debug -R MetaToolsSuite

# 集成层 gtest 直跑（--gtest_filter 排查最方便）
build\debug\tests\gda_integration_tests.exe
build\debug\tests\gda_integration_tests.exe --gtest_filter="IntegrationHarness.*"

# CLI TS 单测
bun test cli/tests
```

`uv run main.py test` 等价于全量默认门，`uv run main.py test --single <用例名>` 单跑；测试编排已收敛为无过滤 ctest（历史 `--l2`/`--all` 旗标随引擎轨删除）。

## 5. 迁移守卫（migration_guard）

- 位置：`tests/guard/migration_guard.cmake` + 清单 `tests/guard/migrated_domains.txt`（30 个域 + `strict` 行）。
- 内容：零依赖读源码断言，`strict` 模式下禁止旧机制回退——已迁移域与全部 `src/tools/domains/*/*_tools.hpp` 不得含旧宏 `GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE`；不得残留 `src/tools/schema_*_ops.cpp`；`src/tools/**/*.hpp|*.cpp` 不得引用 `tool_input_schema` 或 `tools/tool_decl.hpp`。
- 运行：`ctest --preset debug -R migration_guard`（configure 后即可运行，不依赖构建产物），或直接 `cmake -DSOURCE_DIR=<仓库根> -P tests/guard/migration_guard.cmake`；实测输出 `OK (domains: 30, strict: on)`。

## 6. 其它源码守卫与脚本项

ctest 上除单元/集成层外的守卫与脚本项（源码守卫 + `source_manifest_guard` + 守卫自测项 + `cli_contract` / `cli_unit` 两个脚本项 + Bun 可用时的 `skills_guard` / `skills_guard_self_test`）（注册见 `tests/CMakeLists.txt`；逐项说明见 [docs/wiki/tests.md](../docs/wiki/tests.md) 的「守卫与脚本项」）：

- `skills_guard`（`tests/guard/skills_guard.ts`，Bun 可用时注册）：扫描 `skills/` 与 `skills-zh/`，失败输出 `文件:行号: [规则] 说明`；校验旧版本钉/`bunx`/`9527`/`gda serve`/`gda api`/`Allow write`/REST/配置面板与 allow-list、`gda` 命令与参数存在性、`gda status --json` 字段、EN/ZH 10 册 parity 与 frontmatter。`--self-test` 用反例验证守卫自身。Bun 缺失时由 `cli_unit` 的 `bun test cli/tests`（`skills_contract.test.ts`）覆盖同一实现。
- `comment_guard`（`tests/guard/comment_guard.py`）：扫描 `src/` 与 `tests/` 下的 C++ 文件，除 `// namespace` 结尾标记外禁止任何 `//`/`/*…*/` 注释；无违规退出 0，违规退出 1，文件无法解码退出 2。
- `main_thread_guard`（`tests/guard/main_thread_guard.py`）：主线程契约守卫，静态扫描 `src/` 下 `*.cpp/*.hpp` 里的 Godot API 直接调用。判定依据是「线程上下文分类」——每个源文件被显式归入主线程（`main`）/受管（`managed`）/外线程（`foreign`）三类，外线程文件里的 Godot API 调用默认违规，只有落在该文件显式登记的主线程作用域里才放行。退出码：0 = 无违规、1 = 有违规、2 = 文件读或解码失败。
- `version_sync_guard`（`tests/guard/version_sync_guard.py`）：版本号一致性——根 `README*.md` 的每一处版本钉均须与根 `VERSION` 同值，另含 `VERSION` 与 `CLI_VERSION` 两个值源；`skills/` 与 `skills-zh/` 禁止出现版本钉或 Git ref。非版本引用（如 `#develop`）只登记、不参与比对。退出码：0 = 全部一致、1 = 存在版本漂移（逐条打印 `文件:行号 = 值 (标签)`）、2 = 关键文件缺钉 / 技能书出现版本钉 / 未知引用形态 / 文件读失败。覆盖范围以守卫脚本为准。
- `cli_contract`（`tests/scripts/test_cli_contract.py`）：CLI 契约 mock conformance（bun 驱动 `gda.ts`）；PATH 无 `bun` 时打印 `SKIP` 并退出 0。
- `cli_unit`（`tests/CMakeLists.txt` 直接执行 `bun test cli/tests`）：TS 单测 18 文件；实测口径见 wiki 数值总表；bun 缺失时由 CMake 回退打印 `SKIP` 并退出 0。

## 7. 引擎行为验收

真实编辑器行为的验收清单（原 48 份 L2 用例的语义蒸馏，按工具域组织、附 e2e 抽查建议）见 [docs/wiki/engine-behavior-checklist.md](../docs/wiki/engine-behavior-checklist.md)；迁移对照（源 JSON 步骤 ↔ 集成层 gtest 断言 ↔ 等价结论 ↔ 划回 e2e 项）见同页「A 类迁移对照表」。验收以 `gda-e2e-testing` 工作流在 example 场地执行，本仓库测试基建不承载引擎内断言。
