#ifndef GODOT_AUTOPILOT_CONFIG_TOOLS_HPP
#define GODOT_AUTOPILOT_CONFIG_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/config_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace config_tools {

GDA_TOOL_CLASS(GetProjectSettingsTool, "get_project_settings",
               "Read a project setting by name from the project configuration (project.godot). Use it to query project-wide values such as display, physics or input defaults. The optional default is returned when the setting does not exist. Returns the serialized value; contrast with get_editor_settings, which reads editor preferences.",
               "Config", std::vector<std::string>({"config", "project", "settings", "get"}), config_ops::handle_project_settings_get, true)

GDA_TOOL_CLASS(SetProjectSettingsTool, "set_project_settings",
               "Set a project setting in memory. Note: the change is NOT written to project.godot automatically, so call save_project_settings afterwards or the setting is lost when the editor closes. value is a serialized JSON value; the engine keeps the existing type when known. Differs from set_editor_settings, which persists automatically. Returns ok.",
               "Config", std::vector<std::string>({"config", "project", "settings", "set"}), config_ops::handle_project_settings_set, true)

GDA_TOOL_CLASS(HasProjectSettingsTool, "has_project_settings",
               "Check whether a project setting exists by name in the project configuration (project.godot). Use it before reading with get_project_settings to distinguish a missing setting from a stored default value. Contrast with has_editor_settings, which tests editor preferences instead. Returns a boolean result.",
               "Config", std::vector<std::string>({"config", "project", "settings", "has"}), config_ops::handle_project_settings_has, true)

GDA_TOOL_CLASS_SIDE(SaveProjectSettingsTool, "save_project_settings",
               "Persist all project settings to project.godot. Call it after set_project_settings so in-memory changes survive an editor restart; without it the changes are discarded on exit. This is the required flush step, no other config tool writes project.godot. Returns ok, or an error with the code when saving fails.",
               "Config", std::vector<std::string>({"config", "project", "settings", "save"}), config_ops::handle_project_settings_save, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(GetEngineVersionTool, "get_engine_version",
               "Read the version of the running Godot engine, useful for branching behavior or reporting the environment. Returns an object with major, minor and patch (integers) plus string, the full version label such as '4.7.1-stable'. Takes no parameters; all four fields are always present.",
               "Config", std::vector<std::string>({"config", "engine", "version"}), config_ops::handle_engine_get_version, true)

GDA_TOOL_CLASS(GetEngineFpsTool, "get_engine_fps",
               "Measure the current real-time frames per second rendered by the engine, e.g. to verify performance after scene changes. Unlike set_engine_max_fps (a cap on the frame rate) and get_engine_frames_drawn (a cumulative counter), this reports the live measured rate. Returns a float that fluctuates per frame.",
               "Config", std::vector<std::string>({"config", "engine", "fps", "get"}), config_ops::handle_engine_get_fps, true)

GDA_TOOL_CLASS(GetEngineFramesDrawnTool, "get_engine_frames_drawn",
               "Read the total number of frames drawn since the engine started, a monotonically increasing counter that resets only on restart. Use it to measure elapsed frames between two points or to detect stalls, complementing get_engine_fps (the live rate). Returns an integer.",
               "Config", std::vector<std::string>({"config", "engine", "frames"}), config_ops::handle_engine_get_frames_drawn, true)

GDA_TOOL_CLASS(SetEngineTimeScaleTool, "set_engine_time_scale",
               "Set the global time scale multiplier for the game loop: 1.0 is real time, 0.5 halves the speed, 2.0 doubles it. The scale affects both process and physics timing; values below 1.0 create slow-motion effects, and 0.0 stops gameplay processing. Returns ok; read the current value with get_engine_time_scale.",
               "Config", std::vector<std::string>({"config", "engine", "time", "scale", "set"}), config_ops::handle_engine_set_time_scale, true)

GDA_TOOL_CLASS(GetEngineTimeScaleTool, "get_engine_time_scale",
               "Read the current global time scale previously set by set_engine_time_scale, defaulting to 1.0 (real time). Pair it with set_engine_time_scale when debugging slow-motion or sped-up gameplay, or to confirm a scale change was applied. Returns a float representing the multiplier applied to the game loop.",
               "Config", std::vector<std::string>({"config", "engine", "time", "scale", "get"}), config_ops::handle_engine_get_time_scale, true)

GDA_TOOL_CLASS(SetEngineMaxFpsTool, "set_engine_max_fps",
               "Cap the engine's frame rate at fps frames per second; 0 disables the cap entirely and lets the engine run uncapped. This sets an upper limit only, it does not measure the actual rate, use get_engine_fps for that. Returns ok.",
               "Config", std::vector<std::string>({"config", "engine", "fps", "max"}), config_ops::handle_engine_set_max_fps, true)

GDA_TOOL_CLASS(GetEditorSettingsTool, "get_editor_settings",
               "Read an editor preference by name from the editor's own settings store, which persists automatically across sessions. Use it for editor UI configuration such as theme, docks or input preferences. Differs from get_project_settings, which reads project.godot values. Returns the serialized value.",
               "Config", std::vector<std::string>({"config", "editor", "settings", "get"}), config_ops::handle_editor_settings_get, true)

GDA_TOOL_CLASS_SIDE(SetEditorSettingsTool, "set_editor_settings",
               "Set an editor preference by name, e.g. interface or plugin options. Use it to tweak editor behavior without touching project.godot. Unlike set_project_settings, editor settings persist automatically, no explicit save step is required, and the value survives editor restarts. Returns ok; verify the applied value with get_editor_settings.",
               "Config", std::vector<std::string>({"config", "editor", "settings", "set"}), config_ops::handle_editor_settings_set, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(HasEditorSettingsTool, "has_editor_settings",
               "Check whether an editor preference exists by name in the editor's settings store. Use it to probe whether a preference is customized before reading it with get_editor_settings. Contrast with has_project_settings, which tests the project configuration (project.godot) instead. Returns a boolean result.",
               "Config", std::vector<std::string>({"config", "editor", "settings", "has"}), config_ops::handle_editor_settings_has, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(13);
  v.push_back(std::make_unique<GetProjectSettingsTool>());
  v.push_back(std::make_unique<SetProjectSettingsTool>());
  v.push_back(std::make_unique<HasProjectSettingsTool>());
  v.push_back(std::make_unique<SaveProjectSettingsTool>());
  v.push_back(std::make_unique<GetEngineVersionTool>());
  v.push_back(std::make_unique<GetEngineFpsTool>());
  v.push_back(std::make_unique<GetEngineFramesDrawnTool>());
  v.push_back(std::make_unique<SetEngineTimeScaleTool>());
  v.push_back(std::make_unique<GetEngineTimeScaleTool>());
  v.push_back(std::make_unique<SetEngineMaxFpsTool>());
  v.push_back(std::make_unique<GetEditorSettingsTool>());
  v.push_back(std::make_unique<SetEditorSettingsTool>());
  v.push_back(std::make_unique<HasEditorSettingsTool>());
  return v;
}

} // namespace config_tools
} // namespace godot_autopilot

#endif