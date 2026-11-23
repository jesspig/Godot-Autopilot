# AGENTS.md

Godot Autopilot（GDA）项目指令。项目事实、数值口径与文档维护规则见 `docs/wiki/index.md`；本文件只列每次会话必须遵守的跨模块硬约束。

## 命令

- 构建：`bun run gda-dev-tools build`；部署：`bun run gda-dev-tools deploy`。子命令 `package` / `package-check` / `clean` / `test` / `jam` / `tui` 及参数见 `docs/wiki/build.md`。
- 手动构建：`cmake --preset debug && cmake --build --preset debug`；无 ninja 时改用 `debug-portable` / `release-portable`。
- 快速验证：`ctest --preset debug`（单元 + 边界 + 集成 + 守卫 + CLI，零引擎依赖；无 ninja 用 `ctest --preset debug-portable`）；CLI 单测 `bun test cli/tests`。
- 升版同步根 `VERSION`、`cli/src/version.ts`、`cli/assets/skills.json`，随后重新 configure。

## 硬约束

- 所有 Godot API 必须经 `CommandQueue` 在主线程执行（`submit()` / 带预算入口）；直接调用必崩，`main_thread_guard` 在 ctest 拦截。`execute_sync()` 是无界等待，禁止新增调用。
- 新增 `.cpp` 双登记：根侧源表（根 `CMakeLists.txt` 的 `add_library()` 直列或 `cmake/sources/*.cmake` 清单）与 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`；`domains/*/*_ops.cpp` 测试侧由 glob 自动纳入。漏根登记只在运行时暴露，漏测试登记会静默失去覆盖。详见 `.agents/skills/gda-change-gate/SKILL.md`。
- `third_party/` 依赖是 git submodule，构建前须已检出；缺失即 configure 报错，不做网络回退。
- 源码禁止注释（仅 `// namespace` 结尾标记）；`comment_guard` 在 ctest 拦截。
- 默认安全姿态不可静默放宽：仅绑环回、脱敏默认开、高风险工具需 `confirm=true`、导出期拒绝工具调用 / MCP 资源 / MCP 提示词三类入口。
- 数值一律以运行时统计为准，唯一口径见 `docs/wiki/tests.md`「数值核算总表」。
- 发布另产 5 平台二进制与 `sha256sums.txt`；技能书 EN/ZH parity 由守卫校验。

## 版本与提交

- 版本固定 `0.y.x`，默认只递增 `x`；`y` 仅在用户明确要求时递增。`-dev` 后缀表示开发中，未经许可不得移除、打 tag 或发布。
- 提交信息 `<type>(<scope>): <中文描述>`，按功能域拆分。`git status` 的 CRLF 伪变更不作准，核查用 `git diff HEAD`。
- 文档与汇报不写 git 工作树/远端状态。多代理并行时子代理只写代码不编译，主代理统一构建后分批测。
