#ifndef GODOT_AUTOPILOT_CONFIG_TOOLS_HPP
#define GODOT_AUTOPILOT_CONFIG_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/config_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace config_tools {

namespace {

const std::vector<ParamSpec> kGetProjectSettingsParams = {
    {"name", "string", "Project setting name, e.g. display/window/size/viewport_width; settings come from project.godot", true},
    {"default", "object", "Serialized JSON value returned when the setting does not exist; otherwise a null value is returned", false},
};

const std::vector<ParamSpec> kSetProjectSettingsParams = {
    {"name", "string", "Project setting name, e.g. application/config/name; must already exist or be added by this call", true},
    {"value", "object", "Serialized JSON value; when a string, the engine keeps the existing setting's type if one is stored, e.g. 1920 for an int setting", true},
};

const std::vector<ParamSpec> kHasProjectSettingsParams = {
    {"name", "string", "Project setting name to check for existence in project.godot", true},
};

const std::vector<ParamSpec> kSaveProjectSettingsParams = {};

const std::vector<ParamSpec> kGetEngineVersionParams = {};

const std::vector<ParamSpec> kGetEngineFpsParams = {};

const std::vector<ParamSpec> kGetEngineFramesDrawnParams = {};

const std::vector<ParamSpec> kSetEngineTimeScaleParams = {
    {"scale", "number", "Time scale multiplier: 1.0 = real time, 0.5 = half speed, 2.0 = double speed; 0.0 stops gameplay processing", true},
};

const std::vector<ParamSpec> kGetEngineTimeScaleParams = {};

const std::vector<ParamSpec> kSetEngineMaxFpsParams = {
    {"fps", "integer", "Frame rate cap in frames per second; 0 disables the cap (uncapped)", true},
};

const std::vector<ParamSpec> kGetEditorSettingsParams = {
    {"name", "string", "Editor setting name, e.g. interface/theme/base_color; settings come from the editor's own preferences. Alongside 'result' (the raw value, unchanged) the response carries the property list metadata: 'hint' (integer PropertyHint) plus 'hint_string' when non-empty; enum settings (hint 2) also include 'enum_options', an array of {'label', 'value'} entries that decodes bare integers such as run/window_placement/game_embed_mode", true},
};

const std::vector<ParamSpec> kSetEditorSettingsParams = {
    {"name", "string", "Editor setting name to write, e.g. interface/theme/base_color", true},
    {"value", "object", "Serialized JSON value to store; editor settings persist automatically", true},
};

const std::vector<ParamSpec> kHasEditorSettingsParams = {
    {"name", "string", "Editor setting name to check for existence in the editor's preferences", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(13);
  v.push_back(make_spec_tool(ToolSpec{
      "get_project_settings",
      "Read a project setting by name from the project configuration (project.godot). Use it to query project-wide values such as display, physics or input defaults. The optional default is returned when the setting does not exist. Returns the serialized value; contrast with get_editor_settings, which reads editor preferences.",
      "Config", {"config", "project", "settings", "get"}, SideEffect::None, tool_flags::kNone,
      kGetProjectSettingsParams, config_ops::handle_project_settings_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_project_settings",
      "Set a project setting in memory. Note: the change is NOT written to project.godot automatically, so call save_project_settings afterwards or the setting is lost when the editor closes. value is a serialized JSON value; the engine keeps the existing type when known. Differs from set_editor_settings, which persists automatically. Returns ok.",
      "Config", {"config", "project", "settings", "set"}, SideEffect::None, tool_flags::kNone,
      kSetProjectSettingsParams, config_ops::handle_project_settings_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_project_settings",
      "Check whether a project setting exists by name in the project configuration (project.godot). Use it before reading with get_project_settings to distinguish a missing setting from a stored default value. Contrast with has_editor_settings, which tests editor preferences instead. Returns a boolean result.",
      "Config", {"config", "project", "settings", "has"}, SideEffect::None, tool_flags::kNone,
      kHasProjectSettingsParams, config_ops::handle_project_settings_has}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_project_settings",
      "Persist all project settings to project.godot. Call it after set_project_settings so in-memory changes survive an editor restart; without it the changes are discarded on exit. This is the required flush step, no other config tool writes project.godot. Returns ok, or an error with the code when saving fails.",
      "Config", {"config", "project", "settings", "save"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSaveProjectSettingsParams, config_ops::handle_project_settings_save}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_engine_version",
      "Read the version of the running Godot engine, useful for branching behavior or reporting the environment. Returns an object with major, minor and patch (integers) plus string, the full version label such as '4.7.1-stable'. Takes no parameters; all four fields are always present.",
      "Config", {"config", "engine", "version"}, SideEffect::None, tool_flags::kNone,
      kGetEngineVersionParams, config_ops::handle_engine_get_version}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_engine_fps",
      "Measure the current real-time frames per second rendered by the engine, e.g. to verify performance after scene changes. Unlike set_engine_max_fps (a cap on the frame rate) and get_engine_frames_drawn (a cumulative counter), this reports the live measured rate. Returns a float that fluctuates per frame.",
      "Config", {"config", "engine", "fps", "get"}, SideEffect::None, tool_flags::kNone,
      kGetEngineFpsParams, config_ops::handle_engine_get_fps}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_engine_frames_drawn",
      "Read the total number of frames drawn since the engine started, a monotonically increasing counter that resets only on restart. Use it to measure elapsed frames between two points or to detect stalls, complementing get_engine_fps (the live rate). Returns an integer.",
      "Config", {"config", "engine", "frames"}, SideEffect::None, tool_flags::kNone,
      kGetEngineFramesDrawnParams, config_ops::handle_engine_get_frames_drawn}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_engine_time_scale",
      "Set the global time scale multiplier for the game loop: 1.0 is real time, 0.5 halves the speed, 2.0 doubles it. The scale affects both process and physics timing; values below 1.0 create slow-motion effects, and 0.0 stops gameplay processing. Returns ok; read the current value with get_engine_time_scale.",
      "Config", {"config", "engine", "time", "scale", "set"}, SideEffect::None, tool_flags::kNone,
      kSetEngineTimeScaleParams, config_ops::handle_engine_set_time_scale}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_engine_time_scale",
      "Read the current global time scale previously set by set_engine_time_scale, defaulting to 1.0 (real time). Pair it with set_engine_time_scale when debugging slow-motion or sped-up gameplay, or to confirm a scale change was applied. Returns a float representing the multiplier applied to the game loop.",
      "Config", {"config", "engine", "time", "scale", "get"}, SideEffect::None, tool_flags::kNone,
      kGetEngineTimeScaleParams, config_ops::handle_engine_get_time_scale}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_engine_max_fps",
      "Cap the engine's frame rate at fps frames per second; 0 disables the cap entirely and lets the engine run uncapped. This sets an upper limit only, it does not measure the actual rate, use get_engine_fps for that. Returns ok.",
      "Config", {"config", "engine", "fps", "max"}, SideEffect::None, tool_flags::kNone,
      kSetEngineMaxFpsParams, config_ops::handle_engine_set_max_fps}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_editor_settings",
      "Read an editor preference by name from the editor's own settings store, which persists automatically across sessions. Use it for editor UI configuration such as theme, docks or input preferences. Differs from get_project_settings, which reads project.godot values. Returns the serialized value.",
      "Config", {"config", "editor", "settings", "get"}, SideEffect::None, tool_flags::kNone,
      kGetEditorSettingsParams, config_ops::handle_editor_settings_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_editor_settings",
      "Set an editor preference by name, e.g. interface or plugin options. Use it to tweak editor behavior without touching project.godot. Unlike set_project_settings, editor settings persist automatically, no explicit save step is required, and the value survives editor restarts. Returns ok; verify the applied value with get_editor_settings.",
      "Config", {"config", "editor", "settings", "set"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetEditorSettingsParams, config_ops::handle_editor_settings_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_editor_settings",
      "Check whether an editor preference exists by name in the editor's settings store. Use it to probe whether a preference is customized before reading it with get_editor_settings. Contrast with has_project_settings, which tests the project configuration (project.godot) instead. Returns a boolean result.",
      "Config", {"config", "editor", "settings", "has"}, SideEffect::None, tool_flags::kNone,
      kHasEditorSettingsParams, config_ops::handle_editor_settings_has}));
  return v;
}

} // namespace config_tools
} // namespace godot_autopilot

#endif