#ifndef GODOT_AUTOPILOT_DOC_TOOLS_HPP
#define GODOT_AUTOPILOT_DOC_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/doc/doc_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace doc_tools {

namespace {

const std::vector<ParamSpec> kGetDocsClassParams = {
    {"class", "string", "Godot class name (e.g. 'Node2D', 'TileMap') or builtin Variant type name (e.g. 'Vector2', 'Color', 'Rect2', 'StringName'); builtin types return value_kind/variant_type/json_shape plus a curated methods/constants subset", true},
    {"section", "string", "Optional comma-separated subset of sections to return: methods, properties, signals, enums, constants (default: all). Use it to shrink huge classes", false},
    {"member", "string", "Optional member name filter within the returned sections; case-insensitive substring by default, or exact case-sensitive match when 'exact' is true (default: empty = no filter)", false},
    {"exact", "boolean", "When true, 'member' must match the member name exactly; when false (default) it is a case-insensitive substring filter", false},
};

const std::vector<ParamSpec> kFindDocsClassParams = {
    {"query", "string", "Search text matched case-insensitively against class names (e.g. 'node' matches Node, Node2D, SceneTree); returns up to 50 matches", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(2);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_docs_class",
      "Get a Godot class's reflected signature info from ClassDB (no docstrings, unlike the official documentation): parent_class, api_type, can_instantiate, methods, properties, signals, enums and constants. Requires the class name. Builtin Variant type names (Vector2, Color, Rect2, StringName and the rest of the Variant codec coverage) are accepted too and return value_kind/variant_type/json_shape with a curated methods/constants subset. Errors when the class does not exist. Optional section (comma-separated methods/properties/signals/enums/constants), member (case-insensitive substring by default) and exact (case-sensitive exact member match) narrow the output for huge classes. Combine with find_docs_class to locate classes first.",
      "Docs", {"docs", "class", "get"}, SideEffect::None, tool_flags::kNone,
      kGetDocsClassParams, doc_ops::handle_get_class},
      {
          ToolAlias{
              "get_docs_method", "get_docs_class",
              [](const mcp::JsonValue &args) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                const mcp::JsonValue *class_name = args.Find("class");
                const mcp::JsonValue *method = args.Find("method");
                if (class_name != nullptr) out["class"] = *class_name;
                if (method != nullptr) out["member"] = *method;
                out["section"] = mcp::JsonValue("methods");
                out["exact"] = mcp::JsonValue(true);
                return out;
              },
              [](const mcp::JsonValue &result) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                const mcp::JsonValue *inner = result.Find("result");
                const mcp::JsonValue *methods =
                    inner != nullptr && inner->IsObject()
                        ? inner->Find("methods")
                        : nullptr;
                if (methods != nullptr && methods->IsArray() &&
                    methods->Size() > 0) {
                  out["result"] = (*methods)[0];
                  out["result"]["note"] = mcp::JsonValue(
                      "method signature without docstrings");
                } else {
                  out["error"] = mcp::JsonValue("method not found");
                }
                return out;
              },
              "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                const mcp::JsonValue *class_name = args.Find("class");
                const mcp::JsonValue *method = args.Find("method");
                if (class_name == nullptr || !class_name->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: class");
                  return error;
                }
                if (method == nullptr || !method->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: method");
                  return error;
                }
                return mcp::JsonValue();
              }},
          ToolAlias{
              "get_docs_property", "get_docs_class",
              [](const mcp::JsonValue &args) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                const mcp::JsonValue *class_name = args.Find("class");
                const mcp::JsonValue *property = args.Find("property");
                if (class_name != nullptr) out["class"] = *class_name;
                if (property != nullptr) out["member"] = *property;
                out["section"] = mcp::JsonValue("properties");
                out["exact"] = mcp::JsonValue(true);
                return out;
              },
              [](const mcp::JsonValue &result) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                const mcp::JsonValue *inner = result.Find("result");
                const mcp::JsonValue *properties =
                    inner != nullptr && inner->IsObject()
                        ? inner->Find("properties")
                        : nullptr;
                if (properties != nullptr && properties->IsArray() &&
                    properties->Size() > 0) {
                  out["result"] = (*properties)[0];
                } else {
                  out["error"] = mcp::JsonValue("property not found");
                }
                return out;
              },
              "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                const mcp::JsonValue *class_name = args.Find("class");
                const mcp::JsonValue *property = args.Find("property");
                if (class_name == nullptr || !class_name->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: class");
                  return error;
                }
                if (property == nullptr || !property->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: property");
                  return error;
                }
                return mcp::JsonValue();
              }},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "find_docs_class",
      "Search all registered Godot classes by case-insensitive substring match on the name. Requires query; returns up to 50 matching classes with name, parent and api_type. Use it before get_docs_class to confirm the exact class name or to discover related classes.",
      "Docs", {"docs", "search", "class"}, SideEffect::None, tool_flags::kNone,
      kFindDocsClassParams, doc_ops::handle_search}));
  return v;
}

} // namespace doc_tools
} // namespace godot_autopilot

#endif