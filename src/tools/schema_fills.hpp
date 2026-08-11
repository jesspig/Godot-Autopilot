#ifndef GODOT_AUTOPILOT_SCHEMA_FILLS_HPP
#define GODOT_AUTOPILOT_SCHEMA_FILLS_HPP

#include <mcp/JsonValue.hpp>
#include <string>
#include <unordered_map>

namespace godot_autopilot {

void fill_schema_scene(std::unordered_map<std::string, mcp::JsonValue>& m);
void fill_schema_editor_config(std::unordered_map<std::string, mcp::JsonValue>& m);
void fill_schema_physics(std::unordered_map<std::string, mcp::JsonValue>& m);
void fill_schema_render_audio(std::unordered_map<std::string, mcp::JsonValue>& m);
void fill_schema_debug_sys(std::unordered_map<std::string, mcp::JsonValue>& m);
void fill_schema_content(std::unordered_map<std::string, mcp::JsonValue>& m);

} // namespace godot_autopilot

#endif
