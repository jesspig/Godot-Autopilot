# T-03 业务源码 JsonValue 迁移记录

状态：代码改动完成；**未运行任何 cmake / build / ctest / bun test**（按子代理约束，主代理统一构建与分批测试）。
本文件只记录静态改动与静态检查结论，不代表测试通过。

## 结论摘要

- `src/` 中 `mcp::JsonValue` 类型引用与 `#include <mcp/JsonValue.hpp>` 的静态 grep 计数均为 **0**；T-01 基线为 10161 处 / 189 文件。
- `tests/` 中同步清零（67 个文件）；`tools/json_parity_probe.cpp` 保持双类型对照实现不变（按设计保留）。
- 新增 `src/adapters/` 适配层，SDK 类型经 `namespace sdk = mcp` 别名访问，源码中不再出现 `mcp::JsonValue` 字面量。
- 剩余 `mcp::` 引用集中在 22 个 SDK 边界文件（见下），均为非 JsonValue 的 SDK 类型。

## 适配层设计

- `src/adapters/mcp_json_bridge.hpp/.cpp`
  - `namespace sdk = mcp; using SdkJson = sdk::JsonValue;`；导出 `FromSdk` / `ToSdk` / `FromSdkOptional` / `ToSdkOptional`。
  - 采用逐类型递归转换（保留 int 与 double 的区分），不经过 Dump/Parse，避免解析失败路径；对象迭代顺序两侧均为 `std::map<std::string, …, std::less<>>`。
- `src/adapters/mcp_error_bridge.hpp/.cpp`
  - `SdkError : sdk::McpError`，错误 data 使用 GDA `JsonValue`，构造时经 `ToSdkOptional` 转换；`ThrowSdkError` 为唯一抛出入口。
  - `src/util/error_util.hpp` 保留 `util::SdkErrorCode` 别名与 `util::throw_mcp_error` 薄封装，业务调用点无需改写。
- `src/adapters/mcp_sha256.hpp`：`Sha256Hex` 封装 SDK SHA-256（算法未自研替换，仅收敛到适配目录）。
- `src/adapters/mcp_image_content.hpp`：图片 content 算法入参改为 GDA `JsonValue`，输出仍为 SDK `ContentVariant`；`src/util/mcp_image_content.hpp` 改为转发头，兼容既有 include。
- CMake 双登记：新增 `mcp_json_bridge.cpp`、`mcp_error_bridge.cpp` 同时登记到
  `cmake/sources/api_contract.cmake`（根侧）与 `cmake/sources/api_unit_contract.cmake`（测试侧）。

## 分批策略、转换策略、风险与未验证点

### 批次 0 基础适配层（新增文件）
- 文件：上述 4 个适配头 + 2 个适配 .cpp + 两处 CMake 清单 + `src/util/json_value.hpp`（补 `<optional>` 以承接原 mcp 头的传递包含）。
- 策略：SDK 类型只在适配层出现；业务侧统一 `godot_autopilot::util::JsonValue`。
- 风险：`namespace sdk = mcp` 依赖 SDK 头可经 `<mcp/McpTypes.hpp>` / `<mcp/McpError.hpp>` / `<mcp/detail/sha256.hpp>` 传递包含 `JsonValue.hpp`；未编译验证。
- 未验证点：两个新 .cpp 的链接、Unity build 下符号唯一性、source_manifest_guard 认定。

### 批次 1 纯工具逻辑层
- 文件（节选，均在 `src/util/`、`src/tools/base/`）：`instance_registry.*`、`monitor_analysis.*`、`skill_resources.hpp`、`schema_builder.*`、`tool_args.*`、`confirmation.hpp`、`inline_resource_json.hpp`、`input_guards.hpp`、`batch_verdict.hpp`、`json_godot.hpp`、`variant_json.*`、`rid_registry.hpp`、`scene_path.hpp`、`script_capability.hpp`、`error_util.*` 等。
- 策略：`mcp::JsonValue` → `godot_autopilot::util::JsonValue` 的全限定替换；两类型 API 一一对应（构造、object_tag/array_tag、Find/At、GetArray/GetObject、PushBack、Dump、Parse、operator[]、迭代器）。
- 风险：`JsonValue::Parse` 抛 `util::JsonError` 而非 `mcp::McpError`；`input_guards::try_parse_json` 用 `catch (...)`，不受影响。
- 未验证点：全部契约/边界测试（未运行）。

### 批次 2 配置、存储、错误与 SHA-256、图片 content
- 文件：`plugin_config.*`、`legacy_store.*`、`resource_fs.hpp`、`editor_readiness.*`、`sanitize_policy.hpp`、`error_watermark.hpp`、`mcp_image_content.hpp`。
- 策略：配置/存储解析改用 GDA `Parse`；`plugin_config.cpp` 与 `legacy_store.cpp` 的 `catch (const mcp::McpError&)` 改为 `catch (const util::JsonError&)`，取错文本由 `ErrorMessage()` 改为 `what()`；`resource_fs.hpp` 的 `sha256_hex` 委托 `adapters::Sha256Hex`；图片 content 实现移入 adapters。
- 风险：错误路径文本在 GDA 异常下与 SDK 异常下不完全一致（`JsonError` 的 what 带 "JSON parse error: " 前缀），可能影响依赖精确错误串的断言。
- 未验证点：`plugin_config_write_test`、`legacy_compat_test`、`config_loader_strict_test`、`mcp_image_content_test` 等（未运行）。

### 批次 3 SDK 边界转换
- 文件：`register_all.cpp`、`server_context.cpp`、`dispatch.*`、`godot_transport.cpp`、`mcp_http_shim.cpp`、`ipc_transport.cpp`、`prompts/*`、`resources/*`、`core/transport/*`。
- 策略：
  - `register_all.cpp`：`InputSchema` 用 `ToSdk`；`ctx.Params().arguments` / `input_responses` 用 `FromSdk` / `FromSdkOptional`；`structured_content`、`InputRequest::params` 用 `ToSdk`。
  - `server_context.cpp`：入站 rewrite 中 `request->params`、`req.meta` 的指针改为 `SdkJson*`；回写用 `ToSdk`，arguments 读入用 `FromSdk`。
  - `godot_transport.cpp`：`RequestIdToJson` 结果用 `FromSdk`；notification 的 `params` 指针改为 `SdkJson*`，回写用 `ToSdk`。
  - prompt handler 的 `std::optional<JsonValue>` 参数改为 `std::optional<SdkJson>`，与 `RegisterPrompt` 签名一致。
- 风险：这是唯一存在真实跨类型转换的区域；递归转换保持 JSON 语义但增加一次深拷贝，用户可观测输出应保持一致，未实测。
- 未验证点：所有 `RegisterTool` / `RegisterPrompt` / `RegisterResource` 相关集成测试与探针（未运行）。

### 批次 4 runtime 与 domain ops
- 文件：`src/runtime/*`、`src/tools/base/` 其余、`src/tools/domains/**/*_ops.*`、`*_tools.hpp`（约 130 个文件）。
- 策略：纯类型替换；`using JV = mcp::JsonValue` 同步改为 `using JV = godot_autopilot::util::JsonValue`；`#include <mcp/JsonValue.hpp>` 改为 `#include "util/json_value.hpp"`。
- 风险：原 mcp 头传递引入的 `<optional>` 等标准头可能缺失；已在 GDA `json_value.hpp` 补 `<optional>`，其他传递包含未逐一枚举。
- 未验证点：全部 domain 单元/边界/集成测试（未运行）。

### 批次 5 测试同步
- 文件：67 个 `tests/` 下 `.cpp/.hpp`，同样类型与 include 替换；`tools/json_parity_probe.cpp` 不改（对照探针必须同时持有两种类型）。
- 策略：SDK 错误边界测试（如 `export_execution_boundary_test`、`resource_error_mapping_test`、两个 probe）保留 `mcp::McpError`，因为 `SdkError` 仍派生自它，预期行为不变。
- 未验证点：全部测试编译与运行（未运行）。

## 守卫预期变化（未实际运行，基于静态检查）

| 守卫 | 预期 | 依据 |
| --- | --- | --- |
| `no_mcp_json_guard`（migration） | 由 10161 处/189 文件降为 0 处/0 文件 | `src/` grep 零命中；strict 模式亦预期通过 |
| `no_mcp_json_guard --strict` | 通过（0 违规） | 同上 |
| `comment_guard` | OK | 新文件注释仅 `// namespace` 结尾标记 |
| `source_manifest_guard` | OK | 两个新增 .cpp 已根侧 + 测试侧双登记 |
| `migration_guard` | 不变 | 未触及 domain 宏与清单 |
| `no_mcp_guard`（migration） | mcp 计数小幅上升，不失败 | 新增 adapters 的 SDK include；默认非 strict |
| `no_mcp_guard --strict` | 仍不通过 | 22 个边界文件与 adapters 仍含 SDK 类型；属独立后续任务 |

## 剩余未清零项与原因

`src/` 仍含 `mcp::` 引用的 22 个文件（均无 `mcp::JsonValue`）：
- `src/adapters/` 之外的边界文件：`tools/base/register_all.*`、`tools/base/dispatch.*`、`core/server_context.*`、`core/transport/godot_transport.*`、`core/transport/mcp_http_shim.cpp`、`prompts/*`、`resources/*`、`core/observability/log_system.cpp`、`core/observability/monitor.*`。
- 原因：这些文件直接实现 `McpServer` 注册回调、日志/监控的 SDK 类型接入；把它们整体迁入适配目录需要改 CMake 源表与跨目录 include，且本子代理不能编译验证，风险高于收益，因此保留在边界目录并在此显式列出。

## 偏离说明

1. `McpError`、`CallToolResult`、`GetPromptResult`、`ReadResourceResult`、`McpServer` 等 SDK 类型未全部迁入 `src/adapters/`：错误与 content 的构造已迁至适配层，但注册回调文件仍持有 SDK 类型（见上），未完全满足「限制在明确适配目录」。
2. SDK SHA-256 未替换为自研 GDA 实现，仅在适配层封装，收敛范围达成、算法替换未达成。
3. 未运行任何测试/构建命令（任务约束）。

## 风险汇总

- 两类型 API 等价性仅由头文件逐项比对与 T-02 的 62 case parity 探针支撑，本次未编译验证。
- 错误文本前缀差异可能影响精确字符串断言。
- 工作树存在历史混合 CRLF/LF（HEAD blob 亦然），新增适配文件为 LF；`git diff --check` 会在改动行提示 CR 尾随空白，属仓库既有 CRLF 特征，未做全量换行归一化以免扩大改动面。
