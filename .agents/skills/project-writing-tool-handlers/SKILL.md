---
name: project-writing-tool-handlers
description: 编写或修改 src/tools/ 下领域工具 handler 的实现逻辑时使用：线程与队列约束、取参、错误与成功结构、回读校验、撤销、内部组合调用与文本/文件细节。改工具行为而不新增工具时也适用。
---

# 编写工具 handler

## 适用范围

- 用于：实现/修改 `<域>_ops.cpp` 里的 `handle_*`，调整返回值结构，处理节点/资源/文本/文件细节。
- 不适用：注册与 flags 声明（`project-adding-domain-tools`）；埋点写入（`project-instrumenting-traces`）；授权门配置（`project-enforcing-security-gates`）。

## 前置条件

- 明确工具是否在主线程执行：领域工具经 `CommandQueue` marshal 到主线程后由 `SpecTool::execute` 调 handler，因此 handler 体内可以调 Godot API。
- 目标引擎按 Godot 4.7 口径（`GODOTCPP_API_VERSION "4.7"` 锁定），先核对 API 再写。

## 规则与边界

- **线程纪律**：任何 Godot API 调用必须在主线程。handler 内不要自开线程去 `load()`/`get_tree()`；确需从其它线程回主线程，用 `CommandQueue::submit()`/`execute_sync()`，且跨线程时用 `tools::capture_trace_context()` + `ScopedTraceContext` 包住，否则该次调用的 trace 链路会断（既有 `execute_sync` 调用点都已按此包装，数量以 grep `ScopedTraceContext` 为准）。
- **取参现状要认清**：`Args`（`tool_args.hpp`）已实现且 L1 覆盖，但**没有任何领域 handler 接入**。在架 handler 统一 `args.Find("name")` 判类型，缺失返回 `util::error_json("missing required parameter: <name>")`。写新 handler 跟随所在文件的邻居风格，不要在同一文件混用两种取参方式。
- **返回结构**：失败 `util::error_json("...")`（顶层 `error` 键，`call_tool` 据此置 `is_error=true`）；需要包成功载荷用 `util::ok_result(...)`。需要机器可读错误码时才带 `structured_error`，注意 trace 的 `error_code` 只取 `structured_error.code`，其余一律记成 `error`。
- **错误消息必须给下一步**：本仓库好的错误写法是"是什么 + 为什么 + 用什么工具修"，例如 `"node is not a TileMap or TileMapLayer: <path> — create one with create_tilemap (TileMap) or create_scene_node + property_set (TileMapLayer), or fix the path"`。模型会照错误消息决定下一次调用，只写 `"invalid node"` 等于把诊断推回给用户。
- **回读校验**：写属性/结构变更后不要只回 `{"ok":true}`，用 `util::check_readback`（property/resource 侧）与 `util::scene_verify`（保存收据、`compare_tree_with_text`）把"改完确实变成这样"放进响应。
- **撤销**：`kUndoable` 只是标记，管线不做任何 post 动作。需要撤销时自己取 `EditorNode::get_editor_undo_redo()`（必要时经 `EditorUndoRedoManager` 的历史），并保证一次操作一个 undo 粒度（`fill_tilemap_rect` 这类批量工具就是单次 undo）。
- **内部组合调用只走 `tools::invoke_tool(name, args)`**：它做导出期拦截 → 深度上限 8 → `dispatch::call_handler` → 异步 pending 嵌套拦截。不要绕过它直接调别的 handler 函数；它的导出分支无公开 setter，L1 测不到，别为此改可见性。
- **文本与路径**：所有字符串链路用 `String::utf8`（CJK 往返依赖它，别用 `LocalVector<char>` 直转）；保存路径按表成员选 `add_id`/`set_id` 写 UID。
- **脚本新鲜度**：读脚本的只读工具支持 `fresh` 参数走 `CACHE_MODE_IGNORE` 读盘；`attach`/`reload` 恒重读；`create_script` 响应字段是 `cache_refreshed`。写这类工具默认走缓存会让客户端读到旧内容。
- **文件追加写**：`FileAccess::READ_WRITE` 打开不存在的文件会**失败且不创建**。任何持久化/APPEND 写入必须先尝试再回退 `WRITE`（`log_persist.cpp` 与 `text_ops.cpp` 已按此处理），新写文件功能照这个模式。
- **幂等**：重复"打开已打开的场景"这类操作要返回幂等成功并带状态标记（如 `already_open`），不要报错。
- **注释禁令**：`src/` 下除 `// namespace` 结尾标记外不允许任何注释，`comment_guard` 在 ctest 阶段拦，编译过了不算过。

## 常见错误

- 在 lambda / 定时器回调里直接调 Godot API（崩溃）。
- 用 `Args` 重写某个 handler 的两个参数，结果与同文件其余 20 个 handler 风格分裂。
- 报错只写 `failed`，或把内部异常文本原样抛给客户端。
- 属性批量应用不分轮次：OBJECT 型属性必须先于普通属性应用（`create_scene_node` 的两轮应用就是为此）。
- 用 `std::string` 拼 Godot 路径后传给需要 UTF-8 的 API，中文路径丢字。

## 检查清单

- [ ] 失败路径全部经 `util::error_json` 且消息含下一步建议。
- [ ] 成功响应带可核对的回读证据，不只有 `ok`。
- [ ] 无新增注释；跨线程调用都经队列且保留 trace 上下文。
- [ ] `ctest --preset debug -E "^gda_runner_"` 相关用例通过（新增行为若引擎内才可验证，补 `tests/config/*.json`）。

## 协同与参考

- 管线与 flags 行为：[docs/wiki/tool_base_design.md](../../../docs/wiki/tool_base_design.md)
- 线程/安全契约：[docs/wiki/security_contract.md](../../../docs/wiki/security_contract.md)、[docs/wiki/modules/core.md](../../../docs/wiki/modules/core.md)
