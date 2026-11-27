#include "tools/domains/os/os_ops.hpp"
#include "tools/domains/os/os_process_timeout.hpp"
#include "core/config.hpp"
#include "core/observability/log_system.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/runtime_ops.hpp"
#include "util/error_util.hpp"
#include "util/project_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#ifdef GetObject
#undef GetObject
#endif

namespace godot_autopilot {
namespace os_ops {

using JV = godot_autopilot::util::JsonValue;

namespace {

bool has_only_process_parameters(const JV &args, bool execute) {
  static constexpr const char *create_allowed[] = {"path", "arguments"};
  static constexpr const char *execute_allowed[] = {"path", "arguments",
                                                    "output", "timeout_ms"};
  const auto *allowed = execute ? execute_allowed : create_allowed;
  const size_t count = execute ? 4 : 2;
  if (!args.IsObject())
    return false;
  for (const auto &entry : args.GetObject()) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (entry.first == allowed[i]) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

int64_t steady_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

JV process_status_body(godot::OS *os, int32_t pid) {
  const bool running = os->is_process_running(pid);
  JV info(JV::object_tag);
  info["pid"] = JV(static_cast<int64_t>(pid));
  info["is_running"] = JV(running);
  if (running) {
    info["exit_code"] = JV();
    info["available"] = JV(false);
    info["hint"] = JV(
        "the process is still running; the exit code becomes readable after "
        "it exits");
    return info;
  }
  const int32_t code = os->get_process_exit_code(pid);
  info["exit_code"] = JV(static_cast<int64_t>(code));
  info["available"] = JV(code != -1);
  if (code == -1) {
    info["hint"] = JV(
        "no exit code is available for this pid: it was not started by this "
        "editor process or the platform does not expose it");
  }
  return info;
}

constexpr uint64_t kProcessSliceReadBytes = 65536;
constexpr int64_t kProcessKillBudgetMs = 2000;

struct OsProcessState {
  int64_t pid = 0;
  godot::Ref<godot::FileAccess> stdout_pipe;
  godot::Ref<godot::FileAccess> stderr_pipe;
  std::string stdout_text;
  std::string stderr_text;
  bool stdout_truncated = false;
  bool stderr_truncated = false;
  bool capture_output = false;
  int64_t started_ms = 0;
};

std::mutex g_process_mutex;
std::map<int64_t, std::shared_ptr<OsProcessState>> g_process_states;
std::atomic<int64_t> g_process_seq{1};

void drain_pipe(const godot::Ref<godot::FileAccess> &pipe, std::string *out,
                bool &truncated) {
  if (pipe.is_null() || !pipe->is_open())
    return;
  uint64_t available = pipe->get_length();
  if (available == 0)
    return;
  if (available > kProcessSliceReadBytes)
    available = kProcessSliceReadBytes;
  std::vector<uint8_t> buffer(static_cast<size_t>(available));
  const uint64_t read = pipe->get_buffer(buffer.data(), available);
  if (read == 0)
    return;
  if (out == nullptr)
    return;
  constexpr size_t limit = GDA_SCAN_MAX_FILE_BYTES;
  if (out->size() + static_cast<size_t>(read) <= limit) {
    out->append(reinterpret_cast<const char *>(buffer.data()),
                static_cast<size_t>(read));
    return;
  }
  const size_t room = limit > out->size() ? limit - out->size() : 0;
  if (room > 0)
    out->append(reinterpret_cast<const char *>(buffer.data()), room);
  truncated = true;
}

std::shared_ptr<OsProcessState> take_process_state(int64_t op_id) {
  std::lock_guard<std::mutex> lock(g_process_mutex);
  auto it = g_process_states.find(op_id);
  if (it == g_process_states.end())
    return nullptr;
  return it->second;
}

JV advance_os_process(int64_t op_id) {
  std::shared_ptr<OsProcessState> state = take_process_state(op_id);
  if (!state) {
    JV e(JV::object_tag);
    e["error"] = JV("the external process wait state is gone");
    return e;
  }
  const bool capture = state->capture_output;
  drain_pipe(state->stdout_pipe, capture ? &state->stdout_text : nullptr,
             state->stdout_truncated);
  drain_pipe(state->stderr_pipe, capture ? &state->stderr_text : nullptr,
             state->stderr_truncated);
  auto *os = godot::OS::get_singleton();
  const int32_t exit_code =
      os ? os->get_process_exit_code(static_cast<int32_t>(state->pid)) : -1;
  if (exit_code == -1) {
    JV r(JV::object_tag);
    r["done"] = JV(false);
    return r;
  }
  drain_pipe(state->stdout_pipe, capture ? &state->stdout_text : nullptr,
             state->stdout_truncated);
  drain_pipe(state->stderr_pipe, capture ? &state->stderr_text : nullptr,
             state->stderr_truncated);
  {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    g_process_states.erase(op_id);
  }
  godot::Dictionary d;
  d["exit_code"] = static_cast<int64_t>(exit_code);
  d["stdout"] =
      godot::String(capture ? state->stdout_text.c_str() : "");
  d["stderr"] =
      godot::String(capture ? state->stderr_text.c_str() : "");
  d["stdout_truncated"] = capture && state->stdout_truncated;
  d["stderr_truncated"] = capture && state->stderr_truncated;
  d["max_output_bytes"] = static_cast<int64_t>(GDA_SCAN_MAX_FILE_BYTES);
  JV r(JV::object_tag);
  r["done"] = JV(true);
  r["result"] = VariantJson::serialize(d);
  return r;
}

JV kill_os_process(int64_t op_id) {
  std::shared_ptr<OsProcessState> state = take_process_state(op_id);
  if (!state) {
    JV r(JV::object_tag);
    r["killed"] = JV(false);
    return r;
  }
  auto *os = godot::OS::get_singleton();
  bool killed = false;
  if (os) {
    killed = os->kill(static_cast<int32_t>(state->pid)) == godot::Error::OK;
  }
  {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    g_process_states.erase(op_id);
  }
  JV r(JV::object_tag);
  r["killed"] = JV(killed);
  return r;
}

JV os_process_timeout_result(int64_t pid, int64_t timeout_ms, bool termination_confirmed) {
  JV e(JV::object_tag);
  const std::string detail = os_ops_detail::termination_detail(termination_confirmed);
  e["error"] = JV("the external process did not finish within " +
                  std::to_string(timeout_ms) + " ms — " + detail);
  e["code"] = JV("main_thread_timeout");
  e["retryable"] = JV(false);
  e["cancelled"] = JV(true);
  e["timeout_ms"] = JV(timeout_ms);
  e["process_id"] = JV(pid);
  e["termination_confirmed"] = JV(termination_confirmed);
  return e;
}

} // namespace

JV handle_os_alert(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "show_os_alert called");

  auto *text_p = args.Find("text");
  if (!text_p || !text_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: text");
    return e;
  }

  bool modal = false;
  if (auto *modal_p = args.Find("modal")) {
    if (!modal_p->IsBool())
      return util::error_json("modal must be a boolean");
    modal = modal_p->GetBool();
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string text = text_p->GetString();
  std::string title = "Alert!";
  auto *title_p = args.Find("title");
  if (title_p && title_p->IsString()) {
    title = title_p->GetString();
  }

  if (!modal) {
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    r["shown"] = JV(false);
    r["requires_confirmation"] = JV(true);
    r["title"] = JV(title);
    r["text"] = JV(text);
    r["notice"] = JV(
        "show_os_alert did not open a modal dialog: a modal alert blocks the "
        "editor main thread until a human dismisses it, which freezes "
        "unattended sessions. Re-run this tool with modal=true when a human "
        "is watching the editor to actually display the alert.");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "show_os_alert returned the confirmation notice "
                              "without opening a modal dialog");
    return r;
  }

  os->alert(godot::String(text.c_str()), godot::String(title.c_str()));

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["shown"] = JV(true);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "show_os_alert completed");
  return r;
}

JV handle_os_create_process(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_os_process called");

  if (!has_only_process_parameters(args, false))
    return util::error_json("unknown or invalid parameter for create_os_process");
  auto *path_p = args.Find("path");
  if (!path_p || !path_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: path");
    return e;
  }

  auto *args_p = args.Find("arguments");
  if (!args_p || !args_p->IsArray()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: arguments (array)");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string path = path_p->GetString();
  if (path.empty())
    return util::error_json("path must not be empty");
  const auto &json_args = args_p->GetArray();
  godot::PackedStringArray ps_args;
  for (const auto &a : json_args) {
    if (!a.IsString())
      return util::error_json("arguments must contain only strings");
    ps_args.append(godot::String(a.GetString().c_str()));
  }

  int32_t pid = os->create_process(godot::String(path.c_str()), ps_args);

  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(pid));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_os_process completed");
  return r;
}

JV handle_os_execute(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "execute_os_process called");

  if (!has_only_process_parameters(args, true))
    return util::error_json("unknown or invalid parameter for execute_os_process");
  auto *path_p = args.Find("path");
  if (!path_p || !path_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: path");
    return e;
  }

  auto *args_p = args.Find("arguments");
  if (!args_p || !args_p->IsArray()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: arguments (array)");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string path = path_p->GetString();
  if (path.empty())
    return util::error_json("path must not be empty");
  const auto &json_args = args_p->GetArray();
  godot::PackedStringArray ps_args;
  for (const auto &a : json_args) {
    if (!a.IsString())
      return util::error_json("arguments must contain only strings");
    ps_args.append(godot::String(a.GetString().c_str()));
  }

  bool capture_output = false;
  auto *output_p = args.Find("output");
  if (output_p && !output_p->IsBool())
    return util::error_json("output must be a boolean");
  if (output_p)
    capture_output = output_p->GetBool();

  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  if (auto *timeout_p = args.Find("timeout_ms")) {
    if (!timeout_p->IsInt() ||
        timeout_p->GetInt() < GDA_OS_PROCESS_MIN_TIMEOUT_MS ||
        timeout_p->GetInt() > GDA_OS_PROCESS_MAX_TIMEOUT_MS)
      return util::error_json(
          "timeout_ms must be an integer between " +
          std::to_string(GDA_OS_PROCESS_MIN_TIMEOUT_MS) + " and " +
          std::to_string(GDA_OS_PROCESS_MAX_TIMEOUT_MS));
    timeout_ms = timeout_p->GetInt();
  }

  godot::Dictionary pipes = os->execute_with_pipe(
      godot::String(path.c_str()), ps_args, false);
  if (!pipes.has("pid")) {
    return util::error_json("failed to start the external process: " + path);
  }

  auto state = std::make_shared<OsProcessState>();
  state->pid = static_cast<int64_t>(pipes["pid"]);
  state->capture_output = capture_output;
  state->started_ms = steady_now_ms();
  if (pipes.has("stdio"))
    state->stdout_pipe = godot::Ref<godot::FileAccess>(pipes["stdio"]);
  if (pipes.has("stderr"))
    state->stderr_pipe = godot::Ref<godot::FileAccess>(pipes["stderr"]);

  const int64_t op_id = g_process_seq.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    g_process_states[op_id] = state;
  }

  JV wait(JV::object_tag);
  wait["kind"] = JV("os_process");
  wait["id"] = JV(op_id);
  wait["timeout_ms"] = JV(timeout_ms);
  JV marker(JV::object_tag);
  marker["__gda_wait"] = std::move(wait);
  LogSystem::instance().log_detailed(
      LogLevel::Info, LogCategory::Tools,
      "execute_os_process started a non-blocking external process",
      "path=" + path + " process_id=" + std::to_string(state->pid) +
          " timeout_ms=" + std::to_string(timeout_ms) +
          " capture_output=" + (capture_output ? "true" : "false"));
  return marker;
}

int64_t start_csharp_build(const godot::String &path,
                           const godot::PackedStringArray &build_args) {
  auto *os = godot::OS::get_singleton();
  if (os == nullptr) return -1;
  godot::Dictionary pipes = os->execute_with_pipe(path, build_args, false);
  if (!pipes.has("pid")) return -1;
  auto state = std::make_shared<OsProcessState>();
  state->pid = static_cast<int64_t>(pipes["pid"]);
  state->capture_output = true;
  state->started_ms = steady_now_ms();
  if (pipes.has("stdio"))
    state->stdout_pipe = godot::Ref<godot::FileAccess>(pipes["stdio"]);
  if (pipes.has("stderr"))
    state->stderr_pipe = godot::Ref<godot::FileAccess>(pipes["stderr"]);
  const int64_t op_id = g_process_seq.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(g_process_mutex);
    g_process_states[op_id] = state;
  }
  return op_id;
}

JV wait_os_process(int64_t op_id, int64_t timeout_ms) {
  if (!runtime_ops::has_editor_queue()) {
    return util::error_json(
        "the editor main thread queue is unavailable — the external process "
        "cannot be waited for");
  }
  CommandQueue &queue = get_editor_queue();
  if (queue.is_main_thread()) {
    kill_os_process(op_id);
    return util::error_json(
        "execute_os_process cannot wait for the external process on the editor "
        "main thread — the process was terminated; call the tool through the "
        "native control plane so the wait runs on a worker thread");
  }
  const int64_t deadline_ms = steady_now_ms() + timeout_ms;
  for (;;) {
    const int64_t remaining_ms = deadline_ms - steady_now_ms();
    if (remaining_ms <= 0)
      break;
    const JV slice = dispatch::run_main_thread_slice(
        queue, "execute_os_process",
        [op_id]() { return advance_os_process(op_id); }, remaining_ms);
    if (slice.Find("error") != nullptr)
      return slice;
    const auto *done_p = slice.Find("done");
    if (done_p && done_p->IsBool() && done_p->GetBool()) {
      const auto *result_p = slice.Find("result");
      if (result_p)
        return *result_p;
      return util::error_json(
          "the external process finished without a readable result");
    }
  }
  int64_t pid = 0;
  {
    std::shared_ptr<OsProcessState> state = take_process_state(op_id);
    if (state)
      pid = state->pid;
  }
  bool termination_confirmed = false;
  const JV kill_result = dispatch::run_main_thread_slice(
      queue, "execute_os_process kill",
      [op_id]() { return kill_os_process(op_id); }, kProcessKillBudgetMs);
  const JV *killed_p = kill_result.Find("killed");
  if (killed_p != nullptr && killed_p->IsBool()) {
    termination_confirmed = killed_p->GetBool();
  }
  LogSystem::instance().log_detailed(
      LogLevel::Warning, LogCategory::Tools,
      "execute_os_process timed out; termination_confirmed=" +
          std::string(termination_confirmed ? "true" : "false"),
      "wait_id=" + std::to_string(op_id) + " timeout_ms=" +
          std::to_string(timeout_ms));
  return os_process_timeout_result(pid, timeout_ms, termination_confirmed);
}

JV handle_os_get_datetime(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_datetime called");

  auto *time = godot::Time::get_singleton();
  if (!time) {
    JV e(JV::object_tag);
    e["error"] = JV("Time singleton not available");
    return e;
  }

  bool utc = false;
  auto *utc_p = args.Find("utc");
  if (utc_p && utc_p->IsBool()) {
    utc = utc_p->GetBool();
  }

  godot::Dictionary dt = time->get_datetime_dict_from_system(utc);

  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(dt);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_datetime completed");
  return r;
}

JV handle_os_get_environment(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_environment called");

  auto *var_p = args.Find("variable");
  if (!var_p || !var_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: variable");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string variable = var_p->GetString();
  godot::String value = os->get_environment(godot::String(variable.c_str()));

  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(value));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_environment completed");
  return r;
}

JV handle_os_get_locale(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_locale called");

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  godot::String locale = os->get_locale();

  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(locale));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_locale completed");
  return r;
}

JV handle_os_get_system_fonts(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_system_fonts called");

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  godot::PackedStringArray fonts = os->get_system_fonts();
  JV arr(JV::array_tag);
  for (int i = 0; i < fonts.size(); ++i) {
    arr.PushBack(JV(util::to_std(fonts[i])));
  }

  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_system_fonts completed");
  return r;
}

JV handle_os_get_system_info(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_system_info called");

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  JV info(JV::object_tag);
  info["name"] = JV(util::to_std(os->get_name()));
  info["version"] = JV(util::to_std(os->get_version()));
  info["processor_count"] = JV(static_cast<int64_t>(os->get_processor_count()));
  info["processor_name"] = JV(util::to_std(os->get_processor_name()));

  JV r(JV::object_tag);
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_system_info completed");
  return r;
}

JV handle_os_get_unique_id(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_unique_id called");

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  godot::String uid = os->get_unique_id();

  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(uid));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_unique_id completed");
  return r;
}

JV handle_os_get_unix_time(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_unix_time called");

  auto *time = godot::Time::get_singleton();
  if (!time) {
    JV e(JV::object_tag);
    e["error"] = JV("Time singleton not available");
    return e;
  }

  double unix_time = time->get_unix_time_from_system();

  JV r(JV::object_tag);
  r["result"] = JV(unix_time);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_unix_time completed");
  return r;
}

JV handle_os_get_user_data_dir(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_user_data_dir called");

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  godot::String dir = os->get_user_data_dir();

  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(dir));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_os_user_data_dir completed");
  return r;
}

JV handle_os_kill(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "kill_os_process called");

  auto *pid_p = args.Find("pid");
  if (!pid_p || !pid_p->IsInt()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: pid (integer)");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  int32_t pid = static_cast<int32_t>(pid_p->GetInt());
  if (pid_p->GetInt() <= 0 || pid_p->GetInt() > INT32_MAX)
    return util::error_json("pid must be a positive 32-bit integer");
  godot::Error err = os->kill(pid);

  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(err));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "kill_os_process completed");
  return r;
}

JV handle_os_process_status(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_process status called");
  auto *pid_p = args.Find("pid");
  if (!pid_p || !pid_p->IsInt() || pid_p->GetInt() <= 0 ||
      pid_p->GetInt() > INT32_MAX) {
    return util::error_json(
        "missing required parameter: pid (positive 32-bit integer)");
  }
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  JV r(JV::object_tag);
  r["result"] = process_status_body(
      os, static_cast<int32_t>(pid_p->GetInt()));
  return r;
}

JV handle_os_process_exit_code(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_process exit_code called");
  auto *pid_p = args.Find("pid");
  if (!pid_p || !pid_p->IsInt() || pid_p->GetInt() <= 0 ||
      pid_p->GetInt() > INT32_MAX) {
    return util::error_json(
        "missing required parameter: pid (positive 32-bit integer)");
  }
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  JV r(JV::object_tag);
  r["result"] = process_status_body(
      os, static_cast<int32_t>(pid_p->GetInt()));
  return r;
}

JV handle_os_process_is_running(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_process is_running called");
  auto *pid_p = args.Find("pid");
  if (!pid_p || !pid_p->IsInt() || pid_p->GetInt() <= 0 ||
      pid_p->GetInt() > INT32_MAX) {
    return util::error_json(
        "missing required parameter: pid (positive 32-bit integer)");
  }
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  const int32_t pid = static_cast<int32_t>(pid_p->GetInt());
  JV info(JV::object_tag);
  info["pid"] = JV(static_cast<int64_t>(pid));
  info["is_running"] = JV(os->is_process_running(pid));
  JV r(JV::object_tag);
  r["result"] = std::move(info);
  return r;
}

JV handle_os_memory_info(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_system_info memory_info called");
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  godot::Dictionary info = os->get_memory_info();
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(godot::Variant(info));
  return r;
}

JV handle_os_static_memory(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_system_info static_memory called");
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  JV info(JV::object_tag);
  info["usage"] = JV(static_cast<int64_t>(os->get_static_memory_usage()));
  info["peak_usage"] =
      JV(static_cast<int64_t>(os->get_static_memory_peak_usage()));
  JV r(JV::object_tag);
  r["result"] = std::move(info);
  return r;
}

JV handle_os_video_adapter(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_system_info video_adapter called");
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  godot::PackedStringArray fields = os->get_video_adapter_driver_info();
  JV arr(JV::array_tag);
  for (int i = 0; i < fields.size(); ++i) {
    arr.PushBack(JV(util::to_std(fields[i])));
  }
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_os_cmdline_args(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "os_system_info cmdline_args called");
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return util::error_json("OS singleton not available");
  }
  godot::PackedStringArray args = os->get_cmdline_args();
  JV arr(JV::array_tag);
  for (int i = 0; i < args.size(); ++i) {
    arr.PushBack(JV(util::to_std(args[i])));
  }
  JV r(JV::object_tag);
  r["result"] = std::move(arr);
  return r;
}

JV handle_os_move_to_trash(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_os_file_to_trash called");

  auto *path_p = args.Find("path");
  if (!path_p || !path_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: path");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string path = path_p->GetString();
  const util::ProjectPath checked =
      util::normalize_project_path(path, true, false);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "move only project files and user files to the "
                              "trash; the namespace root is not trashable");
  godot::String os_path(checked.value.c_str());
  if (auto *ps = godot::ProjectSettings::get_singleton()) {
    os_path = ps->globalize_path(os_path);
  }
  godot::Error err = os->move_to_trash(os_path);

  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(err));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_os_file_to_trash completed");
  return r;
}

JV handle_os_set_environment(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_os_environment called");

  auto *var_p = args.Find("variable");
  if (!var_p || !var_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: variable");
    return e;
  }

  auto *val_p = args.Find("value");
  if (!val_p || !val_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: value");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string variable = var_p->GetString();
  std::string value = val_p->GetString();
  os->set_environment(godot::String(variable.c_str()),
                      godot::String(value.c_str()));

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_os_environment completed");
  return r;
}

JV handle_os_shell_open(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_os_path called");

  auto *uri_p = args.Find("uri");
  if (!uri_p || !uri_p->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: uri");
    return e;
  }

  auto *os = godot::OS::get_singleton();
  if (!os) {
    JV e(JV::object_tag);
    e["error"] = JV("OS singleton not available");
    return e;
  }

  std::string uri = uri_p->GetString();
  godot::Error err = os->shell_open(godot::String(uri.c_str()));

  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(err));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_os_path completed");
  return r;
}

namespace {

godot_autopilot::util::JsonValue os_action_error(const std::string &message) {
  JV error(JV::object_tag);
  error["error"] = JV(message);
  return error;
}

bool read_os_action(const JV &args, std::string &out, JV &error) {
  const JV *action = args.Find("action");
  if (action == nullptr) {
    error = os_action_error("missing required parameter: action");
    return false;
  }
  if (!action->IsString()) {
    error = os_action_error("invalid parameter: action must be a string");
    return false;
  }
  out = action->GetString();
  return true;
}

JV strip_os_action(const JV &args) {
  JV out = args.IsObject() ? args : JV(JV::object_tag);
  out.GetObject().erase("action");
  return out;
}

} // namespace

JV handle_os_has_environment(const JV &args) {
  const JV *variable = args.Find("variable");
  if (variable == nullptr || !variable->IsString()) {
    return os_action_error("missing required parameter: variable");
  }
  auto *os = godot::OS::get_singleton();
  if (!os) {
    return os_action_error("OS singleton not available");
  }
  const bool has = os->has_environment(
      godot::String(variable->GetString().c_str()));
  JV r(JV::object_tag);
  r["result"] = JV(has);
  return r;
}

JV handle_os_process(const JV &args) {
  std::string action;
  JV error;
  if (!read_os_action(args, action, error)) {
    return error;
  }
  if (action == "create") {
    return handle_os_create_process(strip_os_action(args));
  }
  if (action == "execute") {
    return handle_os_execute(strip_os_action(args));
  }
  if (action == "kill") {
    return handle_os_kill(strip_os_action(args));
  }
  if (action == "status") {
    return handle_os_process_status(strip_os_action(args));
  }
  if (action == "exit_code") {
    return handle_os_process_exit_code(strip_os_action(args));
  }
  if (action == "is_running") {
    return handle_os_process_is_running(strip_os_action(args));
  }
  return os_action_error(
      "unknown action '" + action +
      "' for os_process (expected create, execute, kill, status, exit_code, "
      "is_running)");
}

JV handle_os_environment(const JV &args) {
  std::string action;
  JV error;
  if (!read_os_action(args, action, error)) {
    return error;
  }
  if (action == "get") {
    return handle_os_get_environment(strip_os_action(args));
  }
  if (action == "set") {
    return handle_os_set_environment(strip_os_action(args));
  }
  if (action == "has") {
    return handle_os_has_environment(strip_os_action(args));
  }
  return os_action_error(
      "unknown action '" + action +
      "' for os_environment (expected get, set, has)");
}

JV handle_os_time(const JV &args) {
  std::string action;
  JV error;
  if (!read_os_action(args, action, error)) {
    return error;
  }
  if (action == "datetime") {
    return handle_os_get_datetime(strip_os_action(args));
  }
  if (action == "unix_time") {
    return handle_os_get_unix_time(strip_os_action(args));
  }
  return os_action_error(
      "unknown action '" + action +
      "' for os_time (expected datetime, unix_time)");
}

JV handle_os_system_info(const JV &args) {
  std::string action;
  JV error;
  if (!read_os_action(args, action, error)) {
    return error;
  }
  if (action == "info") {
    return handle_os_get_system_info(strip_os_action(args));
  }
  if (action == "locale") {
    return handle_os_get_locale(strip_os_action(args));
  }
  if (action == "fonts") {
    return handle_os_get_system_fonts(strip_os_action(args));
  }
  if (action == "memory_info") {
    return handle_os_memory_info(strip_os_action(args));
  }
  if (action == "static_memory") {
    return handle_os_static_memory(strip_os_action(args));
  }
  if (action == "video_adapter") {
    return handle_os_video_adapter(strip_os_action(args));
  }
  if (action == "cmdline_args") {
    return handle_os_cmdline_args(strip_os_action(args));
  }
  return os_action_error(
      "unknown action '" + action +
      "' for os_system_info (expected info, locale, fonts, memory_info, "
      "static_memory, video_adapter, cmdline_args)");
}

} // namespace os_ops
} // namespace godot_autopilot
