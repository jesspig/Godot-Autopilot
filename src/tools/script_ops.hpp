#ifndef GODOT_AUTOPILOT_SCRIPT_OPS_HPP
#define GODOT_AUTOPILOT_SCRIPT_OPS_HPP

#include <mcp/JsonValue.hpp>

#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace script_ops {

// —— 写盘自证（纯函数：FNV-1a 64；仓内无 SHA 实现，故成功态用它自证）——
//
// create_script / patch_script 成功响应中的 disk_bytes（readback 字节数）与
// content_hash（磁盘读回字节的 FNV-1a-64 小写 hex，算法见 content_hash_algo
// 字段）均来自磁盘 readback，不做第二次读取即可确认落盘内容。
uint64_t fnv1a64(const std::string &bytes);
std::string fnv1a64_hex(const std::string &bytes);
inline const char *content_hash_algo_name() { return "fnv1a64"; }

// —— 编译门（只编译不保存；create_script 与 patch_script 共用）——
//
// 先编译后保存：正式写盘前调用，失败即中止且零写盘，不留半写文件。
// path_for_cache 仅用于 set_path_cache，不触碰磁盘。
bool try_compile_source(const std::string &source_code,
                        const std::string &path_for_cache,
                        std::string &error_out);

// —— 写盘后缓存刷新（create_script 与 patch_script 共用）——
//
// 用 CACHE_MODE_IGNORE 重读磁盘并重编译，失败时 error_out 带原因并返回 false。
bool refresh_script_cache(const std::string &path, std::string &error_out);

mcp::JsonValue handle_execute_gdscript(const mcp::JsonValue &args);
mcp::JsonValue handle_load(const mcp::JsonValue &args);
mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_attach_to_node(const mcp::JsonValue &args);
mcp::JsonValue handle_detach_from_node(const mcp::JsonValue &args);
mcp::JsonValue handle_get_property(const mcp::JsonValue &args);
mcp::JsonValue handle_set_property(const mcp::JsonValue &args);
mcp::JsonValue handle_call_function(const mcp::JsonValue &args);
mcp::JsonValue handle_reload(const mcp::JsonValue &args);
mcp::JsonValue handle_get_variable_list(const mcp::JsonValue &args);

} // namespace script_ops
} // namespace godot_autopilot

#endif
