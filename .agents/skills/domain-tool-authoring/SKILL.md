---
name: domain-tool-authoring
description: 新增、改名、改参或改副作用领域工具时走 ToolSpec 数据化管线；触发词：新增工具、make_spec_tool、ParamSpec、side_effect、kMutating、args.Find、missing required parameter。
---

# 领域工具编写

按数据化管线新增工具，保证注册、取参、副作用声明一致。

## 步骤

1. 读所在域邻居写法，锁定取参风格：已用 `args.Find` 的文件不混入 `Args`，缺参返回 `missing required parameter: <name>`。
2. 在 `<域>_tools.hpp` 定义 `kXxxParams` 参数表，并在 `make_tools()` 以 `make_spec_tool(ToolSpec{...})` 一行注册；仅嵌套结构用 `raw_schema` 手写，其余由参数表派生。
3. 在 `<域>_ops.hpp` 声明 `handle_xxx`，在 `<域>_ops.cpp` 实现；失败返回 `util::error_json`，成功包 `util::ok_result`。
4. 按实际影响声明 `SideEffect` 与 `flags`（`kMutating`、`kObserve` 等）；合成输入类需截图后处理才加 `kObserve`。
5. 命名按 `<动词>_<类别>_<维度>_<对象>_<修饰>`，动词置首；`signal_connect` 类名词前置仅限规范保留范围，不扩散。
6. 遵循动词语义：`get/fetch/load` 目标必存在否则抛，`find/search/query` 允空，`validate/check` 失败即抛。

## 边界

- 输入：工具名、参数表、副作用声明；输出：handler 加参数表加注册行。
- `register_all.cpp` 经各域 `make_tools()` 自动汇总，不手改注册表。

## 非目标

- 不做文本级增量编辑、glob 匹配、C# 热重载。
- 不放宽监听、授权、脱敏默认值。

## 验证

- 执行 `ctest --preset debug -R "migration_guard|comment_guard"`，预期全过。
- 用 `get_tool_detail` 核对 `side_effect` 可识别；空 query 枚举与遍历排除一致。
- 注册统计以运行时为准，不复制旧数值。

## 权威参考

- [工程约定](../../../docs/wiki/conventions.md)
- [工具注册表](../../../docs/wiki/modules/tools_registry.md)
