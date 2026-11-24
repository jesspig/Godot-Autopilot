#include "tools/domains/render/render_ops.hpp"
#include "tools/domains/render/render_budget.hpp"
#include "core/config.hpp"
#include "core/observability/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/domains/capture/capture_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/rid_registry.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/cpu_particles2d.hpp>
#include <godot_cpp/classes/cpu_particles3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/gpu_particles2d.hpp>
#include <godot_cpp/classes/gpu_particles3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cctype>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace render_ops {

using JV = mcp::JsonValue;

namespace {

std::string action_of(const JV &args) {
  const JV *value = args.Find("action");
  if (!value || !value->IsString()) {
    return std::string();
  }
  return value->GetString();
}

JV missing_action_error() {
  JV r(JV::object_tag);
  r["error"] = JV("missing required parameter: action");
  return r;
}

JV missing_for_action(const std::string &action, const char *name) {
  JV r(JV::object_tag);
  r["error"] = JV("missing required parameter for action '" + action + "': " +
                  name);
  return r;
}

JV unknown_action_error(const char *tool, const std::string &action) {
  JV r(JV::object_tag);
  r["error"] = JV("unknown action for " + std::string(tool) + ": " + action);
  return r;
}

JV ok_result() {
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV invalid_rid_error(const char *name, int64_t id) {
  JV r(JV::object_tag);
  r["error"] =
      JV(std::string(name) + " is not a valid RID: " + std::to_string(id));
  return r;
}

bool read_rid(const JV &args, const char *key, godot::RID *out) {
  const JV *value = args.Find(key);
  if (!value || !value->IsNumber()) {
    return false;
  }
  *out = util::rid_from_json(*value);
  return true;
}

godot::Vector3 vec3_field(const JV *value) {
  if (!value || !value->IsObject()) {
    return godot::Vector3();
  }
  return util::json_to_vec3(*value);
}

godot::Transform3D transform3d_from_args(const JV &args) {
  godot::Transform3D xform;
  const JV *scope = &args;
  const JV *transform = args.Find("transform");
  if (transform && transform->IsObject()) {
    scope = transform;
  }
  const JV *origin = scope->Find("origin");
  if (origin && origin->IsObject()) {
    xform.origin = util::json_to_vec3(*origin);
  } else {
    xform.origin = godot::Vector3(
        static_cast<float>(util::json_number(scope->Find("origin_x"), 0.0)),
        static_cast<float>(util::json_number(scope->Find("origin_y"), 0.0)),
        static_cast<float>(util::json_number(scope->Find("origin_z"), 0.0)));
  }
  const JV *basis = scope->Find("basis");
  if (basis && basis->IsObject()) {
    xform.basis = godot::Basis(vec3_field(basis->Find("x")),
                               vec3_field(basis->Find("y")),
                               vec3_field(basis->Find("z")));
    return xform;
  }
  const JV *rotation = scope->Find("rotation");
  bool has_rotation = rotation && rotation->IsObject();
  has_rotation = has_rotation || scope->Find("rotation_x") != nullptr ||
                 scope->Find("rotation_y") != nullptr ||
                 scope->Find("rotation_z") != nullptr;
  if (has_rotation) {
    godot::Vector3 degrees =
        rotation && rotation->IsObject()
            ? util::json_to_vec3(*rotation)
            : godot::Vector3(
                  static_cast<float>(
                      util::json_number(scope->Find("rotation_x"), 0.0)),
                  static_cast<float>(
                      util::json_number(scope->Find("rotation_y"), 0.0)),
                  static_cast<float>(
                      util::json_number(scope->Find("rotation_z"), 0.0)));
    const double deg_to_rad = 3.14159265358979323846 / 180.0;
    xform.basis = godot::Basis::from_euler(
        degrees * static_cast<float>(deg_to_rad));
  }
  return xform;
}

JV image_to_png_result(const godot::Ref<godot::Image> &image) {
  if (image.is_null() || image->is_empty()) {
    JV r(JV::object_tag);
    r["error"] = JV("texture image is empty");
    r["code"] = JV("texture_image_empty");
    r["retryable"] = JV(true);
    return r;
  }
  godot::PackedByteArray png = image->save_png_to_buffer();
  if (png.size() == 0) {
    JV r(JV::object_tag);
    r["error"] = JV("PNG encoding returned empty buffer");
    r["code"] = JV("texture_png_encode_failed");
    r["retryable"] = JV(false);
    return r;
  }
  if (static_cast<size_t>(png.size()) > GDA_CAPTURE_MAX_PNG_BYTES) {
    JV r(JV::object_tag);
    r["error"] = JV("encoded PNG exceeds the capture limit of " +
                    std::to_string(GDA_CAPTURE_MAX_PNG_BYTES) + " bytes");
    r["code"] = JV("capture_bytes_exceeded");
    r["retryable"] = JV(false);
    return r;
  }
  JV inner(JV::object_tag);
  inner["format"] = JV("png");
  inner["data"] = JV(capture_ops::base64_encode(
      png.ptr(), static_cast<size_t>(png.size())));
  inner["width"] = JV(static_cast<int64_t>(image->get_width()));
  inner["height"] = JV(static_cast<int64_t>(image->get_height()));
  return inner;
}

std::string trimmed_token(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    begin += 1;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    end -= 1;
  }
  return text.substr(begin, end - begin);
}

JV shader_parameter_list(const std::string &code) {
  JV params(JV::array_tag);
  size_t pos = 0;
  while (pos <= code.size()) {
    size_t next = code.find('\n', pos);
    if (next == std::string::npos) {
      next = code.size();
    }
    std::string line = code.substr(pos, next - pos);
    pos = next + 1;
    const size_t comment = line.find("//");
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }
    const size_t semicolon = line.find(';');
    if (semicolon != std::string::npos) {
      line = line.substr(0, semicolon);
    }
    std::string trimmed = trimmed_token(line);
    bool is_global = false;
    if (trimmed.rfind("global ", 0) == 0) {
      is_global = true;
      trimmed = trimmed_token(trimmed.substr(7));
    }
    if (trimmed.rfind("uniform", 0) != 0) {
      continue;
    }
    std::string rest = trimmed_token(trimmed.substr(7));
    const size_t colon = rest.find(':');
    std::string hint;
    if (colon != std::string::npos) {
      hint = trimmed_token(rest.substr(colon + 1));
      rest = trimmed_token(rest.substr(0, colon));
    }
    size_t split = rest.find_first_of(" \t");
    if (split == std::string::npos) {
      continue;
    }
    std::string type = rest.substr(0, split);
    std::string name = trimmed_token(rest.substr(split + 1));
    while (!name.empty() &&
           (name.back() == ']' || name.back() == '[' || name.back() == ',' ||
            std::isspace(static_cast<unsigned char>(name.back())))) {
      name.pop_back();
    }
    const size_t bracket = name.find('[');
    if (bracket != std::string::npos) {
      name = name.substr(0, bracket);
    }
    if (name.empty()) {
      continue;
    }
    JV entry(JV::object_tag);
    entry["name"] = JV(name);
    entry["type"] = JV(type);
    entry["hint"] = JV(hint);
    entry["global"] = JV(is_global);
    params.PushBack(std::move(entry));
  }
  return params;
}

} // namespace

JV handle_canvas_item_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_canvas_item called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID rid = rs->canvas_item_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_canvas_item completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_canvas_item_draw_rect(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  auto *it_rect = args.Find("rect");
  auto *it_color = args.Find("color");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }
  if (!it_rect || !it_rect->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: rect");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_render_canvas_item_rect called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  godot::Rect2 r = util::json_to_rect2(*it_rect);
  godot::Color c = util::json_to_color(*it_color);
  auto *aa = args.Find("antialiased");
  bool antialiased = aa ? aa->GetBool() : false;

  rs->canvas_item_add_rect(ci, r, c, antialiased);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "add_render_canvas_item_rect completed");
  JV ret(JV::object_tag);
  ret["result"] = JV("ok");
  return ret;
}

JV handle_canvas_item_draw_circle(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  auto *it_pos = args.Find("position");
  auto *it_color = args.Find("color");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_render_canvas_item_circle called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  godot::Vector2 pos = util::json_to_vec2(*it_pos);
  auto *rad = args.Find("radius");
  float radius = static_cast<float>(util::json_number(rad, 1.0));
  godot::Color c = util::json_to_color(*it_color);
  auto *aa = args.Find("antialiased");
  bool antialiased = aa ? aa->GetBool() : false;

  rs->canvas_item_add_circle(ci, pos, radius, c, antialiased);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "add_render_canvas_item_circle completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_canvas_item_draw_texture(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  auto *it_tex = args.Find("texture_rid");
  auto *it_rect = args.Find("rect");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }
  if (!it_tex || !it_tex->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: texture_rid");
    return r;
  }
  if (!it_rect || !it_rect->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: rect");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_render_canvas_item_texture_rect called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  godot::RID tex = util::rid_from_json(*it_tex);
  godot::Rect2 r = util::json_to_rect2(*it_rect);
  auto *tile_a = args.Find("tile");
  bool tile = tile_a ? tile_a->GetBool() : false;
  godot::Color modulate(1, 1, 1, 1);
  auto *it_mod = args.Find("modulate");
  if (it_mod && it_mod->IsObject()) {
    modulate = util::json_to_color(*it_mod);
  }
  auto *transp = args.Find("transpose");
  bool transpose = transp ? transp->GetBool() : false;

  rs->canvas_item_add_texture_rect(ci, r, tex, tile, modulate, transpose);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "add_render_canvas_item_texture_rect completed");
  JV ret(JV::object_tag);
  ret["result"] = JV("ok");
  return ret;
}

JV handle_canvas_item_draw_line(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  auto *it_from = args.Find("from");
  auto *it_to = args.Find("to");
  auto *it_color = args.Find("color");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }
  if (!it_from || !it_from->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: from");
    return r;
  }
  if (!it_to || !it_to->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: to");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_render_canvas_item_line called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  godot::Vector2 from = util::json_to_vec2(*it_from);
  godot::Vector2 to = util::json_to_vec2(*it_to);
  godot::Color c = util::json_to_color(*it_color);
  auto *w = args.Find("width");
  float width = static_cast<float>(util::json_number(w, -1.0));
  auto *aa = args.Find("antialiased");
  bool antialiased = aa ? aa->GetBool() : false;

  rs->canvas_item_add_line(ci, from, to, c, width, antialiased);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "add_render_canvas_item_line completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_canvas_item_set_transform(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_canvas_item_transform called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  godot::Transform2D xform;
  auto *xa = args.Find("x");
  auto *ya = args.Find("y");
  auto *ox = args.Find("origin_x");
  auto *oy = args.Find("origin_y");
  xform.columns[0] =
      godot::Vector2(util::json_number(xa, 1.0), 0.0);
  xform.columns[1] =
      godot::Vector2(0.0, util::json_number(ya, 1.0));
  xform.columns[2] = godot::Vector2(util::json_number(ox, 0.0),
                                    util::json_number(oy, 0.0));

  rs->canvas_item_set_transform(ci, xform);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_canvas_item_transform completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_canvas_item_set_visible(const JV &args) {
  auto *it_ci = args.Find("canvas_item_rid");
  if (!it_ci || !it_ci->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: canvas_item_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_canvas_item_visible called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID ci = util::rid_from_json(*it_ci);
  auto *vis = args.Find("visible");
  bool visible = vis ? vis->GetBool() : true;

  rs->canvas_item_set_visible(ci, visible);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_canvas_item_visible completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_scenario_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_scenario called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->scenario_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_scenario completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_scenario_set_environment(const JV &args) {
  auto *it_sc = args.Find("scenario_rid");
  auto *it_env = args.Find("environment_rid");
  if (!it_sc || !it_sc->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: scenario_rid");
    return r;
  }
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_scenario_environment called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID sc = util::rid_from_json(*it_sc);
  godot::RID env = util::rid_from_json(*it_env);

  rs->scenario_set_environment(sc, env);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_scenario_environment completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_camera_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_camera called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->camera_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_camera completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_camera_set_transform(const JV &args) {
  auto *it_cam = args.Find("camera_rid");
  if (!it_cam || !it_cam->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: camera_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_camera_transform called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID cam = util::rid_from_json(*it_cam);
  godot::Transform3D xform;
  auto *ox = args.Find("origin_x");
  auto *oy = args.Find("origin_y");
  auto *oz = args.Find("origin_z");
  xform.origin = godot::Vector3(util::json_number(ox, 0.0),
                                util::json_number(oy, 0.0),
                                util::json_number(oz, 0.0));

  rs->camera_set_transform(cam, xform);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_camera_transform completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_camera_set_perspective(const JV &args) {
  auto *it_cam = args.Find("camera_rid");
  if (!it_cam || !it_cam->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: camera_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_camera_perspective called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID cam = util::rid_from_json(*it_cam);
  auto *fovy_a = args.Find("fovy_degrees");
  auto *znear_a = args.Find("z_near");
  auto *zfar_a = args.Find("z_far");
  float fovy = static_cast<float>(util::json_number(fovy_a, 75.0));
  float znear = static_cast<float>(util::json_number(znear_a, 0.01));
  float zfar = static_cast<float>(util::json_number(zfar_a, 4000.0));

  rs->camera_set_perspective(cam, fovy, znear, zfar);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_camera_perspective completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_camera_set_orthogonal(const JV &args) {
  auto *it_cam = args.Find("camera_rid");
  if (!it_cam || !it_cam->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: camera_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_camera_orthogonal called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID cam = util::rid_from_json(*it_cam);
  auto *size_a = args.Find("size");
  auto *znear_a = args.Find("z_near");
  auto *zfar_a = args.Find("z_far");
  float size = static_cast<float>(util::json_number(size_a, 10.0));
  float znear = static_cast<float>(util::json_number(znear_a, 0.01));
  float zfar = static_cast<float>(util::json_number(zfar_a, 4000.0));

  rs->camera_set_orthogonal(cam, size, znear, zfar);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_camera_orthogonal completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_light_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_light called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  auto *type_a = args.Find("type");
  std::string type = type_a ? type_a->GetString() : "directional";
  godot::RID rid;
  if (type == "directional") {
    rid = rs->directional_light_create();
  } else if (type == "omni") {
    rid = rs->omni_light_create();
  } else if (type == "spot") {
    rid = rs->spot_light_create();
  } else {
    JV r(JV::object_tag);
    r["error"] =
        JV("unknown light type: " + type + " (use directional, omni, or spot)");
    return r;
  }
  util::rid_store<RenderRidDomain>().store(rid);

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_light completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_light_set_param(const JV &args) {
  auto *it_light = args.Find("light_rid");
  auto *it_param = args.Find("param");
  auto *it_val = args.Find("value");
  if (!it_light || !it_light->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: light_rid");
    return r;
  }
  if (!it_param || !it_param->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: param");
    return r;
  }
  if (!it_val || !it_val->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_light_param called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID light = util::rid_from_json(*it_light);
  auto param = static_cast<godot::RenderingServer::LightParam>(
      static_cast<int>(it_param->GetInt()));
  float val =
      static_cast<float>(it_val->IsInt() ? static_cast<double>(it_val->GetInt())
                                         : it_val->GetDouble());

  rs->light_set_param(light, param, val);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_light_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_light_set_color(const JV &args) {
  auto *it_light = args.Find("light_rid");
  auto *it_color = args.Find("color");
  if (!it_light || !it_light->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: light_rid");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_light_color called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID light = util::rid_from_json(*it_light);
  godot::Color c = util::json_to_color(*it_color);

  rs->light_set_color(light, c);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_light_color completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_mesh_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_mesh called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->mesh_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_mesh completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_mesh_add_surface(const JV &args) {
  auto *it_mesh = args.Find("mesh_rid");
  if (!it_mesh || !it_mesh->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mesh_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_render_mesh_surface called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID mesh = util::rid_from_json(*it_mesh);
  auto *prim_a = args.Find("primitive");
  auto primitive = static_cast<godot::RenderingServer::PrimitiveType>(
      prim_a ? static_cast<int>(prim_a->GetInt())
             : static_cast<int>(godot::RenderingServer::PRIMITIVE_TRIANGLES));

  godot::Array arrays;
  arrays.resize(godot::RenderingServer::ARRAY_MAX);

  auto *it_arrays = args.Find("arrays");
  if (it_arrays && it_arrays->IsObject()) {
    const JV &arr = *it_arrays;

    auto *it_verts = arr.Find("vertices");
    if (it_verts && it_verts->IsArray()) {
      godot::PackedVector3Array verts;
      const JV::Array &vert_list = it_verts->GetArray();
      for (const auto &v : vert_list) {
        verts.append(util::json_to_vec3(v));
      }
      arrays[godot::RenderingServer::ARRAY_VERTEX] = verts;
    }

    auto *it_norms = arr.Find("normals");
    if (it_norms && it_norms->IsArray()) {
      godot::PackedVector3Array norms;
      const JV::Array &norm_list = it_norms->GetArray();
      for (const auto &n : norm_list) {
        norms.append(util::json_to_vec3(n));
      }
      arrays[godot::RenderingServer::ARRAY_NORMAL] = norms;
    }

    auto *it_tang = arr.Find("tangents");
    if (it_tang && it_tang->IsArray()) {
      godot::PackedFloat32Array tangs;
      const JV::Array &tang_list = it_tang->GetArray();
      for (const auto &t : tang_list) {
        tangs.append(static_cast<float>(
            t.IsInt() ? static_cast<double>(t.GetInt()) : t.GetDouble()));
      }
      arrays[godot::RenderingServer::ARRAY_TANGENT] = tangs;
    }

    auto *it_colors = arr.Find("colors");
    if (it_colors && it_colors->IsArray()) {
      godot::PackedColorArray cols;
      const JV::Array &color_list = it_colors->GetArray();
      for (const auto &c : color_list) {
        cols.append(util::json_to_color(c));
      }
      arrays[godot::RenderingServer::ARRAY_COLOR] = cols;
    }

    auto *it_uvs = arr.Find("uvs");
    if (it_uvs && it_uvs->IsArray()) {
      godot::PackedVector2Array uvs;
      const JV::Array &uv_list = it_uvs->GetArray();
      for (const auto &uv : uv_list) {
        uvs.append(util::json_to_vec2(uv));
      }
      arrays[godot::RenderingServer::ARRAY_TEX_UV] = uvs;
    }

    auto *it_idx = arr.Find("indices");
    if (it_idx && it_idx->IsArray()) {
      godot::PackedInt32Array idx;
      const JV::Array &idx_list = it_idx->GetArray();
      for (const auto &i : idx_list) {
        idx.append(static_cast<int32_t>(i.GetInt()));
      }
      arrays[godot::RenderingServer::ARRAY_INDEX] = idx;
    }
  }

  rs->mesh_add_surface_from_arrays(mesh, primitive, arrays);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "add_render_mesh_surface completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_mesh_set_material(const JV &args) {
  auto *it_mesh = args.Find("mesh_rid");
  auto *it_surf = args.Find("surface");
  auto *it_mat = args.Find("material_rid");
  if (!it_mesh || !it_mesh->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mesh_rid");
    return r;
  }
  if (!it_surf || !it_surf->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: surface");
    return r;
  }
  if (!it_mat || !it_mat->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: material_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_mesh_surface_material called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID mesh = util::rid_from_json(*it_mesh);
  int surface = static_cast<int>(it_surf->GetInt());
  godot::RID material = util::rid_from_json(*it_mat);

  rs->mesh_surface_set_material(mesh, surface, material);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_mesh_surface_material completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_material_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_material called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->material_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_material completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_material_set_param(const JV &args) {
  auto *it_mat = args.Find("material_rid");
  auto *it_param = args.Find("parameter");
  auto *it_val = args.Find("value");
  if (!it_mat || !it_mat->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: material_rid");
    return r;
  }
  if (!it_param || !it_param->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: parameter");
    return r;
  }
  if (!it_val) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_material_param called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID material = util::rid_from_json(*it_mat);
  godot::StringName param_name(it_param->GetString().c_str());
  auto *th = args.Find("type_hint");
  std::string type_hint = th ? th->GetString() : "";
  godot::Variant val = VariantJson::deserialize(*it_val, type_hint);

  rs->material_set_param(material, param_name, val);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_material_param completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_viewport_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_viewport called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->viewport_create();
  util::rid_store<RenderRidDomain>().store(rid);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_viewport completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_viewport_set_size(const JV &args) {
  auto *it_vp = args.Find("viewport_rid");
  if (!it_vp || !it_vp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: viewport_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_viewport_size called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID vp = util::rid_from_json(*it_vp);
  auto *w = args.Find("width");
  auto *h = args.Find("height");
  if ((w != nullptr && !w->IsNumber()) ||
      (h != nullptr && !h->IsNumber())) {
    JV r(JV::object_tag);
    r["error"] = JV("width and height must be integers");
    r["code"] = JV("invalid_dimension");
    return r;
  }
  const int64_t raw_width = w != nullptr ? w->GetInt() : 640;
  const int64_t raw_height = h != nullptr ? h->GetInt() : 480;
  const render_budget::ViewportSizeVerdict verdict =
      render_budget::validate_viewport_size(raw_width, raw_height);
  if (verdict.failure != render_budget::ViewportSizeFailure::kNone) {
    JV r(JV::object_tag);
    r["error"] = JV("viewport size rejected by budget: width=" +
                    std::to_string(raw_width) +
                    " height=" + std::to_string(raw_height));
    r["code"] = JV(verdict.failure ==
                           render_budget::ViewportSizeFailure::kInvalidDimension
                       ? "invalid_dimension"
                       : verdict.failure ==
                                 render_budget::ViewportSizeFailure::kDimensionLimit
                             ? "dimension_limit"
                             : "pixel_limit");
    r["max_dimension"] =
        JV(static_cast<int64_t>(render_budget::kMaxViewportDimension));
    r["max_pixels"] =
        JV(static_cast<int64_t>(render_budget::kMaxViewportPixels));
    return r;
  }

  rs->viewport_set_size(vp, verdict.width, verdict.height);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_viewport_size completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_viewport_set_clear_mode(const JV &args) {
  auto *it_vp = args.Find("viewport_rid");
  if (!it_vp || !it_vp->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: viewport_rid");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_viewport_clear_mode called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID vp = util::rid_from_json(*it_vp);
  auto *cm = args.Find("clear_mode");
  auto clear_mode = static_cast<godot::RenderingServer::ViewportClearMode>(
      cm ? static_cast<int>(cm->GetInt()) : 0);

  rs->viewport_set_clear_mode(vp, clear_mode);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_viewport_clear_mode completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_particle_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_particles called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->particles_create();
  util::rid_store<RenderRidDomain>().store(rid);
  auto *mode_a = args.Find("mode");
  int mode = mode_a ? static_cast<int>(mode_a->GetInt()) : 1;
  rs->particles_set_mode(
      rid, static_cast<godot::RenderingServer::ParticlesMode>(mode));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_particles completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_fog_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_fog_volume called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->fog_volume_create();
  util::rid_store<RenderRidDomain>().store(rid);

  auto *it_shape = args.Find("shape");
  if (it_shape && it_shape->IsNumber()) {
    rs->fog_volume_set_shape(
        rid, static_cast<godot::RenderingServer::FogVolumeShape>(
                 static_cast<int>(it_shape->GetInt())));
  }

  auto *it_size = args.Find("size");
  if (it_size && it_size->IsObject()) {
    rs->fog_volume_set_size(rid, util::json_to_vec3(*it_size));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_fog_volume completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_shader_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_shader called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID rid = rs->shader_create();
  util::rid_store<RenderRidDomain>().store(rid);

  auto *it_code = args.Find("code");
  if (it_code && it_code->IsString()) {
    rs->shader_set_code(rid, godot::String(it_code->GetString().c_str()));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_render_shader completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_texture_create_2d(const JV &args) {
  auto *it_path = args.Find("image_path");
  if (!it_path || !it_path->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: image_path");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_texture_from_image called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  auto img =
      godot::Image::load_from_file(godot::String(it_path->GetString().c_str()));
  if (img.is_null()) {
    JV r(JV::object_tag);
    r["error"] = JV("failed to load image");
    return r;
  }
  godot::RID rid = rs->texture_2d_create(img);
  util::rid_store<RenderRidDomain>().store(rid);
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_shader_set_code(const JV &args) {
  auto *it_sh = args.Find("shader_rid");
  auto *it_code = args.Find("code");
  if (!it_sh || !it_sh->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: shader_rid");
    return r;
  }
  if (!it_code || !it_code->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: code");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_shader_code called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID sh = util::rid_from_json(*it_sh);
  rs->shader_set_code(sh, godot::String(it_code->GetString().c_str()));
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_sky_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_sky called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID rid = rs->sky_create();
  util::rid_store<RenderRidDomain>().store(rid);
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_sky_set_material(const JV &args) {
  auto *it_sky = args.Find("sky_rid");
  auto *it_mat = args.Find("material_rid");
  if (!it_sky || !it_sky->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: sky_rid");
    return r;
  }
  if (!it_mat || !it_mat->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: material_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_sky_material called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID sky = util::rid_from_json(*it_sky);
  godot::RID mat = util::rid_from_json(*it_mat);
  rs->sky_set_material(sky, mat);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_particles_set_emitting(const JV &args) {
  auto *it_p = args.Find("particles_rid");
  if (!it_p || !it_p->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: particles_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_particles_emitting called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID p = util::rid_from_json(*it_p);
  bool emitting = true;
  auto *em = args.Find("emitting");
  if (em && em->IsBool())
    emitting = em->GetBool();
  rs->particles_set_emitting(p, emitting);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_particles_restart(const JV &args) {
  auto *it_p = args.Find("particles_rid");
  if (!it_p || !it_p->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: particles_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "restart_render_particles called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID p = util::rid_from_json(*it_p);
  rs->particles_restart(p);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_particles_set_lifetime(const JV &args) {
  auto *it_p = args.Find("particles_rid");
  if (!it_p || !it_p->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: particles_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_particles_lifetime called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID p = util::rid_from_json(*it_p);
  auto *lt = args.Find("lifetime");
  float lifetime = static_cast<float>(util::json_number(lt, 1.0));
  rs->particles_set_lifetime(p, lifetime);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_reflection_probe_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_reflection_probe called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID rid = rs->reflection_probe_create();
  util::rid_store<RenderRidDomain>().store(rid);
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_decal_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_render_decal called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID rid = rs->decal_create();
  util::rid_store<RenderRidDomain>().store(rid);
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(rid);
  return r;
}

JV handle_fog_volume_set_shape(const JV &args) {
  auto *it_fog = args.Find("fog_rid");
  auto *it_shape = args.Find("shape");
  if (!it_fog || !it_fog->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: fog_rid");
    return r;
  }
  if (!it_shape || !it_shape->IsInt()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: shape");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_fog_volume_shape called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID fog = util::rid_from_json(*it_fog);
  int shape = static_cast<int>(it_shape->GetInt());
  rs->fog_volume_set_shape(
      fog, static_cast<godot::RenderingServer::FogVolumeShape>(shape));
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_instance_set_visible(const JV &args) {
  auto *it_inst = args.Find("instance_rid");
  if (!it_inst || !it_inst->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: instance_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_instance_visible called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID inst = util::rid_from_json(*it_inst);
  bool visible = true;
  auto *vs = args.Find("visible");
  if (vs && vs->IsBool())
    visible = vs->GetBool();
  rs->instance_set_visible(inst, visible);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_instance_set_layer_mask(const JV &args) {
  auto *it_inst = args.Find("instance_rid");
  auto *it_mask = args.Find("mask");
  if (!it_inst || !it_inst->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: instance_rid");
    return r;
  }
  if (!it_mask || !it_mask->IsInt()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mask");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_instance_layer_mask called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID inst = util::rid_from_json(*it_inst);
  uint32_t mask = static_cast<uint32_t>(it_mask->GetInt());
  rs->instance_set_layer_mask(inst, mask);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_global_shader_parameter_set(const JV &args) {
  auto *it_name = args.Find("name");
  auto *it_val = args.Find("value");
  if (!it_name || !it_name->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: name");
    return r;
  }
  if (!it_val) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: value");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_shader_parameter_global called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::StringName name(it_name->GetString().c_str());
  auto *th = args.Find("type_hint");
  std::string type_hint = th ? th->GetString() : "";
  godot::Variant val = VariantJson::deserialize(*it_val, type_hint);
  rs->global_shader_parameter_set(name, val);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_canvas_item_get_rid(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_render_canvas_item_rid called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: path");
    return r;
  }
  std::string path = pp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  godot::Node *node = util::resolve_scene_node(path, root);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path);
    return r;
  }
  auto *ci = godot::Object::cast_to<godot::CanvasItem>(node);
  if (!ci) {
    JV r(JV::object_tag);
    r["error"] = JV("node is not a CanvasItem: " + path);
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_render_canvas_item_rid completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(ci->get_canvas_item());
  return r;
}

JV handle_resolve_rid(const JV &args) {
  auto *idp = args.Find("rid");
  if (!idp || !idp->IsInt()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: rid (integer)");
    return r;
  }
  godot::RID rid = util::rid_from_json(*idp);
  JV r(JV::object_tag);
  r["rid"] = JV(static_cast<int64_t>(rid.get_id()));
  r["is_valid"] = JV(rid.is_valid());
  if (!rid.is_valid()) {
    r["result"] = JV("ok");
    return r;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *root = editor->get_edited_scene_root();
    if (root) {
      JV nodes_arr(JV::array_tag);
      godot::TypedArray<godot::Node> all_nodes = root->get_children(true);
      for (int64_t i = 0; i < all_nodes.size(); i++) {
        auto *node = godot::Object::cast_to<godot::Node>(all_nodes[i]);
        if (!node)
          continue;
        auto *ci = godot::Object::cast_to<godot::CanvasItem>(node);
        if (ci && ci->get_canvas_item() == rid) {
          JV nj(JV::object_tag);
          nj["node_path"] = JV(util::to_std(node->get_path()));
          nj["type"] = JV("canvas_item");
          nodes_arr.PushBack(std::move(nj));
        }
      }
      if (nodes_arr.Size() > 0) {
        r["nodes"] = std::move(nodes_arr);
      }
    }
  }
  r["result"] = JV("ok");
  return r;
}

struct NodeParticles {
  godot::GPUParticles2D *gpu2 = nullptr;
  godot::GPUParticles3D *gpu3 = nullptr;
  godot::CPUParticles2D *cpu2 = nullptr;
  godot::CPUParticles3D *cpu3 = nullptr;
  bool valid = false;
  std::string type;
};

NodeParticles resolve_node_particles(godot::Node *node) {
  NodeParticles out;
  if (!node) {
    return out;
  }
  out.gpu2 = godot::Object::cast_to<godot::GPUParticles2D>(node);
  if (out.gpu2) {
    out.valid = true;
    out.type = "GPUParticles2D";
    return out;
  }
  out.gpu3 = godot::Object::cast_to<godot::GPUParticles3D>(node);
  if (out.gpu3) {
    out.valid = true;
    out.type = "GPUParticles3D";
    return out;
  }
  out.cpu2 = godot::Object::cast_to<godot::CPUParticles2D>(node);
  if (out.cpu2) {
    out.valid = true;
    out.type = "CPUParticles2D";
    return out;
  }
  out.cpu3 = godot::Object::cast_to<godot::CPUParticles3D>(node);
  if (out.cpu3) {
    out.valid = true;
    out.type = "CPUParticles3D";
    return out;
  }
  return out;
}

JV handle_node_particles_restart(const JV &args) {
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: path");
    return r;
  }
  std::string path = pp->GetString();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "restart_particles called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(path, root, &hint);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path + " - " + hint);
    return r;
  }
  NodeParticles p = resolve_node_particles(node);
  if (!p.valid) {
    JV r(JV::object_tag);
    r["error"] = JV("node is not a GPUParticles2D/GPUParticles3D/CPUParticles2D/CPUParticles3D: " + path + " (actual class: " + util::to_std(node->get_class()) + ")");
    return r;
  }
  if (p.gpu2) {
    p.gpu2->restart();
  } else if (p.gpu3) {
    p.gpu3->restart();
  } else if (p.cpu2) {
    p.cpu2->restart();
  } else if (p.cpu3) {
    p.cpu3->restart();
  }
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "restart_particles completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_node_particles_set_params(const JV &args) {
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: path");
    return r;
  }
  std::string path = pp->GetString();
  auto *em = args.Find("emitting");
  auto *am = args.Find("amount");
  auto *lt = args.Find("lifetime");
  auto *os = args.Find("one_shot");
  auto *ss = args.Find("speed_scale");
  auto *ex = args.Find("explosiveness");
  if (em && !em->IsBool()) {
    JV r(JV::object_tag);
    r["error"] = JV("emitting must be a boolean");
    return r;
  }
  if (am && !am->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("amount must be an integer");
    return r;
  }
  if (lt && !lt->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("lifetime must be a number");
    return r;
  }
  if (os && !os->IsBool()) {
    JV r(JV::object_tag);
    r["error"] = JV("one_shot must be a boolean");
    return r;
  }
  if (ss && !ss->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("speed_scale must be a number");
    return r;
  }
  if (ex && !ex->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("explosiveness must be a number");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_particles_params called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(path, root, &hint);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path + " - " + hint);
    return r;
  }
  NodeParticles p = resolve_node_particles(node);
  if (!p.valid) {
    JV r(JV::object_tag);
    r["error"] = JV("node is not a GPUParticles2D/GPUParticles3D/CPUParticles2D/CPUParticles3D: " + path + " (actual class: " + util::to_std(node->get_class()) + ")");
    return r;
  }
  if (em) {
    bool v = em->GetBool();
    if (p.gpu2) {
      p.gpu2->set_emitting(v);
    } else if (p.gpu3) {
      p.gpu3->set_emitting(v);
    } else if (p.cpu2) {
      p.cpu2->set_emitting(v);
    } else if (p.cpu3) {
      p.cpu3->set_emitting(v);
    }
  }
  if (am) {
    int32_t v = static_cast<int32_t>(am->IsInt() ? am->GetInt() : am->GetDouble());
    if (p.gpu2) {
      p.gpu2->set_amount(v);
    } else if (p.gpu3) {
      p.gpu3->set_amount(v);
    } else if (p.cpu2) {
      p.cpu2->set_amount(v);
    } else if (p.cpu3) {
      p.cpu3->set_amount(v);
    }
  }
  if (lt) {
    double v = lt->IsInt() ? static_cast<double>(lt->GetInt()) : lt->GetDouble();
    if (p.gpu2) {
      p.gpu2->set_lifetime(v);
    } else if (p.gpu3) {
      p.gpu3->set_lifetime(v);
    } else if (p.cpu2) {
      p.cpu2->set_lifetime(v);
    } else if (p.cpu3) {
      p.cpu3->set_lifetime(v);
    }
  }
  if (os) {
    bool v = os->GetBool();
    if (p.gpu2) {
      p.gpu2->set_one_shot(v);
    } else if (p.gpu3) {
      p.gpu3->set_one_shot(v);
    } else if (p.cpu2) {
      p.cpu2->set_one_shot(v);
    } else if (p.cpu3) {
      p.cpu3->set_one_shot(v);
    }
  }
  if (ss) {
    double v = ss->IsInt() ? static_cast<double>(ss->GetInt()) : ss->GetDouble();
    if (p.gpu2) {
      p.gpu2->set_speed_scale(v);
    } else if (p.gpu3) {
      p.gpu3->set_speed_scale(v);
    } else if (p.cpu2) {
      p.cpu2->set_speed_scale(v);
    } else if (p.cpu3) {
      p.cpu3->set_speed_scale(v);
    }
  }
  if (ex) {
    float v = static_cast<float>(ex->IsInt() ? static_cast<double>(ex->GetInt()) : ex->GetDouble());
    if (p.gpu2) {
      p.gpu2->set_explosiveness_ratio(v);
    } else if (p.gpu3) {
      p.gpu3->set_explosiveness_ratio(v);
    } else if (p.cpu2) {
      p.cpu2->set_explosiveness_ratio(v);
    } else if (p.cpu3) {
      p.cpu3->set_explosiveness_ratio(v);
    }
  }
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_particles_params completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_node_particles_get_state(const JV &args) {
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: path");
    return r;
  }
  std::string path = pp->GetString();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_particles_state called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    JV r(JV::object_tag);
    r["error"] = JV("EditorInterface not available");
    return r;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    JV r(JV::object_tag);
    r["error"] = JV("no edited scene root");
    return r;
  }
  std::string hint;
  godot::Node *node = util::resolve_scene_node(path, root, &hint);
  if (!node) {
    JV r(JV::object_tag);
    r["error"] = JV("node not found: " + path + " - " + hint);
    return r;
  }
  NodeParticles p = resolve_node_particles(node);
  if (!p.valid) {
    JV r(JV::object_tag);
    r["error"] = JV("node is not a GPUParticles2D/GPUParticles3D/CPUParticles2D/CPUParticles3D: " + path + " (actual class: " + util::to_std(node->get_class()) + ")");
    return r;
  }
  JV state(JV::object_tag);
  state["type"] = JV(p.type);
  if (p.gpu2) {
    state["emitting"] = JV(p.gpu2->is_emitting());
    state["amount"] = JV(static_cast<int64_t>(p.gpu2->get_amount()));
    state["lifetime"] = JV(static_cast<double>(p.gpu2->get_lifetime()));
    state["one_shot"] = JV(p.gpu2->get_one_shot());
    state["speed_scale"] = JV(static_cast<double>(p.gpu2->get_speed_scale()));
    state["explosiveness"] = JV(static_cast<double>(p.gpu2->get_explosiveness_ratio()));
  } else if (p.gpu3) {
    state["emitting"] = JV(p.gpu3->is_emitting());
    state["amount"] = JV(static_cast<int64_t>(p.gpu3->get_amount()));
    state["lifetime"] = JV(static_cast<double>(p.gpu3->get_lifetime()));
    state["one_shot"] = JV(p.gpu3->get_one_shot());
    state["speed_scale"] = JV(static_cast<double>(p.gpu3->get_speed_scale()));
    state["explosiveness"] = JV(static_cast<double>(p.gpu3->get_explosiveness_ratio()));
  } else if (p.cpu2) {
    state["emitting"] = JV(p.cpu2->is_emitting());
    state["amount"] = JV(static_cast<int64_t>(p.cpu2->get_amount()));
    state["lifetime"] = JV(static_cast<double>(p.cpu2->get_lifetime()));
    state["one_shot"] = JV(p.cpu2->get_one_shot());
    state["speed_scale"] = JV(static_cast<double>(p.cpu2->get_speed_scale()));
    state["explosiveness"] = JV(static_cast<double>(p.cpu2->get_explosiveness_ratio()));
  } else if (p.cpu3) {
    state["emitting"] = JV(p.cpu3->is_emitting());
    state["amount"] = JV(static_cast<int64_t>(p.cpu3->get_amount()));
    state["lifetime"] = JV(static_cast<double>(p.cpu3->get_lifetime()));
    state["one_shot"] = JV(p.cpu3->get_one_shot());
    state["speed_scale"] = JV(static_cast<double>(p.cpu3->get_speed_scale()));
    state["explosiveness"] = JV(static_cast<double>(p.cpu3->get_explosiveness_ratio()));
  }
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_particles_state completed");
  JV r(JV::object_tag);
  r["result"] = std::move(state);
  return r;
}

JV handle_instance_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  if (action == "create") {
    godot::RID base;
    if (!read_rid(args, "base_rid", &base)) {
      return missing_for_action(action, "base_rid");
    }
    if (!base.is_valid()) {
      return invalid_rid_error("base_rid",
                               args.Find("base_rid")->GetInt());
    }
    godot::RID scenario;
    read_rid(args, "scenario_rid", &scenario);
    godot::RID rid = rs->instance_create2(base, scenario);
    util::rid_store<RenderRidDomain>().store(rid);
    return util::ok_result(util::rid_to_json(rid));
  }
  godot::RID instance;
  if (!read_rid(args, "instance_rid", &instance)) {
    return missing_for_action(action, "instance_rid");
  }
  if (!instance.is_valid()) {
    return invalid_rid_error("instance_rid",
                             args.Find("instance_rid")->GetInt());
  }
  if (action == "attach") {
    godot::RID scenario;
    if (!read_rid(args, "scenario_rid", &scenario)) {
      return missing_for_action(action, "scenario_rid");
    }
    rs->instance_set_scenario(instance, scenario);
    return ok_result();
  }
  if (action == "set_transform") {
    const godot::Transform3D xform = transform3d_from_args(args);
    rs->instance_set_transform(instance, xform);
    return ok_result();
  }
  if (action == "set_visible") {
    return handle_instance_set_visible(args);
  }
  if (action == "set_layer_mask") {
    return handle_instance_set_layer_mask(args);
  }
  if (action == "free") {
    const int64_t instance_id = static_cast<int64_t>(instance.get_id());
    if (!util::rid_store<RenderRidDomain>().contains(instance_id)) {
      JV e(JV::object_tag);
      e["error"] = JV(
          "instance_rid is not owned by the GDA render RID store; use "
          "render_rid_free with confirm=true to free an external RID");
      e["code"] = JV("rid_not_owned");
      return e;
    }
    const JV *confirm_p = args.Find("confirm");
    if (confirm_p == nullptr || !confirm_p->IsBool() ||
        !confirm_p->GetBool()) {
      JV e(JV::object_tag);
      e["error"] = JV(
          "render_instance action=free is destructive; pass confirm=true to "
          "free instance_rid " +
          std::to_string(instance_id));
      e["code"] = JV("confirmation_required");
      e["confirm_required"] = JV(true);
      e["side_effect"] = JV("destructive");
      return e;
    }
    rs->free_rid(instance);
    util::rid_store<RenderRidDomain>().erase(instance_id);
    return ok_result();
  }
  return unknown_action_error("render_instance", action);
}

JV handle_viewport_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  if (action == "create") {
    return handle_viewport_create(args);
  }
  godot::RID viewport;
  if (!read_rid(args, "viewport_rid", &viewport)) {
    return missing_for_action(action, "viewport_rid");
  }
  if (!viewport.is_valid()) {
    return invalid_rid_error("viewport_rid",
                             args.Find("viewport_rid")->GetInt());
  }
  if (action == "attach_camera") {
    godot::RID camera;
    if (!read_rid(args, "camera_rid", &camera)) {
      return missing_for_action(action, "camera_rid");
    }
    rs->viewport_attach_camera(viewport, camera);
    return ok_result();
  }
  if (action == "set_scenario") {
    godot::RID scenario;
    if (!read_rid(args, "scenario_rid", &scenario)) {
      return missing_for_action(action, "scenario_rid");
    }
    rs->viewport_set_scenario(viewport, scenario);
    return ok_result();
  }
  if (action == "attach_canvas") {
    godot::RID canvas;
    if (!read_rid(args, "canvas_rid", &canvas)) {
      return missing_for_action(action, "canvas_rid");
    }
    rs->viewport_attach_canvas(viewport, canvas);
    return ok_result();
  }
  if (action == "attach_to_screen") {
    godot::Rect2 rect;
    if (const JV *value = args.Find("rect"); value && value->IsObject()) {
      rect = godot::Rect2(
          static_cast<float>(util::json_number(value->Find("x"), 0.0)),
          static_cast<float>(util::json_number(value->Find("y"), 0.0)),
          static_cast<float>(util::json_number(value->Find("width"), 0.0)),
          static_cast<float>(util::json_number(value->Find("height"), 0.0)));
    }
    int screen = 0;
    if (const JV *value = args.Find("screen"); value && value->IsInt()) {
      screen = static_cast<int>(value->GetInt());
    }
    rs->viewport_attach_to_screen(viewport, rect, screen);
    return ok_result();
  }
  if (action == "set_size") {
    return handle_viewport_set_size(args);
  }
  if (action == "set_clear_mode") {
    return handle_viewport_set_clear_mode(args);
  }
  if (action == "capture_texture") {
    rs->viewport_set_update_mode(
        viewport,
        godot::RenderingServer::ViewportUpdateMode::VIEWPORT_UPDATE_ALWAYS);
    rs->force_draw();
    godot::RID texture = rs->viewport_get_texture(viewport);
    godot::Ref<godot::Image> image =
        texture.is_valid() ? rs->texture_2d_get(texture)
                           : godot::Ref<godot::Image>();
    if (image.is_null() || image->is_empty()) {
      rs->force_draw();
      texture = rs->viewport_get_texture(viewport);
      image = texture.is_valid() ? rs->texture_2d_get(texture)
                                 : godot::Ref<godot::Image>();
    }
    if (image.is_null() || image->is_empty()) {
      JV r(JV::object_tag);
      r["error"] =
          JV("viewport texture is empty after two rendered frames — position: "
             "render_ops.cpp handle_viewport_action capture_texture — "
             "expected: a viewport attached to a camera or canvas that has "
             "rendered content — action: attach a camera with render_viewport "
             "action=attach_camera, set a size, then call capture_texture "
             "again");
      r["code"] = JV("viewport_texture_not_ready");
      r["retryable"] = JV(true);
      return r;
    }
    JV inner = image_to_png_result(image);
    if (inner.Find("error") != nullptr) {
      return inner;
    }
    return util::ok_result(std::move(inner));
  }
  return unknown_action_error("render_viewport", action);
}

JV handle_camera_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_camera_create(args);
  }
  if (action == "transform") {
    const bool extended = args.Find("transform") != nullptr ||
                          args.Find("basis") != nullptr ||
                          args.Find("rotation") != nullptr ||
                          args.Find("rotation_x") != nullptr ||
                          args.Find("rotation_y") != nullptr ||
                          args.Find("rotation_z") != nullptr;
    if (!extended) {
      return handle_camera_set_transform(args);
    }
    godot::RID camera;
    if (!read_rid(args, "camera_rid", &camera)) {
      return missing_for_action(action, "camera_rid");
    }
    if (!camera.is_valid()) {
      return invalid_rid_error("camera_rid",
                               args.Find("camera_rid")->GetInt());
    }
    auto *rs = godot::RenderingServer::get_singleton();
    if (!rs) {
      JV r(JV::object_tag);
      r["error"] = JV("RenderingServer not available");
      return r;
    }
    rs->camera_set_transform(camera, transform3d_from_args(args));
    return ok_result();
  }
  if (action == "perspective") {
    return handle_camera_set_perspective(args);
  }
  if (action == "orthogonal") {
    return handle_camera_set_orthogonal(args);
  }
  return unknown_action_error("render_camera", action);
}

JV handle_canvas_item_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_canvas_item_create(args);
  }
  if (action == "add_rect") {
    return handle_canvas_item_draw_rect(args);
  }
  if (action == "add_circle") {
    return handle_canvas_item_draw_circle(args);
  }
  if (action == "add_line") {
    return handle_canvas_item_draw_line(args);
  }
  if (action == "add_texture_rect") {
    return handle_canvas_item_draw_texture(args);
  }
  if (action == "add_polygon") {
    godot::RID item;
    if (!read_rid(args, "canvas_item_rid", &item)) {
      return missing_for_action(action, "canvas_item_rid");
    }
    const JV *points = args.Find("points");
    const JV *colors = args.Find("colors");
    if (!points || !points->IsArray()) {
      return missing_for_action(action, "points");
    }
    if (!colors || !colors->IsArray()) {
      return missing_for_action(action, "colors");
    }
    auto *rs = godot::RenderingServer::get_singleton();
    if (!rs) {
      JV r(JV::object_tag);
      r["error"] = JV("RenderingServer not available");
      return r;
    }
    godot::PackedVector2Array point_array;
    for (const JV &point : points->GetArray()) {
      point_array.append(util::json_to_vec2(point));
    }
    godot::PackedColorArray color_array;
    for (const JV &color : colors->GetArray()) {
      color_array.append(util::json_to_color(color));
    }
    godot::PackedVector2Array uv_array;
    if (const JV *uvs = args.Find("uvs"); uvs && uvs->IsArray()) {
      for (const JV &uv : uvs->GetArray()) {
        uv_array.append(util::json_to_vec2(uv));
      }
    }
    godot::RID texture;
    read_rid(args, "texture_rid", &texture);
    rs->canvas_item_add_polygon(item, point_array, color_array, uv_array,
                                texture);
    return ok_result();
  }
  if (action == "set_transform") {
    return handle_canvas_item_set_transform(args);
  }
  if (action == "set_visible") {
    return handle_canvas_item_set_visible(args);
  }
  return unknown_action_error("render_canvas_item", action);
}

JV handle_light_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_light_create(args);
  }
  if (action == "set_param") {
    return handle_light_set_param(args);
  }
  if (action == "set_color") {
    return handle_light_set_color(args);
  }
  return unknown_action_error("render_light", action);
}

JV handle_particles_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_particle_create(args);
  }
  const bool has_path = args.Find("path") != nullptr &&
                        args.Find("path")->IsString();
  if (action == "set_emitting") {
    if (has_path && args.Find("particles_rid") == nullptr) {
      return handle_node_particles_set_params(args);
    }
    return handle_particles_set_emitting(args);
  }
  if (action == "restart") {
    if (has_path && args.Find("particles_rid") == nullptr) {
      return handle_node_particles_restart(args);
    }
    return handle_particles_restart(args);
  }
  if (action == "set_lifetime") {
    return handle_particles_set_lifetime(args);
  }
  if (action == "set_params") {
    return handle_node_particles_set_params(args);
  }
  if (action == "get_state") {
    return handle_node_particles_get_state(args);
  }
  return unknown_action_error("render_particles", action);
}

JV handle_material_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_material_create(args);
  }
  if (action == "set_param") {
    return handle_material_set_param(args);
  }
  return unknown_action_error("render_material", action);
}

JV handle_shader_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_shader_create(args);
  }
  if (action == "set_code") {
    return handle_shader_set_code(args);
  }
  if (action == "set_global_param") {
    return handle_global_shader_parameter_set(args);
  }
  return unknown_action_error("render_shader", action);
}

JV handle_sky_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_sky_create(args);
  }
  if (action == "set_material") {
    return handle_sky_set_material(args);
  }
  return unknown_action_error("render_sky", action);
}

JV handle_fog_volume_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_fog_create(args);
  }
  if (action == "set_shape") {
    return handle_fog_volume_set_shape(args);
  }
  return unknown_action_error("render_fog_volume", action);
}

JV handle_decal_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_decal_create(args);
  }
  return unknown_action_error("render_decal", action);
}

JV handle_reflection_probe_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_reflection_probe_create(args);
  }
  return unknown_action_error("render_reflection_probe", action);
}

JV handle_mesh_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create") {
    return handle_mesh_create(args);
  }
  if (action == "add_surface") {
    return handle_mesh_add_surface(args);
  }
  if (action == "set_surface_material") {
    return handle_mesh_set_material(args);
  }
  return unknown_action_error("render_mesh", action);
}

JV handle_texture_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  if (action == "create_from_image") {
    return handle_texture_create_2d(args);
  }
  return unknown_action_error("render_texture", action);
}

JV handle_inspect_action(const JV &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return missing_action_error();
  }
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  if (action == "material_get_param") {
    godot::RID material;
    if (!read_rid(args, "material_rid", &material)) {
      return missing_for_action(action, "material_rid");
    }
    const JV *parameter = args.Find("parameter");
    if (!parameter || !parameter->IsString()) {
      return missing_for_action(action, "parameter");
    }
    godot::StringName name(parameter->GetString().c_str());
    JV inner(JV::object_tag);
    inner["parameter"] = *parameter;
    inner["value"] =
        VariantJson::serialize(rs->material_get_param(material, name));
    return util::ok_result(std::move(inner));
  }
  if (action == "shader_get_code" || action == "shader_get_param_list") {
    godot::RID shader;
    if (!read_rid(args, "shader_rid", &shader)) {
      return missing_for_action(action, "shader_rid");
    }
    const std::string code = util::to_std(rs->shader_get_code(shader));
    JV inner(JV::object_tag);
    inner["code"] = JV(code);
    if (action == "shader_get_param_list") {
      JV parameters = shader_parameter_list(code);
      inner["count"] = JV(static_cast<int64_t>(parameters.Size()));
      inner["parameters"] = std::move(parameters);
    }
    return util::ok_result(std::move(inner));
  }
  if (action == "mesh_surface_get_arrays") {
    godot::RID mesh;
    if (!read_rid(args, "mesh_rid", &mesh)) {
      return missing_for_action(action, "mesh_rid");
    }
    const JV *surface_p = args.Find("surface");
    if (!surface_p || !surface_p->IsInt()) {
      return missing_for_action(action, "surface");
    }
    const int surface = static_cast<int>(surface_p->GetInt());
    godot::Array arrays = rs->mesh_surface_get_arrays(mesh, surface);
    if (arrays.size() == 0) {
      JV r(JV::object_tag);
      r["error"] = JV("mesh surface has no arrays: surface " +
                      std::to_string(surface) +
                      " (the surface index must be below the surface count)");
      return r;
    }
    const char *names[] = {"vertices", "normals", "tangents", "colors",
                           "uvs",      "uv2",     "custom0",  "custom1",
                           "custom2",  "custom3", "bones",    "weights",
                           "indices"};
    JV objects(JV::object_tag);
    for (int i = 0; i < arrays.size() && i < 13; ++i) {
      if (arrays[i].get_type() == godot::Variant::NIL) {
        continue;
      }
      objects[names[i]] = VariantJson::serialize(arrays[i]);
    }
    JV inner(JV::object_tag);
    inner["surface"] = JV(static_cast<int64_t>(surface));
    inner["arrays"] = std::move(objects);
    return util::ok_result(std::move(inner));
  }
  if (action == "texture_2d_get") {
    godot::RID texture;
    if (!read_rid(args, "texture_rid", &texture)) {
      return missing_for_action(action, "texture_rid");
    }
    JV inner = image_to_png_result(rs->texture_2d_get(texture));
    if (inner.Find("error") != nullptr) {
      return inner;
    }
    return util::ok_result(std::move(inner));
  }
  if (action == "global_shader_parameter_get") {
    const JV *name_p = args.Find("name");
    if (!name_p || !name_p->IsString()) {
      return missing_for_action(action, "name");
    }
    godot::StringName name(name_p->GetString().c_str());
    JV inner(JV::object_tag);
    inner["name"] = *name_p;
    inner["value"] =
        VariantJson::serialize(rs->global_shader_parameter_get(name));
    return util::ok_result(std::move(inner));
  }
  if (action == "global_shader_parameter_list") {
    godot::TypedArray<godot::StringName> names =
        rs->global_shader_parameter_get_list();
    JV values(JV::array_tag);
    for (int64_t i = 0; i < names.size(); ++i) {
      values.PushBack(JV(util::to_std(godot::String(names[i]))));
    }
    JV inner(JV::object_tag);
    inner["count"] = JV(static_cast<int64_t>(names.size()));
    inner["parameters"] = std::move(values);
    return util::ok_result(std::move(inner));
  }
  if (action == "global_shader_parameter_remove") {
    const JV *name_p = args.Find("name");
    if (!name_p || !name_p->IsString()) {
      return missing_for_action(action, "name");
    }
    rs->global_shader_parameter_remove(
        godot::StringName(name_p->GetString().c_str()));
    return ok_result();
  }
  return unknown_action_error("render_inspect", action);
}

JV handle_free_rid(const JV &args) {
  godot::RID rid;
  if (!read_rid(args, "rid", &rid)) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: rid");
    return r;
  }
  if (!rid.is_valid()) {
    return invalid_rid_error("rid", args.Find("rid")->GetInt());
  }
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  const int64_t rid_id = static_cast<int64_t>(rid.get_id());
  rs->free_rid(rid);
  util::rid_store<RenderRidDomain>().erase(rid_id);
  forget_environment_state(rid_id);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["freed"] = JV(true);
  r["rid"] = JV(rid_id);
  return r;
}

} // namespace render_ops
} // namespace godot_autopilot
