#ifndef GODOT_AUTOPILOT_TOOL_REGISTRY_HPP
#define GODOT_AUTOPILOT_TOOL_REGISTRY_HPP

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <tools/base/tool_base.hpp>
#include <tools/base/tool_catalog.hpp>
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {

inline ToolInfo make_tool_info(const ToolBase& t) {
  const ToolMeta& m = t.meta();
  const uint32_t flags = t.tool_flags();
  ToolInfo info{m.name, m.description, m.category, m.tags, t.input_schema()};
  info.dynamic = (flags & tool_flags::kDynamic) != 0;
  info.mutating = (flags & tool_flags::kMutating) != 0;
  if (const auto *spec_tool = dynamic_cast<const SpecTool *>(&t)) {
    info.examples = spec_tool->spec().examples;
  }
  return info;
}

class ToolRegistry {
public:
  void add(std::unique_ptr<ToolBase> tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string name = tool->meta().name;
    if (find_instantiated_locked(name) != nullptr) {
      throw std::invalid_argument("duplicate tool name: " + name);
    }
    aliases_.erase(name);
    auto *spec_tool = dynamic_cast<SpecTool *>(tool.get());
    if ((tool->tool_flags() & tool_flags::kMeta) != 0) {
      meta_[name] = std::shared_ptr<ToolBase>(std::move(tool));
    } else {
      tools_[name] = std::shared_ptr<ToolBase>(std::move(tool));
    }
    if (spec_tool != nullptr) {
      for (const ToolAlias &alias : spec_tool->spec().aliases) {
        register_alias_locked(alias);
      }
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

  std::optional<ToolAlias> resolve_alias(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = aliases_.find(name);
    if (it == aliases_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  std::unordered_map<std::string, std::string> alias_map() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, std::string> result;
    result.reserve(aliases_.size());
    for (const auto& entry : aliases_) {
      result[entry.first] = entry.second.canonical;
    }
    return result;
  }

  size_t alias_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aliases_.size();
  }

private:
  std::shared_ptr<ToolBase> find_instantiated_locked(
      const std::string& name) const {
    auto tool = tools_.find(name);
    if (tool != tools_.end()) {
      return tool->second;
    }
    auto meta = meta_.find(name);
    return meta != meta_.end() ? meta->second : nullptr;
  }

  void register_alias_locked(const ToolAlias& alias) {
    if (alias.name.empty() || alias.canonical.empty() ||
        alias.name == alias.canonical) {
      return;
    }
    if (tools_.find(alias.name) != tools_.end() ||
        meta_.find(alias.name) != meta_.end() ||
        aliases_.find(alias.name) != aliases_.end()) {
      return;
    }
    aliases_[alias.name] = alias;
  }

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
  std::unordered_map<std::string, ToolAlias> aliases_;
};

} // namespace godot_autopilot

#endif
