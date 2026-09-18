#ifndef GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP
#define GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace script_patch_ops {

// 局部补丁：按字面锚点替换/插入已存在脚本的一处文本，与 create_script 并存。
// 锚点失配显式失败且零写盘；preview/dry_run 只返回 diff 预览不写盘；
// 正式写盘前复用 script_ops::try_compile_source 编译门（先编译后保存），
// 编译失败零写盘，保存/回读失败则尽力恢复原文（不留半写文件）。
// 并发说明：同文件并发写不加锁，调用方须串行（沿用 create_script 口径）。
mcp::JsonValue handle_patch(const mcp::JsonValue &args);

} // namespace script_patch_ops
} // namespace godot_autopilot

#endif
