#ifndef GODOT_AUTOPILOT_CONFIG_TOOLS_HPP
#define GODOT_AUTOPILOT_CONFIG_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/config/config_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace config_tools {

namespace {

const std::vector<ParamSpec> kGetProjectSettingsParams = {
    {"name", "string", "Project setting name, e.g. display/window/size/viewport_width; settings come from project.godot", true},
    {"default", "object", "Serialized JSON value returned when the setting does not exist; otherwise a null value is returned", false},
};

const std::vector<ParamSpec> kUpdateProjectSettingsParams = {
    {"name", "string", "Project setting name for action='set', e.g. application/config/name; must already exist or be added by the call", false},
    {"value", "json", "Serialized JSON value for action='set'; when a string, the engine keeps the existing setting's type if one is stored, e.g. 1920 for an int setting", false},
};

const std::vector<ParamSpec> kGetEditorSettingsParams = {
    {"name", "string", "Editor setting name, e.g. interface/theme/base_color; settings come from the editor's own preferences. Alongside 'result' (the raw value, unchanged) the response carries the property list metadata: 'hint' (integer PropertyHint) plus 'hint_string' when non-empty; enum settings (hint 2) also include 'enum_options', an array of {'label', 'value'} entries that decodes bare integers such as run/window_placement/game_embed_mode", true},
};

const std::vector<ParamSpec> kUpdateEditorSettingsParams = {
    {"name", "string", "Editor setting name to write, e.g. interface/theme/base_color", true},
    {"value", "json", "Serialized JSON value to store; editor settings persist automatically", true},
};

const std::vector<ParamSpec> kGetEngineStateParams = {};

const std::vector<ParamSpec> kUpdateEngineStateParams = {
    {"scale", "number", "Time scale multiplier for action='time_scale': 1.0 = real time, 0.5 = half speed, 2.0 = double speed; 0.0 stops gameplay processing", false},
    {"fps", "integer", "Frame rate cap for action='max_fps'; 0 disables the cap (uncapped)", false},
};

const std::vector<ParamSpec> kGetAutoloadsParams = {};

const std::vector<ParamSpec> kUpdateAutoloadsParams = {
    {"name", "string", "Autoload singleton name, e.g. Global; must not contain '/' and must not exist for action='add' while it must already be registered for action='remove' and action='move'", false},
    {"path", "string", "res:// scene or script to autoload for action='add', e.g. res://global.gd; must exist, load and be instantiable", false},
    {"enabled", "boolean", "Whether the singleton marker is stored for action='add'; defaults to true, which writes the leading * marker exactly like the editor does", false},
    {"new_index", "integer", "Zero-based target position in the autoload load order for action='move'; must lie between 0 and the list size minus one", false},
};

inline std::vector<std::string> config_actions(
    std::initializer_list<const char *> actions) {
  std::vector<std::string> out;
  out.reserve(actions.size());
  for (const char *action : actions) {
    out.emplace_back(action);
  }
  return out;
}

inline std::vector<std::string> config_examples(
    std::initializer_list<const char *> examples) {
  std::vector<std::string> out;
  out.reserve(examples.size());
  for (const char *example : examples) {
    out.emplace_back(example);
  }
  return out;
}

inline ToolSpec config_action_spec(ToolSpec spec,
                                   std::initializer_list<const char *> actions,
                                   std::initializer_list<const char *> examples) {
  spec.raw_schema = schema::build_action_schema(
      config_actions(actions), spec.params,
      "Action to perform; per-action parameters are documented in the tool "
      "description");
  spec.examples = config_examples(examples);
  return spec;
}

inline ToolSpec config_optional_action_spec(
    ToolSpec spec, std::initializer_list<const char *> actions,
    std::initializer_list<const char *> examples) {
  mcp::JsonValue raw = schema::build_schema(spec.params);
  mcp::JsonValue action(mcp::JsonValue::object_tag);
  action["type"] = mcp::JsonValue("string");
  action["description"] = mcp::JsonValue(
      "Action to perform; defaults to 'get' when omitted");
  mcp::JsonValue values(mcp::JsonValue::array_tag);
  for (const char *value : actions) {
    values.PushBack(mcp::JsonValue(value));
  }
  action["enum"] = std::move(values);
  mcp::JsonValue *properties = raw.Find("properties");
  if (properties != nullptr && properties->IsObject()) {
    (*properties)["action"] = std::move(action);
  }
  spec.raw_schema = std::move(raw);
  spec.examples = config_examples(examples);
  return spec;
}

inline mcp::JsonValue config_args_with_action(const mcp::JsonValue &args,
                                              const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline bool config_result_has_error(const mcp::JsonValue &result) {
  return result.IsObject() &&
         (result.Find("error") != nullptr ||
          result.Find("structured_error") != nullptr ||
          result.Find("runtime_error") != nullptr);
}

inline ToolAlias config_alias(const char *name, const char *canonical,
                              const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return config_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

inline mcp::JsonValue project_config_found(const mcp::JsonValue &result) {
  if (config_result_has_error(result)) {
    return result;
  }
  const mcp::JsonValue *found = result.Find("result");
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = found != nullptr && found->IsBool()
                      ? *found
                      : mcp::JsonValue(false);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(with_aliases(
      config_optional_action_spec(
          ToolSpec{
              "get_project_settings",
              "Read project configuration (project.godot), actions: get (default, serialized value of one setting; the optional default is returned when it does not exist) and has (boolean existence check reported as result, with found; use it to distinguish a missing setting from a stored default value). name is the setting key, e.g. display/window/size/viewport_width. Deprecated alias: has_project_settings.",
              "Config",
              {"config", "project", "settings", "get", "has",
               "has_project_settings"},
              SideEffect::None,
              tool_flags::kNone,
              kGetProjectSettingsParams,
              config_ops::handle_get_project_settings},
          {"get", "has"},
          {"{\"action\":\"get\",\"name\":\"application/config/name\"}",
           "{\"action\":\"has\",\"name\":\"application/config/name\"}"}),
      {
          [](ToolAlias alias) {
            alias.project_result = project_config_found;
            return alias;
          }(config_alias("has_project_settings", "get_project_settings",
                         "has")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "update_project_settings",
              "Change project configuration (project.godot), actions: set (write one setting in memory; value is a serialized JSON value and the engine keeps the existing type when known) and save (persist all in-memory project settings to project.godot; required after set or the change is lost when the editor closes). Returns ok; saving failures carry the engine error code. Deprecated aliases: set_project_settings, save_project_settings.",
              "Config",
              {"config", "project", "settings", "set", "save",
               "set_project_settings", "save_project_settings"},
              SideEffect::WritesConfig,
              tool_flags::kMutating,
              kUpdateProjectSettingsParams,
              config_ops::handle_update_project_settings},
          {"set", "save"},
          {"{\"action\":\"set\",\"name\":\"application/config/name\",\"value\":\"Demo\"}",
           "{\"action\":\"save\"}"}),
      {
          config_alias("set_project_settings", "update_project_settings",
                       "set"),
          config_alias("save_project_settings", "update_project_settings",
                       "save"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_optional_action_spec(
          ToolSpec{
              "get_editor_settings",
              "Read editor preferences (the editor's own settings store, persisted across sessions), actions: get (default, serialized value of one preference; the response also carries hint, hint_string and enum_options when available) and has (boolean existence check reported as result, with found). name is the preference key, e.g. interface/theme/base_color. Deprecated alias: has_editor_settings.",
              "Config",
              {"config", "editor", "settings", "get", "has",
               "has_editor_settings"},
              SideEffect::None,
              tool_flags::kNone,
              kGetEditorSettingsParams,
              config_ops::handle_get_editor_settings},
          {"get", "has"},
          {"{\"action\":\"get\",\"name\":\"interface/theme/base_color\"}",
           "{\"action\":\"has\",\"name\":\"interface/theme/base_color\"}"}),
      {
          [](ToolAlias alias) {
            alias.project_result = project_config_found;
            return alias;
          }(config_alias("has_editor_settings", "get_editor_settings",
                         "has")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "update_editor_settings",
              "Change an editor preference by name, e.g. interface or plugin options, actions: set (store the serialized value; editor settings persist automatically, no explicit save step is required, and the value survives editor restarts). Returns ok; verify the applied value with get_editor_settings action='get'. Deprecated alias: set_editor_settings.",
              "Config",
              {"config", "editor", "settings", "set",
               "set_editor_settings"},
              SideEffect::WritesConfig,
              tool_flags::kMutating,
              kUpdateEditorSettingsParams,
              config_ops::handle_update_editor_settings},
          {"set"},
          {"{\"action\":\"set\",\"name\":\"interface/theme/base_color\",\"value\":\"#202020\"}"}),
      {
          config_alias("set_editor_settings", "update_editor_settings",
                       "set"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "get_engine_state",
              "Read live engine state, actions: version (major, minor, patch and the full string such as '4.7.1-stable'), fps (measured real-time frame rate), frames_drawn (cumulative frames since startup, resets only on restart), time_scale (current global time scale, default 1.0). All actions return their value in result. Deprecated aliases: get_engine_version, get_engine_fps, get_engine_frames_drawn, get_engine_time_scale.",
              "Config",
              {"config", "engine", "version", "fps", "frames", "time_scale",
               "get_engine_version", "get_engine_fps",
               "get_engine_frames_drawn", "get_engine_time_scale"},
              SideEffect::None,
              tool_flags::kNone,
              kGetEngineStateParams,
              config_ops::handle_get_engine_state},
          {"version", "fps", "frames_drawn", "time_scale"},
          {"{\"action\":\"version\"}",
           "{\"action\":\"fps\"}",
           "{\"action\":\"frames_drawn\"}",
           "{\"action\":\"time_scale\"}"}),
      {
          config_alias("get_engine_version", "get_engine_state", "version"),
          config_alias("get_engine_fps", "get_engine_state", "fps"),
          config_alias("get_engine_frames_drawn", "get_engine_state",
                       "frames_drawn"),
          config_alias("get_engine_time_scale", "get_engine_state",
                       "time_scale"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "update_engine_state",
              "Change live engine state, actions: time_scale (global multiplier for the game loop: 1.0 real time, 0.5 half speed, 2.0 double speed, 0.0 stops gameplay; affects process and physics timing) and max_fps (frame rate cap; 0 disables the cap and lets the engine run uncapped). Returns ok; read the applied value with get_engine_state. Deprecated aliases: set_engine_time_scale, set_engine_max_fps.",
              "Config",
              {"config", "engine", "time_scale", "max_fps",
               "set_engine_time_scale", "set_engine_max_fps"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating,
              kUpdateEngineStateParams,
              config_ops::handle_update_engine_state},
          {"time_scale", "max_fps"},
          {"{\"action\":\"time_scale\",\"scale\":0.5}",
           "{\"action\":\"max_fps\",\"fps\":60}"}),
      {
          config_alias("set_engine_time_scale", "update_engine_state",
                       "time_scale"),
          config_alias("set_engine_max_fps", "update_engine_state",
                       "max_fps"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "get_autoloads",
              "List all autoload singletons registered in project.godot in load order, actions: list (each entry carries name, path with the leading * singleton marker stripped, enabled and order; the response also reports count). Read-only; change the list with update_autoloads. Deprecated alias: get_autoload_list.",
              "Config",
              {"config", "autoload", "list", "get_autoload_list"},
              SideEffect::None,
              tool_flags::kNone,
              kGetAutoloadsParams,
              config_ops::handle_get_autoloads},
          {"list"},
          {"{\"action\":\"list\"}"}),
      {
          config_alias("get_autoload_list", "get_autoloads", "list"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      config_action_spec(
          ToolSpec{
              "update_autoloads",
              "Change the project autoload singletons in project.godot and save immediately, actions: add (name must be unique and must not contain '/', path must be a res:// scene or script that loads and can be instantiated, enabled defaults to true and controls the leading * marker), remove (clears the autoload/Name setting and reports removed, updated_files, stale_references {file, token} and scan_truncated when the reference scan was cut) and move (new_index is zero-based within the current list; every entry is renumbered 0..n-1). Returns ok plus the action-specific fields. Deprecated aliases: add_autoload, remove_autoload, move_autoload.",
              "Config",
              {"config", "autoload", "add", "remove", "move", "add_autoload",
               "remove_autoload", "move_autoload"},
              SideEffect::WritesConfig,
              tool_flags::kMutating,
              kUpdateAutoloadsParams,
              config_ops::handle_update_autoloads},
          {"add", "remove", "move"},
          {"{\"action\":\"add\",\"name\":\"Global\",\"path\":\"res://global.gd\"}",
           "{\"action\":\"remove\",\"name\":\"Global\"}",
           "{\"action\":\"move\",\"name\":\"Global\",\"new_index\":0}"}),
      {
          config_alias("add_autoload", "update_autoloads", "add"),
          config_alias("remove_autoload", "update_autoloads", "remove"),
          config_alias("move_autoload", "update_autoloads", "move"),
      })));
  return v;
}

} // namespace config_tools
} // namespace godot_autopilot

#endif
