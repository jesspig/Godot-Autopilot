#include "tools/domains/scene/scene_ops.hpp"
#include "util/scene_path.hpp"
#include "util/scene_verify.hpp"
#include "core/observability/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/domains/property/property_ops.hpp"
#include "tools/domains/resource/resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/path2d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/path_follow2d.hpp>
#include <godot_cpp/classes/path_follow3d.hpp>
#include <godot_cpp/classes/scene_state.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot_autopilot {
namespace scene_ops {
mcp::JsonValue handle_advance_path_follow(const mcp::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString() || p->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *o = args.Find("offset");
  if (!o || !o->IsNumber()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: offset (number "
                                "of pixels to advance; negative moves back)");
    return e;
  }
  const std::string path = p->GetString();
  const double offset = util::json_number(o, 0.0);
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }
  auto *follow2d = godot::Object::cast_to<godot::PathFollow2D>(node);
  auto *follow3d = godot::Object::cast_to<godot::PathFollow3D>(node);
  if (!follow2d && !follow3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node is not a PathFollow2D/PathFollow3D: " +
                                path);
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  if (follow2d) {
    const float next = follow2d->get_progress() + static_cast<float>(offset);
    follow2d->set_progress(next);
    r["result"] = mcp::JsonValue("advanced");
    r["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
    r["type"] = mcp::JsonValue("PathFollow2D");
    r["progress"] =
        mcp::JsonValue(static_cast<double>(follow2d->get_progress()));
    r["progress_ratio"] =
        mcp::JsonValue(static_cast<double>(follow2d->get_progress_ratio()));
    r["position"] =
        util::vec2_to_json(follow2d->get_global_position());
  } else {
    const float next = follow3d->get_progress() + static_cast<float>(offset);
    follow3d->set_progress(next);
    r["result"] = mcp::JsonValue("advanced");
    r["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
    r["type"] = mcp::JsonValue("PathFollow3D");
    r["progress"] =
        mcp::JsonValue(static_cast<double>(follow3d->get_progress()));
    r["progress_ratio"] =
        mcp::JsonValue(static_cast<double>(follow3d->get_progress_ratio()));
    r["position"] =
        util::vec3_to_json(follow3d->get_global_position());
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_focus_camera_on_node(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  auto *tp = args.Find("target_path");
  if (!tp || !tp->IsString() || tp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: target_path");
    return e;
  }
  double margin_value = 0.0;
  auto *mp = args.Find("margin");
  if (mp) {
    if (!mp->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'margin' must be a number");
      return e;
    }
    margin_value =
        mp->IsInt() ? static_cast<double>(mp->GetInt()) : mp->GetDouble();
    if (!(margin_value >= 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'margin' must not be negative");
      return e;
    }
  }
  std::string camera_path = cp->GetString();
  std::string target_path = tp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  godot::Node *camera_node = godot_autopilot::util::resolve_scene_node(
      camera_path, scene_root, &hint);
  if (!camera_node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + camera_path +
                                " — " + hint);
    return e;
  }
  godot::Node *target = godot_autopilot::util::resolve_scene_node(
      target_path, scene_root, &hint);
  if (!target) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + target_path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d =
      godot::Object::cast_to<godot::Camera2D>(camera_node);
  godot::Camera3D *cam3d =
      godot::Object::cast_to<godot::Camera3D>(camera_node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node is not a Camera2D/Camera3D: " +
                                camera_path + " (actual class: " +
                                util::to_std(camera_node->get_class()) + ")");
    return e;
  }
  godot::Node3D *target_3d = godot::Object::cast_to<godot::Node3D>(target);
  godot::CanvasItem *target_canvas =
      godot::Object::cast_to<godot::CanvasItem>(target);
  std::string space;
  if (target_3d)
    space = "3d";
  else if (target_canvas)
    space = "2d";
  else {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("unsupported node type: " + target_path +
                                " (actual class: " +
                                util::to_std(target->get_class()) + ")");
    return e;
  }
  if (space == "2d" && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "camera type " + util::to_std(camera_node->get_class()) +
        " is not compatible with 2D target '" + target_path + "'");
    return e;
  }
  if (space == "3d" && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "camera type " + util::to_std(camera_node->get_class()) +
        " is not compatible with 3D target '" + target_path + "'");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  if (cam2d) {
    godot::Vector2 anchor;
    godot::Rect2 base_rect;
    if (auto *ctrl = godot::Object::cast_to<godot::Control>(target)) {
      godot::Rect2 gr = ctrl->get_global_rect();
      anchor = gr.position + gr.size * 0.5f;
      base_rect = gr;
    } else {
      anchor = target_canvas->get_global_transform().get_origin();
      base_rect = godot::Rect2(anchor, godot::Vector2());
    }
    godot::Rect2 focus_rect =
        base_rect.grow(static_cast<float>(margin_value));
    godot::Vector2 center = focus_rect.get_center();
    godot::Vector2 old_pos = cam2d->get_global_position();
    if (undo_redo) {
      undo_redo->create_action(godot::String("Focus Camera On Node"));
      undo_redo->add_do_method(cam2d, godot::StringName("set_global_position"),
                               godot::Variant(center));
      undo_redo->add_undo_method(cam2d,
                                 godot::StringName("set_global_position"),
                                 godot::Variant(old_pos));
      undo_redo->commit_action();
    } else {
      cam2d->set_global_position(center);
    }
    r["result"] = mcp::JsonValue("focused");
    r["camera_path"] =
        mcp::JsonValue(scene_relative_path(camera_node, scene_root));
    r["camera_type"] =
        mcp::JsonValue(util::to_std(camera_node->get_class()));
    r["target_path"] = mcp::JsonValue(scene_relative_path(target, scene_root));
    r["target_type"] = mcp::JsonValue(util::to_std(target->get_class()));
    r["space"] = mcp::JsonValue("2d");
    r["position"] = util::vec2_to_json(cam2d->get_global_position());
    r["zoom"] = util::vec2_to_json(cam2d->get_zoom());
    mcp::JsonValue rect_pos(mcp::JsonValue::object_tag);
    rect_pos["x"] = mcp::JsonValue(static_cast<double>(focus_rect.position.x));
    rect_pos["y"] = mcp::JsonValue(static_cast<double>(focus_rect.position.y));
    mcp::JsonValue rect_size(mcp::JsonValue::object_tag);
    rect_size["x"] = mcp::JsonValue(static_cast<double>(focus_rect.size.x));
    rect_size["y"] = mcp::JsonValue(static_cast<double>(focus_rect.size.y));
    mcp::JsonValue focus_rect_j(mcp::JsonValue::object_tag);
    focus_rect_j["position"] = std::move(rect_pos);
    focus_rect_j["size"] = std::move(rect_size);
    r["focus_rect"] = std::move(focus_rect_j);
    r["margin"] = mcp::JsonValue(margin_value);
  } else {
    godot::Vector3 focus = target_3d->get_global_transform().get_origin();
    godot::Transform3D old_gt = cam3d->get_global_transform();
    godot::Vector3 eye = old_gt.get_origin();
    godot::Vector3 aim_eye = eye;
    if (margin_value > 0.0) {
      godot::Vector3 back = eye - focus;
      if (back.length_squared() < 0.00000001)
        back = godot::Vector3(0.0f, 0.0f, 1.0f);
      aim_eye = eye + back.normalized() * static_cast<float>(margin_value);
    }
    if ((focus - aim_eye).length_squared() < 0.00000001) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "camera and target coincide — move the camera before look_at: " +
          scene_relative_path(camera_node, scene_root));
      return e;
    }
    godot::Transform3D next_gt = old_gt;
    next_gt.origin = aim_eye;
    godot::Transform3D aimed = next_gt.looking_at(focus);
    if (undo_redo) {
      undo_redo->create_action(godot::String("Focus Camera On Node"));
      undo_redo->add_do_method(cam3d, godot::StringName("set_global_transform"),
                               godot::Variant(aimed));
      undo_redo->add_undo_method(cam3d,
                                 godot::StringName("set_global_transform"),
                                 godot::Variant(old_gt));
      undo_redo->commit_action();
    } else {
      cam3d->set_global_transform(aimed);
    }
    bool behind = cam3d->is_position_behind(focus);
    r["result"] = mcp::JsonValue("focused");
    r["camera_path"] =
        mcp::JsonValue(scene_relative_path(camera_node, scene_root));
    r["camera_type"] =
        mcp::JsonValue(util::to_std(camera_node->get_class()));
    r["target_path"] = mcp::JsonValue(scene_relative_path(target, scene_root));
    r["target_type"] = mcp::JsonValue(util::to_std(target->get_class()));
    r["space"] = mcp::JsonValue("3d");
    r["position"] = util::vec3_to_json(cam3d->get_global_position());
    r["target_position"] = util::vec3_to_json(focus);
    r["behind"] = mcp::JsonValue(behind);
    r["margin"] = mcp::JsonValue(margin_value);
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_set_camera_params(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  auto *params_p = args.Find("params");
  if (!params_p || !params_p->IsObject()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "missing required parameter: params (object with any of zoom, fov, "
        "near, far, limits)");
    return e;
  }
  std::string path = cp->GetString();
  const char *limit_names[5] = {"limit_left", "limit_top", "limit_right",
                                "limit_bottom", "limit_smoothed"};
  for (const auto &kv : *params_p) {
    if (kv.first != "zoom" && kv.first != "fov" && kv.first != "near" &&
        kv.first != "far" && kv.first != "limits") {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("unknown params key '" + kv.first +
                                  "' — expected any of zoom, fov, near, far, "
                                  "limits");
      return e;
    }
  }
  godot::Vector2 zoom2(1.0f, 1.0f);
  bool has_zoom = false;
  auto *zoom_p = params_p->Find("zoom");
  if (zoom_p) {
    if (zoom_p->IsNumber()) {
      double z = zoom_p->IsInt() ? static_cast<double>(zoom_p->GetInt())
                                 : zoom_p->GetDouble();
      if (!(z > 0.0)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] =
            mcp::JsonValue("parameter 'zoom' must be greater than 0");
        return e;
      }
      zoom2 = godot::Vector2(static_cast<float>(z), static_cast<float>(z));
      has_zoom = true;
    } else if (zoom_p->IsObject()) {
      auto *x = zoom_p->Find("x");
      auto *y = zoom_p->Find("y");
      if (!x || !x->IsNumber() || !y || !y->IsNumber()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "parameter 'zoom' object must have numeric x and y");
        return e;
      }
      double zx = util::json_number(x, 0.0);
      double zy = util::json_number(y, 0.0);
      if (!(zx > 0.0) || !(zy > 0.0)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "parameter 'zoom' components must be greater than 0");
        return e;
      }
      zoom2 = godot::Vector2(static_cast<float>(zx), static_cast<float>(zy));
      has_zoom = true;
    } else {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'zoom' must be a number or {x, y} object");
      return e;
    }
  }
  double fov_value = 0.0;
  bool has_fov = false;
  auto *fov_p = params_p->Find("fov");
  if (fov_p) {
    if (!fov_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'fov' must be a number");
      return e;
    }
    fov_value = fov_p->IsInt() ? static_cast<double>(fov_p->GetInt())
                               : fov_p->GetDouble();
    if (!(fov_value >= 1.0) || !(fov_value <= 179.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'fov' must be between 1 and 179");
      return e;
    }
    has_fov = true;
  }
  double near_value = 0.0;
  bool has_near = false;
  auto *near_p = params_p->Find("near");
  if (near_p) {
    if (!near_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'near' must be a number");
      return e;
    }
    near_value = near_p->IsInt() ? static_cast<double>(near_p->GetInt())
                                 : near_p->GetDouble();
    if (!(near_value > 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'near' must be greater than 0");
      return e;
    }
    has_near = true;
  }
  double far_value = 0.0;
  bool has_far = false;
  auto *far_p = params_p->Find("far");
  if (far_p) {
    if (!far_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'far' must be a number");
      return e;
    }
    far_value = far_p->IsInt() ? static_cast<double>(far_p->GetInt())
                               : far_p->GetDouble();
    if (!(far_value > 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'far' must be greater than 0");
      return e;
    }
    has_far = true;
  }
  struct LimitEntry {
    std::string name;
    godot::Variant value;
  };
  std::vector<LimitEntry> limit_entries;
  bool has_limits = false;
  auto *limits_p = params_p->Find("limits");
  if (limits_p) {
    if (!limits_p->IsObject()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'limits' must be an object");
      return e;
    }
    for (const auto &kv : *limits_p) {
      bool known = false;
      for (int i = 0; i < 5; i++) {
        if (kv.first == limit_names[i]) {
          known = true;
          break;
        }
      }
      if (!known) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("unknown limits key '" + kv.first + "'");
        return e;
      }
    }
    for (int i = 0; i < 5; i++) {
      const mcp::JsonValue *entry = limits_p->Find(limit_names[i]);
      if (!entry)
        continue;
      if (i < 4) {
        if (!entry->IsNumber()) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(std::string("parameter 'limits.") +
                                      limit_names[i] + "' must be a number");
          return e;
        }
        double v = entry->IsInt() ? static_cast<double>(entry->GetInt())
                                  : entry->GetDouble();
        limit_entries.push_back(
            LimitEntry{limit_names[i], godot::Variant(static_cast<int>(v))});
      } else {
        if (!entry->IsBool()) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(
              "parameter 'limits.limit_smoothed' must be a boolean");
          return e;
        }
        limit_entries.push_back(
            LimitEntry{limit_names[i], godot::Variant(entry->GetBool())});
      }
    }
    if (limit_entries.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "parameter 'limits' must contain at least one of limit_left, "
          "limit_top, limit_right, limit_bottom, limit_smoothed");
      return e;
    }
    has_limits = true;
  }
  if (!has_zoom && !has_fov && !has_near && !has_far && !has_limits) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "nothing to set — params must contain at least one of: zoom, fov, "
        "near, far, limits");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  godot::Node *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d = godot::Object::cast_to<godot::Camera2D>(node);
  godot::Camera3D *cam3d = godot::Object::cast_to<godot::Camera3D>(node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "node is not a Camera2D/Camera3D: " + path + " (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_zoom && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'zoom' applies only to a Camera2D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_limits && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'limits' applies only to a Camera2D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_fov && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'fov' applies only to a Camera3D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if ((has_near || has_far) && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameters 'near' and 'far' apply only to a Camera3D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (cam3d && (has_near || has_far)) {
    double eff_near =
        has_near ? near_value : static_cast<double>(cam3d->get_near());
    double eff_far =
        has_far ? far_value : static_cast<double>(cam3d->get_far());
    if (!(eff_near < eff_far)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'near' must be below 'far'");
      return e;
    }
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  mcp::JsonValue applied(mcp::JsonValue::array_tag);
  if (undo_redo) {
    undo_redo->create_action(godot::String("Set Camera Params"));
    if (has_zoom) {
      undo_redo->add_do_method(cam2d, godot::StringName("set_zoom"),
                               godot::Variant(zoom2));
      undo_redo->add_undo_method(cam2d, godot::StringName("set_zoom"),
                                 godot::Variant(cam2d->get_zoom()));
      applied.PushBack(mcp::JsonValue("zoom"));
    }
    if (has_fov) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_fov"),
                               godot::Variant(static_cast<float>(fov_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_fov"),
                                 godot::Variant(cam3d->get_fov()));
      applied.PushBack(mcp::JsonValue("fov"));
    }
    if (has_near) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_near"),
                               godot::Variant(static_cast<float>(near_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_near"),
                                 godot::Variant(cam3d->get_near()));
      applied.PushBack(mcp::JsonValue("near"));
    }
    if (has_far) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_far"),
                               godot::Variant(static_cast<float>(far_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_far"),
                                 godot::Variant(cam3d->get_far()));
      applied.PushBack(mcp::JsonValue("far"));
    }
    if (has_limits) {
      for (const LimitEntry &entry : limit_entries) {
        undo_redo->add_do_method(cam2d, godot::StringName("set"),
                                 godot::StringName(entry.name.c_str()),
                                 entry.value);
        undo_redo->add_undo_method(
            cam2d, godot::StringName("set"),
            godot::StringName(entry.name.c_str()),
            cam2d->get(godot::StringName(entry.name.c_str())));
      }
      applied.PushBack(mcp::JsonValue("limits"));
    }
    undo_redo->commit_action();
  } else {
    if (has_zoom) {
      cam2d->set_zoom(zoom2);
      applied.PushBack(mcp::JsonValue("zoom"));
    }
    if (has_fov) {
      cam3d->set_fov(static_cast<float>(fov_value));
      applied.PushBack(mcp::JsonValue("fov"));
    }
    if (has_near) {
      cam3d->set_near(static_cast<float>(near_value));
      applied.PushBack(mcp::JsonValue("near"));
    }
    if (has_far) {
      cam3d->set_far(static_cast<float>(far_value));
      applied.PushBack(mcp::JsonValue("far"));
    }
    if (has_limits) {
      for (const LimitEntry &entry : limit_entries)
        cam2d->set(godot::StringName(entry.name.c_str()), entry.value);
      applied.PushBack(mcp::JsonValue("limits"));
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["camera_path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  r["type"] = mcp::JsonValue(util::to_std(node->get_class()));
  r["applied"] = std::move(applied);
  if (cam2d) {
    r["position"] = util::vec2_to_json(cam2d->get_global_position());
    r["zoom"] = util::vec2_to_json(cam2d->get_zoom());
    mcp::JsonValue limits_j(mcp::JsonValue::object_tag);
    for (int i = 0; i < 5; i++)
      limits_j[limit_names[i]] = VariantJson::serialize(
          cam2d->get(godot::StringName(limit_names[i])));
    r["limits"] = std::move(limits_j);
  } else {
    r["position"] = util::vec3_to_json(cam3d->get_global_position());
    r["fov"] = mcp::JsonValue(static_cast<double>(cam3d->get_fov()));
    r["near"] = mcp::JsonValue(static_cast<double>(cam3d->get_near()));
    r["far"] = mcp::JsonValue(static_cast<double>(cam3d->get_far()));
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_get_camera_view_rect(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  std::string path = cp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    return util::error_detail(
        "no edited scene root available",
        "scene_ops.cpp handle_get_camera_view_rect", "an edited scene",
        "open or create a scene first (create_editor_scene)");
  }
  std::string hint;
  godot::Node *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d = godot::Object::cast_to<godot::Camera2D>(node);
  godot::Camera3D *cam3d = godot::Object::cast_to<godot::Camera3D>(node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "node is not a Camera2D/Camera3D: " + path + " (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  std::string space = cam2d ? "2d" : "3d";
  godot::SubViewport *vp2d = editor->get_editor_viewport_2d();
  godot::SubViewport *vp3d = editor->get_editor_viewport_3d(0);
  godot::Transform2D t2d;
  if (vp2d)
    t2d = vp2d->get_screen_transform() * vp2d->get_global_canvas_transform();
  godot::Transform2D t3d;
  if (vp3d)
    t3d = vp3d->get_screen_transform();
  godot::Camera3D *editor_camera = vp3d ? vp3d->get_camera_3d() : nullptr;
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["camera_path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  inner["camera_type"] = mcp::JsonValue(util::to_std(node->get_class()));
  inner["viewport"] = mcp::JsonValue(space);
  inner["window_id"] = mcp::JsonValue(0);
  inner["space"] = mcp::JsonValue("window");
  if (space == "2d") {
    if (!vp2d) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("2D editor viewport not available");
      return e;
    }
    godot::Vector2 world = cam2d->get_global_position();
    godot::Vector2 center = t2d.xform(world);
    godot::Vector2 zoom = cam2d->get_zoom();
    godot::Vector2 vp_size = vp2d->get_visible_rect().size;
    inner["world_position"] = util::vec2_to_json(world);
    mcp::JsonValue window_center(mcp::JsonValue::object_tag);
    window_center["x"] = mcp::JsonValue(static_cast<double>(center.x));
    window_center["y"] = mcp::JsonValue(static_cast<double>(center.y));
    inner["window_center"] = std::move(window_center);
    inner["zoom"] = util::vec2_to_json(zoom);
    inner["behind"] = mcp::JsonValue(false);
    mcp::JsonValue viewport_size(mcp::JsonValue::object_tag);
    viewport_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    viewport_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    inner["viewport_size"] = std::move(viewport_size);
    mcp::JsonValue view_pos(mcp::JsonValue::object_tag);
    view_pos["x"] =
        mcp::JsonValue(static_cast<double>(center.x - vp_size.x * 0.5));
    view_pos["y"] =
        mcp::JsonValue(static_cast<double>(center.y - vp_size.y * 0.5));
    mcp::JsonValue view_size(mcp::JsonValue::object_tag);
    view_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    view_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    mcp::JsonValue view_rect(mcp::JsonValue::object_tag);
    view_rect["position"] = std::move(view_pos);
    view_rect["size"] = std::move(view_size);
    inner["view_rect"] = std::move(view_rect);
    godot::Vector2 scale = t2d.get_scale();
    godot::Vector2 origin = t2d.get_origin();
    mcp::JsonValue mapping(mcp::JsonValue::object_tag);
    mapping["scale_x"] = mcp::JsonValue(static_cast<double>(scale.x));
    mapping["scale_y"] = mcp::JsonValue(static_cast<double>(scale.y));
    mapping["offset_x"] = mcp::JsonValue(static_cast<double>(origin.x));
    mapping["offset_y"] = mcp::JsonValue(static_cast<double>(origin.y));
    inner["mapping"] = std::move(mapping);
  } else {
    if (!vp3d) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("3D editor viewport not available");
      return e;
    }
    if (!editor_camera) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("3D editor viewport has no camera");
      return e;
    }
    godot::Vector3 world = cam3d->get_global_position();
    godot::Vector2 local = editor_camera->unproject_position(world);
    godot::Vector2 center = t3d.xform(local);
    godot::Transform3D gt = cam3d->get_global_transform();
    godot::Vector3 tip = gt.xform(godot::Vector3(0.0f, 0.0f, -1.0f));
    godot::Vector3 forward = tip - gt.get_origin();
    godot::Vector2 vp_size = vp3d->get_visible_rect().size;
    inner["world_position"] = util::vec3_to_json(world);
    inner["forward"] = util::vec3_to_json(forward);
    inner["fov"] = mcp::JsonValue(static_cast<double>(cam3d->get_fov()));
    inner["near"] = mcp::JsonValue(static_cast<double>(cam3d->get_near()));
    inner["far"] = mcp::JsonValue(static_cast<double>(cam3d->get_far()));
    mcp::JsonValue window_center(mcp::JsonValue::object_tag);
    window_center["x"] = mcp::JsonValue(static_cast<double>(center.x));
    window_center["y"] = mcp::JsonValue(static_cast<double>(center.y));
    inner["window_center"] = std::move(window_center);
    inner["behind"] =
        mcp::JsonValue(editor_camera->is_position_behind(world));
    mcp::JsonValue viewport_size(mcp::JsonValue::object_tag);
    viewport_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    viewport_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    inner["viewport_size"] = std::move(viewport_size);
    mcp::JsonValue view_pos(mcp::JsonValue::object_tag);
    view_pos["x"] =
        mcp::JsonValue(static_cast<double>(center.x - vp_size.x * 0.5));
    view_pos["y"] =
        mcp::JsonValue(static_cast<double>(center.y - vp_size.y * 0.5));
    mcp::JsonValue view_size(mcp::JsonValue::object_tag);
    view_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    view_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    mcp::JsonValue view_rect(mcp::JsonValue::object_tag);
    view_rect["position"] = std::move(view_pos);
    view_rect["size"] = std::move(view_size);
    inner["view_rect"] = std::move(view_rect);
    godot::Vector2 scale = t3d.get_scale();
    godot::Vector2 origin = t3d.get_origin();
    mcp::JsonValue mapping(mcp::JsonValue::object_tag);
    mapping["scale_x"] = mcp::JsonValue(static_cast<double>(scale.x));
    mapping["scale_y"] = mcp::JsonValue(static_cast<double>(scale.y));
    mapping["offset_x"] = mcp::JsonValue(static_cast<double>(origin.x));
    mapping["offset_y"] = mcp::JsonValue(static_cast<double>(origin.y));
    inner["mapping"] = std::move(mapping);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}
} // namespace scene_ops
} // namespace godot_autopilot
