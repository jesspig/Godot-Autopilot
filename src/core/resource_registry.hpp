#ifndef GODOT_AUTOPILOT_RESOURCE_REGISTRY_HPP
#define GODOT_AUTOPILOT_RESOURCE_REGISTRY_HPP

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/object.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace resource_registry {

bool name_key_points_to(const std::string &key, int64_t instance_id,
                        int64_t candidate_instance_id);
std::vector<std::string> stale_name_keys(
    const std::vector<std::pair<std::string, int64_t>> &entries,
    int64_t object_id);

void register_resource(const godot::Ref<godot::Resource> &res,
                       const std::string &name);
godot::Ref<godot::Resource> lookup_memory(const std::string &name);
void erase_oid(int64_t object_id);

} // namespace resource_registry
} // namespace godot_autopilot

#endif
