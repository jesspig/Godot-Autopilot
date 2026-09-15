#include "capture_ops.hpp"
#include "../util/error_util.hpp"
#include "core/config.hpp"
#include "core/editor_coords.hpp"
#include "core/log_system.hpp"
#include "tools/editor_ui_ops.hpp"
#include "tools/runtime_ops.hpp"
#include "util/json_godot.hpp"
#include <algorithm>
#include <cmath>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>
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

  if (target == "game") {
    int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
    if (auto *tp = args.Find("timeout_ms")) {
      if (tp->IsInt() && tp->GetInt() > 0)
        timeout_ms = tp->GetInt();
    }
    if (timeout_ms > GDA_MAX_TIMEOUT_MS)
      timeout_ms = GDA_MAX_TIMEOUT_MS;

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
    return runtime_ops::handle_gda_send("capture", params, timeout_ms);
  }
  if (target != "editor") {
    return util::error_detail("invalid target '" + target + "'",
                              "capture_ops.cpp handle_capture_viewport",
                              "'editor' or 'game'",
                              "pass target='editor' or target='game'");
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");

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
  if (diff_against_last) {
    diff["comparable"] = mcp::JsonValue(false);
    diff["changed_ratio"] = mcp::JsonValue(0.0);
    if (g_has_baseline) {
      mcp::JsonValue baseline_size(mcp::JsonValue::object_tag);
      baseline_size["x"] = mcp::JsonValue(g_last_w);
      baseline_size["y"] = mcp::JsonValue(g_last_h);
      diff["baseline_size"] = std::move(baseline_size);
    }
    const auto fill_diff_sample = [&diff](const coords::DiffResult &sample) {
      diff["comparable"] = mcp::JsonValue(sample.comparable);
      diff["changed_ratio"] = mcp::JsonValue(sample.changed_ratio);
      if (sample.has_bbox) {
        mcp::JsonValue bbox(mcp::JsonValue::object_tag);
        bbox["x"] = mcp::JsonValue(sample.bbox.x);
        bbox["y"] = mcp::JsonValue(sample.bbox.y);
        bbox["w"] = mcp::JsonValue(sample.bbox.w);
        bbox["h"] = mcp::JsonValue(sample.bbox.h);
        diff["changed_bbox"] = std::move(bbox);
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

} // namespace capture_ops
} // namespace godot_autopilot
