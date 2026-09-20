#include "runtime_ops.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/authorization.hpp"
#include "tools/debugger_access.hpp"
#include "tools/tool_base.hpp"
#include "util/error_util.hpp"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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
  if (timeout > GDA_MAX_GAME_OP_TIMEOUT_MS)
    timeout = GDA_MAX_GAME_OP_TIMEOUT_MS;
  return timeout;
}

std::string game_op_timeout_error() {
  return "timeout_ms must be an integer between 1 and " +
         std::to_string(GDA_MAX_GAME_OP_TIMEOUT_MS);
}

void copy_optional(const JV &from, JV &to, const char *key) {
  if (auto *v = from.Find(key))
    to[key] = *v;
}

constexpr const char *INPUT_PARAM_WHITELIST[] = {
    "type",      "keycode",    "pressed",   "button_index", "position",
    "action",    "duration_ms", "mode",     "timeout_ms",   "direction",
    "amount",    "relative",
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
      "source_code", "persist", "persist_name", "timeout_ms", "assert"};
  for (const char *candidate : allowed)
    if (key == candidate)
      return true;
  return false;
}

constexpr int64_t EVAL_ASSERT_MAX_CHARS = 1024;

constexpr size_t SEQUENCE_MAX_ITEMS = 256;
constexpr int64_t SEQUENCE_FRAME_BUDGET_MS = 33;
constexpr int64_t SEQUENCE_BASE_TIMEOUT_MS = 2000;

constexpr const char *SEQUENCE_ITEM_KINDS[] = {
    "key", "mouse_button", "mouse_motion", "wheel", "action",
};

constexpr int64_t CLICK_MAX_ELEMENTS_LIMIT = 1000;

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

int64_t steady_game_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void expire_stale_game_jobs(int64_t now_ms) {
  std::vector<std::pair<int64_t, int64_t>> stale_jobs;
  {
    std::lock_guard<std::mutex> lock(game_jobs_mutex());
    GameJobTable &table = game_jobs();
    for (auto it = table.jobs.begin(); it != table.jobs.end();) {
      if ((now_ms - it->second.started_ms) >
          (it->second.timeout_ms + GameJobTable::kExpiryGraceMs)) {
        stale_jobs.emplace_back(it->second.job_id, it->second.request_id);
        it = table.jobs.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (const auto &stale : stale_jobs) {
    std::string stale_detail;
    discard_pending(stale.second, stale_detail);
    LogSystem::instance().log_detailed(
        LogLevel::Warning, LogCategory::Tools,
        "game job " + std::to_string(stale.first) +
            " expired during stale cleanup",
        "job_id=" + std::to_string(stale.first) +
            " request_id=" + std::to_string(stale.second) +
            " reason=expired");
  }
}

int64_t erase_game_job(int64_t job_id) {
  std::lock_guard<std::mutex> lock(game_jobs_mutex());
  auto &jobs = game_jobs().jobs;
  auto it = jobs.find(job_id);
  if (it == jobs.end())
    return 0;
  int64_t request_id = it->second.request_id;
  jobs.erase(it);
  return request_id;
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
                  timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  bool has_assert = false;
  if (auto *assert_p = args.Find("assert")) {
    if (!assert_p->IsString() || assert_p->GetString().empty())
      return error_json("assert must be a non-empty expression string with the "
                        "eval result bound as `value` (e.g. \"value > 0\")");
    if (assert_p->GetString().size() >
        static_cast<size_t>(EVAL_ASSERT_MAX_CHARS))
      return error_json("assert exceeds 1024 characters");
    has_assert = true;
  }
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
  copy_optional(args, params, "assert");
  const std::string op =
      has_assert ? std::string(GDA_OP_EVAL_ASSERT) : std::string(GDA_OP_EVAL);
  return handle_gda_send(op, params, extract_timeout(args),
                         needs_error_break_suppression(std::string(GDA_OP_EVAL),
                                                       action));
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
    return error_json("missing required parameter: type "
                      "(key|mouse_button|wheel|mouse_motion|action)");
  }
  const std::string type = type_p->GetString();
  if (type != "key" && type != "mouse_button" && type != "wheel" &&
      type != "mouse_motion" && type != "action")
    return error_json("type must be key|mouse_button|wheel|mouse_motion|action");
  if (auto *direction = args.Find("direction"); direction && !direction->IsString())
    return error_json("direction must be a string (up|down|left|right)");
  if (auto *amount = args.Find("amount"); amount &&
      (!amount->IsInt() || amount->GetInt() < 1 || amount->GetInt() > 10))
    return error_json("amount must be an integer between 1 and 10");
  if (auto *relative = args.Find("relative"); relative && !relative->IsObject())
    return error_json("relative must be an object with numeric x and y");
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
       timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  JV params(JV::object_tag);
  params["type"] = JV(type);
  copy_optional(args, params, "keycode");
  copy_optional(args, params, "pressed");
  copy_optional(args, params, "button_index");
  copy_optional(args, params, "position");
  copy_optional(args, params, "action");
  copy_optional(args, params, "duration_ms");
  copy_optional(args, params, "mode");
  copy_optional(args, params, "direction");
  copy_optional(args, params, "amount");
  copy_optional(args, params, "relative");

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
       timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
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
                        "(key|mouse_button|mouse_motion|wheel|action)");
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
  if (timeout > GDA_MAX_GAME_OP_TIMEOUT_MS)
    timeout = GDA_MAX_GAME_OP_TIMEOUT_MS;

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

mcp::JsonValue handle_click_game_ui_element(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "click_game_ui_element", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  if (!args.IsObject())
    return error_json("click_game_ui_element parameters must be an object");
  static constexpr const char *allowed[] = {"path", "text", "text_index", "button_index",
                                             "double_click", "max_elements",
                                             "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for click_game_ui_element: " + unknown + " — annotate id is a per-capture sequence number and is not clickable; use path from the same get_game_ui_elements row (path takes priority) or text/text_index");
  auto *path_p = args.Find("path");
  auto *text_p = args.Find("text");
  const bool has_path =
      path_p && path_p->IsString() && !path_p->GetString().empty();
  const bool has_text =
      text_p && text_p->IsString() && !text_p->GetString().empty();
  if (!has_path && !has_text) {
    return error_json("missing required parameter: path or text (non-empty node path "
                      "from get_game_ui_elements, or visible element text; path takes "
                      "priority when both are given)");
  }
  int64_t text_index = 0;
  const bool has_text_index = args.Find("text_index") != nullptr;
  if (auto *index_p = args.Find("text_index")) {
    if (!index_p->IsInt() || index_p->GetInt() < 0)
      return error_json("text_index must be an integer greater than or equal "
                        "to 0 (selects the nth visible text match)");
    text_index = index_p->GetInt();
    if (!has_text)
      return error_json("text_index requires text (path takes priority when "
                        "both path and text are given)");
  }
  int64_t button_index = 1;
  if (auto *button_p = args.Find("button_index")) {
    if (!button_p->IsInt() || button_p->GetInt() < 1 || button_p->GetInt() > 3)
      return error_json(
          "button_index must be an integer between 1 and 3 (1=left, 2=right, "
          "3=middle)");
    button_index = button_p->GetInt();
  }
  bool double_click = false;
  if (auto *double_p = args.Find("double_click")) {
    if (!double_p->IsBool())
      return error_json("double_click must be a boolean");
    double_click = double_p->GetBool();
  }
  int64_t max_elements = CLICK_MAX_ELEMENTS_LIMIT;
  if (auto *max_p = args.Find("max_elements")) {
    if (!max_p->IsInt() || max_p->GetInt() <= 0)
      return error_json("max_elements must be a positive integer");
    max_elements = max_p->GetInt();
    if (max_elements > CLICK_MAX_ELEMENTS_LIMIT)
      max_elements = CLICK_MAX_ELEMENTS_LIMIT;
  }
  if (auto *timeout = args.Find("timeout_ms"); timeout &&
      (!timeout->IsInt() || timeout->GetInt() <= 0 ||
       timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  const std::string path = has_path ? path_p->GetString() : std::string();
  const std::string text = has_text ? text_p->GetString() : std::string();
  LogSystem::instance().log(
      LogLevel::Debug, LogCategory::Tools,
      "click_game_ui_element dispatch: path=" + path + ", text=" + text + ", button_index=" +
          std::to_string(button_index) +
          ", double_click=" + (double_click ? "true" : "false") +
          ", max_elements=" + std::to_string(max_elements) + ", text_index=" + std::to_string(text_index));

  JV click(JV::object_tag);
  if (has_path)
    click["path"] = JV(path);
  if (has_text) {
    click["text"] = JV(text);
    if (has_text_index)
      click["text_index"] = JV(text_index);
  }
  click["button_index"] = JV(button_index);
  click["double_click"] = JV(double_click);
  JV params(JV::object_tag);
  params["max_elements"] = JV(max_elements);
  params["click"] = std::move(click);
  JV result = handle_gda_send(std::string(GDA_OP_UI_ELEMENTS), params,
                              extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_capture(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_game_viewport called");
  if (!args.IsObject())
    return error_json("capture_game_viewport parameters must be an object");
  static constexpr const char *allowed[] = {
      "timeout_ms", "region",  "max_dimension", "annotate",
      "after_frames", "when",  "scale",         "annotate_nodes",
      "annotate_nodes_max"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed), unknown))
    return error_json("unknown parameter for capture_game_viewport: " + unknown);
  if (auto *region = args.Find("region")) {
    if (!region->IsObject())
      return error_json("region must be an object with numeric x, y, width and "
                        "height");
    const JV *rx = region->Find("x");
    const JV *ry = region->Find("y");
    const JV *rw = region->Find("width");
    const JV *rh = region->Find("height");
    if (!rx || !ry || !rw || !rh || !rx->IsNumber() || !ry->IsNumber() ||
        !rw->IsNumber() || !rh->IsNumber())
      return error_json("region must be an object with numeric x, y, width and "
                        "height (width/height greater than 0)");
    const double width = rw->IsInt() ? static_cast<double>(rw->GetInt())
                                     : rw->GetDouble();
    const double height = rh->IsInt() ? static_cast<double>(rh->GetInt())
                                      : rh->GetDouble();
    if (width <= 0.0 || height <= 0.0)
      return error_json("region must be an object with numeric x, y, width and "
                        "height (width/height greater than 0)");
  }
  if (auto *max_dimension = args.Find("max_dimension");
      max_dimension && (!max_dimension->IsInt() ||
                        max_dimension->GetInt() < 64 ||
                        max_dimension->GetInt() > 4096))
    return error_json("max_dimension must be an integer between 64 and 4096");
  if (auto *annotate = args.Find("annotate"); annotate && !annotate->IsBool())
    return error_json("annotate must be a boolean");
  if (auto *nodes = args.Find("annotate_nodes")) {
    if (!nodes->IsArray() || nodes->GetArray().empty() ||
        nodes->GetArray().size() > 50)
      return error_json("annotate_nodes must be an array of 1-50 strings");
    for (const auto &item : nodes->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return error_json(
            "annotate_nodes must be an array of non-empty strings");
    }
  }
  if (auto *nodes_max = args.Find("annotate_nodes_max");
      nodes_max && (!nodes_max->IsInt() || nodes_max->GetInt() < 1 ||
                    nodes_max->GetInt() > 50))
    return error_json("annotate_nodes_max must be an integer between 1 and 50");
  if (auto *after_frames = args.Find("after_frames");
      after_frames && (!after_frames->IsInt() || after_frames->GetInt() < 0))
    return error_json(
        "after_frames must be an integer greater than or equal to 0");
  if (auto *when = args.Find("when"); when && !when->IsString())
    return error_json("when must be a string");
  if (auto *scale = args.Find("scale");
      scale && (!scale->IsInt() || scale->GetInt() < 1 || scale->GetInt() > 8))
    return error_json("scale must be an integer between 1 and 8");
  JV params(JV::object_tag);
  copy_optional(args, params, "region");
  copy_optional(args, params, "max_dimension");
  copy_optional(args, params, "annotate");
  copy_optional(args, params, "annotate_nodes");
  copy_optional(args, params, "annotate_nodes_max");
  copy_optional(args, params, "after_frames");
  copy_optional(args, params, "when");
  copy_optional(args, params, "scale");
  const int64_t timeout_ms = extract_timeout(args);
  params["timeout_ms"] = JV(timeout_ms);
  JV result = handle_gda_send("capture", params, timeout_ms);
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
      "soft reload is applied by the game on its next idle poll; no confirmation is returned — verify with get_game_log_entries. Workflow after editing a script file: call reload_game_scripts first, then reload_current_scene (or retry the failed operation); reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).");
  return result;
}

mcp::JsonValue handle_game_job_start(const mcp::JsonValue &args) {
  JV denied = authorization::deny_if_unauthorized(
      "start_game_job", SideEffect::GameRuntime);
  if (!denied.IsNull())
    return denied;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "start_game_job called");
  if (!args.IsObject())
    return error_json("start_game_job parameters must be an object");
  static constexpr const char *allowed[] = {"op", "params", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return error_json("unknown parameter for start_game_job: " + unknown);
  auto *op_p = args.Find("op");
  if (!op_p || !op_p->IsString() || op_p->GetString().empty())
    return error_json("missing required parameter: op (currently only \"eval\")");
  const std::string op = op_p->GetString();
  if (op != "eval")
    return error_json("unsupported op: " + op +
                      " — start_game_job currently supports op \"eval\" only");
  auto *params_p = args.Find("params");
  if (!params_p || !params_p->IsObject())
    return error_json(
        "missing required parameter: params (object passed to the wrapped op "
        "verbatim, e.g. {\"action\":\"script\",\"source_code\":\"...\"})");
  if (auto *timeout = args.Find("timeout_ms"); timeout &&
      (!timeout->IsInt() || timeout->GetInt() <= 0 ||
       timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  const int64_t timeout_ms = extract_timeout(args);

  std::string action;
  if (auto *action_p = params_p->Find("action");
      action_p && action_p->IsString())
    action = action_p->GetString();
  const bool suppress = needs_error_break_suppression(op, action);

  const int64_t now_ms = steady_game_now_ms();
  expire_stale_game_jobs(now_ms);

  int64_t request_id = 0;
  JV sent = start_pending_op(op, *params_p, suppress, &request_id);
  if (sent.Contains("error")) {
    append_runtime_degradation_hint(sent);
    return sent;
  }

  int64_t job_id = 0;
  {
    std::lock_guard<std::mutex> lock(game_jobs_mutex());
    job_id = game_jobs().register_job(request_id, op, timeout_ms, now_ms);
  }
  if (job_id < 0) {
    std::string discard_detail;
    discard_pending(request_id, discard_detail);
    LogSystem::instance().log_detailed(
        LogLevel::Warning, LogCategory::Tools,
        "start_game_job rejected: job table full",
        "jobs=" + std::to_string(GameJobTable::kMaxJobs) + " op=" + op +
            " timeout_ms=" + std::to_string(timeout_ms) +
            " request_id=" + std::to_string(request_id));
    return util::error_detail(
        "game job table is full (" + std::to_string(GameJobTable::kMaxJobs) +
            " jobs)",
        "start_game_job",
        "a free job slot in the game job table",
        "collect or cancel earlier jobs with get_game_job first, then retry "
        "start_game_job");
  }

  JV job(JV::object_tag);
  job["job_id"] = JV(job_id);
  job["request_id"] = JV(request_id);
  job["op"] = JV(op);
  job["timeout_ms"] = JV(timeout_ms);
  job["status"] = JV("pending");
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "start_game_job registered job " + std::to_string(job_id) +
          " (request_id " + std::to_string(request_id) + ", op " + op + ")");
  return util::ok_result(std::move(job));
}

mcp::JsonValue handle_game_job_get(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_game_job called");
  if (!args.IsObject())
    return error_json("get_game_job parameters must be an object");
  static constexpr const char *allowed[] = {"job_id", "cancel", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return error_json("unknown parameter for get_game_job: " + unknown);
  auto *job_p = args.Find("job_id");
  if (!job_p || !job_p->IsInt())
    return error_json("missing required parameter: job_id (integer returned "
                      "by start_game_job)");
  const int64_t job_id = job_p->GetInt();
  bool cancel = false;
  if (auto *cancel_p = args.Find("cancel")) {
    if (!cancel_p->IsBool())
      return error_json("cancel must be a boolean");
    cancel = cancel_p->GetBool();
  }
  int64_t wait_budget_ms = 0;
  if (auto *timeout = args.Find("timeout_ms")) {
    if (!timeout->IsInt() || timeout->GetInt() < 0 ||
        timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS)
      return error_json("timeout_ms must be an integer between 0 and " +
                        std::to_string(GDA_MAX_GAME_OP_TIMEOUT_MS));
    wait_budget_ms = timeout->GetInt();
  }

  const int64_t deadline_ms = steady_game_now_ms() + wait_budget_ms;
  for (;;) {
    int64_t request_id = 0;
    int64_t started_ms = 0;
    int64_t job_timeout_ms = 0;
    bool found = false;
    {
      std::lock_guard<std::mutex> lock(game_jobs_mutex());
      auto &jobs = game_jobs().jobs;
      auto it = jobs.find(job_id);
      if (it != jobs.end()) {
        found = true;
        request_id = it->second.request_id;
        started_ms = it->second.started_ms;
        job_timeout_ms = it->second.timeout_ms;
      }
    }
    if (!found) {
      return util::error_detail(
          "unknown or already collected game job: " + std::to_string(job_id),
          "get_game_job",
          "a job_id returned by start_game_job that is still tracked",
          "re-check the start_game_job response for the correct job_id; jobs "
          "are removed once collected or after they expire timeout_ms + 2000 "
          "ms after start");
    }
    if (cancel) {
      cancel_game_op_request(request_id);
      erase_game_job(job_id);
      std::string discard_detail;
      discard_pending(request_id, discard_detail);
      JV payload(JV::object_tag);
      payload["status"] = JV("cancelled");
      payload["job_id"] = JV(job_id);
      payload["request_id"] = JV(request_id);
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "get_game_job cancelled job " + std::to_string(job_id) +
              " (request_id " + std::to_string(request_id) + ")");
      return util::ok_result(std::move(payload));
    }
    JV response;
    if (try_collect_response(request_id, response)) {
      erase_game_job(job_id);
      JV payload(JV::object_tag);
      payload["status"] = JV("done");
      payload["job_id"] = JV(job_id);
      payload["result"] = std::move(response);
      return util::ok_result(std::move(payload));
    }
    const int64_t now_ms = steady_game_now_ms();
    if ((now_ms - started_ms) >
        (job_timeout_ms + GameJobTable::kExpiryGraceMs)) {
      erase_game_job(job_id);
      std::string discard_detail;
      discard_pending(request_id, discard_detail);
      JV payload(JV::object_tag);
      payload["status"] = JV("expired");
      payload["job_id"] = JV(job_id);
      payload["request_id"] = JV(request_id);
      payload["timeout_ms"] = JV(job_timeout_ms);
      LogSystem::instance().log_detailed(
          LogLevel::Info, LogCategory::Tools,
          "get_game_job dropped expired job " + std::to_string(job_id),
          "job_id=" + std::to_string(job_id) +
              " request_id=" + std::to_string(request_id) + " timeout_ms=" +
              std::to_string(job_timeout_ms) + " elapsed_ms=" +
              std::to_string(now_ms - started_ms));
      return util::ok_result(std::move(payload));
    }
    if (wait_budget_ms <= 0 || now_ms >= deadline_ms) {
      JV payload(JV::object_tag);
      payload["status"] = JV("pending");
      payload["job_id"] = JV(job_id);
      return util::ok_result(std::move(payload));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

namespace {

double sample_json_number(const JV &value) {
  return value.IsInt() ? static_cast<double>(value.GetInt())
                       : value.GetDouble();
}

} // namespace

mcp::JsonValue handle_game_sample_property(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "sample_game_property called");
  if (!args.IsObject())
    return error_json("sample_game_property parameters must be an object");
  static constexpr const char *allowed[] = {"node_path", "property", "frames",
                                            "interval_frames", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return error_json("unknown parameter for sample_game_property: " + unknown);
  auto *path_p = args.Find("node_path");
  auto *prop_p = args.Find("property");
  auto *frames_p = args.Find("frames");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return error_json("missing required parameter: node_path (non-empty node "
                      "path in the running game)");
  if (!prop_p || !prop_p->IsString() || prop_p->GetString().empty())
    return error_json("missing required parameter: property (non-empty "
                      "property name)");
  if (!frames_p || !frames_p->IsInt())
    return error_json("missing required parameter: frames (integer 1-120)");
  const int64_t frames = frames_p->GetInt();
  if (frames < 1 || frames > 120)
    return error_json("frames out of range (1-120): " +
                      std::to_string(frames));
  int64_t interval = 0;
  if (auto *interval_p = args.Find("interval_frames")) {
    if (!interval_p->IsInt())
      return error_json("interval_frames must be an integer");
    interval = interval_p->GetInt();
    if (interval < 0 || interval > 60)
      return error_json("interval_frames out of range (0-60): " +
                        std::to_string(interval));
  }
  if (frames * (interval + 1) > 3600)
    return error_json("sample span frames*(interval_frames+1) exceeds 3600 "
                      "process frames — raise interval_frames less, or split "
                      "into shorter samples");
  if (auto *timeout = args.Find("timeout_ms");
      timeout && (!timeout->IsInt() || timeout->GetInt() <= 0 ||
                  timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  JV params(JV::object_tag);
  params["node_path"] = *path_p;
  params["property"] = *prop_p;
  params["frames"] = JV(frames);
  params["interval_frames"] = JV(interval);
  if (auto *timeout = args.Find("timeout_ms"))
    params["timeout_ms"] = *timeout;
  JV result = handle_gda_send(std::string(GDA_OP_SAMPLE), params,
                              extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_collect_evidence(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "collect_game_evidence called");
  if (!args.IsObject())
    return error_json("collect_game_evidence parameters must be an object");
  static constexpr const char *allowed[] = {
      "include_status", "include_capture", "include_errors", "limit",
      "region", "max_dimension", "scale", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return error_json("unknown parameter for collect_game_evidence: " +
                      unknown);
  for (const char *key :
       {"include_status", "include_capture", "include_errors"}) {
    if (auto *v = args.Find(key); v && !v->IsBool())
      return error_json(std::string(key) + " must be a boolean");
  }
  if (auto *limit = args.Find("limit");
      limit && (!limit->IsInt() || limit->GetInt() < 1 ||
                limit->GetInt() > 200))
    return error_json("limit must be an integer between 1 and 200");
  if (auto *region = args.Find("region");
      region && !region->IsObject())
    return error_json("region must be an object with numeric x, y, width and "
                      "height");
  if (auto *max_dimension = args.Find("max_dimension");
      max_dimension && (!max_dimension->IsInt() ||
                        max_dimension->GetInt() < 64 ||
                        max_dimension->GetInt() > 4096))
    return error_json("max_dimension must be an integer between 64 and 4096");
  if (auto *scale = args.Find("scale");
      scale && (!scale->IsInt() || scale->GetInt() < 1 ||
                scale->GetInt() > 8))
    return error_json("scale must be an integer between 1 and 8");
  if (auto *timeout = args.Find("timeout_ms");
      timeout && (!timeout->IsInt() || timeout->GetInt() <= 0 ||
                  timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  JV params(JV::object_tag);
  copy_optional(args, params, "include_status");
  copy_optional(args, params, "include_capture");
  copy_optional(args, params, "include_errors");
  copy_optional(args, params, "limit");
  copy_optional(args, params, "region");
  copy_optional(args, params, "max_dimension");
  copy_optional(args, params, "scale");
  JV result = handle_gda_send(std::string(GDA_OP_COLLECT_EVIDENCE), params,
                              extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

mcp::JsonValue handle_game_validate_ui_layout(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "validate_game_ui_layout called");
  if (!args.IsObject())
    return error_json("validate_game_ui_layout parameters must be an object");
  static constexpr const char *allowed[] = {
      "max_elements", "ignore_paths", "ignore_classes", "min_area",
      "bounds_margin", "occlude_ratio", "timeout_ms"};
  std::string unknown;
  if (!has_only_fields(args, allowed, sizeof(allowed) / sizeof(*allowed),
                       unknown))
    return error_json("unknown parameter for validate_game_ui_layout: " +
                      unknown);
  if (auto *max = args.Find("max_elements");
      max && (!max->IsInt() || max->GetInt() < 1))
    return error_json("max_elements must be a positive integer");
  if (auto *v = args.Find("ignore_paths")) {
    if (!v->IsArray() || v->GetArray().size() > 50)
      return error_json("ignore_paths must be an array of at most 50 strings");
    for (const auto &item : v->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return error_json("ignore_paths must contain non-empty strings");
    }
  }
  if (auto *v = args.Find("ignore_classes")) {
    if (!v->IsArray() || v->GetArray().size() > 20)
      return error_json(
          "ignore_classes must be an array of at most 20 strings");
    for (const auto &item : v->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return error_json("ignore_classes must contain non-empty strings");
    }
  }
  if (auto *v = args.Find("min_area")) {
    if (!v->IsNumber() || sample_json_number(*v) < 0.0)
      return error_json("min_area must be a non-negative number");
  }
  if (auto *v = args.Find("bounds_margin")) {
    if (!v->IsNumber() || sample_json_number(*v) < 0.0)
      return error_json("bounds_margin must be a non-negative number");
  }
  if (auto *v = args.Find("occlude_ratio")) {
    if (!v->IsNumber() || sample_json_number(*v) < 0.5 ||
        sample_json_number(*v) > 1.0)
      return error_json("occlude_ratio must be a number between 0.5 and 1.0");
  }
  if (auto *timeout = args.Find("timeout_ms");
      timeout && (!timeout->IsInt() || timeout->GetInt() <= 0 ||
                  timeout->GetInt() > GDA_MAX_GAME_OP_TIMEOUT_MS))
    return error_json(game_op_timeout_error());
  JV params(JV::object_tag);
  copy_optional(args, params, "max_elements");
  copy_optional(args, params, "ignore_paths");
  copy_optional(args, params, "ignore_classes");
  copy_optional(args, params, "min_area");
  copy_optional(args, params, "bounds_margin");
  copy_optional(args, params, "occlude_ratio");
  JV result = handle_gda_send(std::string(GDA_OP_VALIDATE_UI_LAYOUT), params,
                              extract_timeout(args));
  append_runtime_degradation_hint(result);
  return result;
}

} // namespace runtime_ops
} // namespace godot_autopilot
