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
#include <godot_cpp/classes/expression.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/node3d.hpp>
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
#include <godot_cpp/variant/color.hpp>
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

constexpr int64_t kCaptureScaleMin = 1;
constexpr int64_t kCaptureScaleMax = 8;

struct CaptureRequest {
  bool has_region = false;
  double region_x = 0.0;
  double region_y = 0.0;
  double region_w = 0.0;
  double region_h = 0.0;
  int64_t max_dimension = 0;
  int64_t scale = 1;
  bool annotate = false;
  std::vector<std::string> annotate_nodes;
  int64_t annotate_nodes_max = 50;
  int64_t after_frames = 0;
  std::string when;
  int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
};

JV parse_capture_request(const JV &params, CaptureRequest *out) {
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
    out->region_x = json_double(*rx);
    out->region_y = json_double(*ry);
    out->region_w = json_double(*rw);
    out->region_h = json_double(*rh);
    if (out->region_w <= 0.0 || out->region_h <= 0.0)
      return error_result(
          "capture region width and height must be greater than 0");
    out->has_region = true;
  }

  if (auto *md_p = params.Find("max_dimension")) {
    if (!md_p->IsInt())
      return error_result("capture max_dimension must be an integer");
    out->max_dimension = md_p->GetInt();
    if (out->max_dimension < kCaptureMaxDimensionMin ||
        out->max_dimension > kCaptureMaxDimensionMax)
      return error_result("capture max_dimension out of range (64-4096): " +
                          std::to_string(out->max_dimension));
  }

  if (auto *scale_p = params.Find("scale")) {
    if (!scale_p->IsInt() || scale_p->GetInt() < kCaptureScaleMin ||
        scale_p->GetInt() > kCaptureScaleMax)
      return error_result("scale must be an integer between 1 and 8");
    out->scale = scale_p->GetInt();
  }

  if (auto *annotate_p = params.Find("annotate")) {
    if (!annotate_p->IsBool())
      return error_result("capture annotate must be a boolean");
    out->annotate = annotate_p->GetBool();
  }

  if (auto *max_p = params.Find("annotate_nodes_max")) {
    if (!max_p->IsInt() || max_p->GetInt() < 1 || max_p->GetInt() > 50)
      return error_result(
          "capture annotate_nodes_max must be an integer between 1 and 50");
    out->annotate_nodes_max = max_p->GetInt();
  }

  if (auto *nodes_p = params.Find("annotate_nodes")) {
    if (!nodes_p->IsArray())
      return error_result("capture annotate_nodes must be an array of strings");
    const auto &items = nodes_p->GetArray();
    if (items.empty() || items.size() > 50)
      return error_result(
          "capture annotate_nodes must contain 1-50 paths");
    for (const auto &item : items) {
      if (!item.IsString() || item.GetString().empty())
        return error_result(
            "capture annotate_nodes must be an array of non-empty strings");
      out->annotate_nodes.push_back(item.GetString());
    }
    if (out->annotate_nodes.size() >
        static_cast<size_t>(out->annotate_nodes_max))
      return error_result("annotate_nodes exceeds annotate_nodes_max (" +
                          std::to_string(out->annotate_nodes.size()) + " > " +
                          std::to_string(out->annotate_nodes_max) + ")");
  }

  if (auto *after_p = params.Find("after_frames")) {
    if (!after_p->IsInt() || after_p->GetInt() < 0)
      return error_result(
          "after_frames must be an integer greater than or equal to 0");
    out->after_frames = after_p->GetInt();
  }

  if (auto *when_p = params.Find("when")) {
    if (!when_p->IsString())
      return error_result("when must be a string");
    out->when = when_p->GetString();
  }

  if (auto *timeout_p = params.Find("timeout_ms")) {
    if (!timeout_p->IsInt() || timeout_p->GetInt() <= 0)
      return error_result("capture timeout_ms must be a positive integer");
    out->timeout_ms = timeout_p->GetInt();
    if (out->timeout_ms > GDA_MAX_TIMEOUT_MS)
      out->timeout_ms = GDA_MAX_TIMEOUT_MS;
  }
  return JV();
}

namespace {

constexpr size_t kNodeMarkBudget = 200;

void fill_rect_clamped_blue(const godot::Ref<godot::Image> &image, int image_w,
                                int image_h, int x, int y, int w, int h,
                                const godot::Color &color) {
  const int x0 = std::max(0, std::min(x, image_w));
  const int y0 = std::max(0, std::min(y, image_h));
  const int x1 = std::max(0, std::min(x + w, image_w));
  const int y1 = std::max(0, std::min(y + h, image_h));
  if (x1 <= x0 || y1 <= y0)
    return;
  image->fill_rect(godot::Rect2i(x0, y0, x1 - x0, y1 - y0), color);
}

void draw_blue_annotations(const godot::Ref<godot::Image> &image,
                           const std::vector<editor_ui_ops::MarkRect> &marks) {
  if (image.is_null() || marks.empty())
    return;
  const godot::Image::Format format = image->get_format();
  if (format != godot::Image::FORMAT_RGBA8 &&
      format != godot::Image::FORMAT_RGB8)
    return;
  const int image_w = image->get_width();
  const int image_h = image->get_height();
  if (image_w <= 0 || image_h <= 0)
    return;
  static const uint8_t kDigits[10][5] = {
      {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7},
      {0x7, 0x1, 0x7, 0x4, 0x7}, {0x7, 0x1, 0x7, 0x1, 0x7},
      {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
      {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x2, 0x2, 0x2},
      {0x7, 0x5, 0x7, 0x5, 0x7}, {0x7, 0x5, 0x7, 0x1, 0x7},
  };
  const godot::Color box_color(0.25f, 0.55f, 1.0f, 1.0f);
  const godot::Color label_bg(0.05f, 0.1f, 0.25f, 1.0f);
  const godot::Color digit_color(1.0f, 1.0f, 1.0f, 1.0f);
  constexpr int kScale = 2;
  for (const auto &mark : marks) {
    const int rx = static_cast<int>(std::lround(mark.x));
    const int ry = static_cast<int>(std::lround(mark.y));
    int rw = static_cast<int>(std::lround(mark.w));
    int rh = static_cast<int>(std::lround(mark.h));
    if (rw <= 0 || rh <= 0) {
      continue;
    }
    fill_rect_clamped_blue(image, image_w, image_h, rx, ry, rw, 1, box_color);
    fill_rect_clamped_blue(image, image_w, image_h, rx, ry + rh - 1, rw, 1,
                           box_color);
    fill_rect_clamped_blue(image, image_w, image_h, rx, ry, 1, rh, box_color);
    fill_rect_clamped_blue(image, image_w, image_h, rx + rw - 1, ry, 1, rh,
                           box_color);
    const std::string label = std::to_string(mark.id);
    const int digits = static_cast<int>(label.size());
    if (digits == 0)
      continue;
    const int label_x = rx + 2;
    const int label_y = ry + 2;
    fill_rect_clamped_blue(image, image_w, image_h, label_x, label_y,
                           digits * 4 * kScale + 2, 5 * kScale + 2, label_bg);
    for (int d = 0; d < digits; ++d) {
      const char ch = label[static_cast<size_t>(d)];
      if (ch < '0' || ch > '9')
        continue;
      const int digit = ch - '0';
      const int ox = label_x + 1 + d * 4 * kScale;
      const int oy = label_y + 1;
      for (int row = 0; row < 5; ++row) {
        const uint8_t bits = kDigits[digit][row];
        for (int col = 0; col < 3; ++col) {
          if ((bits & (1u << (2 - col))) == 0)
            continue;
          for (int sy = 0; sy < kScale; ++sy)
            for (int sx = 0; sx < kScale; ++sx) {
              const int px = ox + col * kScale + sx;
              const int py = oy + row * kScale + sy;
              if (px < 0 || px >= image_w || py < 0 || py >= image_h)
                continue;
              image->set_pixel(px, py, digit_color);
            }
        }
      }
    }
  }
}

void collect_game_paths(godot::Node *node, std::vector<std::string> &out,
                        size_t cap) {
  if (!node || out.size() >= cap)
    return;
  out.push_back(util::to_std(godot::String(node->get_path())));
  const int64_t n = node->get_child_count();
  for (int64_t i = 0; i < n && out.size() < cap; ++i)
    collect_game_paths(node->get_child(i), out, cap);
}

godot::Node *resolve_game_node_suffixed(godot::Node *root,
                                        const std::string &path) {
  if (!root)
    return nullptr;
  if (godot::Node *direct = resolve_node(path))
    return direct;
  const std::string suffix = "/" + path;
  std::vector<godot::Node *> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    godot::Node *node = stack.back();
    stack.pop_back();
    const std::string node_path =
        util::to_std(godot::String(node->get_path()));
    if (node_path == path)
      return node;
    if (node_path.size() >= suffix.size() &&
        node_path.compare(node_path.size() - suffix.size(), suffix.size(),
                          suffix) == 0)
      return node;
    const int64_t n = node->get_child_count();
    for (int64_t i = 0; i < n; ++i)
      if (godot::Node *child = node->get_child(i))
        stack.push_back(child);
  }
  return nullptr;
}

struct GameNodeRect {
  bool ok = false;
  std::string type;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  bool visible = true;
  bool behind = false;
  bool drawable = false;
  std::string error;
};

GameNodeRect game_node_viewport_rect(godot::Node *node,
                                     godot::Viewport *viewport,
                                     godot::Camera3D *camera) {
  GameNodeRect out;
  if (!node || !viewport) {
    out.error = "viewport incompatible";
    return out;
  }
  out.type = util::to_std(godot::String(node->get_class()));
  if (auto *ctrl = godot::Object::cast_to<godot::Control>(node)) {
    const godot::Rect2 rect = ctrl->get_global_rect();
    out.x = rect.position.x;
    out.y = rect.position.y;
    out.w = rect.size.x;
    out.h = rect.size.y;
    out.visible = ctrl->is_visible_in_tree();
    out.ok = true;
    out.drawable = out.visible && out.w > 0.0 && out.h > 0.0;
    return out;
  }
  if (auto *item = godot::Object::cast_to<godot::CanvasItem>(node)) {
    out.visible = item->is_visible_in_tree();
    const godot::Transform2D canvas = viewport->get_canvas_transform();
    const godot::Vector2 vp = canvas.xform(item->get_global_transform().get_origin());
    out.x = vp.x;
    out.y = vp.y;
    out.w = 0.0;
    out.h = 0.0;
    out.ok = true;
    out.drawable = false;
    return out;
  }
  if (auto *node_3d = godot::Object::cast_to<godot::Node3D>(node)) {
    (void)node_3d;
    if (!camera) {
      out.error = "3D camera not available";
      return out;
    }
    const godot::Vector3 world = node_3d->get_global_transform().get_origin();
    const godot::Vector2 pos = camera->unproject_position(world);
    out.x = pos.x;
    out.y = pos.y;
    out.w = 0.0;
    out.h = 0.0;
    out.visible = true;
    out.behind = camera->is_position_behind(world);
    out.ok = true;
    out.drawable = false;
    return out;
  }
  out.error = "unsupported node type";
  return out;
}

}  // namespace

JV capture_viewport_now(const CaptureRequest &request, int64_t request_id) {
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

  const bool has_region = request.has_region;
  const double region_x = request.region_x;
  const double region_y = request.region_y;
  const double region_w = request.region_w;
  const double region_h = request.region_h;

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
  if (request.scale > 1) {
    const coords::ImageSize scaled = coords::scale_size(
        coords::ImageSize{final_width, final_height},
        static_cast<int>(request.scale));
    if (scaled.width <= 0 || scaled.height <= 0) {
      return capture_error(
          "capture_dimensions_exceeded",
          "scaled capture exceeds the image pixel limit (" +
              std::to_string(final_width) + "x" + std::to_string(final_height) +
              " x" + std::to_string(request.scale) + ")");
    }
    image->resize(scaled.width, scaled.height,
                  godot::Image::INTERPOLATE_NEAREST);
    final_width = scaled.width;
    final_height = scaled.height;
  }
  if (request.max_dimension > 0) {
    const coords::ImageSize fitted = coords::fit_within(
        coords::ImageSize{final_width, final_height},
        static_cast<int>(request.max_dimension));
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
  if (request.annotate) {
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

  JV node_elements(JV::array_tag);
  bool node_truncated = false;
  if (!request.annotate_nodes.empty()) {
    godot::Viewport *viewport = root;
    godot::Camera3D *camera = viewport ? viewport->get_camera_3d() : nullptr;
    const double scale_x =
        cropped_width > 0 ? static_cast<double>(final_width) / cropped_width
                          : 1.0;
    const double scale_y =
        cropped_height > 0 ? static_cast<double>(final_height) / cropped_height
                           : 1.0;
    const double offset_x = static_cast<double>(applied_region_x);
    const double offset_y = static_cast<double>(applied_region_y);
    const size_t ui_mark_count =
        request.annotate ? annotated_elements.GetArray().size() : 0;
    size_t node_budget = kNodeMarkBudget;
    if (ui_mark_count < kNodeMarkBudget)
      node_budget = kNodeMarkBudget - ui_mark_count;
    else
      node_budget = 0;
    std::vector<editor_ui_ops::MarkRect> node_marks;
    node_marks.reserve(request.annotate_nodes.size());
    int64_t node_id = 1;
    for (const std::string &node_path : request.annotate_nodes) {
      JV item(JV::object_tag);
      item["id"] = JV(node_id);
      item["path"] = JV(node_path);
      godot::Node *node = resolve_game_node_suffixed(root, node_path);
      if (!node) {
        item["ok"] = JV(false);
        item["error"] = JV("node not found");
        std::vector<std::string> candidates;
        collect_game_paths(tree->get_current_scene() ? tree->get_current_scene()
                                                     : root,
                           candidates, 20);
        JV arr(JV::array_tag);
        for (const std::string &c : candidates)
          arr.PushBack(JV(c));
        item["candidates"] = std::move(arr);
        node_elements.PushBack(std::move(item));
        ++node_id;
        continue;
      }
      GameNodeRect rect = game_node_viewport_rect(node, viewport, camera);
      if (!rect.ok) {
        item["ok"] = JV(false);
        item["type"] = JV(rect.type);
        item["error"] = JV(rect.error);
        node_elements.PushBack(std::move(item));
        ++node_id;
        continue;
      }
      const double px = (rect.x - offset_x) * scale_x;
      const double py = (rect.y - offset_y) * scale_y;
      const double pw = rect.w * scale_x;
      const double ph = rect.h * scale_y;
      item["ok"] = JV(true);
      item["type"] = JV(rect.type);
      item["visible"] = JV(rect.visible);
      if (rect.behind)
        item["behind"] = JV(true);
      JV position(JV::object_tag);
      position["x"] = JV(px);
      position["y"] = JV(py);
      JV size(JV::object_tag);
      size["x"] = JV(pw);
      size["y"] = JV(ph);
      item["position"] = std::move(position);
      item["size"] = std::move(size);
      node_elements.PushBack(std::move(item));
      if (rect.drawable && node_marks.size() < node_budget) {
        editor_ui_ops::MarkRect mark;
        mark.id = static_cast<int>(node_id);
        mark.x = px;
        mark.y = py;
        mark.w = pw;
        mark.h = ph;
        node_marks.push_back(mark);
      } else if (rect.drawable && node_marks.size() >= node_budget) {
        node_truncated = true;
      }
      ++node_id;
    }
    draw_blue_annotations(image, node_marks);
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
  if (request.annotate) {
    r["annotated"] = JV(true);
    r["elements"] = std::move(annotated_elements);
  }
  if (!request.annotate_nodes.empty()) {
    r["node_elements"] = std::move(node_elements);
    if (node_truncated)
      r["node_truncated"] = JV(true);
  }
  if (final_width != source_width || final_height != source_height) {
    r["source_width"] = JV(static_cast<int64_t>(source_width));
    r["source_height"] = JV(static_cast<int64_t>(source_height));
  }
  return ok_result(std::move(r));
}

class GameBridgeCaptureAwaiter : public godot::Node {
  GDCLASS(GameBridgeCaptureAwaiter, godot::Node)

  int64_t request_id_ = 0;
  CaptureRequest request_;
  godot::Ref<godot::Expression> when_expr_;
  uint64_t start_process_frames_ = 0;
  uint64_t start_ticks_ms_ = 0;
  int64_t frames_waited_ = 0;
  std::string when_error_;
  bool finished_ = false;

protected:
  static void _bind_methods() {}

public:
  void setup(int64_t request_id, CaptureRequest request,
             const godot::Ref<godot::Expression> &when_expr) {
    request_id_ = request_id;
    request_ = std::move(request);
    when_expr_ = when_expr;
    set_process_mode(godot::Node::PROCESS_MODE_ALWAYS);
    set_process(true);
    auto *engine = godot::Engine::get_singleton();
    start_process_frames_ = engine ? engine->get_process_frames() : 0;
    start_ticks_ms_ = godot::Time::get_singleton()
                          ? godot::Time::get_singleton()->get_ticks_msec()
                          : 0;
  }

  void cancel() {
    if (finished_)
      return;
    finished_ = true;
    unregister_cancel_handler(request_id_);
    queue_free();
  }

  void _process(double delta) override {
    (void)delta;
    if (finished_)
      return;
    auto *engine = godot::Engine::get_singleton();
    const uint64_t current_frame =
        engine ? engine->get_process_frames() : start_process_frames_;
    frames_waited_ = static_cast<int64_t>(current_frame - start_process_frames_);
    bool satisfied = frames_waited_ >= request_.after_frames;
    if (satisfied && when_expr_.is_valid()) {
      godot::Node *base = nullptr;
      if (godot::SceneTree *tree = get_scene_tree()) {
        base = tree->get_current_scene();
        if (!base)
          base = tree->get_root();
      }
      godot::Variant value = when_expr_->execute(godot::Array(), base, false);
      if (when_expr_->has_execute_failed()) {
        when_error_ = util::to_std(when_expr_->get_error_text());
        satisfied = false;
      } else {
        satisfied = value.booleanize();
      }
    }
    if (satisfied) {
      finish(capture_viewport_now(request_, request_id_));
      return;
    }
    const uint64_t now_ms = godot::Time::get_singleton()
                                ? godot::Time::get_singleton()->get_ticks_msec()
                                : 0;
    if (now_ms - start_ticks_ms_ >= static_cast<uint64_t>(request_.timeout_ms)) {
      std::string message =
          "capture condition not met within " +
          std::to_string(request_.timeout_ms) + " ms (" +
          std::to_string(frames_waited_) + " frames waited";
      if (request_.after_frames > 0)
        message += ", after_frames=" + std::to_string(request_.after_frames);
      if (!request_.when.empty())
        message += ", when=" + request_.when;
      if (!when_error_.empty())
        message += ", last expression error: " + when_error_;
      message += ")";
      JV body = error_result(message);
      JV details(JV::object_tag);
      details["code"] = JV("when_timeout");
      details["frames_waited"] = JV(frames_waited_);
      details["timeout_ms"] = JV(request_.timeout_ms);
      if (request_.after_frames > 0)
        details["after_frames"] = JV(request_.after_frames);
      if (!request_.when.empty())
        details["when"] = JV(request_.when);
      if (!when_error_.empty())
        details["when_error"] = JV(when_error_);
      body["structured_error"] = std::move(details);
      finish(std::move(body));
    }
  }

private:
  void finish(JV body) {
    finished_ = true;
    unregister_cancel_handler(request_id_);
    send_response(request_id_, std::move(body));
    queue_free();
  }
};

JV op_capture(const JV &params, int64_t request_id) {
  CaptureRequest request;
  if (JV error = parse_capture_request(params, &request); !error.IsNull())
    return error;

  if (request.after_frames <= 0 && request.when.empty())
    return capture_viewport_now(request, request_id);

  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  if (!tree->get_root())
    return error_result("no root window");

  godot::Ref<godot::Expression> when_expr;
  if (!request.when.empty()) {
    when_expr.instantiate();
    godot::Error parse_err =
        when_expr->parse(godot::String(request.when.c_str()));
    if (parse_err != godot::OK) {
      const std::string text = util::to_std(when_expr->get_error_text());
      JV body = error_result("invalid when expression: " + text +
                             " (expression: " + request.when + ")");
      JV details(JV::object_tag);
      details["code"] = JV("when_parse_error");
      details["when"] = JV(request.when);
      details["expression_error"] = JV(text);
      body["structured_error"] = std::move(details);
      return body;
    }
  }

  GameBridgeCaptureAwaiter *awaiter = memnew(GameBridgeCaptureAwaiter);
  awaiter->setup(request_id, request, when_expr);
  tree->get_root()->add_child(awaiter);
  register_cancel_handler(request_id, [awaiter] { awaiter->cancel(); });
  return JV();
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
  // UiElement 矩形来自 Control::get_global_rect()（画布空间），而注入的
  // InputEventMouseButton.position 必须是窗口客户区坐标：引擎在
  // viewport.cpp:_make_input_local 用 get_final_transform() 的反变换把窗口坐标
  // 换回画布空间，stretch（Example：320x180 视口 → 1280x720 窗口 = 4x）与
  // letterbox 边距都在这条链上。取控件所属视口的 screen transform 再复合它自己的
  // canvas transform（默认画布的 Camera2D 或 CanvasLayer），无 transform 时按恒等
  // 处理（保持旧行为）。
  godot::Transform2D screen_transform;
  if (godot::SceneTree *tree = get_scene_tree()) {
    if (auto *ctrl = godot::Object::cast_to<godot::Control>(
            resolve_game_node_suffixed(tree->get_root(), match->path))) {
      if (godot::Viewport *viewport = ctrl->get_viewport())
        screen_transform =
            viewport->get_screen_transform() * ctrl->get_canvas_transform();
    }
  }
  const godot::Vector2 window_center = coords::viewport_rect_center_to_window(
      screen_transform, godot::Rect2(match->x, match->y, match->w, match->h));
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
      position["x"] = JV(static_cast<double>(window_center.x));
      position["y"] = JV(static_cast<double>(window_center.y));
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
  // position/viewport_position 保留画布空间坐标（向后兼容），window_position 是
  // 实际注入窗口客户区的坐标。
  JV position(JV::object_tag);
  position["x"] = JV(center_x);
  position["y"] = JV(center_y);
  clicked["position"] = std::move(position);
  JV viewport_position(JV::object_tag);
  viewport_position["x"] = JV(center_x);
  viewport_position["y"] = JV(center_y);
  clicked["viewport_position"] = std::move(viewport_position);
  JV window_position(JV::object_tag);
  window_position["x"] = JV(static_cast<double>(window_center.x));
  window_position["y"] = JV(static_cast<double>(window_center.y));
  clicked["window_position"] = std::move(window_position);
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
    godot::ClassDB::register_class<GameBridgeCaptureAwaiter>();
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
