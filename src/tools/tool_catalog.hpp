#ifndef GODOT_AUTOPILOT_TOOL_CATALOG_HPP
#define GODOT_AUTOPILOT_TOOL_CATALOG_HPP

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

struct ToolInfo {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  mcp::JsonValue input_schema;
};

class ToolCatalog {
public:
  void add_tool(const ToolInfo &info);
  std::optional<ToolInfo> get_tool(const std::string &name) const;
  std::vector<ToolInfo> get_all_tools() const;
  void replace_tools(std::vector<ToolInfo> tools);
  std::vector<std::string> get_categories() const;
  size_t size() const;

  void populate_default_tools();

  ToolCatalog() = default;

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ToolInfo> tools_;
};

} // namespace godot_autopilot

#endif
