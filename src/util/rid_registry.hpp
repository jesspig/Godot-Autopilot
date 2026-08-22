#ifndef GODOT_AUTOPILOT_RID_REGISTRY_HPP
#define GODOT_AUTOPILOT_RID_REGISTRY_HPP

#include <cstdint>

#include <godot_cpp/variant/rid.hpp>
#include <mcp/JsonValue.hpp>
#include <unordered_map>

namespace godot_autopilot {
namespace util {

class RidStore {
public:
  int64_t store(const godot::RID &rid) {
    int64_t id = rid.get_id();
    map[id] = rid;
    return id;
  }

  godot::RID get(int64_t id) const {
    auto it = map.find(id);
    if (it != map.end())
      return it->second;
    return godot::RID();
  }

private:
  std::unordered_map<int64_t, godot::RID> map;
};

template <typename Domain> inline RidStore &rid_store() {
  static RidStore store;
  return store;
}

template <typename Domain>
inline godot::RID resolve_rid(const mcp::JsonValue &args, const char *key) {
  auto *it = args.Find(key);
  if (!it || !it->IsNumber())
    return godot::RID();
  return rid_store<Domain>().get(it->GetInt());
}

} // namespace util
} // namespace godot_autopilot

#endif
