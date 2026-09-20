#include "tools/tool_catalog.hpp"

#include <algorithm>

namespace godot_autopilot {

void ToolCatalog::add_tool(const ToolInfo &info) {
  std::lock_guard<std::mutex> lock(mutex_);
  tools_[info.name] = info;
}

std::optional<ToolInfo> ToolCatalog::get_tool(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tools_.find(name);
  if (it == tools_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<ToolInfo> ToolCatalog::get_all_tools() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<ToolInfo> result;
  result.reserve(tools_.size());
  for (const auto &[_, info] : tools_) {
    result.push_back(info);
  }
  std::sort(result.begin(), result.end(),
            [](const ToolInfo &lhs, const ToolInfo &rhs) {
              return lhs.name < rhs.name;
            });
  return result;
}

void ToolCatalog::replace_tools(std::vector<ToolInfo> tools) {
  std::unordered_map<std::string, ToolInfo> replacement;
  replacement.reserve(tools.size());
  for (auto &info : tools) {
    replacement[info.name] = std::move(info);
  }
  std::lock_guard<std::mutex> lock(mutex_);
  tools_ = std::move(replacement);
}

std::vector<std::string> ToolCatalog::get_categories() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> result;
  for (const auto &[_, info] : tools_) {
    if (std::find(result.begin(), result.end(), info.category) ==
        result.end()) {
      result.push_back(info.category);
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

size_t ToolCatalog::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return tools_.size();
}

} // namespace godot_autopilot
