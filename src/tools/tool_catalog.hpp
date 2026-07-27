#ifndef GODOT_SELF_DRIVING_TOOL_CATALOG_HPP
#define GODOT_SELF_DRIVING_TOOL_CATALOG_HPP

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {

struct ToolInfo {
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    mcp::JsonValue input_schema;
};

class ToolCatalog {
public:
    void add_tool(const ToolInfo& info);
    const ToolInfo* get_tool(const std::string& name) const;
    std::vector<const ToolInfo*> get_all_tools() const;
    std::vector<std::string> get_categories() const;
    size_t size() const;

    void populate_default_tools();

    static ToolCatalog& instance();

    ToolCatalog() = default;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolInfo> tools_;
};

} // namespace godot_self_driving

#endif
