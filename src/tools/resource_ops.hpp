#ifndef GODOT_AUTOPILOT_RESOURCE_OPS_HPP
#define GODOT_AUTOPILOT_RESOURCE_OPS_HPP

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace resource_ops {

godot::Ref<godot::Resource> resolve_memory_resource(const std::string &name);
void register_memory_resource(const godot::Ref<godot::Resource> &res,
                              const std::string &name);
bool try_resolve_resource_value(const mcp::JsonValue &val, godot::Variant &out,
                                std::string &out_error);

mcp::JsonValue handle_load(const mcp::JsonValue &args);
mcp::JsonValue handle_load_threaded(const mcp::JsonValue &args);
mcp::JsonValue handle_load_threaded_get_status(const mcp::JsonValue &args);
mcp::JsonValue handle_load_threaded_wait(const mcp::JsonValue &args);
mcp::JsonValue handle_save(const mcp::JsonValue &args);
mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_set_property(const mcp::JsonValue &args);
mcp::JsonValue handle_get_property(const mcp::JsonValue &args);
mcp::JsonValue handle_duplicate(const mcp::JsonValue &args);
mcp::JsonValue handle_get_type(const mcp::JsonValue &args);
mcp::JsonValue handle_exists(const mcp::JsonValue &args);
mcp::JsonValue handle_list_types(const mcp::JsonValue &args);
mcp::JsonValue handle_get_extensions(const mcp::JsonValue &args);
mcp::JsonValue handle_list_dir(const mcp::JsonValue &args);
mcp::JsonValue handle_get_uid(const mcp::JsonValue &args);
mcp::JsonValue handle_set_uid(const mcp::JsonValue &args);
mcp::JsonValue handle_remove(const mcp::JsonValue &args);
mcp::JsonValue handle_rename(const mcp::JsonValue &args);
mcp::JsonValue apply_path_rewrite_transaction(const std::string &old_path,
                                              const std::string &new_path);
mcp::JsonValue handle_move(const mcp::JsonValue &args);
mcp::JsonValue handle_create_directory(const mcp::JsonValue &args);
mcp::JsonValue handle_get_dependencies(const mcp::JsonValue &args);
mcp::JsonValue handle_has_dependency(const mcp::JsonValue &args);
mcp::JsonValue handle_get_references(const mcp::JsonValue &args);
mcp::JsonValue handle_reimport(const mcp::JsonValue &args);

} // namespace resource_ops
} // namespace godot_autopilot

#endif
