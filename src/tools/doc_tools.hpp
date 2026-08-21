#ifndef GODOT_AUTOPILOT_DOC_TOOLS_HPP
#define GODOT_AUTOPILOT_DOC_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/doc_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace doc_tools {

GDA_TOOL_CLASS(GetDocsClassTool, "get_docs_class",
               "Get a Godot class's reflected signature info from ClassDB (no docstrings, unlike the official documentation): parent_class, api_type, can_instantiate, methods, properties, signals, enums and constants. Requires the class name. Errors when the class does not exist. Combine with find_docs_class to locate classes first.",
               "Docs", std::vector<std::string>({"docs", "class", "get"}), doc_ops::handle_get_class, false)

GDA_TOOL_CLASS(FindDocsClassTool, "find_docs_class",
               "Search all registered Godot classes by case-insensitive substring match on the name. Requires query; returns up to 50 matching classes with name, parent and api_type. Use it before get_docs_class to confirm the exact class name or to discover related classes.",
               "Docs", std::vector<std::string>({"docs", "search", "class"}), doc_ops::handle_search, false)

GDA_TOOL_CLASS(GetDocsMethodTool, "get_docs_method",
               "Look up the reflected signature of a single method on a class via ClassDB. Requires class and method names; returns the method dictionary (name, arguments, return type, flags) plus a note that docstrings are not included. Errors when the class or method does not exist. Use get_docs_class first to verify the method name.",
               "Docs", std::vector<std::string>({"docs", "method", "get"}), doc_ops::handle_get_method, true)

GDA_TOOL_CLASS(GetDocsPropertyTool, "get_docs_property",
               "Look up the reflected property info of a single property on a class via ClassDB. Requires class and property names; returns the property dictionary (name, type, hint, usage, class_name). Errors when the class or property does not exist. Explore a class with get_docs_class first.",
               "Docs", std::vector<std::string>({"docs", "property", "get"}), doc_ops::handle_get_property, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(std::make_unique<GetDocsClassTool>());
  v.push_back(std::make_unique<FindDocsClassTool>());
  v.push_back(std::make_unique<GetDocsMethodTool>());
  v.push_back(std::make_unique<GetDocsPropertyTool>());
  return v;
}

} // namespace doc_tools
} // namespace godot_autopilot

#endif