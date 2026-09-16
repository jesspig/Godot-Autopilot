#include "capture_ops.hpp"
#include "../util/error_util.hpp"
#include "../util/scene_path.hpp"
#include "core/config.hpp"
#include "core/editor_coords.hpp"
#include "core/log_system.hpp"
#include "tools/editor_ui_ops.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/scene_ops.hpp"
#include "util/json_godot.hpp"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace capture_ops {

static std::vector<uint8_t> g_last_rgba;
static int g_last_w = 0;
static int g_last_h = 0;
static bool g_has_baseline = false;

namespace {

constexpr size_t kBaselineMaxBytes = 32u * 1024u * 1024u;
constexpr int64_t kMaxDimensionMin = 64;
constexpr int64_t kMaxDimensionMax = 4096;
constexpr int64_t kScaleMin = 1;
constexpr int64_t kScaleMax = 8;

mcp::JsonValue capture_limit_error(const std::string &code,
                                   const std::string &message) {
  mcp::JsonValue error = util::error_json(message);
  mcp::JsonValue details(mcp::JsonValue::object_tag);
  details["code"] = mcp::JsonValue(code);
  error["structured_error"] = std::move(details);
  return error;
}

struct RegionRequest {
  bool present = false;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
};

mcp::JsonValue parse_region_request(const mcp::JsonValue &args,
                                    RegionRequest *out) {
  const mcp::JsonValue *region = args.Find("region");
  if (!region)
    return mcp::JsonValue();
  const char *message = "invalid parameter: region must be an object with "
                        "numeric x, y, width and height";
  if (!region->IsObject())
    return util::error_json(message);
  const mcp::JsonValue *x = region->Find("x");
  const mcp::JsonValue *y = region->Find("y");
  const mcp::JsonValue *width = region->Find("width");
  const mcp::JsonValue *height = region->Find("height");
  if (!x || !y || !width || !height || !x->IsNumber() || !y->IsNumber() ||
      !width->IsNumber() || !height->IsNumber())
    return util::error_json(message);
  out->x = util::json_number(x, 0.0);
  out->y = util::json_number(y, 0.0);
  out->width = util::json_number(width, 0.0);
  out->height = util::json_number(height, 0.0);
  if (out->width <= 0.0 || out->height <= 0.0)
    return util::error_json(
        "invalid parameter: region width and height must be greater than 0");
  out->present = true;
  return mcp::JsonValue();
}

mcp::JsonValue parse_max_dimension_request(const mcp::JsonValue &args,
                                           int64_t *out) {
  const mcp::JsonValue *max_dimension = args.Find("max_dimension");
  if (!max_dimension)
    return mcp::JsonValue();
  if (!max_dimension->IsInt())
    return util::error_json(
        "invalid parameter: max_dimension must be an integer");
  const int64_t value = max_dimension->GetInt();
  if (value < kMaxDimensionMin || value > kMaxDimensionMax)
    return util::error_json(
        "invalid parameter: max_dimension out of range (64-4096): " +
        std::to_string(value));
  *out = value;
  return mcp::JsonValue();
}

godot::Ref<godot::ViewportTexture>
usable_viewport_texture(godot::Viewport *viewport) {
  godot::Ref<godot::ViewportTexture> texture;
  if (!viewport)
    return texture;
  texture = viewport->get_texture();
  if (texture.is_null() || texture->get_image().is_null()) {
    texture.unref();
  }
  return texture;
}

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr int64_t kAnnotateNodesMaxPaths = 50;
constexpr int kNodeAnnotateBoxBudget = 200;
constexpr double kNodePointMarkerSize = 24.0;

struct AnnotateNodesRequest {
  bool present = false;
  std::vector<std::string> paths;
  int64_t max_limit = 50;
};

mcp::JsonValue parse_annotate_nodes_request(const mcp::JsonValue &args,
                                            AnnotateNodesRequest *out) {
  const mcp::JsonValue *max_p = args.Find("annotate_nodes_max");
  if (max_p) {
    if (!max_p->IsInt())
      return util::error_json(
          "invalid parameter: annotate_nodes_max must be an integer");
    const int64_t max_value = max_p->GetInt();
    if (max_value < 1 || max_value > kAnnotateNodesMaxPaths)
      return util::error_json(
          "invalid parameter: annotate_nodes_max out of range (1-50): " +
          std::to_string(max_value));
    out->max_limit = max_value;
  }
  const mcp::JsonValue *paths_p = args.Find("annotate_nodes");
  if (!paths_p)
    return mcp::JsonValue();
  if (!paths_p->IsArray())
    return util::error_json("invalid parameter: annotate_nodes must be an "
                              "array of node path strings");
  const auto &entries = paths_p->GetArray();
  if (entries.empty() ||
      entries.size() > static_cast<size_t>(kAnnotateNodesMaxPaths))
    return util::error_json("invalid parameter: annotate_nodes must contain "
                              "between 1 and 50 entries");
  for (const auto &entry : entries) {
    if (!entry.IsString() || entry.GetString().empty())
      return util::error_json("invalid parameter: annotate_nodes must "
                                "contain non-empty path strings");
    out->paths.push_back(entry.GetString());
  }
  if (static_cast<int64_t>(out->paths.size()) > out->max_limit)
    return util::error_json(
        "invalid parameter: annotate_nodes length exceeds annotate_nodes_max (" +
        std::to_string(out->max_limit) + ")");
  out->present = true;
  return mcp::JsonValue();
}

struct NodeAnnotateItem {
  int id = 0;
  std::string path;
  std::string type;
  bool ok = false;
  bool is_3d = false;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  bool behind = false;
  bool visible = false;
  bool drawable = false;
  std::string error;
};

int clamp_node_px(int value, int low, int high) {
  if (value < low)
    return low;
  if (value > high)
    return high;
  return value;
}

// Blue-box variant of the red UI-control annotation renderer. Lives here
// (rather than beside editor_ui_ops::draw_annotations) so the editor UI
// enumeration files stay untouched; the glyph renderer is intentionally
// mirrored to keep both number styles identical.
const uint8_t kNodeDigitGlyphs[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7}, {0x7, 0x1, 0x7, 0x4, 0x7},
    {0x7, 0x1, 0x7, 0x1, 0x7}, {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
    {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x2, 0x2, 0x2}, {0x7, 0x5, 0x7, 0x5, 0x7},
    {0x7, 0x5, 0x7, 0x1, 0x7},
};

void fill_node_rect(const godot::Ref<godot::Image> &image, int image_width,
                    int image_height, int x, int y, int w, int h,
                    const godot::Color &color) {
  const int x0 = clamp_node_px(x, 0, image_width);
  const int y0 = clamp_node_px(y, 0, image_height);
  const int x1 = clamp_node_px(x + w, 0, image_width);
  const int y1 = clamp_node_px(y + h, 0, image_height);
  if (x1 <= x0 || y1 <= y0)
    return;
  image->fill_rect(godot::Rect2i(x0, y0, x1 - x0, y1 - y0), color);
}

void draw_node_annotations(godot::Ref<godot::Image> image,
                           const std::vector<editor_ui_ops::MarkRect> &marks) {
  if (image.is_null() || marks.empty())
    return;
  const godot::Image::Format format = image->get_format();
  if (format != godot::Image::FORMAT_RGBA8 && format != godot::Image::FORMAT_RGB8)
    return;

  const int image_width = image->get_width();
  const int image_height = image->get_height();
  if (image_width <= 0 || image_height <= 0)
    return;

  const godot::Color box_color(0.2f, 0.45f, 1.0f, 1.0f);
  const godot::Color label_background(0.1f, 0.1f, 0.1f, 1.0f);
  const godot::Color digit_color(1.0f, 1.0f, 1.0f, 1.0f);
  const int scale = 2;

  for (const editor_ui_ops::MarkRect &mark : marks) {
    const int rx = static_cast<int>(std::lround(mark.x));
    const int ry = static_cast<int>(std::lround(mark.y));
    const int rw = static_cast<int>(std::lround(mark.w));
    const int rh = static_cast<int>(std::lround(mark.h));
    if (rw <= 0 || rh <= 0)
      continue;

    fill_node_rect(image, image_width, image_height, rx, ry, rw, 1, box_color);
    fill_node_rect(image, image_width, image_height, rx, ry + rh - 1, rw, 1, box_color);
    fill_node_rect(image, image_width, image_height, rx, ry, 1, rh, box_color);
    fill_node_rect(image, image_width, image_height, rx + rw - 1, ry, 1, rh, box_color);

    const std::string label = std::to_string(mark.id);
    const int digits = static_cast<int>(label.size());
    if (digits == 0)
      continue;
    const int label_x = rx + 2;
    const int label_y = ry + 2;
    fill_node_rect(image, image_width, image_height, label_x, label_y, digits * 4 * scale + 2,
                   5 * scale + 2, label_background);
    for (int d = 0; d < digits; ++d) {
      const char character = label[static_cast<size_t>(d)];
      if (character < '0' || character > '9')
        continue;
      const int digit = character - '0';
      const int origin_x = label_x + 1 + d * 4 * scale;
      const int origin_y = label_y + 1;
      for (int row = 0; row < 5; ++row) {
        const uint8_t bits = kNodeDigitGlyphs[digit][row];
        for (int column = 0; column < 3; ++column) {
          if ((bits & (1u << (2 - column))) == 0)
            continue;
          for (int sy = 0; sy < scale; ++sy) {
            for (int sx = 0; sx < scale; ++sx) {
              const int px = origin_x + column * scale + sx;
              const int py = origin_y + row * scale + sy;
              if (px < 0 || px >= image_width || py < 0 || py >= image_height)
                continue;
              image->set_pixel(px, py, digit_color);
            }
          }
        }
      }
    }
  }
}

} // namespace

std::string base64_encode(const uint8_t *data, size_t len) {
  std::string result;
  result.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    int n = (i + 2 < len) ? 3 : (len - i);
    unsigned int val = static_cast<unsigned int>(data[i]) << 16;
    if (n > 1)
      val |= static_cast<unsigned int>(data[i + 1]) << 8;
    if (n > 2)
      val |= static_cast<unsigned int>(data[i + 2]);
    result += b64_chars[(val >> 18) & 0x3F];
    result += b64_chars[(val >> 12) & 0x3F];
    if (n > 1)
      result += b64_chars[(val >> 6) & 0x3F];
    else
      result += '=';
    if (n > 2)
      result += b64_chars[val & 0x3F];
    else
      result += '=';
  }
  return result;
}

void prune_capture_files(const godot::String &dir_path, int keep) {
  if (keep <= 0)
    return;
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(dir_path);
  if (dir.is_null())
    return;
  if (dir->list_dir_begin() != godot::OK)
    return;

  std::vector<std::pair<uint64_t, godot::String>> files;
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && !dir->current_is_dir() &&
        entry.begins_with("gda_capture") && entry.ends_with(".png")) {
      godot::String full_path = dir_path.path_join(entry);
      files.emplace_back(godot::FileAccess::get_modified_time(full_path), entry);
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();

  if (files.size() <= static_cast<size_t>(keep))
    return;

  std::sort(files.begin(), files.end(),
            [](const std::pair<uint64_t, godot::String> &a,
               const std::pair<uint64_t, godot::String> &b) {
              if (a.first != b.first)
                return a.first > b.first;
              return a.second > b.second;
            });

  for (size_t i = static_cast<size_t>(keep); i < files.size(); ++i) {
    godot::Error err =
        godot::DirAccess::remove_absolute(dir_path.path_join(files[i].second));
    if (err != godot::OK) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Tools,
          "prune_capture_files: failed to remove " +
              util::to_std(files[i].second));
    }
  }
}

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_editor_viewport called");

  std::string target = "editor";
  if (auto *target_p = args.Find("target")) {
    if (!target_p->IsString()) {
      return util::error_detail(
          "invalid target type", "capture_ops.cpp handle_capture_viewport",
          "'editor' or 'game'", "pass target as a string");
    }
    target = target_p->GetString();
  }

  bool save = false;
  if (auto *save_p = args.Find("save")) {
    if (!save_p->IsBool()) {
      return util::error_json("invalid parameter: save must be a boolean");
    }
    save = save_p->GetBool();
  }

  bool annotate = false;
  if (auto *annotate_p = args.Find("annotate")) {
    if (!annotate_p->IsBool()) {
      return util::error_json("invalid parameter: annotate must be a boolean");
    }
    annotate = annotate_p->GetBool();
  }

  int64_t after_frames = 0;
  if (auto *after_p = args.Find("after_frames")) {
    if (!after_p->IsInt() || after_p->GetInt() < 0) {
      return util::error_json(
          "after_frames must be an integer greater than or equal to 0");
    }
    after_frames = after_p->GetInt();
  }

  std::string when;
  if (auto *when_p = args.Find("when")) {
    if (!when_p->IsString()) {
      return util::error_json("when must be a string");
    }
    when = when_p->GetString();
  }

  int64_t scale = 1;
  if (auto *scale_p = args.Find("scale")) {
    if (!scale_p->IsInt() || scale_p->GetInt() < kScaleMin ||
        scale_p->GetInt() > kScaleMax) {
      return util::error_json("scale must be an integer between 1 and 8");
    }
    scale = scale_p->GetInt();
  }

  AnnotateNodesRequest annotate_nodes;
  if (mcp::JsonValue annotate_nodes_error =
          parse_annotate_nodes_request(args, &annotate_nodes);
      !annotate_nodes_error.IsNull())
    return annotate_nodes_error;

  bool diff_image_requested = false;
  if (auto *diff_image_p = args.Find("diff_image")) {
    if (!diff_image_p->IsBool()) {
      return util::error_json(
          "invalid parameter: diff_image must be a boolean");
    }
    diff_image_requested = diff_image_p->GetBool();
  }

  if (target == "game") {
    int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
    if (auto *tp = args.Find("timeout_ms")) {
      if (tp->IsInt() && tp->GetInt() > 0)
        timeout_ms = tp->GetInt();
    }
    if (timeout_ms > GDA_MAX_GAME_OP_TIMEOUT_MS)
      timeout_ms = GDA_MAX_GAME_OP_TIMEOUT_MS;

    RegionRequest region_request;
    if (mcp::JsonValue region_error =
            parse_region_request(args, &region_request);
        !region_error.IsNull())
      return region_error;
    int64_t max_dimension = 0;
    if (mcp::JsonValue max_dimension_error =
            parse_max_dimension_request(args, &max_dimension);
        !max_dimension_error.IsNull())
      return max_dimension_error;

    mcp::JsonValue params(mcp::JsonValue::object_tag);
    if (region_request.present) {
      if (const mcp::JsonValue *region = args.Find("region"))
        params["region"] = *region;
    }
    if (max_dimension > 0)
      params["max_dimension"] = mcp::JsonValue(max_dimension);
    if (annotate)
      params["annotate"] = mcp::JsonValue(true);
    if (annotate_nodes.present) {
      if (const mcp::JsonValue *nodes_param = args.Find("annotate_nodes"))
        params["annotate_nodes"] = *nodes_param;
      if (const mcp::JsonValue *nodes_max_param =
              args.Find("annotate_nodes_max"))
        params["annotate_nodes_max"] = *nodes_max_param;
    }
    if (after_frames > 0)
      params["after_frames"] = mcp::JsonValue(after_frames);
    if (!when.empty())
      params["when"] = mcp::JsonValue(when);
    if (scale > 1)
      params["scale"] = mcp::JsonValue(scale);
    params["timeout_ms"] = mcp::JsonValue(timeout_ms);
    if (diff_image_requested) {
      return util::error_json(
          "diff_image is only supported for target='editor'");
    }
    return runtime_ops::handle_gda_send("capture", params, timeout_ms);
  }
  if (target != "editor") {
    return util::error_detail("invalid target '" + target + "'",
                              "capture_ops.cpp handle_capture_viewport",
                              "'editor' or 'game'",
                              "pass target='editor' or target='game'");
  }
  if (after_frames > 0 || !when.empty()) {
    return util::error_json(
        "after_frames/when are only supported for target='game'");
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");
  if (annotate_nodes.present && !editor->get_edited_scene_root()) {
    return util::error_detail(
        "no edited scene root available",
        "capture_ops.cpp handle_capture_viewport", "an edited scene",
        "open or create a scene first (create_editor_scene)");
  }

  std::string space = "viewport";
  if (auto *space_p = args.Find("space")) {
    if (!space_p->IsString()) {
      return util::error_json("invalid parameter: space must be a string");
    }
    space = space_p->GetString();
    if (space != "viewport" && space != "window") {
      return util::error_json(
          "invalid parameter: space must be 'viewport' or 'window'");
    }
  }

  int64_t max_dimension = 0;
  if (mcp::JsonValue max_dimension_error =
          parse_max_dimension_request(args, &max_dimension);
      !max_dimension_error.IsNull())
    return max_dimension_error;

  bool diff_against_last = false;
  if (auto *diff_p = args.Find("diff_against_last")) {
    if (!diff_p->IsBool()) {
      return util::error_json(
          "invalid parameter: diff_against_last must be a boolean");
    }
    diff_against_last = diff_p->GetBool();
  }
  if (diff_image_requested && !diff_against_last) {
    return util::error_json(
        "invalid parameter: diff_image requires diff_against_last");
  }

  RegionRequest region_request;
  if (mcp::JsonValue region_error =
          parse_region_request(args, &region_request);
      !region_error.IsNull())
    return region_error;

  godot::Ref<godot::ViewportTexture> texture;
  std::string captured_viewport = "window";
  if (space == "window") {
    godot::Control *base_control = editor->get_base_control();
    godot::Viewport *window_viewport =
        base_control ? base_control->get_viewport() : nullptr;
    texture = usable_viewport_texture(window_viewport);
    if (texture.is_null()) {
      return util::error_detail(
          "editor window viewport texture unavailable",
          "capture_ops.cpp handle_capture_viewport",
          "editor main window rendered",
          "make sure the editor window is visible and not minimized");
    }
  } else {
    texture = usable_viewport_texture(editor->get_editor_viewport_2d());
    if (!texture.is_null()) {
      captured_viewport = "2d";
    } else {
      texture = usable_viewport_texture(editor->get_editor_viewport_3d());
      if (!texture.is_null())
        captured_viewport = "3d";
    }
    if (texture.is_null()) {
      return util::error_detail("editor viewport texture unavailable",
                                "capture_ops.cpp handle_capture_viewport",
                                "2D/3D editor viewport rendered",
                                "open a scene with a visible viewport first");
    }
  }

  if (auto *rendering_server = godot::RenderingServer::get_singleton()) {
    rendering_server->force_draw();
  }

  auto img = texture->get_image();
  const int source_width = img->get_width();
  const int source_height = img->get_height();

  bool region_applied = false;
  int applied_region_x = 0;
  int applied_region_y = 0;
  int applied_region_w = 0;
  int applied_region_h = 0;
  if (region_request.present) {
    const double x0 = std::max(0.0, std::floor(region_request.x));
    const double y0 = std::max(0.0, std::floor(region_request.y));
    const double x1 =
        std::min(static_cast<double>(source_width),
                 std::floor(region_request.x + region_request.width));
    const double y1 =
        std::min(static_cast<double>(source_height),
                 std::floor(region_request.y + region_request.height));
    if (x1 - x0 < 1.0 || y1 - y0 < 1.0) {
      return capture_limit_error(
          "region_out_of_bounds",
          "region does not intersect the " + std::to_string(source_width) +
              "x" + std::to_string(source_height) + " captured image");
    }
    applied_region_x = static_cast<int>(x0);
    applied_region_y = static_cast<int>(y0);
    applied_region_w = static_cast<int>(x1 - x0);
    applied_region_h = static_cast<int>(y1 - y0);
    img = img->get_region(godot::Rect2i(applied_region_x, applied_region_y,
                                        applied_region_w, applied_region_h));
    region_applied = true;
  }

  int final_width = img->get_width();
  int final_height = img->get_height();
  bool scaled = false;
  if (scale > 1) {
    const coords::ImageSize scaled_size = coords::scale_size(
        coords::ImageSize{final_width, final_height},
        static_cast<int>(scale));
    if (scaled_size.width <= 0 || scaled_size.height <= 0) {
      return capture_limit_error(
          "capture_dimensions_exceeded",
          "scaled capture exceeds the image pixel limit (" +
              std::to_string(final_width) + "x" + std::to_string(final_height) +
              " x" + std::to_string(scale) + ")");
    }
    img->resize(scaled_size.width, scaled_size.height,
                godot::Image::INTERPOLATE_NEAREST);
    final_width = scaled_size.width;
    final_height = scaled_size.height;
    scaled = true;
  }
  if (max_dimension > 0) {
    const coords::ImageSize fitted =
        coords::fit_within(coords::ImageSize{final_width, final_height},
                           static_cast<int>(max_dimension));
    if (fitted.width != final_width || fitted.height != final_height) {
      img->resize(fitted.width, fitted.height,
                  godot::Image::INTERPOLATE_BILINEAR);
      final_width = fitted.width;
      final_height = fitted.height;
      scaled = true;
    }
  }

  if (final_width > GDA_CAPTURE_MAX_DIMENSION ||
      final_height > GDA_CAPTURE_MAX_DIMENSION) {
    return capture_limit_error(
        "capture_dimensions_exceeded",
        "viewport dimensions exceed the capture limit of " +
            std::to_string(GDA_CAPTURE_MAX_DIMENSION) + " pixels per side");
  }

  std::vector<editor_ui_ops::Element> annotate_elements;
  std::vector<editor_ui_ops::MarkRect> annotate_marks;
  bool annotate_truncated = false;
  if (annotate) {
    editor_ui_ops::WindowMapping mapping;
    bool mapping_ok = captured_viewport == "window";
    if (!mapping_ok) {
      std::string mapping_error;
      mapping_ok = editor_ui_ops::editor_viewport_window_mapping(
          captured_viewport, 0, &mapping, &mapping_error);
    }
    if (mapping_ok && (std::fabs(mapping.scale_x) < 1e-9 ||
                       std::fabs(mapping.scale_y) < 1e-9)) {
      mapping_ok = false;
    }

    if (mapping_ok) {
      const double crop_w =
          region_applied ? static_cast<double>(applied_region_w)
                         : static_cast<double>(source_width);
      const double crop_h =
          region_applied ? static_cast<double>(applied_region_h)
                         : static_cast<double>(source_height);
      const double region_x =
          region_applied ? static_cast<double>(applied_region_x) : 0.0;
      const double region_y =
          region_applied ? static_cast<double>(applied_region_y) : 0.0;
      const double output_scale_x =
          crop_w > 0.0 ? static_cast<double>(final_width) / crop_w : 1.0;
      const double output_scale_y =
          crop_h > 0.0 ? static_cast<double>(final_height) / crop_h : 1.0;

      const std::vector<editor_ui_ops::Element> all =
          editor_ui_ops::collect_elements(std::string(), std::string(), false,
                                          200, &annotate_truncated);
      for (const editor_ui_ops::Element &element : all) {
        const auto to_final = [&](double client_x, double client_y,
                                  double *final_x, double *final_y) {
          const double full_x = (client_x - mapping.offset_x) / mapping.scale_x;
          const double full_y = (client_y - mapping.offset_y) / mapping.scale_y;
          *final_x = (full_x - region_x) * output_scale_x;
          *final_y = (full_y - region_y) * output_scale_y;
        };
        double x0 = 0.0;
        double y0 = 0.0;
        double x1 = 0.0;
        double y1 = 0.0;
        to_final(element.x, element.y, &x0, &y0);
        to_final(element.x + element.w, element.y + element.h, &x1, &y1);
        const double rx = std::min(x0, x1);
        const double ry = std::min(y0, y1);
        const double rw = std::fabs(x1 - x0);
        const double rh = std::fabs(y1 - y0);
        if (rw <= 0.0 || rh <= 0.0)
          continue;
        if (rx + rw <= 0.0 || ry + rh <= 0.0 ||
            rx >= static_cast<double>(final_width) ||
            ry >= static_cast<double>(final_height))
          continue;
        editor_ui_ops::Element kept = element;
        kept.x = rx;
        kept.y = ry;
        kept.w = rw;
        kept.h = rh;
        annotate_elements.push_back(std::move(kept));
        editor_ui_ops::MarkRect mark;
        mark.id = element.id;
        mark.x = rx;
        mark.y = ry;
        mark.w = rw;
        mark.h = rh;
        annotate_marks.push_back(mark);
      }
      editor_ui_ops::draw_annotations(img, annotate_marks);
    }
  }

  std::vector<NodeAnnotateItem> node_items;
  std::vector<editor_ui_ops::MarkRect> node_marks;
  bool node_truncated = false;
  if (annotate_nodes.present) {
    godot::Node *scene_root = editor->get_edited_scene_root();
    if (!scene_root) {
      int missing_id = 1;
      for (const std::string &input_path : annotate_nodes.paths) {
        NodeAnnotateItem missing;
        missing.id = missing_id++;
        missing.path = input_path;
        missing.error = "no edited scene root available";
        node_items.push_back(missing);
      }
    } else {
      editor_ui_ops::WindowMapping node_mapping;
      bool node_mapping_ok = captured_viewport == "window";
      if (!node_mapping_ok) {
        std::string node_mapping_error;
        node_mapping_ok = editor_ui_ops::editor_viewport_window_mapping(
            captured_viewport, 0, &node_mapping, &node_mapping_error);
      }
      if (node_mapping_ok && (std::fabs(node_mapping.scale_x) < 1e-9 ||
                               std::fabs(node_mapping.scale_y) < 1e-9)) {
        node_mapping_ok = false;
      }
      if (!node_mapping_ok) {
        int unmapped_id = 1;
        for (const std::string &input_path : annotate_nodes.paths) {
          NodeAnnotateItem unmapped;
          unmapped.id = unmapped_id++;
          unmapped.path = input_path;
          unmapped.error = "editor viewport mapping unavailable";
          node_items.push_back(unmapped);
        }
      } else {
        const double node_crop_w =
            region_applied ? static_cast<double>(applied_region_w)
                           : static_cast<double>(source_width);
        const double node_crop_h =
            region_applied ? static_cast<double>(applied_region_h)
                           : static_cast<double>(source_height);
        const double node_region_x =
            region_applied ? static_cast<double>(applied_region_x) : 0.0;
        const double node_region_y =
            region_applied ? static_cast<double>(applied_region_y) : 0.0;
        const double node_out_sx = node_crop_w > 0.0
                                       ? static_cast<double>(final_width) / node_crop_w
                                       : 1.0;
        const double node_out_sy = node_crop_h > 0.0
                                       ? static_cast<double>(final_height) / node_crop_h
                                       : 1.0;
        const auto node_to_final = [&](double client_x, double client_y,
                                       double *final_x, double *final_y) {
          const double full_x =
              (client_x - node_mapping.offset_x) / node_mapping.scale_x;
          const double full_y =
              (client_y - node_mapping.offset_y) / node_mapping.scale_y;
          *final_x = (full_x - node_region_x) * node_out_sx;
          *final_y = (full_y - node_region_y) * node_out_sy;
        };

        godot::SubViewport *node_vp2d = editor->get_editor_viewport_2d();
        godot::SubViewport *node_vp3d = editor->get_editor_viewport_3d(0);
        godot::Transform2D node_t2d;
        bool node_has_t2d = false;
        if (node_vp2d) {
          node_t2d = node_vp2d->get_screen_transform() *
                     node_vp2d->get_global_canvas_transform();
          node_has_t2d = true;
        }
        godot::Transform2D node_t3d;
        if (node_vp3d)
          node_t3d = node_vp3d->get_screen_transform();
        godot::Camera3D *node_camera =
            node_vp3d ? node_vp3d->get_camera_3d() : nullptr;

        int node_id = 1;
        for (const std::string &input_path : annotate_nodes.paths) {
          NodeAnnotateItem item;
          item.id = node_id++;
          item.path = input_path;
          std::string resolve_hint;
          godot::Node *node =
              util::resolve_scene_node(input_path, scene_root, &resolve_hint);
          if (!node) {
            item.error = "node not found";
            node_items.push_back(item);
            continue;
          }
          item.type = util::to_std(node->get_class());
          godot::Node3D *node_3d = godot::Object::cast_to<godot::Node3D>(node);
          godot::CanvasItem *canvas_item =
              godot::Object::cast_to<godot::CanvasItem>(node);
          if (node_3d) {
            item.is_3d = true;
          } else if (!canvas_item) {
            item.error = "unsupported node type";
            node_items.push_back(item);
            continue;
          }
          const std::string node_space = item.is_3d ? "3d" : "2d";
          if (captured_viewport != "window" && captured_viewport != node_space) {
            item.error = "viewport incompatible";
            node_items.push_back(item);
            continue;
          }
          double ncx = 0.0;
          double ncy = 0.0;
          double ncw = 0.0;
          double nch = 0.0;
          bool node_is_point = false;
          if (item.is_3d) {
            if (!node_camera) {
              item.error = "3D editor viewport has no camera";
              node_items.push_back(item);
              continue;
            }
            const godot::Vector3 world =
                node_3d->get_global_transform().get_origin();
            const godot::Vector2 local = node_camera->unproject_position(world);
            const godot::Vector2 client = node_t3d.xform(local);
            ncx = client.x;
            ncy = client.y;
            node_is_point = true;
            item.behind = node_camera->is_position_behind(world);
            item.visible = node_3d->is_visible_in_tree() && !item.behind;
          } else if (!node_has_t2d) {
            item.error = "2D editor viewport not available";
            node_items.push_back(item);
            continue;
          } else if (auto *control = godot::Object::cast_to<godot::Control>(node)) {
            const godot::Transform2D global = control->get_global_transform();
            const godot::Vector2 corner_a =
                node_t2d.xform(global.xform(godot::Vector2()));
            const godot::Vector2 corner_b =
                node_t2d.xform(global.xform(control->get_size()));
            ncx = std::min(static_cast<double>(corner_a.x),
                           static_cast<double>(corner_b.x));
            ncy = std::min(static_cast<double>(corner_a.y),
                           static_cast<double>(corner_b.y));
            ncw = std::fabs(static_cast<double>(corner_b.x) -
                            static_cast<double>(corner_a.x));
            nch = std::fabs(static_cast<double>(corner_b.y) -
                            static_cast<double>(corner_a.y));
            item.visible = control->is_visible_in_tree();
          } else if (auto *sprite = godot::Object::cast_to<godot::Sprite2D>(node)) {
            const godot::Rect2 local = sprite->get_rect();
            const godot::Transform2D global = sprite->get_global_transform();
            const godot::Vector2 corners[4] = {
                node_t2d.xform(global.xform(local.position)),
                node_t2d.xform(global.xform(
                    local.position + godot::Vector2(local.size.x, 0.0f))),
                node_t2d.xform(global.xform(
                    local.position + godot::Vector2(0.0f, local.size.y))),
                node_t2d.xform(global.xform(local.position + local.size)),
            };
            double min_x = corners[0].x;
            double max_x = corners[0].x;
            double min_y = corners[0].y;
            double max_y = corners[0].y;
            for (int corner = 1; corner < 4; ++corner) {
              min_x = std::min(min_x, static_cast<double>(corners[corner].x));
              max_x = std::max(max_x, static_cast<double>(corners[corner].x));
              min_y = std::min(min_y, static_cast<double>(corners[corner].y));
              max_y = std::max(max_y, static_cast<double>(corners[corner].y));
            }
            ncx = min_x;
            ncy = min_y;
            ncw = max_x - min_x;
            nch = max_y - min_y;
            item.visible = sprite->is_visible_in_tree();
          } else {
            const godot::Vector2 client = node_t2d.xform(
                canvas_item->get_global_transform().get_origin());
            ncx = client.x;
            ncy = client.y;
            node_is_point = true;
            item.visible = canvas_item->is_visible_in_tree();
          }
          if (node_is_point) {
            ncx -= kNodePointMarkerSize / 2.0;
            ncy -= kNodePointMarkerSize / 2.0;
            ncw = kNodePointMarkerSize;
            nch = kNodePointMarkerSize;
          }
          if (!item.visible || ncw <= 0.0 || nch <= 0.0)
            item.visible = false;
          double nfx0 = 0.0;
          double nfy0 = 0.0;
          double nfx1 = 0.0;
          double nfy1 = 0.0;
          node_to_final(ncx, ncy, &nfx0, &nfy0);
          node_to_final(ncx + ncw, ncy + nch, &nfx1, &nfy1);
          item.x = std::min(nfx0, nfx1);
          item.y = std::min(nfy0, nfy1);
          item.w = std::fabs(nfx1 - nfx0);
          item.h = std::fabs(nfy1 - nfy0);
          if (item.w <= 0.0 || item.h <= 0.0)
            item.visible = false;
          item.ok = true;
          if (item.visible) {
            if (item.x + item.w <= 0.0 || item.y + item.h <= 0.0 ||
                item.x >= static_cast<double>(final_width) ||
                item.y >= static_cast<double>(final_height)) {
              item.visible = false;
            } else if (annotate_marks.size() + node_marks.size() >=
                       static_cast<size_t>(kNodeAnnotateBoxBudget)) {
              node_truncated = true;
            } else {
              item.drawable = true;
              editor_ui_ops::MarkRect mark;
              mark.id = item.id;
              mark.x = item.x;
              mark.y = item.y;
              mark.w = item.w;
              mark.h = item.h;
              node_marks.push_back(mark);
            }
          }
          node_items.push_back(item);
        }
        draw_node_annotations(img, node_marks);
      }
    }
  }

  godot::Ref<godot::Image> diff_source = img;
  bool convertible = img->get_format() == godot::Image::FORMAT_RGBA8;
  if (!convertible) {
    // Keep the original image intact; convert a copy only for diff/baseline.
    godot::Ref<godot::Image> copy;
    copy.instantiate();
    copy->copy_from(img);
    copy->convert(godot::Image::FORMAT_RGBA8);
    convertible = copy->get_format() == godot::Image::FORMAT_RGBA8;
    if (convertible)
      diff_source = copy;
  }
  godot::PackedByteArray pixels = diff_source->get_data();
  const bool baseline_storable =
      convertible && static_cast<size_t>(pixels.size()) <= kBaselineMaxBytes;

  mcp::JsonValue diff(mcp::JsonValue::object_tag);
  bool diff_bbox_available = false;
  double diff_bbox_x = 0.0;
  double diff_bbox_y = 0.0;
  double diff_bbox_w = 0.0;
  double diff_bbox_h = 0.0;
  if (diff_against_last) {
    diff["comparable"] = mcp::JsonValue(false);
    diff["changed_ratio"] = mcp::JsonValue(0.0);
    if (g_has_baseline) {
      mcp::JsonValue baseline_size(mcp::JsonValue::object_tag);
      baseline_size["x"] = mcp::JsonValue(g_last_w);
      baseline_size["y"] = mcp::JsonValue(g_last_h);
      diff["baseline_size"] = std::move(baseline_size);
    }
    const auto fill_diff_sample = [&diff, &diff_bbox_available, &diff_bbox_x,
                                   &diff_bbox_y, &diff_bbox_w, &diff_bbox_h](
                                      const coords::DiffResult &sample) {
      diff["comparable"] = mcp::JsonValue(sample.comparable);
      diff["changed_ratio"] = mcp::JsonValue(sample.changed_ratio);
      if (sample.has_bbox) {
        mcp::JsonValue bbox(mcp::JsonValue::object_tag);
        bbox["x"] = mcp::JsonValue(sample.bbox.x);
        bbox["y"] = mcp::JsonValue(sample.bbox.y);
        bbox["w"] = mcp::JsonValue(sample.bbox.w);
        bbox["h"] = mcp::JsonValue(sample.bbox.h);
        diff["changed_bbox"] = std::move(bbox);
        diff_bbox_available = true;
        diff_bbox_x = sample.bbox.x;
        diff_bbox_y = sample.bbox.y;
        diff_bbox_w = sample.bbox.w;
        diff_bbox_h = sample.bbox.h;
      }
    };
    if (!convertible) {
      diff["reason"] = mcp::JsonValue("unsupported_format");
    } else if (!g_has_baseline) {
      diff["reason"] = mcp::JsonValue("no_baseline");
    } else if (static_cast<size_t>(pixels.size()) > kBaselineMaxBytes) {
      diff["reason"] = mcp::JsonValue("baseline_too_large");
    } else if (final_width != g_last_w || final_height != g_last_h) {
      const int cw = std::min(final_width, g_last_w);
      const int ch = std::min(final_height, g_last_h);
      const size_t current_stride = static_cast<size_t>(final_width) * 4u;
      const size_t baseline_stride = static_cast<size_t>(g_last_w) * 4u;
      const size_t row_bytes = static_cast<size_t>(cw) * 4u;
      const size_t common_bytes = row_bytes * static_cast<size_t>(ch);
      std::vector<uint8_t> tmp_current(common_bytes);
      std::vector<uint8_t> tmp_baseline(common_bytes);
      // Row strides differ between the two buffers, so rows are copied one by one.
      for (int y = 0; y < ch; ++y) {
        std::copy_n(pixels.ptr() + static_cast<size_t>(y) * current_stride,
                    row_bytes,
                    tmp_current.data() + static_cast<size_t>(y) * row_bytes);
        std::copy_n(g_last_rgba.data() + static_cast<size_t>(y) * baseline_stride,
                    row_bytes,
                    tmp_baseline.data() + static_cast<size_t>(y) * row_bytes);
      }
      const coords::DiffResult sample = coords::diff_sample(
          tmp_current.data(), tmp_baseline.data(), cw, ch, 4, 4, 16);
      fill_diff_sample(sample);
      diff["size_mismatch"] = mcp::JsonValue(true);
      mcp::JsonValue current_size(mcp::JsonValue::object_tag);
      current_size["x"] = mcp::JsonValue(final_width);
      current_size["y"] = mcp::JsonValue(final_height);
      diff["current_size"] = std::move(current_size);
    } else {
      const coords::DiffResult sample =
          coords::diff_sample(pixels.ptr(), g_last_rgba.data(), final_width,
                              final_height, 4, 4, 16);
      fill_diff_sample(sample);
    }
  }

  std::string diff_image_b64;
  bool diff_image_available = false;
  if (diff_image_requested && diff_against_last && diff_bbox_available) {
    godot::Ref<godot::Image> diff_img;
    diff_img.instantiate();
    diff_img->copy_from(img);
    godot::Image::Format diff_format = diff_img->get_format();
    if (diff_format != godot::Image::FORMAT_RGBA8 &&
        diff_format != godot::Image::FORMAT_RGB8) {
      diff_img->convert(godot::Image::FORMAT_RGBA8);
      diff_format = diff_img->get_format();
    }
    if (diff_format == godot::Image::FORMAT_RGBA8 ||
        diff_format == godot::Image::FORMAT_RGB8) {
      const int diff_w = diff_img->get_width();
      const int diff_h = diff_img->get_height();
      const int bx = static_cast<int>(std::lround(diff_bbox_x));
      const int by = static_cast<int>(std::lround(diff_bbox_y));
      const int bw = static_cast<int>(std::lround(diff_bbox_w));
      const int bh = static_cast<int>(std::lround(diff_bbox_h));
      if (bw > 0 && bh > 0 && diff_w > 0 && diff_h > 0) {
        const godot::Color highlight(1.0f, 1.0f, 0.0f, 1.0f);
        const int kThickness = 2;
        fill_node_rect(diff_img, diff_w, diff_h, bx, by, bw, kThickness,
                       highlight);
        fill_node_rect(diff_img, diff_w, diff_h, bx, by + bh - kThickness, bw,
                       kThickness, highlight);
        fill_node_rect(diff_img, diff_w, diff_h, bx, by, kThickness, bh,
                       highlight);
        fill_node_rect(diff_img, diff_w, diff_h, bx + bw - kThickness, by,
                       kThickness, bh, highlight);
        godot::PackedByteArray diff_png = diff_img->save_png_to_buffer();
        if (diff_png.size() > 0) {
          if (static_cast<size_t>(diff_png.size()) > GDA_CAPTURE_MAX_PNG_BYTES) {
            return capture_limit_error(
                "capture_bytes_exceeded",
                "encoded diff PNG exceeds the capture limit of " +
                    std::to_string(GDA_CAPTURE_MAX_PNG_BYTES) + " bytes");
          }
          diff_image_b64 = base64_encode(
              diff_png.ptr(), static_cast<size_t>(diff_png.size()));
          diff_image_available = true;
        }
      }
    }
  }

  godot::PackedByteArray png_buffer = img->save_png_to_buffer();
  if (png_buffer.size() == 0)
    return util::error_json("PNG encoding returned empty buffer");
  if (static_cast<size_t>(png_buffer.size()) > GDA_CAPTURE_MAX_PNG_BYTES) {
    return capture_limit_error(
        "capture_bytes_exceeded",
        "encoded PNG exceeds the capture limit of " +
            std::to_string(GDA_CAPTURE_MAX_PNG_BYTES) + " bytes");
  }

  std::string saved_path;
  if (save) {
    const godot::String capture_dir = "user://godot_autopilot/captures";
    godot::DirAccess::make_dir_recursive_absolute(capture_dir);
    const godot::String capture_path =
        capture_dir + godot::String("/gda_capture_editor_") +
        godot::String(std::to_string(
                          godot::Time::get_singleton()->get_ticks_msec())
                          .c_str()) +
        godot::String(".png");
    godot::Error save_err = img->save_png(capture_path);
    if (save_err != godot::OK) {
      return util::error_detail("failed to save editor capture",
                                "capture_ops.cpp handle_capture_viewport",
                                "writable user:// path",
                                "check that the user:// directory is writable");
    }
    prune_capture_files(capture_dir, 20);
    saved_path = util::to_std(capture_path);
  }

  std::string b64 =
      base64_encode(png_buffer.ptr(), static_cast<size_t>(png_buffer.size()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["data"] = mcp::JsonValue(b64);
  inner["format"] = mcp::JsonValue("png");
  inner["width"] = mcp::JsonValue(final_width);
  inner["height"] = mcp::JsonValue(final_height);
  inner["space"] = mcp::JsonValue(space);
  if (region_applied) {
    mcp::JsonValue applied(mcp::JsonValue::object_tag);
    applied["x"] = mcp::JsonValue(applied_region_x);
    applied["y"] = mcp::JsonValue(applied_region_y);
    applied["width"] = mcp::JsonValue(applied_region_w);
    applied["height"] = mcp::JsonValue(applied_region_h);
    inner["region"] = std::move(applied);
  }
  const bool cropped =
      region_applied && (applied_region_w != source_width ||
                         applied_region_h != source_height);
  if (cropped || scaled) {
    inner["source_width"] = mcp::JsonValue(source_width);
    inner["source_height"] = mcp::JsonValue(source_height);
  }
  if (diff_against_last) {
    inner["diff"] = std::move(diff);
    if (diff_image_available)
      inner["diff_image_data"] = mcp::JsonValue(diff_image_b64);
  }
  if (annotate) {
    mcp::JsonValue elements(mcp::JsonValue::array_tag);
    for (const editor_ui_ops::Element &element : annotate_elements) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["id"] = mcp::JsonValue(element.id);
      item["path"] = mcp::JsonValue(element.path);
      item["type"] = mcp::JsonValue(element.type);
      if (!element.text.empty())
        item["text"] = mcp::JsonValue(element.text);
      mcp::JsonValue position(mcp::JsonValue::object_tag);
      position["x"] = mcp::JsonValue(element.x);
      position["y"] = mcp::JsonValue(element.y);
      item["position"] = std::move(position);
      mcp::JsonValue size(mcp::JsonValue::object_tag);
      size["x"] = mcp::JsonValue(element.w);
      size["y"] = mcp::JsonValue(element.h);
      item["size"] = std::move(size);
      elements.PushBack(std::move(item));
    }
    inner["elements"] = std::move(elements);
    inner["annotated"] = mcp::JsonValue(true);
    if (annotate_truncated)
      inner["elements_truncated"] = mcp::JsonValue(true);
  }
  if (annotate_nodes.present) {
    mcp::JsonValue node_elements(mcp::JsonValue::array_tag);
    for (const NodeAnnotateItem &node_item : node_items) {
      mcp::JsonValue node_entry(mcp::JsonValue::object_tag);
      node_entry["id"] = mcp::JsonValue(node_item.id);
      node_entry["path"] = mcp::JsonValue(node_item.path);
      node_entry["ok"] = mcp::JsonValue(node_item.ok);
      if (!node_item.type.empty())
        node_entry["type"] = mcp::JsonValue(node_item.type);
      if (node_item.ok) {
        mcp::JsonValue node_position(mcp::JsonValue::object_tag);
        node_position["x"] = mcp::JsonValue(node_item.x);
        node_position["y"] = mcp::JsonValue(node_item.y);
        node_entry["position"] = std::move(node_position);
        mcp::JsonValue node_size(mcp::JsonValue::object_tag);
        node_size["x"] = mcp::JsonValue(node_item.w);
        node_size["y"] = mcp::JsonValue(node_item.h);
        node_entry["size"] = std::move(node_size);
        node_entry["visible"] = mcp::JsonValue(node_item.visible);
        if (node_item.is_3d)
          node_entry["behind"] = mcp::JsonValue(node_item.behind);
      } else {
        node_entry["error"] = mcp::JsonValue(node_item.error);
      }
      node_elements.PushBack(std::move(node_entry));
    }
    inner["node_elements"] = std::move(node_elements);
    if (node_truncated)
      inner["node_truncated"] = mcp::JsonValue(true);
  }
  if (save)
    inner["path"] = mcp::JsonValue(saved_path);
  r["result"] = std::move(inner);
  if (r.Dump().size() > GDA_MAX_JSON_RESPONSE_BYTES)
    return capture_limit_error(
        "response_too_large",
        "capture response exceeds the JSON response limit of " +
            std::to_string(GDA_MAX_JSON_RESPONSE_BYTES) + " bytes");

  if (baseline_storable) {
    g_last_rgba.assign(pixels.ptr(), pixels.ptr() + pixels.size());
    g_last_w = final_width;
    g_last_h = final_height;
    g_has_baseline = true;
  } else {
    g_last_rgba.clear();
    g_last_w = 0;
    g_last_h = 0;
    g_has_baseline = false;
  }

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "capture_editor_viewport completed: " +
          std::to_string(final_width) + "x" + std::to_string(final_height) +
          " space=" + space);
  return r;
}

mcp::JsonValue handle_review_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "review_scene_visually called");
  if (!args.IsObject())
    return util::error_json("review_scene_visually parameters must be an object");
  static constexpr const char *kAllowed[] = {
      "region",           "max_dimension", "scale",        "annotate",
      "annotate_nodes",   "annotate_nodes_max", "include_editor", "include_game",
      "timeout_ms",       "space",       "viewport",     "index",
      "node_viewport"};
  for (const auto &entry : args.GetObject()) {
    bool known = false;
    for (const char *key : kAllowed) {
      if (entry.first == key) {
        known = true;
        break;
      }
    }
    if (!known)
      return util::error_json("unknown parameter for review_scene_visually: " + entry.first);
  }
  if (auto *region = args.Find("region")) {
    if (!region->IsObject())
      return util::error_json("region must be an object with numeric x, y, width and height");
    const mcp::JsonValue *rx = region->Find("x");
    const mcp::JsonValue *ry = region->Find("y");
    const mcp::JsonValue *rw = region->Find("width");
    const mcp::JsonValue *rh = region->Find("height");
    if (!rx || !ry || !rw || !rh || !rx->IsNumber() || !ry->IsNumber() ||
        !rw->IsNumber() || !rh->IsNumber())
      return util::error_json("region must be an object with numeric x, y, width and height");
    const double w = rw->IsInt() ? static_cast<double>(rw->GetInt()) : rw->GetDouble();
    const double h = rh->IsInt() ? static_cast<double>(rh->GetInt()) : rh->GetDouble();
    if (w <= 0.0 || h <= 0.0)
      return util::error_json("region must be an object with numeric x, y, width and height (width/height greater than 0)");
  }
  if (auto *v = args.Find("max_dimension")) {
    if (!v->IsInt() || v->GetInt() < kMaxDimensionMin || v->GetInt() > kMaxDimensionMax)
      return util::error_json("max_dimension must be an integer between 64 and 4096");
  }
  if (auto *v = args.Find("scale")) {
    if (!v->IsInt() || v->GetInt() < kScaleMin || v->GetInt() > kScaleMax)
      return util::error_json("scale must be an integer between 1 and 8");
  }
  if (auto *v = args.Find("annotate")) {
    if (!v->IsBool())
      return util::error_json("annotate must be a boolean");
  }
  const mcp::JsonValue *nodes_param = args.Find("annotate_nodes");
  if (nodes_param) {
    if (!nodes_param->IsArray() || nodes_param->GetArray().empty() ||
        nodes_param->GetArray().size() > 50)
      return util::error_json("annotate_nodes must be an array of 1-50 strings");
    for (const auto &item : nodes_param->GetArray()) {
      if (!item.IsString() || item.GetString().empty())
        return util::error_json("annotate_nodes must be an array of non-empty strings");
    }
  }
  if (auto *v = args.Find("annotate_nodes_max")) {
    if (!v->IsInt() || v->GetInt() < 1 || v->GetInt() > 50)
      return util::error_json("annotate_nodes_max must be an integer between 1 and 50");
  }
  bool include_editor = true;
  if (auto *v = args.Find("include_editor")) {
    if (!v->IsBool())
      return util::error_json("include_editor must be a boolean");
    include_editor = v->GetBool();
  }
  bool include_game = true;
  if (auto *v = args.Find("include_game")) {
    if (!v->IsBool())
      return util::error_json("include_game must be a boolean");
    include_game = v->GetBool();
  }
  if (!include_editor && !include_game)
    return util::error_json("at least one of include_editor/include_game must be true");
  if (auto *v = args.Find("timeout_ms")) {
    if (!v->IsInt() || v->GetInt() <= 0)
      return util::error_json("timeout_ms must be a positive integer");
  }
  std::string space = "viewport";
  if (auto *v = args.Find("space")) {
    if (!v->IsString())
      return util::error_json("space must be a string");
    space = v->GetString();
    if (space != "viewport" && space != "window")
      return util::error_json("space must be 'viewport' or 'window'");
  }
  std::string viewport = "2d";
  if (auto *v = args.Find("viewport")) {
    if (!v->IsString())
      return util::error_json("viewport must be a string");
    viewport = v->GetString();
    if (viewport != "2d" && viewport != "3d")
      return util::error_json("viewport must be '2d' or '3d'");
  }
  int64_t index = 0;
  if (auto *v = args.Find("index")) {
    if (!v->IsInt())
      return util::error_json("index must be an integer");
    index = v->GetInt();
  }
  std::string node_viewport = "auto";
  if (auto *v = args.Find("node_viewport")) {
    if (!v->IsString())
      return util::error_json("node_viewport must be a string");
    node_viewport = v->GetString();
    if (node_viewport != "auto" && node_viewport != "2d" && node_viewport != "3d")
      return util::error_json("node_viewport must be one of: auto, 2d, 3d");
  }
  auto copy_optional = [](const mcp::JsonValue &from, mcp::JsonValue &to, const char *key) {
    if (const mcp::JsonValue *v = from.Find(key))
      to[key] = *v;
  };
  auto unwrap_section = [](const mcp::JsonValue &response) {
    mcp::JsonValue section(mcp::JsonValue::object_tag);
    if (const mcp::JsonValue *r = response.Find("result")) {
      section = *r;
      return section;
    }
    if (const mcp::JsonValue *e = response.Find("error")) {
      section["error"] = *e;
      return section;
    }
    section["error"] = mcp::JsonValue("unexpected sub-call response shape");
    return section;
  };
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  if (!include_editor) {
    mcp::JsonValue skipped(mcp::JsonValue::object_tag);
    skipped["skipped"] = mcp::JsonValue(true);
    inner["editor_capture"] = std::move(skipped);
  } else {
    mcp::JsonValue editor_args(mcp::JsonValue::object_tag);
    editor_args["target"] = mcp::JsonValue("editor");
    editor_args["space"] = mcp::JsonValue(space);
    copy_optional(args, editor_args, "region");
    copy_optional(args, editor_args, "max_dimension");
    copy_optional(args, editor_args, "scale");
    copy_optional(args, editor_args, "annotate");
    copy_optional(args, editor_args, "annotate_nodes");
    copy_optional(args, editor_args, "annotate_nodes_max");
    inner["editor_capture"] = unwrap_section(handle_capture_viewport(editor_args));
  }
  if (!include_game) {
    mcp::JsonValue skipped(mcp::JsonValue::object_tag);
    skipped["skipped"] = mcp::JsonValue(true);
    inner["game_capture"] = std::move(skipped);
  } else {
    mcp::JsonValue game_args(mcp::JsonValue::object_tag);
    copy_optional(args, game_args, "region");
    copy_optional(args, game_args, "max_dimension");
    copy_optional(args, game_args, "scale");
    copy_optional(args, game_args, "annotate");
    copy_optional(args, game_args, "annotate_nodes");
    copy_optional(args, game_args, "annotate_nodes_max");
    copy_optional(args, game_args, "timeout_ms");
    inner["game_capture"] = unwrap_section(runtime_ops::handle_game_capture(game_args));
  }
  if (!nodes_param) {
    mcp::JsonValue skipped(mcp::JsonValue::object_tag);
    skipped["skipped"] = mcp::JsonValue(true);
    skipped["reason"] = mcp::JsonValue("no annotate_nodes");
    inner["nodes"] = std::move(skipped);
  } else {
    mcp::JsonValue node_args(mcp::JsonValue::object_tag);
    node_args["paths"] = *nodes_param;
    node_args["viewport"] = mcp::JsonValue(node_viewport);
    inner["nodes"] = unwrap_section(scene_ops::handle_get_node_screen_rect(node_args));
  }
  {
    mcp::JsonValue geom_args(mcp::JsonValue::object_tag);
    geom_args["viewport"] = mcp::JsonValue(viewport);
    geom_args["index"] = mcp::JsonValue(index);
    inner["mapping"] = unwrap_section(editor_ui_ops::handle_get_editor_viewport_geometry(geom_args));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  if (r.Dump().size() > GDA_MAX_JSON_RESPONSE_BYTES)
    return capture_limit_error(
        "response_too_large",
        "review response exceeds the JSON response limit of " +
            std::to_string(GDA_MAX_JSON_RESPONSE_BYTES) + " bytes");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "review_scene_visually completed");
  return r;
}

} // namespace capture_ops
} // namespace godot_autopilot
