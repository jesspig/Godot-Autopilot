#ifndef GODOT_AUTOPILOT_RESOURCE_REGISTRY_HPP
#define GODOT_AUTOPILOT_RESOURCE_REGISTRY_HPP

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/object.hpp>
#include <string>

namespace godot_autopilot {
namespace resource_registry {

void register_resource(const godot::Ref<godot::Resource> &res,
                       const std::string &name);
godot::Ref<godot::Resource> lookup_memory(const std::string &name);
void erase_oid(int64_t object_id);
bool is_registered(const std::string &name);

} // namespace resource_registry
} // namespace godot_autopilot

#endif
