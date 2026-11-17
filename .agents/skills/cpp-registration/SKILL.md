---
name: cpp-registration
description: 新增、删除或移动 C++ 编译单元时同步 CMake 双登记并通过双守卫。何时用：新增 .cpp、双登记漏项、GDA_UNIT_BUSINESS_SOURCES 不匹配、migration_guard / comment_guard 报错、编译链接失败。
---

# 编译单元登记

新增或移动 `.cpp` 时保证两处登记一致，并通过 ctest 双守卫。

## 步骤

1. 判定是否为 header-only：仅 `.hpp` 变更时跳过登记，转步骤 4。
2. 新增 `.cpp` 时同批修改两处：
   - 根侧源表：根 `CMakeLists.txt` 的 `add_library()` 直列条目或 `cmake/sources/*.cmake` 清单（现有清单优先，避免与他人分支冲突）。
   - `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`（`src/tools/domains/*/*_ops.cpp` 由 glob 自动纳入，不逐个登记）。
3. 检查旧宏残留：头文件禁 `GDA_TOOL_CLASS`，禁新增 `schema_*_ops.cpp` 与 `tool_decl.hpp` 引用。
4. 检查注释：`src/`、`tests/` 下 `*.cpp/*.hpp` 禁 `//`（仅 `// namespace` 结尾放行）与 `/*…*/`。
5. 子代理只写代码不编译；主代理统一 configure 与 build，再分批测。

## 边界

- 输入：文件增删清单；输出：两处 CMake 修改与守卫通过证据。

## 验证

- 执行 `ctest --preset debug -R comment_guard`，预期 `comment-guard OK`。
- 执行 `ctest --preset debug -R migration_guard`，预期 `OK` 且域数为当前清单值。
- 执行 `ctest --preset debug`，预期全绿；总数以 `docs/wiki/tests.md` 数值核算总表为准，不硬编码。
- 用 `git diff HEAD` 确认真实变更，忽略 CRLF 伪变更。

## 权威参考

- [工程约定](../../../docs/wiki/conventions.md)
- [测试体系](../../../docs/wiki/tests.md)
