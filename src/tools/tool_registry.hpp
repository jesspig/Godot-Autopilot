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
      meta_[name] = std::shared_ptr<ToolBase>(std::move(tool));
    } else {
      tools_[name] = std::shared_ptr<ToolBase>(std::move(tool));
    }
  }

  std::shared_ptr<ToolBase> find(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second : nullptr;
  }

  std::shared_ptr<ToolBase> find_meta(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = meta_.find(name);
    return it != meta_.end() ? it->second : nullptr;
  }

  std::shared_ptr<ToolBase> find_any(const std::string& name) const {
    std::shared_ptr<ToolBase> tool = find(name);
    return tool != nullptr ? tool : find_meta(name);
  }

  std::vector<std::shared_ptr<ToolBase>> all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect(tools_);
  }

  std::vector<std::shared_ptr<ToolBase>> all_meta() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect(meta_);
  }

  std::vector<std::shared_ptr<ToolBase>> all_any() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<ToolBase>> result = collect(tools_);
    std::vector<std::shared_ptr<ToolBase>> m = collect(meta_);
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

private:
  std::vector<std::shared_ptr<ToolBase>> collect(
      const std::unordered_map<std::string, std::shared_ptr<ToolBase>>& m) const {
    std::vector<std::shared_ptr<ToolBase>> result;
    result.reserve(m.size());
    for (const auto& entry : m)
      result.push_back(entry.second);
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
      return lhs->meta().name < rhs->meta().name;
    });
    return result;
  }

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<ToolBase>> tools_;
  std::unordered_map<std::string, std::shared_ptr<ToolBase>> meta_;
};

} // namespace godot_autopilot

#endif
