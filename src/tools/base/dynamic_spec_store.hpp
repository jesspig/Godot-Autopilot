#ifndef GODOT_AUTOPILOT_DYNAMIC_SPEC_STORE_HPP
#define GODOT_AUTOPILOT_DYNAMIC_SPEC_STORE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace dynamic_specs {

inline std::vector<ToolSpec> &store() {
  static std::vector<ToolSpec> v;
  return v;
}

inline std::mutex &mutex() {
  static std::mutex m;
  return m;
}

inline std::atomic<bool> &enabled() {
  static std::atomic<bool> value{true};
  return value;
}

class NodeOwnerRegistry {
public:
  void retain(uint64_t object_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++owners_[object_id];
  }

  bool owns(uint64_t object_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return owners_.find(object_id) != owners_.end();
  }

  bool release(uint64_t object_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = owners_.find(object_id);
    if (it == owners_.end()) {
      return false;
    }
    if (--it->second > 0) {
      return false;
    }
    owners_.erase(it);
    return true;
  }

  std::vector<uint64_t> release_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint64_t> released;
    released.reserve(owners_.size());
    for (const auto &entry : owners_) {
      released.push_back(entry.first);
    }
    owners_.clear();
    return released;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return owners_.size();
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<uint64_t, size_t> owners_;
};

} // namespace dynamic_specs
} // namespace godot_autopilot

#endif
