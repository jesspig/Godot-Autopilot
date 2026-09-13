#include "runtime_ops.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/authorization.hpp"
#include "tools/debugger_access.hpp"
#include "tools/tool_base.hpp"
#include "util/error_util.hpp"
#include <string>
#include <vector>
#ifdef GetObject
#undef GetObject
#endif

namespace godot_autopilot {
namespace runtime_ops {

using godot_autopilot::util::error_json;

namespace {

using JV = mcp::JsonValue;

int64_t extract_timeout(const JV &args) {
  int64_t timeout = GDA_DEFAULT_TIMEOUT_MS;
  if (auto *tp = args.Find("timeout_ms")) {
    if (tp->IsInt() && tp->GetInt() > 0)
      timeout = tp->GetInt();
  }
  if (timeout > GDA_MAX_TIMEOUT_MS)
    timeout = GDA_MAX_TIMEOUT_MS;
  return timeout;
}

void copy_optional(const JV &from, JV &to, const char *key) {
  if (auto *v = from.Find(key))
    to[key] = *v;
}

constexpr const char *INPUT_PARAM_WHITELIST[] = {
    "type",   "keycode",     "pressed", "button_index", "position",
    "action", "duration_ms", "mode",    "timeout_ms",
};

bool has_only_fields(const JV &value, const char *const *allowed,
                     size_t count, std::string &unknown) {
  if (!value.IsObject())
    return false;
  for (const auto &entry : value.GetObject()) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (entry.first == allowed[i]) {
        found = true;
        break;
      }
    }
    if (!found) {
      unknown = entry.first;
      return false;
    }
  }
  return true;
}

bool is_eval_param_allowed(const std::string &key) {
  static constexpr const char *allowed[] = {
      "action", "node_path", "property", "value", "method", "args",
      "source_code", "persist", "persist_name", "timeout_ms"};
  for (const char *candidate : allowed)
    if (key == candidate)
      return true;
  return false;
}

constexpr size_t SEQUENCE_MAX_ITEMS = 256;
constexpr int64_t SEQUENCE_FRAME_BUDGET_MS = 33;
constexpr int64_t SEQUENCE_BASE_TIMEOUT_MS = 2000;

constexpr const char *SEQUENCE_ITEM_KINDS[] = {
    "key", "mouse_button", "mouse_motion", "action",
};

bool is_sequence_item_kind(const std::string &kind) {
  for (const char *allowed : SEQUENCE_ITEM_KINDS) {
    if (kind == allowed)
      return true;
  }
  return false;
}

constexpr const char *RUNTIME_DEGRADATION_HINT =
    "runtime channel unavailable: the game process did not answer. In the "
    "meantime you can read the game log from disk (get_game_log_entries), "
    "capture the whole desktop (capture_display_screen) or the editor "
    "viewport (capture_editor_viewport); inspect get_plugin_log for the "
    "plugin-side timeout diagnostics.";

void append_runtime_degradation_hint(JV &result) {
  if (!result.IsObject() || !result.Contains("error"))
    return;
  result["hint"] = JV(RUNTIME_DEGRADATION_HINT);
}

} // namespace

mcp::JsonValue handle_game_status(const mcp::JsonValue &args) {
  if (!args.IsObject() || !args.Empty())
    return error_json("get_game_status accepts no parameters");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_status called");
  JV params(JV::object_tag);
  return handle_gda_send("status", params, extract_timeout(args));
}

mcp::JsonValue handle_game_eval(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "execute_game_script", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "execute_game_script called");
  if (!args.IsObject())
    return error_json("execute_game_script parameters must be an object");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action "
                      "(script|get_property|set_property|call_method)");
  }
  const std::string action = action_p->GetString();
  if (action != "script" && action != "get_property" &&
      action != "set_property" && action != "call_method")
    return error_json("invalid action: expected script|get_property|set_property|call_method");
  if (action == "script") {
    auto *source = args.Find("source_code");
    if (!source || !source->IsString() || source->GetString().empty())
      return error_json("script action requires non-empty source_code (string)");
  } else if (action == "get_property" || action == "set_property") {
    auto *property = args.Find("property");
    if (!property || !property->IsString() || property->GetString().empty())
      return error_json("property action requires non-empty property (string)");
  } else {
    auto *method = args.Find("method");
    if (!method || !method->IsString() || method->GetString().empty())
      return error_json("call_method action requires non-empty method (string)");
  }
  if (auto *path = args.Find("node_path"); path && !path->IsString())
    return error_json("node_path must be a string");
  if (auto *call_args = args.Find("args"); call_args && !call_args->IsArray())
    return error_json("args must be an array");
  if (auto *persist = args.Find("persist"); persist && !persist->IsBool())
    return error_json("persist must be a boolean");
  if (auto *timeout = args.Find("timeout_ms");
      timeout && (!timeout->IsInt() || timeout->GetInt() <= 0 ||
                  timeout->GetInt() > GDA_MAX_TIMEOUT_MS))
    return error_json("timeout_ms must be an integer between 1 and 30000");
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (!is_eval_param_allowed(entry.first))
        return error_json("unknown parameter for execute_game_script: " +
                          entry.first);
    }
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  copy_optional(args, params, "node_path");
  copy_optional(args, params, "property");
  copy_optional(args, params, "value");
  copy_optional(args, params, "method");
  copy_optional(args, params, "args");
  copy_optional(args, params, "source_code");
  copy_optional(args, params, "persist");
  copy_optional(args, params, "persist_name");
  copy_optional(args, params, "timeout_ms");
  return handle_gda_send("eval", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "queue_game_input", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "queue_game_input called");
  if (!args.IsObject())
    return error_json("queue_game_input parameters must be an object");
  std::string unknown;
  if (!has_only_fields(args, INPUT_PARAM_WHITELIST,
                       sizeof(INPUT_PARAM_WHITELIST) / sizeof(*INPUT_PARAM_WHITELIST),
                       unknown))
    return error_json("unknown parameter for queue_game_input: " + unknown);
  auto *type_p = args.Find("type");
  if (!type_p || !type_p->IsString()) {
    return error_json(
        "missing required parameter: type (key|mouse_button|action)");
  }
  if (type_p->GetString() != "key" && type_p->GetString() != "mouse_button" &&
      type_p->GetString() != "action")
    return error_json("type must be key|mouse_button|action");
  if (auto *pressed = args.Find("pressed"); pressed && !pressed->IsBool())
    return error_json("pressed must be a boolean");
  if (auto *duration = args.Find("duration_ms"); duration &&
      (!duration->IsInt() || duration->GetInt() < 0))
    return error_json("duration_ms must be a non-negative integer");
  if (auto *mode = args.Find("mode"); mode &&
      (!mode->IsString() || (mode->GetString() != "event" &&
                             mode->GetString() != "api" && mode->GetString() != "hold")))
    return error_json("mode must be event|api|hold");
  if (auto *position = args.Find("position"); position && !position->IsObject())
    return error_json("position must be an object");
  if (auto *timeout = args.Find("timeout_ms"); timeout &&
      (!timeout->IsInt() || timeout->GetInt() <= 0 ||
       timeout->GetInt() > GDA_MAX_TIMEOUT_MS))
    return error_json("timeout_ms must be an integer between 1 and 30000");
  JV params(JV::object_tag);
  params["type"] = *type_p;
  copy_optional(args, params, "keycode");
  copy_optional(args, params, "pressed");
  copy_optional(args, params, "button_index");
  copy_optional(args, params, "position");
  copy_optional(args, params, "action");
  copy_optional(args, params, "duration_ms");
  copy_optional(args, params, "mode");

  JV result = handle_gda_send("input", params, extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_input_wait(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "wait_game_input", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "wait_game_input called");
  static constexpr const char *allowed[] = {"action", "state", "inject",
                                             "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for wait_game_input: " + unknown);
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  if (auto *state = args.Find("state"); state && !state->IsString())
    return error_json("state must be a string");
  if (auto *inject = args.Find("inject"); inject && !inject->IsObject())
    return error_json("inject must be an object");
  if (auto *timeout = args.Find("timeout_ms"); timeout &&
      (!timeout->IsInt() || timeout->GetInt() <= 0 ||
       timeout->GetInt() > GDA_MAX_TIMEOUT_MS))
    return error_json("timeout_ms must be an integer between 1 and 30000");
  JV params(JV::object_tag);
  params["action"] = *action_p;
  copy_optional(args, params, "state");
  copy_optional(args, params, "inject");
  copy_optional(args, params, "timeout_ms");
  JV result = handle_gda_send("input_wait", params, extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_input_status called");
  if (!args.IsObject())
    return error_json("get_game_input_status parameters must be an object");
  static constexpr const char *allowed[] = {"action", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for get_game_input_status: " + unknown);
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  JV result = handle_gda_send("input_status", params, extract_timeout(args));
  append_runtime_degradation_hint(result);
  if (!result.Contains("error")) {
    std::string recent_errors = debugger_ops::capture_get_errors_text(5);
    if (!recent_errors.empty()) {
      result["recent_engine_errors"] = JV(recent_errors);
    }
  }
  return result;
}

mcp::JsonValue handle_sequence_game_inputs(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "sequence_game_inputs", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "sequence_game_inputs called");
  static constexpr const char *allowed[] = {"inputs", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for sequence_game_inputs: " + unknown);
  auto *inputs_p = args.Find("inputs");
  if (!inputs_p || !inputs_p->IsArray() || inputs_p->GetArray().empty()) {
    return error_json(
        "missing required parameter: inputs (non-empty array of {kind, "
        "at_frame, ...} input items)");
  }
  const auto &items = inputs_p->GetArray();
  if (items.size() > SEQUENCE_MAX_ITEMS) {
    return error_json("inputs exceeds maximum of " +
                      std::to_string(SEQUENCE_MAX_ITEMS) + " items");
  }
  int64_t max_at_frame = 0;
  for (const auto &item : items) {
    if (!item.IsObject()) {
      return error_json(
          "each inputs item must be an object with kind and at_frame");
    }
    auto *kind_p = item.Find("kind");
    if (!kind_p || !kind_p->IsString() ||
        !is_sequence_item_kind(kind_p->GetString())) {
      return error_json("each inputs item requires kind "
                        "(key|mouse_button|mouse_motion|action)");
    }
    auto *frame_p = item.Find("at_frame");
    if (!frame_p || !frame_p->IsInt() || frame_p->GetInt() < 0) {
      return error_json("each inputs item requires at_frame (non-negative "
                        "integer physics frame offset)");
    }
    if (frame_p->GetInt() > max_at_frame)
      max_at_frame = frame_p->GetInt();
  }

  int64_t timeout;
  if (auto *tp = args.Find("timeout_ms");
      tp && tp->IsInt() && tp->GetInt() > 0) {
    timeout = tp->GetInt();
  } else {
    timeout =
        max_at_frame * SEQUENCE_FRAME_BUDGET_MS + SEQUENCE_BASE_TIMEOUT_MS;
  }
  if (timeout > GDA_MAX_TIMEOUT_MS)
    timeout = GDA_MAX_TIMEOUT_MS;

  JV params(JV::object_tag);
  params["inputs"] = *inputs_p;
  params["timeout_ms"] = JV(timeout);

  JV result =
      handle_gda_send(std::string(GDA_OP_INPUT_SEQUENCE), params, timeout);
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_ui_elements(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_ui_elements called");
  static constexpr const char *allowed[] = {"max_elements", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for get_game_ui_elements: " + unknown);
  if (auto *max = args.Find("max_elements"); max &&
      (!max->IsInt() || max->GetInt() <= 0))
    return error_json("max_elements must be a positive integer");
  JV params(JV::object_tag);
  copy_optional(args, params, "max_elements");
  return handle_gda_send(std::string(GDA_OP_UI_ELEMENTS), params,
                         extract_timeout(args));
}

mcp::JsonValue handle_game_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_game_viewport called");
  if (!args.IsObject())
    return error_json("capture_game_viewport parameters must be an object");
  static constexpr const char *allowed[] = {"timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for capture_game_viewport: " + unknown);
  JV params(JV::object_tag);
  JV result = handle_gda_send("capture", params, extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_reload_scripts(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "reload_game_scripts", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_game_scripts called");
  static constexpr const char *allowed[] = {"paths"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for reload_game_scripts: " + unknown);
  auto *paths_p = args.Find("paths");
  if (!paths_p || !paths_p->IsArray() || paths_p->GetArray().empty()) {
    return error_json(
        "reload_game_scripts requires paths: non-empty array of res:// script paths");
  }
  std::vector<std::string> paths_vec;
  for (const auto &item : paths_p->GetArray()) {
    if (!item.IsString() || item.GetString().empty()) {
      return error_json(
          "reload_game_scripts requires paths: non-empty array of res:// script paths");
    }
    paths_vec.push_back(item.GetString());
  }
  int32_t sent = debugger_broadcast_reload_scripts(paths_vec);
  if (sent <= 0) {
    return error_json(
        "game not ready: no active debug session — start the game from the editor first");
  }
  JV paths_arr(JV::array_tag);
  for (const auto &path : paths_vec)
    paths_arr.PushBack(JV(path));
  JV inner(JV::object_tag);
  inner["sent_sessions"] = JV(static_cast<int64_t>(sent));
  inner["paths"] = std::move(paths_arr);
  JV result(JV::object_tag);
  result["result"] = std::move(inner);
  result["note"] = JV(
      "soft reload is applied by the game on its next idle poll; no confirmation is returned — verify with get_game_log_entries");
  return result;
}

} // namespace runtime_ops
} // namespace godot_autopilot
