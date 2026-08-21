#ifndef GODOT_AUTOPILOT_TOOL_REGISTRY_HPP
#define GODOT_AUTOPILOT_TOOL_REGISTRY_HPP

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <tools/tool_base.hpp>
#include <tools/tool_catalog.hpp>

namespace godot_autopilot {

inline ToolInfo make_tool_info(const ToolBase& t) {
  const ToolMeta& m = t.meta();
  return ToolInfo{m.name, m.description, m.category, m.tags, t.input_schema()};
}

class ToolRegistry {
public:
  void add(std::unique_ptr<ToolBase> tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string name = tool->meta().name;
    if (dynamic_cast<const IMetaTool*>(tool.get())) {
      meta_[name] = std::move(tool);
    } else {
      tools_[name] = std::move(tool);
    }
  }

  void add_meta(std::unique_ptr<ToolBase> tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    meta_[tool->meta().name] = std::move(tool);
  }

  ToolBase* find(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.get() : nullptr;
  }

  ToolBase* find_meta(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = meta_.find(name);
    return it != meta_.end() ? it->second.get() : nullptr;
  }

  ToolBase* find_any(const std::string& name) {
    ToolBase* tool = find(name);
    return tool != nullptr ? tool : find_meta(name);
  }

  std::vector<ToolBase*> all() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect(tools_);
  }

  std::vector<ToolBase*> all_meta() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect(meta_);
  }

  std::vector<ToolBase*> all_any() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolBase*> result = collect(tools_);
    std::vector<ToolBase*> m = collect(meta_);
    result.insert(result.end(), m.begin(), m.end());
    return result;
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.size();
  }

  size_t meta_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return meta_.size();
  }

  std::vector<std::string> categories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& entry : tools_) {
      const std::string& cat = entry.second->meta().category;
      if (std::find(result.begin(), result.end(), cat) == result.end())
        result.push_back(cat);
    }
    for (const auto& entry : meta_) {
      const std::string& cat = entry.second->meta().category;
      if (std::find(result.begin(), result.end(), cat) == result.end())
        result.push_back(cat);
    }
    return result;
  }

  std::vector<ToolInfo> to_tool_info_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolInfo> result;
    result.reserve(tools_.size() + meta_.size());
    for (const auto& entry : tools_)
      result.push_back(make_tool_info(*entry.second));
    for (const auto& entry : meta_)
      result.push_back(make_tool_info(*entry.second));
    return result;
  }

private:
  std::vector<ToolBase*> collect(
      const std::unordered_map<std::string, std::unique_ptr<ToolBase>>& m) {
    std::vector<ToolBase*> result;
    result.reserve(m.size());
    for (const auto& entry : m)
      result.push_back(entry.second.get());
    return result;
  }

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<ToolBase>> tools_;
  std::unordered_map<std::string, std::unique_ptr<ToolBase>> meta_;
};

} // namespace godot_autopilot

#endif