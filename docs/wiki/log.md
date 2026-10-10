# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-10-10

- **依赖管理 FetchContent → git submodule（纯 submodule，零 FetchContent）**：分支 `feature/submodule-deps`。`_deps` 位于被 `.gitignore` 排除的 `build/` 下，无法随仓库流转，换机或内网克隆后必然重新访问 GitHub；改为 submodule 后源码在 `third_party/`，检出后 configure 零网络。`cmake/FetchDependencies.cmake` 经 `git mv` 改名 `Dependencies.cmake` 并重写为纯 `add_subdirectory`，新增 `gda_require_submodule()` 缺失即 `FATAL_ERROR`、**故意不做网络回退**。CI build job checkout 加 `submodules: true`，cache 路径改 `third_party/` 三目录、key 取 gitlink sha（不可用 `hashFiles('.gitmodules')`，否则升级 commit 会命中旧源码缓存）。**验证**：代理指向不可达地址时 configure 仍成功（零网络）；构建产出 `godot-autopilot.dll` 与 `gda_unit_tests.exe`；L1 **394/394 全绿**（含两守卫与 `skill_scripts`，225.99 s）。详见 [2026-10-10-log.md](changelog/2026-10-10-log.md)
- **文档同步**：`build.md`（新增「依赖获取」小节与三依赖 tag/commit 表）、`conventions.md`、`tests.md`、`overview.md`、`security_contract.md`、`tests/README.md`、`AGENTS.md`（硬约束改为 submodule 检出要求）；顺带修正 `demo/networking_multiplayer_pong/docs/tech/tdd.md` 中「删除 `_deps/` 缓存」的过时约束。详见 [2026-10-10-log.md](changelog/2026-10-10-log.md)
