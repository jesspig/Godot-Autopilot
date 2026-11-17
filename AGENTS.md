# AGENTS.md

Godot Autopilot（GDA）：进程内 GDExtension，把 Godot 编辑器经 MCP（`127.0.0.1:<port>/mcp`，端口首次启动在 20000–29999 随机分配并持久化）暴露给 AI 客户端。C++17 + godot-cpp + mcp-cpp-sdk，工具经 `ToolSpec` 数据化注册。项目事实与文档规则见 `docs/wiki/index.md`。

## 命令

- 构建部署：`bun run gda-dev-tools build` + `bun run gda-dev-tools deploy`；子命令 `package` / `package-check` / `clean` / `test` / `jam` / `tui` 及参数见 `docs/wiki/build.md`。
- 手动构建：`cmake --preset debug && cmake --build --preset debug`；无 ninja 时改用 `debug-portable` / `release-portable`。
- 快速验证：`ctest --preset debug`（单元 + 边界 + 集成 + 守卫 + CLI，零引擎依赖）；CLI 单测 `bun test cli/tests`。
- 升版同步根 `VERSION`、`cli/src/version.ts`、`cli/assets/skills.json`，随后重新 configure。

## 硬约束

- 所有 Godot API 必须经 `CommandQueue` 在主线程执行（`submit()` / 带预算入口）；直接调用必崩，`main_thread_guard` 在 ctest 拦截。`execute_sync()` 是无界等待，禁止新增调用。
- 新增 `.cpp` 双登记：根 `CMakeLists.txt` 的 `add_library()` 与 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`；`domains/*/*_ops.cpp` 由 glob 自动纳入。漏根登记只在运行时暴露，漏测试登记会静默失去覆盖。详见 `.agents/skills/cpp-registration/SKILL.md`。
- `third_party/` 依赖是 git submodule，构建前须已检出；缺失即 configure 报错，不做网络回退。
- 源码禁止注释（仅 `// namespace` 结尾标记）；`comment_guard` 在 ctest 拦截。
- 默认安全姿态不可静默放宽：仅绑环回、脱敏默认开、高风险工具需 `confirm=true`、导出期拒绝工具调用 / MCP 资源 / MCP 提示词三类入口。
- 数值一律以运行时统计为准，唯一口径见 `docs/wiki/tests.md`「数值核算总表」。
- CLI 为 Bun+TS 源码直跑（零运行时依赖、无构建步骤）；发布另产 5 平台二进制与 `sha256sums.txt`。技能书为 1 张入口卡 + 9 篇 references，EN/ZH parity 由守卫校验。

## 版本与提交

- 版本固定 `0.y.x`，默认只递增 `x`；`y` 仅在用户明确要求时递增。`-dev` 后缀表示开发中，未经许可不得移除、打 tag 或发布。
- 提交信息 `<type>(<scope>): <中文描述>`，按功能域拆分。`git status` 的 CRLF 伪变更不作准，核查用 `git diff HEAD`。
- 文档与汇报不写 git 工作树/远端状态；知识落点与维护规则见 `docs/wiki/index.md`。多代理并行时子代理只写代码不编译，主代理统一构建后分批测。
