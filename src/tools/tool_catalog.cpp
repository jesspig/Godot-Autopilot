#include "tools/tool_catalog.hpp"

#include <algorithm>

namespace godot_autopilot {

void ToolCatalog::add_tool(const ToolInfo &info) {
  std::lock_guard<std::mutex> lock(mutex_);
  tools_[info.name] = info;
}

const ToolInfo *ToolCatalog::get_tool(const std::string &name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tools_.find(name);
  if (it == tools_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::vector<const ToolInfo *> ToolCatalog::get_all_tools() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<const ToolInfo *> result;
  result.reserve(tools_.size());
  for (const auto &[_, info] : tools_) {
    result.push_back(&info);
  }
  return result;
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
  return result;
}

size_t ToolCatalog::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return tools_.size();
}

void ToolCatalog::populate_default_tools() {
  if (size() > 0)
    return;

  {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    add_tool({"ping",
              "Health check ping",
              "Meta",
              {"health", "ping"},
              std::move(s)});
  }

  {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    add_tool({"system_status",
              "Get server status info",
              "System",
              {"status", "info"},
              std::move(s)});
  }

  {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");

    mcp::JsonValue props(mcp::JsonValue::object_tag);

    mcp::JsonValue query_prop(mcp::JsonValue::object_tag);
    query_prop["type"] = mcp::JsonValue("string");
    props["query"] = std::move(query_prop);

    mcp::JsonValue cat_prop(mcp::JsonValue::object_tag);
    cat_prop["type"] = mcp::JsonValue("string");
    props["category"] = std::move(cat_prop);

    mcp::JsonValue tags_prop(mcp::JsonValue::object_tag);
    tags_prop["type"] = mcp::JsonValue("array");
    mcp::JsonValue items(mcp::JsonValue::object_tag);
    items["type"] = mcp::JsonValue("string");
    tags_prop["items"] = std::move(items);
    props["tags"] = std::move(tags_prop);

    s["properties"] = std::move(props);

    mcp::JsonValue req(mcp::JsonValue::array_tag);
    req.PushBack(mcp::JsonValue("query"));
    s["required"] = std::move(req);

    add_tool({"search_tools",
              "Search available tools by query",
              "Meta",
              {"search", "discovery"},
              std::move(s)});
  }

  {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    add_tool({"list_categories",
              "List all tool categories",
              "Meta",
              {"categories", "discovery"},
              std::move(s)});
  }

  {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");

    mcp::JsonValue props(mcp::JsonValue::object_tag);
    mcp::JsonValue name_prop(mcp::JsonValue::object_tag);
    name_prop["type"] = mcp::JsonValue("string");
    props["name"] = std::move(name_prop);
    s["properties"] = std::move(props);

    mcp::JsonValue req(mcp::JsonValue::array_tag);
    req.PushBack(mcp::JsonValue("name"));
    s["required"] = std::move(req);

    add_tool({"get_tool_detail",
              "Get complete schema for one tool",
              "Meta",
              {"detail", "schema"},
              std::move(s)});
  }
}

} // namespace godot_autopilot
