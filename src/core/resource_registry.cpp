#include "resource_registry.hpp"

#include <mutex>
#include <unordered_map>

namespace godot_autopilot {
namespace resource_registry {

namespace {

std::unordered_map<std::string, godot::Ref<godot::Resource>> g_cache;
std::mutex g_cache_mutex;

} // namespace

std::string oid_key(int64_t object_id) { return std::to_string(object_id); }

bool is_object_id_key(const std::string &s) {
  if (s.empty())
    return false;
  std::size_t index = 0;
  if (s[0] == '-') {
    index = 1;
    if (s.size() == 1)
      return false;
  }
  for (; index < s.size(); ++index) {
    if (s[index] < '0' || s[index] > '9')
      return false;
  }
  return true;
}

void register_resource(const godot::Ref<godot::Resource> &res,
                       const std::string &name) {
  std::lock_guard<std::mutex> lock(g_cache_mutex);
  std::string oid = oid_key(static_cast<int64_t>(res->get_instance_id()));
  g_cache[oid] = res;
  if (!name.empty()) {
    g_cache["name:" + name] = res;
  }
}

godot::Ref<godot::Resource> lookup_memory(const std::string &name) {
  std::lock_guard<std::mutex> lock(g_cache_mutex);
  auto it = g_cache.find("name:" + name);
  if (it != g_cache.end()) {
    return it->second;
  }
  if (is_object_id_key(name)) {
    auto it_oid = g_cache.find(name);
    if (it_oid != g_cache.end()) {
      return it_oid->second;
    }
  }
  return godot::Ref<godot::Resource>();
}

bool name_key_points_to(const std::string &key, int64_t instance_id,
                        int64_t candidate_instance_id) {
  const std::string prefix = "name:";
  return key.size() > prefix.size() && key.compare(0, prefix.size(), prefix) == 0 &&
         instance_id == candidate_instance_id;
}

std::vector<std::string> stale_name_keys(
    const std::vector<std::pair<std::string, int64_t>> &entries,
    int64_t object_id) {
  std::vector<std::string> result;
  for (const auto &entry : entries) {
    if (name_key_points_to(entry.first, entry.second, object_id)) {
      result.push_back(entry.first);
    }
  }
  return result;
}

std::vector<std::string> erase_oid_and_extract_names(int64_t object_id) {
  std::lock_guard<std::mutex> lock(g_cache_mutex);
  std::vector<std::pair<std::string, int64_t>> entries;
  entries.reserve(g_cache.size());
  for (const auto &entry : g_cache) {
    if (entry.second.is_null()) {
      continue;
    }
    entries.emplace_back(entry.first,
                         static_cast<int64_t>(entry.second->get_instance_id()));
  }
  std::vector<std::string> names = stale_name_keys(entries, object_id);
  for (const std::string &key : names) {
    g_cache.erase(key);
  }
  g_cache.erase(oid_key(object_id));
  return names;
}

void erase_oid(int64_t object_id) {
  erase_oid_and_extract_names(object_id);
}

} // namespace resource_registry
} // namespace godot_autopilot
