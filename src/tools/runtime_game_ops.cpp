#include "runtime_ops.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "tools/debugger_access.hpp"
#include "util/error_util.hpp"
#include <string>
#include <vector>

namespace godot_self_driving {
namespace runtime_ops {

using godot_self_driving::util::error_json;

namespace {

using JV = mcp::JsonValue;

int64_t extract_timeout(const JV &args) {
  int64_t timeout = GSD_DEFAULT_TIMEOUT_MS;
  if (auto *tp = args.Find("timeout_ms")) {
    if (tp->IsInt() && tp->GetInt() > 0)
      timeout = tp->GetInt();
  }
  if (timeout > GSD_MAX_TIMEOUT_MS)
    timeout = GSD_MAX_TIMEOUT_MS;
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

bool is_input_param_allowed(const std::string &key) {
  for (const char *allowed : INPUT_PARAM_WHITELIST) {
    if (key == allowed)
      return true;
  }
  return false;
}

} // namespace

mcp::JsonValue handle_game_status(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_status called");
  JV params(JV::object_tag);
  return handle_gsd_send("status", params, extract_timeout(args));
}

mcp::JsonValue handle_game_eval(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_eval called");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action "
                      "(script|get_property|set_property|call_method)");
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
  return handle_gsd_send("eval", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_input called");
  auto *type_p = args.Find("type");
  if (!type_p || !type_p->IsString()) {
    return error_json(
        "missing required parameter: type (key|mouse_button|action)");
  }
  JV params(JV::object_tag);
  params["type"] = *type_p;
  copy_optional(args, params, "keycode");
  copy_optional(args, params, "pressed");
  copy_optional(args, params, "button_index");
  copy_optional(args, params, "position");
  copy_optional(args, params, "action");
  copy_optional(args, params, "duration_ms");
  copy_optional(args, params, "mode");

  std::vector<std::string> ignored;
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (!is_input_param_allowed(entry.first)) {
        ignored.push_back(entry.first);
      }
    }
  }

  JV result = handle_gsd_send("input", params, extract_timeout(args));
  if (!result.Contains("error") && !ignored.empty()) {
    JV ignored_arr(JV::array_tag);
    for (const auto &key : ignored)
      ignored_arr.PushBack(JV(key));
    result["ignored_params"] = std::move(ignored_arr);
    std::string warning = "ignored unknown parameters: ";
    for (size_t i = 0; i < ignored.size(); i++) {
      if (i > 0)
        warning += ", ";
      warning += ignored[i];
    }
    result["warning"] = JV(warning);
  }
  return result;
}

mcp::JsonValue handle_game_input_wait(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_input_wait called");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  copy_optional(args, params, "state");
  copy_optional(args, params, "inject");
  copy_optional(args, params, "timeout_ms");
  return handle_gsd_send("input_wait", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_input_status called");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  JV result = handle_gsd_send("input_status", params, extract_timeout(args));
  if (!result.Contains("error")) {
    std::string recent_errors = debugger_ops::capture_get_errors_text(5);
    if (!recent_errors.empty()) {
      result["recent_engine_errors"] = JV(recent_errors);
    }
  }
  return result;
}

mcp::JsonValue handle_game_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "game_capture called");
  JV params(JV::object_tag);
  return handle_gsd_send("capture", params, extract_timeout(args));
}

} // namespace runtime_ops
} // namespace godot_self_driving
