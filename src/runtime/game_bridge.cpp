#include "game_bridge.hpp"

#include "core/config.hpp"
#include "core/editor_coords.hpp"
#include "core/log_system.hpp"
#include "gda_protocol.hpp"
#include "tools/authorization.hpp"
#include "tools/capture_ops.hpp"
#include "tools/editor_ui_ops.hpp"
#include "tools/tool_base.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/control.hpp>
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
#include <godot_cpp/variant/rect2i.hpp>
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

double json_double(const JV &value) {
  return value.IsInt() ? static_cast<double>(value.GetInt())
                       : value.GetDouble();
}

JV capture_error(const std::string &code, const std::string &message) {
  JV error = error_result(message);
  JV details(JV::object_tag);
  details["code"] = JV(code);
  error["structured_error"] = std::move(details);
  return error;
}

constexpr int64_t kCaptureMaxDimensionMin = 64;
constexpr int64_t kCaptureMaxDimensionMax = 4096;

constexpr int MAX_TREE_DEPTH = 64;

constexpr int64_t GDA_UI_ELEMENTS_DEFAULT = 100;
constexpr int64_t GDA_UI_ELEMENTS_MAX = 1000;

struct UiElement {
  std::string path;
  std::string type;
  std::string text;
  bool has_text = false;
  bool visible = true;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
};

void walk_ui_elements(godot::Node *node, int depth, int64_t max_elements,
                      std::vector<UiElement> &elements, bool &truncated) {
  if (truncated || depth > MAX_TREE_DEPTH)
    return;
  if (auto *ctrl = godot::Object::cast_to<godot::Control>(node)) {
    if (static_cast<int64_t>(elements.size()) >= max_elements) {
      truncated = true;
      return;
    }
    UiElement element;
    element.path = util::to_std(godot::String(node->get_path()));
    element.type = util::to_std(godot::String(node->get_class()));
    element.visible = ctrl->is_visible_in_tree();
    godot::Variant text_value = ctrl->get("text");
    if (text_value.get_type() == godot::Variant::STRING) {
      element.has_text = true;
      element.text = util::to_std(static_cast<godot::String>(text_value));
    }
    godot::Rect2 rect = ctrl->get_global_rect();
    element.x = rect.position.x;
    element.y = rect.position.y;
    element.w = rect.size.x;
    element.h = rect.size.y;
    elements.push_back(std::move(element));
  }
  int64_t child_count = node->get_child_count();
  for (int64_t i = 0; i < child_count; i++)
    walk_ui_elements(node->get_child(i), depth + 1, max_elements, elements,
                     truncated);
}

std::vector<UiElement> collect_ui_elements(godot::Node *root,
                                           int64_t max_elements,
                                           bool &truncated) {
  std::vector<UiElement> elements;
  truncated = false;
  if (!root)
    return elements;
  walk_ui_elements(root, 0, max_elements, elements, truncated);
  return elements;
}

const UiElement *find_ui_element(const std::vector<UiElement> &elements,
                                 const std::string &path) {
  for (const UiElement &element : elements) {
    if (element.path == path)
      return &element;
  }
  const std::string suffix = "/" + path;
  for (const UiElement &element : elements) {
    if (element.path.size() >= suffix.size() &&
        element.path.compare(element.path.size() - suffix.size(),
                             suffix.size(), suffix) == 0)
      return &element;
  }
  return nullptr;
}

JV op_capture(const JV &params, int64_t request_id) {
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
  const int source_width = image->get_width();
  const int source_height = image->get_height();

  bool has_region = false;
  double region_x = 0.0;
  double region_y = 0.0;
  double region_w = 0.0;
  double region_h = 0.0;
  if (auto *region_p = params.Find("region")) {
    if (!region_p->IsObject())
      return error_result("capture region must be an object with numeric x, y, "
                          "width and height");
    const JV *rx = region_p->Find("x");
    const JV *ry = region_p->Find("y");
    const JV *rw = region_p->Find("width");
    const JV *rh = region_p->Find("height");
    if (!rx || !ry || !rw || !rh || !rx->IsNumber() || !ry->IsNumber() ||
        !rw->IsNumber() || !rh->IsNumber())
      return error_result("capture region must be an object with numeric x, y, "
                          "width and height");
    region_x = json_double(*rx);
    region_y = json_double(*ry);
    region_w = json_double(*rw);
    region_h = json_double(*rh);
    if (region_w <= 0.0 || region_h <= 0.0)
      return error_result(
          "capture region width and height must be greater than 0");
    has_region = true;
  }

  int64_t max_dimension = 0;
  if (auto *md_p = params.Find("max_dimension")) {
    if (!md_p->IsInt())
      return error_result("capture max_dimension must be an integer");
    max_dimension = md_p->GetInt();
    if (max_dimension < kCaptureMaxDimensionMin ||
        max_dimension > kCaptureMaxDimensionMax)
      return error_result("capture max_dimension out of range (64-4096): " +
                          std::to_string(max_dimension));
  }

  bool annotate = false;
  if (auto *annotate_p = params.Find("annotate")) {
    if (!annotate_p->IsBool())
      return error_result("capture annotate must be a boolean");
    annotate = annotate_p->GetBool();
  }

  int applied_region_x = 0;
  int applied_region_y = 0;
  int applied_region_w = 0;
  int applied_region_h = 0;
  if (has_region) {
    const double x0 = std::max(0.0, std::floor(region_x));
    const double y0 = std::max(0.0, std::floor(region_y));
    const double x1 = std::min(static_cast<double>(source_width),
                               std::floor(region_x + region_w));
    const double y1 = std::min(static_cast<double>(source_height),
                               std::floor(region_y + region_h));
    if (x1 - x0 < 1.0 || y1 - y0 < 1.0) {
      return capture_error(
          "region_out_of_bounds",
          "region does not intersect the " + std::to_string(source_width) +
              "x" + std::to_string(source_height) + " captured image");
    }
    applied_region_x = static_cast<int>(x0);
    applied_region_y = static_cast<int>(y0);
    applied_region_w = static_cast<int>(x1 - x0);
    applied_region_h = static_cast<int>(y1 - y0);
    image = image->get_region(godot::Rect2i(applied_region_x, applied_region_y,
                                            applied_region_w,
                                            applied_region_h));
  }

  int final_width = image->get_width();
  int final_height = image->get_height();
  const int cropped_width = final_width;
  const int cropped_height = final_height;
  if (max_dimension > 0) {
    const coords::ImageSize fitted = coords::fit_within(
        coords::ImageSize{final_width, final_height},
        static_cast<int>(max_dimension));
    if (fitted.width != final_width || fitted.height != final_height) {
      image->resize(fitted.width, fitted.height,
                    godot::Image::INTERPOLATE_BILINEAR);
      final_width = fitted.width;
      final_height = fitted.height;
    }
  }
  if (final_width > GDA_CAPTURE_MAX_DIMENSION ||
      final_height > GDA_CAPTURE_MAX_DIMENSION) {
    return capture_error(
        "capture_dimensions_exceeded",
        "viewport dimensions exceed the capture limit of " +
            std::to_string(GDA_CAPTURE_MAX_DIMENSION) + " pixels per side");
  }

  JV annotated_elements(JV::array_tag);
  if (annotate) {
    bool truncated = false;
    const std::vector<UiElement> elements =
        collect_ui_elements(root, GDA_UI_ELEMENTS_MAX, truncated);
    const double scale_x =
        cropped_width > 0 ? static_cast<double>(final_width) / cropped_width
                          : 1.0;
    const double scale_y =
        cropped_height > 0 ? static_cast<double>(final_height) / cropped_height
                           : 1.0;
    const double offset_x = static_cast<double>(applied_region_x);
    const double offset_y = static_cast<double>(applied_region_y);
    std::vector<editor_ui_ops::MarkRect> marks;
    marks.reserve(elements.size());
    int64_t id = 1;
    for (const UiElement &element : elements) {
      const double px = (element.x - offset_x) * scale_x;
      const double py = (element.y - offset_y) * scale_y;
      const double pw = element.w * scale_x;
      const double ph = element.h * scale_y;
      editor_ui_ops::MarkRect mark;
      mark.id = static_cast<int>(id);
      mark.x = px;
      mark.y = py;
      mark.w = pw;
      mark.h = ph;
      marks.push_back(mark);
      JV item(JV::object_tag);
      item["id"] = JV(id);
      item["path"] = JV(element.path);
      item["type"] = JV(element.type);
      if (!element.text.empty())
        item["text"] = JV(element.text);
      JV position(JV::object_tag);
      position["x"] = JV(px);
      position["y"] = JV(py);
      JV size(JV::object_tag);
      size["x"] = JV(pw);
      size["y"] = JV(ph);
      item["position"] = std::move(position);
      item["size"] = std::move(size);
      annotated_elements.PushBack(std::move(item));
      id++;
    }
    editor_ui_ops::draw_annotations(image, marks);
  }

  std::string path = util::to_std(godot::OS::get_singleton()->get_cache_dir()) +
                     "/gda_capture_" + std::to_string(request_id) + ".png";
  godot::Error save_err = image->save_png(godot::String(path.c_str()));
  if (save_err != godot::OK) {
    return error_result("save_png failed (ERR code " +
                        std::to_string(static_cast<int>(save_err)) + ") at " +
                        path);
  }
  capture_ops::prune_capture_files(godot::OS::get_singleton()->get_cache_dir(),
                                   20);
  JV r(JV::object_tag);
  r["path"] = JV(path);
  r["width"] = JV(static_cast<int64_t>(final_width));
  r["height"] = JV(static_cast<int64_t>(final_height));
  if (has_region) {
    JV applied(JV::object_tag);
    applied["x"] = JV(static_cast<int64_t>(applied_region_x));
    applied["y"] = JV(static_cast<int64_t>(applied_region_y));
    applied["width"] = JV(static_cast<int64_t>(applied_region_w));
    applied["height"] = JV(static_cast<int64_t>(applied_region_h));
    r["region"] = std::move(applied);
  }
  if (annotate) {
    r["annotated"] = JV(true);
    r["elements"] = std::move(annotated_elements);
  }
  if (final_width != source_width || final_height != source_height) {
    r["source_width"] = JV(static_cast<int64_t>(source_width));
    r["source_height"] = JV(static_cast<int64_t>(source_height));
  }
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

JV run_ui_click(const JV &click, const std::vector<UiElement> &elements,
                bool truncated) {
  if (!click.IsObject())
    return error_result("ui_elements click must be an object");
  auto *path_p = click.Find("path");
  if (!path_p || !path_p->IsString() || path_p->GetString().empty())
    return error_result("ui_elements click requires path (non-empty string)");
  const std::string path = path_p->GetString();
  int64_t button_index = 1;
  if (auto *button_p = click.Find("button_index")) {
    if (!button_p->IsInt() || button_p->GetInt() < 1 ||
        button_p->GetInt() > 3)
      return error_result(
          "ui_elements click button_index must be an integer between 1 and 3");
    button_index = button_p->GetInt();
  }
  bool double_click = false;
  if (auto *double_p = click.Find("double_click")) {
    if (!double_p->IsBool())
      return error_result("ui_elements click double_click must be a boolean");
    double_click = double_p->GetBool();
  }
  const UiElement *match = find_ui_element(elements, path);
  if (!match) {
    std::string message = "click target not found: " + path + " (enumerated " +
                          std::to_string(elements.size()) + " elements";
    if (truncated)
      message += ", truncated at the max_elements cap";
    message += "; candidates:";
    const size_t preview = std::min<size_t>(elements.size(), 10);
    if (preview == 0)
      message += " none";
    for (size_t i = 0; i < preview; i++)
      message += " " + elements[i].path;
    if (elements.size() > preview)
      message += " ...";
    message += ")";
    return error_result(message);
  }
  const double center_x = match->x + match->w * 0.5;
  const double center_y = match->y + match->h * 0.5;
  if (!authorization::capability_enabled("game_runtime"))
    return authorization::deny_if_unauthorized("click_game_ui_element",
                                               SideEffect::GameRuntime);
  const int rounds = double_click ? 2 : 1;
  const bool states[2] = {true, false};
  for (int round = 0; round < rounds; round++) {
    for (bool pressed : states) {
      JV press(JV::object_tag);
      press["type"] = JV("mouse_button");
      press["button_index"] = JV(button_index);
      JV position(JV::object_tag);
      position["x"] = JV(center_x);
      position["y"] = JV(center_y);
      press["position"] = std::move(position);
      press["mode"] = JV("event");
      press["pressed"] = JV(pressed);
      JV injected = op_input(press, 0);
      if (injected.Contains("error"))
        return injected;
    }
  }
  JV clicked(JV::object_tag);
  clicked["ok"] = JV(true);
  clicked["path"] = JV(match->path);
  JV position(JV::object_tag);
  position["x"] = JV(center_x);
  position["y"] = JV(center_y);
  clicked["position"] = std::move(position);
  clicked["clicks"] = JV(static_cast<int64_t>(rounds));
  JV inner(JV::object_tag);
  inner["result"] = std::move(clicked);
  return ok_result(std::move(inner));
}

JV op_ui_elements(const JV &params) {
  int64_t max_elements = GDA_UI_ELEMENTS_DEFAULT;
  if (auto *m = params.Find("max_elements")) {
    if (m->IsInt() && m->GetInt() > 0)
      max_elements = m->GetInt();
  }
  if (max_elements > GDA_UI_ELEMENTS_MAX)
    max_elements = GDA_UI_ELEMENTS_MAX;
  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Node *root = tree->get_root();
  if (!root)
    return error_result("no root node");
  bool truncated = false;
  const std::vector<UiElement> elements =
      collect_ui_elements(root, max_elements, truncated);
  if (auto *click_p = params.Find("click"))
    return run_ui_click(*click_p, elements, truncated);
  JV arr(JV::array_tag);
  for (const UiElement &element : elements) {
    JV item(JV::object_tag);
    item["path"] = JV(element.path);
    item["type"] = JV(element.type);
    item["visible"] = JV(element.visible);
    if (element.has_text)
      item["text"] = JV(element.text);
    JV position(JV::object_tag);
    position["x"] = JV(element.x);
    position["y"] = JV(element.y);
    JV size(JV::object_tag);
    size["x"] = JV(element.w);
    size["y"] = JV(element.h);
    JV rect_json(JV::object_tag);
    rect_json["position"] = std::move(position);
    rect_json["size"] = std::move(size);
    item["global_rect"] = std::move(rect_json);
    arr.PushBack(std::move(item));
  }
  JV r(JV::object_tag);
  r["elements"] = std::move(arr);
  r["count"] = JV(static_cast<int64_t>(elements.size()));
  r["truncated"] = JV(truncated);
  return ok_result(std::move(r));
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
    if (op == GDA_OP_STATUS) {
      body = op_status();
    } else if (op == GDA_OP_PING) {
      body = op_ping();
    } else if (op == GDA_OP_CANCEL) {
      body = op_cancel(params);
    } else if (op == GDA_OP_EVAL || op == GDA_OP_INPUT ||
               op == GDA_OP_INPUT_WAIT || op == GDA_OP_INPUT_SEQUENCE) {
      if (!authorization::capability_enabled("game_runtime")) {
        body = authorization::deny_if_unauthorized(
            "game_runtime", SideEffect::GameRuntime);
      } else if (op == GDA_OP_EVAL) {
        body = op_eval(params, request_id);
      } else if (op == GDA_OP_INPUT) {
        body = op_input(params, request_id);
      } else if (op == GDA_OP_INPUT_WAIT) {
        body = op_input_wait(params, request_id);
      } else {
        body = op_input_sequence(params, request_id);
      }
    } else if (op == GDA_OP_INPUT_STATUS) {
      body = op_input_status(params);
    } else if (op == GDA_OP_UI_ELEMENTS) {
      body = op_ui_elements(params);
    } else if (op == GDA_OP_CAPTURE) {
      body = op_capture(params, request_id);
    } else if (op == GDA_OP_GET_ERRORS) {
      body = op_get_errors(params);
    } else if (op == GDA_OP_GET_OUTPUT) {
      body = op_get_output(params);
    } else if (op == GDA_OP_GET_TREE) {
      body = op_get_tree();
    } else {
      body = error_result("unknown op: " + op);
    }

    if (body.Contains("error")) {
      std::string err_text = body["error"].GetString();
      push_game_error("game_bridge.cpp", "on_gda_message", 0,
                      "op '" + op + "' failed: " + err_text, "", false, {});
      push_game_output("op " + op + " error: " + err_text, 1);
    }

    if (!body.IsNull()) {
      body[GDA_FIELD_OK] = JV(!body.Contains("error"));
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
  body[GDA_FIELD_REQUEST_ID] = JV(request_id);
  std::string serialized = body.Dump();
  if (serialized.size() > GDA_MAX_JSON_RESPONSE_BYTES) {
    body = error_result("JSON response exceeds the response limit of " +
                        std::to_string(GDA_MAX_JSON_RESPONSE_BYTES) +
                        " bytes");
    JV details(JV::object_tag);
    details["code"] = JV("response_too_large");
    details["limit_bytes"] = JV(static_cast<int64_t>(GDA_MAX_JSON_RESPONSE_BYTES));
    details["actual_bytes"] = JV(static_cast<int64_t>(serialized.size()));
    body["structured_error"] = std::move(details);
    body[GDA_FIELD_REQUEST_ID] = JV(request_id);
    serialized = body.Dump();
  }
  godot::Array payload;
  payload.push_back(godot::String(serialized.c_str()));
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
    const godot::StringName capture_name(gda_string(GDA_PREFIX));
    if (dbg->has_capture(capture_name)) {
      LogSystem::instance().log(
          LogLevel::Error, LogCategory::System,
          "game bridge: failed to register 'gda' message capture");
      return;
    }
    dbg->register_message_capture(
        capture_name,
        godot::Callable(g_listener.ptr(), godot::StringName("on_gda_message")));
    if (!dbg->has_capture(capture_name)) {
      LogSystem::instance().log(
          LogLevel::Error, LogCategory::System,
          "game bridge: failed to register 'gda' message capture");
      return;
    }
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
