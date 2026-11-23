#include "tools/domains/monitor/monitor_ops.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include <mcp/JsonValue.hpp>

#include "core/observability/game_monitor.hpp"
#include "core/observability/monitor_analysis.hpp"
#include "core/storage/gda_home.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"

namespace godot_autopilot {
namespace monitor_ops {

namespace {

const char *kPrefix = "gda-monitor-";
const char *kSuffix = ".jsonl";

bool valid_session_id(const std::string &id) {
  if (id.empty()) {
    return false;
  }
  for (char c : id) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
    if (!ok) {
      return false;
    }
  }
  return true;
}

std::string session_dir() { return game_monitor::sessions_dir(); }

std::string session_path(const std::string &id) {
  return session_dir() + "/" + kPrefix + id + kSuffix;
}

std::vector<std::string> list_sessions() {
  std::vector<std::string> ids;
  const std::string dir_path = session_dir();
  if (dir_path.empty()) {
    return ids;
  }
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(
      godot::String::utf8(dir_path.c_str()));
  if (dir.is_null() || dir->list_dir_begin() != godot::OK) {
    return ids;
  }
  godot::String name = dir->get_next();
  while (name != godot::String()) {
    const std::string entry = util::to_std(name);
    if (!dir->current_is_dir() && entry.rfind(kPrefix, 0) == 0 &&
        entry.size() > std::strlen(kPrefix) + std::strlen(kSuffix) &&
        entry.compare(entry.size() - std::strlen(kSuffix), std::strlen(kSuffix),
                      kSuffix) == 0) {
      ids.push_back(entry.substr(std::strlen(kPrefix),
                                 entry.size() - std::strlen(kPrefix) -
                                     std::strlen(kSuffix)));
    }
    name = dir->get_next();
  }
  std::sort(ids.begin(), ids.end(), std::greater<std::string>());
  return ids;
}

bool read_text(const std::string &path, std::string *out) {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(godot::String::utf8(path.c_str()),
                              godot::FileAccess::READ);
  if (file.is_null()) {
    return false;
  }
  *out = util::to_std(file->get_as_text());
  file->close();
  return true;
}

mcp::JsonValue parse_json_object(const std::string &text) {
  mcp::JsonValue value;
  if (!util::try_parse_json(text, value) || !value.IsObject()) {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  }
  return value;
}

mcp::JsonValue error_result(const std::string &message) {
  return util::error_json(message);
}

mcp::JsonValue ok_result(mcp::JsonValue value) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = std::move(value);
  return out;
}

} // namespace

mcp::JsonValue handle_game_monitor(const mcp::JsonValue &args) {
  const mcp::JsonValue *action_p = args.Find("action");
  const std::string action = action_p != nullptr && action_p->IsString()
                                 ? action_p->GetString()
                                 : std::string("status");

  if (action == "status") {
    mcp::JsonValue value(mcp::JsonValue::object_tag);
    value["supported_scope"] = mcp::JsonValue("editor_launched_game");
    value["custom_discovery"] = mcp::JsonValue("Performance.add_custom_monitor");
    value["sessions_dir"] = mcp::JsonValue(session_dir());
    value["schema"] = mcp::JsonValue(static_cast<int64_t>(1));
    return ok_result(std::move(value));
  }

  if (action == "sessions") {
    mcp::JsonValue value(mcp::JsonValue::object_tag);
    const std::vector<std::string> ids = list_sessions();
    mcp::JsonValue array(mcp::JsonValue::array_tag);
    for (const std::string &id : ids) {
      array.PushBack(mcp::JsonValue(id));
    }
    value["sessions"] = std::move(array);
    value["count"] = mcp::JsonValue(static_cast<int64_t>(ids.size()));
    return ok_result(std::move(value));
  }

  const mcp::JsonValue *id_p = args.Find("session");
  if (id_p == nullptr || !id_p->IsString() ||
      !valid_session_id(id_p->GetString())) {
    return error_result("invalid parameter: session must be a recorded session id");
  }
  const std::string id = id_p->GetString();
  const std::string path = session_path(id);

  if (action == "report") {
    std::string text;
    if (!read_text(path, &text)) {
      return error_result("monitor session not found: " + id);
    }
    std::string session_id;
    std::vector<game_monitor_analysis::MonitorDef> builtin;
    std::vector<game_monitor_analysis::MonitorDef> custom;
    std::vector<game_monitor_analysis::FrameSample> frames;
    double slow_ms = 33.3;
    if (const mcp::JsonValue *slow = args.Find("slow_ms")) {
      if (slow->IsNumber()) {
        slow_ms = slow->IsInt() ? static_cast<double>(slow->GetInt())
                                : slow->GetDouble();
      }
    }
    std::size_t start = 0;
    bool have_header = false;
    while (start < text.size()) {
      const std::size_t end = text.find('\n', start);
      const std::string line = text.substr(
          start, end == std::string::npos ? std::string::npos : end - start);
      if (!line.empty()) {
        if (!have_header && game_monitor_analysis::parse_header(
                                line, &session_id, &builtin, &custom)) {
          have_header = true;
        } else if (game_monitor_analysis::parse_custom_change(line, &custom)) {
          continue;
        } else {
          game_monitor_analysis::FrameSample frame;
          if (game_monitor_analysis::parse_frame(line, &frame)) {
            frames.push_back(std::move(frame));
          }
        }
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
    if (!have_header) {
      return error_result("monitor session has no valid header: " + id);
    }
    const auto report =
        game_monitor_analysis::analyze(frames, builtin, custom, slow_ms);
    return ok_result(parse_json_object(
        game_monitor_analysis::report_to_json(report)));
  }

  if (action == "export") {
    std::string text;
    if (!read_text(path, &text)) {
      return error_result("monitor session not found: " + id);
    }
    const std::string dir = HomeResolver::gda_home() + "/monitor-sessions";
    godot::DirAccess::make_dir_recursive_absolute(
        godot::String::utf8(dir.c_str()));
    const std::string target = dir + "/" + kPrefix + id + kSuffix;
    godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
        godot::String::utf8(target.c_str()), godot::FileAccess::WRITE);
    if (file.is_null()) {
      return error_result("failed to write monitor export: " + target);
    }
    file->store_string(godot::String::utf8(text.c_str()));
    file->close();
    mcp::JsonValue value(mcp::JsonValue::object_tag);
    value["path"] = mcp::JsonValue(target);
    value["bytes"] = mcp::JsonValue(static_cast<int64_t>(text.size()));
    return ok_result(std::move(value));
  }

  return error_result("unknown action '" + action +
                      "': expected status, sessions, report or export");
}

} // namespace monitor_ops
} // namespace godot_autopilot
