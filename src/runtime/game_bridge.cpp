#include "game_bridge.hpp"

#include "core/config.hpp"
#include "core/log_system.hpp"
#include "gda_protocol.hpp"
#include "util/error_util.hpp"
#include <cstdio>
#include <functional>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace runtime {
namespace game_bridge {

JV error_result(const std::string &message) {
  JV r(JV::object_tag);
  r["error"] = JV(message);
  return r;
}

JV ok_result(JV result) {
  JV r(JV::object_tag);
  r["result"] = std::move(result);
  return r;
}

godot::SceneTree *get_scene_tree() {
  auto *engine = godot::Engine::get_singleton();
  if (!engine)
    return nullptr;
  return godot::Object::cast_to<godot::SceneTree>(engine->get_main_loop());
}

godot::Node *resolve_node(const std::string &node_path) {
  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return nullptr;
  if (node_path.empty())
    return tree->get_current_scene();
  godot::NodePath path(godot::String(node_path.c_str()));
  if (auto *current = tree->get_current_scene()) {
    if (auto *node = current->get_node_or_null(path))
      return node;
  }
  return tree->get_root()->get_node_or_null(path);
}

namespace {

godot::String gda_string(const std::string_view &sv) {
  return godot::String(std::string(sv).c_str());
}

uint64_t g_last_activity_ms = 0;

struct GameErrorEntry {
  int hr, min, sec, msec;
  std::string file, func;
  int line;
  std::string error, descr;
  bool is_warning;
  std::vector<std::string> stack;
  uint64_t seq;
};

struct GameOutputEntry {
  std::string text;
  int type;
};

std::mutex g_buffer_mtx;
std::vector<GameErrorEntry> g_error_buffer;
std::vector<GameOutputEntry> g_output_buffer;

uint64_t g_error_seq = 0;

void push_game_output(const std::string &text, int type) {
  std::lock_guard<std::mutex> lock(g_buffer_mtx);
  g_output_buffer.push_back({text, type});
  if (g_output_buffer.size() > GDA_OUTPUT_BUFFER_MAX) {
    g_output_buffer.erase(g_output_buffer.begin());
  }
}

} // namespace

void push_game_error(const std::string &file, const std::string &func, int line,
                     const std::string &error, const std::string &descr,
                     bool is_warning, std::vector<std::string> stack) {
  uint64_t time = godot::Time::get_singleton()
                      ? godot::Time::get_singleton()->get_ticks_msec()
                      : 0;
  std::lock_guard<std::mutex> lock(g_buffer_mtx);
  g_error_buffer.push_back(
      {static_cast<int>(time / 3600000), static_cast<int>((time / 60000) % 60),
       static_cast<int>((time / 1000) % 60), static_cast<int>(time % 1000),
       file, func, line, error, descr, is_warning, std::move(stack),
       ++g_error_seq});
  if (g_error_buffer.size() > GDA_ERROR_BUFFER_MAX) {
    g_error_buffer.erase(g_error_buffer.begin());
  }
}

uint64_t current_error_seq() {
  std::lock_guard<std::mutex> lock(g_buffer_mtx);
  return g_error_seq;
}

EvalErrorDelta eval_error_delta(uint64_t since_seq) {
  EvalErrorDelta delta;
  delta.structured = JV(JV::object_tag);
  {
    std::lock_guard<std::mutex> lock(g_buffer_mtx);
    for (const GameErrorEntry &e : g_error_buffer) {
      if (e.seq <= since_seq)
        continue;
      std::string line;
      if (!e.file.empty()) {
        line = e.file;
        if (e.line > 0)
          line += ":" + std::to_string(e.line);
        line += " - ";
      }
      line += e.error;
      if (!e.descr.empty())
        line += ": " + e.descr;
      if (!delta.text.empty())
        delta.text += "\n";
      delta.text += line;
      if (delta.structured.Empty()) {
        if (!e.file.empty())
          delta.structured["file"] = JV(e.file);
        if (e.line > 0)
          delta.structured["line"] = JV(static_cast<int64_t>(e.line));
        if (!e.error.empty())
          delta.structured["message"] = JV(e.error);
      }
    }
  }
  return delta;
}

std::string truncate_error_text(const std::string &text) {
  if (text.size() > GDA_EVAL_TRUNCATE_BYTES) {
    return text.substr(0, GDA_EVAL_TRUNCATE_BYTES) + "\n...(truncated, total " +
           std::to_string(text.size()) + " bytes)";
  }
  return text;
}

void append_eval_runtime_errors(JV &body, uint64_t since_seq) {
  EvalErrorDelta delta = eval_error_delta(since_seq);
  if (delta.text.empty())
    return;
  body["runtime_error"] = JV(true);
  body["error_details"] = JV(truncate_error_text(delta.text));
  if (!delta.structured.Empty()) {
    body["structured_error"] = std::move(delta.structured);
  }
}

namespace {

constexpr int32_t ENGINE_ERROR_TYPE_ERR_WARNING = 3;

class GameBridgeLogger : public godot::Logger {
  GDCLASS(GameBridgeLogger, godot::Logger)

protected:
  static void _bind_methods() {}

public:
  void _log_error(const godot::String &p_function, const godot::String &p_file,
                  int32_t p_line, const godot::String &p_code,
                  const godot::String &p_rationale, bool p_editor_notify,
                  int32_t p_error_type,
                  const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>
                      &p_script_backtraces) override {
    (void)p_editor_notify;
    (void)p_script_backtraces;
    push_game_error(util::to_std(p_file), util::to_std(p_function), p_line,
                    util::to_std(p_code), util::to_std(p_rationale),
                    p_error_type == ENGINE_ERROR_TYPE_ERR_WARNING, {});
  }

  void _log_message(const godot::String &p_message, bool p_error) override {
    if (p_error) {
      push_game_error("", "", 0, util::to_std(p_message), "", false, {});
    } else {
      push_game_output(util::to_std(p_message), 0);
    }
  }
};

void append_activity_fields(JV &r) {
  r[GDA_FIELD_LAST_ACTIVITY_MS] = JV(static_cast<int64_t>(g_last_activity_ms));
  bool healthy = true;
  if (g_last_activity_ms != 0 && godot::Time::get_singleton()) {
    uint64_t now_ms = godot::Time::get_singleton()->get_ticks_msec();
    healthy = (now_ms - g_last_activity_ms) <
              static_cast<uint64_t>(GDA_HEALTHY_ACTIVITY_THRESHOLD_MS);
  }
  r[GDA_FIELD_HEALTHY] = JV(healthy);
}

JV op_status() {
  JV r(JV::object_tag);
  auto *engine = godot::Engine::get_singleton();
  if (engine) {
    godot::Dictionary version_info = engine->get_version_info();
    if (version_info.has("string")) {
      r["version"] = JV(util::to_std(godot::String(version_info["string"])));
    }
    r["fps"] = JV(engine->get_frames_per_second());
    r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
  }
  append_activity_fields(r);
  if (auto *tree = get_scene_tree()) {
    r["paused"] = JV(tree->is_paused());
    r["node_count"] = JV(static_cast<int64_t>(tree->get_node_count()));
    godot::Node *current = tree->get_current_scene();
    if (current) {
      godot::String scene_path = current->get_scene_file_path();
      if (scene_path.is_empty())
        scene_path = godot::String(current->get_name());
      r["scene"] = JV(util::to_std(scene_path));
    } else {
      r["scene"] = JV("");
    }
  }
  return ok_result(std::move(r));
}

JV op_ping() {
  JV r(JV::object_tag);
  auto *engine = godot::Engine::get_singleton();
  if (engine) {
    r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
    r["process_frame"] = JV(static_cast<int64_t>(engine->get_process_frames()));
  }
  append_activity_fields(r);
  return r;
}

JV op_cancel(const JV &params) {
  auto *rid_p = params.Find(GDA_FIELD_REQUEST_ID);
  if (!rid_p || !rid_p->IsInt()) {
    return error_result("cancel requires request_id (integer)");
  }
  int64_t target = rid_p->GetInt();
  JV r(JV::object_tag);
  auto it = g_cancel_handlers.find(target);
  if (it == g_cancel_handlers.end()) {
    r[GDA_FIELD_CANCELLED] = JV(false);
    r["reason"] =
        JV("no pending operation for request_id " + std::to_string(target));
    return r;
  }

  std::function<void()> handler = std::move(it->second);
  g_cancel_handlers.erase(it);
  handler();
  r[GDA_FIELD_CANCELLED] = JV(true);
  return r;
}

JV op_capture(int64_t request_id) {
  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Window *root = tree->get_root();
  if (!root)
    return error_result("no root window");
  godot::Ref<godot::Image> image = root->get_texture()->get_image();
  if (image.is_null() || image->is_empty()) {
    return error_result("failed to read viewport texture");
  }
  std::string path = util::to_std(godot::OS::get_singleton()->get_cache_dir()) +
                     "/gda_capture_" + std::to_string(request_id) + ".png";
  godot::Error save_err = image->save_png(godot::String(path.c_str()));
  if (save_err != godot::OK) {
    return error_result("save_png failed (ERR code " +
                        std::to_string(static_cast<int>(save_err)) + ") at " +
                        path);
  }
  JV r(JV::object_tag);
  r["path"] = JV(path);
  r["width"] = JV(static_cast<int64_t>(image->get_width()));
  r["height"] = JV(static_cast<int64_t>(image->get_height()));
  return ok_result(std::move(r));
}

JV op_get_errors(const JV &params) {
  int64_t limit = 50;
  if (auto *l = params.Find("limit")) {
    if (l->IsInt() && l->GetInt() > 0)
      limit = l->GetInt();
  }
  JV arr(JV::array_tag);
  {
    std::lock_guard<std::mutex> lock(g_buffer_mtx);
    size_t start = (static_cast<size_t>(limit) >= g_error_buffer.size())
                       ? 0
                       : g_error_buffer.size() - static_cast<size_t>(limit);
    for (size_t i = start; i < g_error_buffer.size(); i++) {
      const GameErrorEntry &e = g_error_buffer[i];
      JV item(JV::object_tag);
      char time_buf[16];
      snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d", e.hr, e.min,
               e.sec, e.msec);
      item["time"] = JV(std::string(time_buf));
      item["file"] = JV(e.file);
      item["func"] = JV(e.func);
      item["line"] = JV(static_cast<int64_t>(e.line));
      item["error"] = JV(e.error);
      item["descr"] = JV(e.descr);
      item["is_warning"] = JV(e.is_warning);
      JV stack(JV::array_tag);
      for (const std::string &f : e.stack)
        stack.PushBack(JV(f));
      item["stack"] = std::move(stack);
      arr.PushBack(std::move(item));
    }
  }
  return ok_result(std::move(arr));
}

JV op_get_output(const JV &params) {
  int64_t limit = 200;
  if (auto *l = params.Find("limit")) {
    if (l->IsInt() && l->GetInt() > 0)
      limit = l->GetInt();
  }
  JV arr(JV::array_tag);
  {
    std::lock_guard<std::mutex> lock(g_buffer_mtx);
    size_t start = (static_cast<size_t>(limit) >= g_output_buffer.size())
                       ? 0
                       : g_output_buffer.size() - static_cast<size_t>(limit);
    for (size_t i = start; i < g_output_buffer.size(); i++) {
      JV item(JV::object_tag);
      item["type"] = JV(static_cast<int64_t>(g_output_buffer[i].type));
      item["text"] = JV(g_output_buffer[i].text);
      arr.PushBack(std::move(item));
    }
  }
  return ok_result(std::move(arr));
}

constexpr int MAX_TREE_DEPTH = 64;
constexpr int64_t MAX_TREE_NODES = 2000;

struct TreeWalkState {
  std::string out;
  int64_t count = 0;
  bool truncated = false;
};

void walk_tree(godot::Node *node, int depth, TreeWalkState &state) {
  if (state.truncated)
    return;
  if (depth > MAX_TREE_DEPTH) {
    state.out += std::string(static_cast<size_t>(depth) * 2, ' ') +
                 "(max depth " + std::to_string(MAX_TREE_DEPTH) + ")\n";
    return;
  }
  if (state.count >= MAX_TREE_NODES) {
    state.truncated = true;
    return;
  }
  state.out += std::string(static_cast<size_t>(depth) * 2, ' ') +
               util::to_std(godot::String(node->get_name())) + " (" +
               util::to_std(godot::String(node->get_class())) + ")\n";
  state.count++;
  int64_t child_count = node->get_child_count();
  for (int64_t i = 0; i < child_count; i++) {
    walk_tree(node->get_child(i), depth + 1, state);
  }
}

JV op_get_tree() {
  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Node *root = tree->get_root();
  if (!root)
    return error_result("no root node");
  TreeWalkState state;
  walk_tree(root, 0, state);
  if (state.truncated) {
    state.out +=
        "(tree truncated at " + std::to_string(MAX_TREE_NODES) + " nodes)\n";
  }
  return ok_result(JV(state.out));
}

class GameBridgeListener : public godot::RefCounted {
  GDCLASS(GameBridgeListener, godot::RefCounted)

protected:
  static void _bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("on_gda_message"),
                                &GameBridgeListener::on_gda_message);
  }

public:
  bool on_gda_message(const godot::String &p_message,
                      const godot::Array &p_data) {
    (void)p_message;
    g_last_activity_ms = godot::Time::get_singleton()
                             ? godot::Time::get_singleton()->get_ticks_msec()
                             : 0;
    if (p_data.size() < 1)
      return true;
    std::string req_str = util::to_std(godot::String(p_data[0]));
    JV request = JV::Parse(req_str);
    if (!request.IsObject()) {
      push_game_error("game_bridge.cpp", "on_gda_message", 0,
                      "malformed gda request", req_str.substr(0, 200), false,
                      {});
      push_game_output("malformed gda request: " + req_str.substr(0, 200), 1);
      return true;
    }

    int64_t request_id = 0;
    if (auto *rid = request.Find(GDA_FIELD_REQUEST_ID)) {
      if (rid->IsInt())
        request_id = rid->GetInt();
    }
    std::string op;
    if (auto *op_p = request.Find(GDA_FIELD_OP)) {
      if (op_p->IsString())
        op = op_p->GetString();
    }
    JV params(JV::object_tag);
    if (auto *params_p = request.Find(GDA_FIELD_PARAMS)) {
      if (params_p->IsObject())
        params = *params_p;
    }

    JV body;
    if (op == GDA_OP_STATUS)
      body = op_status();
    else if (op == GDA_OP_PING)
      body = op_ping();
    else if (op == GDA_OP_CANCEL)
      body = op_cancel(params);
    else if (op == GDA_OP_EVAL)
      body = op_eval(params, request_id);
    else if (op == GDA_OP_INPUT)
      body = op_input(params, request_id);
    else if (op == GDA_OP_INPUT_WAIT)
      body = op_input_wait(params, request_id);
    else if (op == GDA_OP_INPUT_STATUS)
      body = op_input_status(params);
    else if (op == GDA_OP_CAPTURE)
      body = op_capture(request_id);
    else if (op == GDA_OP_GET_ERRORS)
      body = op_get_errors(params);
    else if (op == GDA_OP_GET_OUTPUT)
      body = op_get_output(params);
    else if (op == GDA_OP_GET_TREE)
      body = op_get_tree();
    else
      body = error_result("unknown op: " + op);

    if (body.Contains("error")) {
      std::string err_text = body["error"].GetString();
      push_game_error("game_bridge.cpp", "on_gda_message", 0,
                      "op '" + op + "' failed: " + err_text, "", false, {});
      push_game_output("op " + op + " error: " + err_text, 1);
    }

    if (!body.IsNull()) {
      body["ok"] = JV(!body.Contains("error"));
      send_response(request_id, std::move(body));
    }
    return true;
  }
};

godot::Ref<GameBridgeListener> g_listener;
godot::Ref<GameBridgeLogger> g_logger;
bool g_registered = false;

} // namespace

void send_response(int64_t request_id, JV body) {
  body["request_id"] = JV(request_id);
  godot::Array payload;
  payload.push_back(godot::String(body.Dump().c_str()));
  if (auto *dbg = godot::EngineDebugger::get_singleton()) {
    dbg->send_message(gda_string(GDA_MSG_RESPONSE), payload);
  }
}

void register_listener() {
  if (g_registered)
    return;
  static bool class_registered = false;
  if (!class_registered) {
    godot::ClassDB::register_class<GameBridgeListener>();
    register_eval_bridge_classes();
    register_input_bridge_classes();
    godot::ClassDB::register_class<GameBridgeLogger>();
    class_registered = true;
  }
  if (g_listener.is_null()) {
    g_listener.instantiate();
  }
  if (g_logger.is_null()) {
    g_logger.instantiate();
  }
  if (auto *dbg = godot::EngineDebugger::get_singleton()) {
    dbg->register_message_capture(
        godot::StringName(gda_string(GDA_PREFIX)),
        godot::Callable(g_listener.ptr(), godot::StringName("on_gda_message")));
    if (auto *os = godot::OS::get_singleton()) {
      os->add_logger(g_logger);
    }
    g_registered = true;

    {
      JV body(JV::object_tag);
      body[GDA_FIELD_READY] = JV(true);
      append_activity_fields(body);
      godot::Array payload;
      payload.push_back(godot::String(body.Dump().c_str()));
      dbg->send_message(gda_string(GDA_MSG_READY), payload);
    }
    LogSystem::instance().log(
        LogLevel::Info, LogCategory::System,
        "Game bridge listener registered (gda message capture + game logger)");
  }
}

void unregister_listener() {
  if (!g_registered)
    return;
  if (auto *dbg = godot::EngineDebugger::get_singleton()) {
    dbg->unregister_message_capture(godot::StringName(gda_string(GDA_PREFIX)));
  }
  if (auto *os = godot::OS::get_singleton()) {
    os->remove_logger(g_logger);
  }
  g_logger.unref();
  g_listener.unref();
  g_registered = false;
  LogSystem::instance().log(LogLevel::Info, LogCategory::System,
                            "Game bridge listener unregistered");
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_autopilot
