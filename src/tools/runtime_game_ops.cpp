#include "runtime_ops.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/debugger_access.hpp"
#include "util/error_util.hpp"
#include <string>
#include <vector>

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

bool is_input_param_allowed(const std::string &key) {
  for (const char *allowed : INPUT_PARAM_WHITELIST) {
    if (key == allowed)
      return true;
  }
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

} // namespace

mcp::JsonValue handle_game_status(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_status called");
  JV params(JV::object_tag);
  return handle_gda_send("status", params, extract_timeout(args));
}

mcp::JsonValue handle_game_eval(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "execute_game_script called");
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
  return handle_gda_send("eval", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "queue_game_input called");
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

  JV result = handle_gda_send("input", params, extract_timeout(args));
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
                            "wait_game_input called");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  copy_optional(args, params, "state");
  copy_optional(args, params, "inject");
  copy_optional(args, params, "timeout_ms");
  return handle_gda_send("input_wait", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_input_status called");
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    return error_json("missing required parameter: action");
  }
  JV params(JV::object_tag);
  params["action"] = *action_p;
  JV result = handle_gda_send("input_status", params, extract_timeout(args));
  if (!result.Contains("error")) {
    std::string recent_errors = debugger_ops::capture_get_errors_text(5);
    if (!recent_errors.empty()) {
      result["recent_engine_errors"] = JV(recent_errors);
    }
  }
  return result;
}

mcp::JsonValue handle_sequence_game_inputs(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "sequence_game_inputs called");
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

  return handle_gda_send(std::string(GDA_OP_INPUT_SEQUENCE), params, timeout);
}

mcp::JsonValue handle_game_ui_elements(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_ui_elements called");
  JV params(JV::object_tag);
  copy_optional(args, params, "max_elements");
  return handle_gda_send(std::string(GDA_OP_UI_ELEMENTS), params,
                         extract_timeout(args));
}

mcp::JsonValue handle_game_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_game_viewport called");
  JV params(JV::object_tag);
  return handle_gda_send("capture", params, extract_timeout(args));
}

mcp::JsonValue handle_game_reload_scripts(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_game_scripts called");
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
