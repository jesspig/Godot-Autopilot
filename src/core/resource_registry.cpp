#include "resource_registry.hpp"

#include <mutex>
#include <unordered_map>

namespace godot_self_driving {
namespace resource_registry {

namespace {

std::unordered_map<std::string, godot::Ref<godot::Resource>> g_cache;
std::mutex g_cache_mutex;

std::string oid_key(int64_t object_id) {
    return std::to_string(object_id);
}

bool is_pure_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // namespace

void register_resource(const godot::Ref<godot::Resource>& res, const std::string& name) {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    std::string oid = oid_key(static_cast<int64_t>(res->get_instance_id()));
    g_cache[oid] = res;
    if (!name.empty()) {
        g_cache["name:" + name] = res;
    }
}

godot::Ref<godot::Resource> lookup_memory(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    auto it = g_cache.find("name:" + name);
    if (it != g_cache.end()) {
        return it->second;
    }
    if (is_pure_digits(name)) {
        auto it_oid = g_cache.find(name);
        if (it_oid != g_cache.end()) {
            return it_oid->second;
        }
    }
    return godot::Ref<godot::Resource>();
}

void erase_oid(int64_t object_id) {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    g_cache.erase(oid_key(object_id));
}

bool is_registered(const std::string& name) {
    return !lookup_memory(name).is_null();
}

} // namespace resource_registry
} // namespace godot_self_driving
