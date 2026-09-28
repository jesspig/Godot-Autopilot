#ifndef GODOT_AUTOPILOT_DOC_TOOLS_HPP
#define GODOT_AUTOPILOT_DOC_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/doc_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace doc_tools {

namespace {

const std::vector<ParamSpec> kGetDocsClassParams = {
    {"class", "string", "Godot class name (e.g. 'Node2D', 'TileMap') or builtin Variant type name (e.g. 'Vector2', 'Color', 'Rect2', 'StringName'); builtin types return value_kind/variant_type/json_shape plus a curated methods/constants subset", true},
    {"section", "string", "Optional comma-separated subset of sections to return: methods, properties, signals, enums, constants (default: all). Use it to shrink huge classes", false},
    {"member", "string", "Optional case-insensitive substring filter on member names within the returned sections (default: empty = no filter)", false},
};

const std::vector<ParamSpec> kFindDocsClassParams = {
    {"query", "string", "Search text matched case-insensitively against class names (e.g. 'node' matches Node, Node2D, SceneTree); returns up to 50 matches", true},
};

const std::vector<ParamSpec> kGetDocsMethodParams = {
    {"class", "string", "Godot class name (e.g. 'Node2D')", true},
    {"method", "string", "Method name to look up (e.g. 'queue_free'); returns the reflected signature without docstrings", true},
};

const std::vector<ParamSpec> kGetDocsPropertyParams = {
    {"class", "string", "Godot class name (e.g. 'Node2D')", true},
    {"property", "string", "Property name to look up (e.g. 'position'); returns the reflected property info without docstrings", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(ToolSpec{
      "get_docs_class",
      "Get a Godot class's reflected signature info from ClassDB (no docstrings, unlike the official documentation): parent_class, api_type, can_instantiate, methods, properties, signals, enums and constants. Requires the class name. Builtin Variant type names (Vector2, Color, Rect2, StringName and the rest of the Variant codec coverage) are accepted too and return value_kind/variant_type/json_shape with a curated methods/constants subset. Errors when the class does not exist. Optional section (comma-separated methods/properties/signals/enums/constants) and member (case-insensitive substring) narrow the output for huge classes. Combine with find_docs_class to locate classes first.",
      "Docs", {"docs", "class", "get"}, SideEffect::None, tool_flags::kNone,
      kGetDocsClassParams, doc_ops::handle_get_class}));
  v.push_back(make_spec_tool(ToolSpec{
      "find_docs_class",
      "Search all registered Godot classes by case-insensitive substring match on the name. Requires query; returns up to 50 matching classes with name, parent and api_type. Use it before get_docs_class to confirm the exact class name or to discover related classes.",
      "Docs", {"docs", "search", "class"}, SideEffect::None, tool_flags::kNone,
      kFindDocsClassParams, doc_ops::handle_search}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_docs_method",
      "Look up the reflected signature of a single method on a class via ClassDB. Requires class and method names; returns the method dictionary (name, arguments, return type, flags) plus a note that docstrings are not included. Errors when the class or method does not exist. Use get_docs_class first to verify the method name.",
      "Docs", {"docs", "method", "get"}, SideEffect::None, tool_flags::kNone,
      kGetDocsMethodParams, doc_ops::handle_get_method}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_docs_property",
      "Look up the reflected property info of a single property on a class via ClassDB. Requires class and property names; returns the property dictionary (name, type, hint, usage, class_name). Errors when the class or property does not exist. Per-node theme_override_* values are not ClassDB properties: querying one here returns guidance to use property_get_list on the live node instead. Explore a class with get_docs_class first.",
      "Docs", {"docs", "property", "get"}, SideEffect::None, tool_flags::kNone,
      kGetDocsPropertyParams, doc_ops::handle_get_property}));
  return v;
}

} // namespace doc_tools
} // namespace godot_autopilot

#endif