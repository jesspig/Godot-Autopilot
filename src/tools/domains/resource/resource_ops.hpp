#ifndef GODOT_AUTOPILOT_RESOURCE_OPS_HPP
#define GODOT_AUTOPILOT_RESOURCE_OPS_HPP

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <cstdint>
#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace resource_ops {

inline constexpr const char kLoadFailSuffix[] =
    " (file exists but failed to load — not imported or wrong type)";

bool normalize_resource_path(const std::string &raw, std::string &out,
                             std::string &error, bool allow_root = false);
std::string describe_target(const godot_autopilot::util::JsonValue &args);
bool load_resource_or_error(
    const std::string &path, godot::Ref<godot::Resource> &out_res,
    std::string &out_error, const std::string &type_hint = {},
    const char *load_fail_suffix = kLoadFailSuffix,
    godot::ResourceLoader::CacheMode cache_mode =
        godot::ResourceLoader::CACHE_MODE_REUSE);
godot::Ref<godot::Resource>
resolve_resource(const godot_autopilot::util::JsonValue &args, bool &out_has_oid,
                 int64_t &out_obj_id, std::string &out_error,
                 std::string *out_warning = nullptr);
godot::Ref<godot::Resource> resolve_memory_resource(const std::string &name);
void register_memory_resource(const godot::Ref<godot::Resource> &res,
                              const std::string &name);
bool try_resolve_resource_value(const godot_autopilot::util::JsonValue &val, godot::Variant &out,
                                std::string &out_error);
godot::Ref<godot::Resource> instantiate_resource_class(const std::string &type,
                                                       std::string &out_error);

godot_autopilot::util::JsonValue handle_load(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reload(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_load_threaded(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_load_threaded_get_status(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_load_threaded_wait(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_property(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_property(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_duplicate(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_type(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_exists(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_list_types(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_extensions(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_list_dir(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_uid(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_uid(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_remove(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_rename(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue apply_path_rewrite_transaction(const std::string &old_path,
                                              const std::string &new_path);
godot_autopilot::util::JsonValue handle_move(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_copy(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_directory(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_dependencies(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_has_dependency(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_references(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reimport(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_import_options(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_curve(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_curve_points(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_curve_baked_points(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_primitive_mesh(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_array_mesh(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_mesh_surface_material(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_assign_mesh_to_instance(const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue handle_load_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_query_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mesh_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_property_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_file_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_uid_set_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_import_action(const godot_autopilot::util::JsonValue &args);

} // namespace resource_ops
} // namespace godot_autopilot

#endif
