#include "theme_ops.hpp"

#include "core/scene_dirty_tracker.hpp"
#include "util/error_util.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include "util/scene_path.hpp"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace theme_ops {

namespace {

constexpr int kMinAnchorPreset = 0;
constexpr int kMaxAnchorPreset = 15;

bool need_string(const mcp::JsonValue &args, const char *key,
                 std::string &out_value) {
  auto *it = args.Find(key);
  if (!it || !it->IsString() || it->GetString().empty()) {
    return false;
  }
  out_value = it->GetString();
  return true;
}

bool load_theme_or_error(const std::string &path,
                         godot::Ref<godot::Theme> &out_theme,
                         std::string &out_error) {
  if (!godot::FileAccess::file_exists(godot::String(path.c_str()))) {
    out_error = "file does not exist: " + path +
                " — create it with create_theme_resource first";
    return false;
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    out_error = "ResourceLoader not available";
    return false;
  }
  godot::Ref<godot::Resource> res =
      loader->load(godot::String(path.c_str()), godot::String("Theme"));
  if (res.is_null()) {
    out_error = "failed to load resource: " + path +
                " (file exists but failed to load — not a Theme .tres or not "
                "imported)";
    return false;
  }
  auto *theme = godot::Object::cast_to<godot::Theme>(res.ptr());
  if (!theme) {
    out_error = "resource is not a Theme: " + path + " (" +
                util::to_std(res->get_class()) + ")";
    return false;
  }
  out_theme = godot::Ref<godot::Theme>(theme);
  return true;
}

bool persist_theme(const godot::Ref<godot::Theme> &theme,
                   const std::string &path, std::string &out_error,
                   bool &out_verified) {
  out_verified = false;
  const util::ProjectPath checked =
      util::normalize_project_path(path, true, false);
  if (!checked.valid()) {
    out_error = "invalid theme path: " + path + " — " + checked.error;
    return false;
  }
  const std::string normalized_path = checked.value;

  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver) {
    out_error = "ResourceSaver not available";
    return false;
  }
  std::string dir_created;
  std::string dir_error;
  if (!resource_fs::ensure_parent_directory(normalized_path, dir_created,
                                            dir_error)) {
    out_error = dir_error;
    return false;
  }
  godot::Error err = saver->save(theme, godot::String(normalized_path.c_str()));
  if (err != godot::OK) {
    out_error = "failed to save theme, error code: " +
                std::to_string(static_cast<int>(err));
    return false;
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  if (loader) {
    godot::Ref<godot::Resource> verify =
        loader->load(godot::String(normalized_path.c_str()),
                     godot::String("Theme"),
                     godot::ResourceLoader::CACHE_MODE_IGNORE);
    out_verified =
        verify.is_valid() &&
        godot::Object::cast_to<godot::Theme>(verify.ptr()) != nullptr;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(godot::String(normalized_path.c_str()));
    }
  }
  return true;
}

bool parse_color_value(const mcp::JsonValue &val, godot::Color &out_color,
                       std::string &out_error) {
  if (val.IsString()) {
    const std::string hex = val.GetString();
    const godot::String hex_gs(hex.c_str());
    if (!godot::Color::html_is_valid(hex_gs)) {
      out_error = "invalid color string: \"" + hex +
                  "\" — expected \"#rrggbb\" or \"#rrggbbaa\"";
      return false;
    }
    out_color = godot::Color::html(hex_gs);
    return true;
  }
  if (val.IsObject()) {
    static const char *kChannelKeys[4] = {"r", "g", "b", "a"};
    float channels[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 4; i++) {
      auto *it = val.Find(kChannelKeys[i]);
      if (!it || !it->IsNumber()) {
        continue;
      }
      const double v = it->GetDouble();
      if (v < 0.0 || v > 1.0) {
        out_error = std::string("color channel '") + kChannelKeys[i] +
                    "' out of range: expected a float in [0, 1]";
        return false;
      }
      channels[i] = static_cast<float>(v);
    }
    out_color = godot::Color(channels[0], channels[1], channels[2], channels[3]);
    return true;
  }
  out_error =
      "invalid color: expected a hex string like \"#rrggbb\" or an object "
      "{r,g,b,a} with floats in [0, 1]";
  return false;
}

template <typename FnAll, typename FnSide>
bool apply_per_side(const mcp::JsonValue &val, FnAll apply_all,
                    FnSide apply_side, const char *param_name,
                    const char *const (&side_keys)[4],
                    std::string &out_error) {
  if (val.IsNumber()) {
    apply_all(val.GetDouble());
    return true;
  }
  if (!val.IsObject()) {
    out_error = std::string("invalid ") + param_name + ": expected a number " +
                "(applied to all sides) or an object with keys " +
                side_keys[0] + "/" + side_keys[1] + "/" + side_keys[2] + "/" +
                side_keys[3];
    return false;
  }
  for (int i = 0; i < 4; i++) {
    auto *it = val.Find(side_keys[i]);
    if (it && it->IsNumber()) {
      apply_side(i, it->GetDouble());
    }
  }
  return true;
}

const char *const kSideKeys[4] = {"left", "top", "right", "bottom"};
const char *const kCornerKeys[4] = {"top_left", "top_right", "bottom_right",
                                    "bottom_left"};

godot::Control *resolve_control_or_error(const std::string &control_path,
                                         std::string &out_error) {
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  auto *node = util::resolve_scene_node(control_path, scene_root, &hint);
  if (!node) {
    out_error = "node not found: " + control_path + " — " + hint;
    return nullptr;
  }
  auto *control = godot::Object::cast_to<godot::Control>(node);
  if (!control) {
    out_error = "node is not a Control: " + control_path + " (" +
                util::to_std(node->get_class()) + ")";
    return nullptr;
  }
  return control;
}

const char kSceneNotSavedNote[] =
    "scene change is in memory only — call save_editor_scene to persist it";

const std::string kUserScheme = "user://";

bool is_user_path(const std::string &p) {
  return p.compare(0, kUserScheme.size(), kUserScheme) == 0;
}

} // namespace

mcp::JsonValue handle_create_theme_resource(const mcp::JsonValue &args) {
  std::string path;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  if (is_user_path(path)) {
    return util::error_json("path must be inside res://, got: " + path);
  }

  if (godot::FileAccess::file_exists(godot::String(path.c_str()))) {
    return util::error_json(
        "file already exists: " + path +
        " — remove it with remove_resource_file first or pick another path");
  }

  godot::Ref<godot::Theme> theme;
  theme.instantiate();
  if (theme.is_null()) {
    return util::error_json("failed to create Theme instance");
  }

  std::string base_type;
  auto *it_base = args.Find("base_type");
  if (it_base && it_base->IsString() && !it_base->GetString().empty()) {
    base_type = it_base->GetString();
    theme->set_meta(godot::StringName("base_type"),
                    godot::Variant(godot::String(base_type.c_str())));
  }

  std::string save_error;
  bool verified = false;
  if (!persist_theme(theme, path, save_error, verified)) {
    return util::error_json(save_error);
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  if (!base_type.empty()) {
    r["base_type"] = mcp::JsonValue(base_type);
  }
  return r;
}

mcp::JsonValue handle_set_theme_color(const mcp::JsonValue &args) {
  std::string path, theme_type, varname;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  if (!need_string(args, "theme_type", theme_type)) {
    return util::error_json("missing required parameter: theme_type");
  }
  if (!need_string(args, "varname", varname)) {
    return util::error_json("missing required parameter: varname");
  }
  auto *it_color = args.Find("color");
  if (!it_color) {
    return util::error_json("missing required parameter: color");
  }
  godot::Color color;
  std::string parse_error;
  if (!parse_color_value(*it_color, color, parse_error)) {
    return util::error_json(parse_error);
  }

  godot::Ref<godot::Theme> theme;
  std::string error;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }
  theme->set_color(godot::StringName(varname.c_str()),
                   godot::StringName(theme_type.c_str()), color);

  bool verified = false;
  if (!persist_theme(theme, path, error, verified)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  return r;
}

mcp::JsonValue handle_set_theme_constant(const mcp::JsonValue &args) {
  std::string path, theme_type, varname;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  if (!need_string(args, "theme_type", theme_type)) {
    return util::error_json("missing required parameter: theme_type");
  }
  if (!need_string(args, "varname", varname)) {
    return util::error_json("missing required parameter: varname");
  }
  auto *it_constant = args.Find("constant");
  if (!it_constant || !it_constant->IsInt()) {
    return util::error_json(
        "missing or invalid required parameter: constant (integer)");
  }
  const int32_t constant = static_cast<int32_t>(it_constant->GetInt());

  godot::Ref<godot::Theme> theme;
  std::string error;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }
  theme->set_constant(godot::StringName(varname.c_str()),
                      godot::StringName(theme_type.c_str()), constant);

  bool verified = false;
  if (!persist_theme(theme, path, error, verified)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  return r;
}

mcp::JsonValue handle_set_theme_font_size(const mcp::JsonValue &args) {
  std::string path, theme_type, varname;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  if (!need_string(args, "theme_type", theme_type)) {
    return util::error_json("missing required parameter: theme_type");
  }
  if (!need_string(args, "varname", varname)) {
    return util::error_json("missing required parameter: varname");
  }
  auto *it_size = args.Find("font_size");
  if (!it_size || !it_size->IsInt()) {
    return util::error_json(
        "missing or invalid required parameter: font_size (integer)");
  }
  const int32_t font_size = static_cast<int32_t>(it_size->GetInt());

  godot::Ref<godot::Theme> theme;
  std::string error;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }
  theme->set_font_size(godot::StringName(varname.c_str()),
                       godot::StringName(theme_type.c_str()), font_size);

  bool verified = false;
  if (!persist_theme(theme, path, error, verified)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  return r;
}

mcp::JsonValue handle_set_theme_stylebox_flat(const mcp::JsonValue &args) {
  std::string path, theme_type, varname;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  if (!need_string(args, "theme_type", theme_type)) {
    return util::error_json("missing required parameter: theme_type");
  }
  if (!need_string(args, "varname", varname)) {
    return util::error_json("missing required parameter: varname");
  }
  auto *it_box = args.Find("stylebox");
  if (!it_box || !it_box->IsObject()) {
    return util::error_json(
        "missing or invalid required parameter: stylebox (object with optional "
        "bg_color, border_color, border_width, corner_radius, content_margin)");
  }

  godot::Ref<godot::StyleBoxFlat> box;
  box.instantiate();
  if (box.is_null()) {
    return util::error_json("failed to create StyleBoxFlat instance");
  }

  std::string error;
  auto *it_bg = it_box->Find("bg_color");
  if (it_bg) {
    godot::Color color;
    if (!parse_color_value(*it_bg, color, error)) {
      return util::error_json("stylebox.bg_color: " + error);
    }
    box->set_bg_color(color);
  }
  auto *it_border = it_box->Find("border_color");
  if (it_border) {
    godot::Color color;
    if (!parse_color_value(*it_border, color, error)) {
      return util::error_json("stylebox.border_color: " + error);
    }
    box->set_border_color(color);
  }
  auto *it_width = it_box->Find("border_width");
  if (it_width &&
      !apply_per_side(
          *it_width,
          [&box](double v) { box->set_border_width_all(static_cast<int32_t>(v)); },
          [&box](int side, double v) {
            box->set_border_width(static_cast<godot::Side>(side),
                                  static_cast<int32_t>(v));
          },
          "stylebox.border_width", kSideKeys, error)) {
    return util::error_json(error);
  }
  auto *it_radius = it_box->Find("corner_radius");
  if (it_radius &&
      !apply_per_side(
          *it_radius,
          [&box](double v) { box->set_corner_radius_all(static_cast<int32_t>(v)); },
          [&box](int corner, double v) {
            box->set_corner_radius(static_cast<godot::Corner>(corner),
                                   static_cast<int32_t>(v));
          },
          "stylebox.corner_radius", kCornerKeys, error)) {
    return util::error_json(error);
  }
  auto *it_margin = it_box->Find("content_margin");
  if (it_margin &&
      !apply_per_side(
          *it_margin,
          [&box](double v) { box->set_content_margin_all(static_cast<float>(v)); },
          [&box](int side, double v) {
            box->set_content_margin(static_cast<godot::Side>(side),
                                    static_cast<float>(v));
          },
          "stylebox.content_margin", kSideKeys, error)) {
    return util::error_json(error);
  }

  godot::Ref<godot::Theme> theme;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }
  theme->set_stylebox(godot::StringName(varname.c_str()),
                      godot::StringName(theme_type.c_str()), box);

  bool verified = false;
  if (!persist_theme(theme, path, error, verified)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  return r;
}

mcp::JsonValue handle_get_theme_info(const mcp::JsonValue &args) {
  std::string path;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  godot::Ref<godot::Theme> theme;
  std::string error;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }

  godot::PackedStringArray types = theme->get_type_list();
  mcp::JsonValue types_json(mcp::JsonValue::array_tag);
  for (int i = 0; i < types.size(); i++) {
    const std::string type_name = util::to_std(types[i]);
    const godot::String &type_gs = types[i];

    mcp::JsonValue entry(mcp::JsonValue::object_tag);
    entry["type"] = mcp::JsonValue(type_name);

    const godot::PackedStringArray color_items = theme->get_color_list(type_gs);
    mcp::JsonValue colors(mcp::JsonValue::array_tag);
    for (int c = 0; c < color_items.size(); c++) {
      colors.PushBack(mcp::JsonValue(util::to_std(color_items[c])));
    }
    entry["colors"] = std::move(colors);

    const godot::PackedStringArray constant_items =
        theme->get_constant_list(type_gs);
    mcp::JsonValue constants(mcp::JsonValue::array_tag);
    for (int c = 0; c < constant_items.size(); c++) {
      constants.PushBack(mcp::JsonValue(util::to_std(constant_items[c])));
    }
    entry["constants"] = std::move(constants);

    const godot::PackedStringArray font_size_items =
        theme->get_font_size_list(type_gs);
    mcp::JsonValue font_sizes(mcp::JsonValue::array_tag);
    for (int c = 0; c < font_size_items.size(); c++) {
      font_sizes.PushBack(mcp::JsonValue(util::to_std(font_size_items[c])));
    }
    entry["font_sizes"] = std::move(font_sizes);

    const godot::PackedStringArray stylebox_items =
        theme->get_stylebox_list(type_gs);
    mcp::JsonValue styleboxes(mcp::JsonValue::array_tag);
    for (int c = 0; c < stylebox_items.size(); c++) {
      styleboxes.PushBack(mcp::JsonValue(util::to_std(stylebox_items[c])));
    }
    entry["styleboxes"] = std::move(styleboxes);

    types_json.PushBack(std::move(entry));
  }

  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["path"] = mcp::JsonValue(path);
  result["type_count"] = mcp::JsonValue(static_cast<int64_t>(types.size()));
  result["types"] = std::move(types_json);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

mcp::JsonValue handle_apply_theme_to_control(const mcp::JsonValue &args) {
  std::string theme_path, control_path;
  if (!need_string(args, "theme_path", theme_path)) {
    return util::error_json("missing required parameter: theme_path");
  }
  if (!need_string(args, "control_path", control_path)) {
    return util::error_json("missing required parameter: control_path");
  }

  godot::Ref<godot::Theme> theme;
  std::string error;
  if (!load_theme_or_error(theme_path, theme, error)) {
    return util::error_json(error);
  }
  auto *control = resolve_control_or_error(control_path, error);
  if (!control) {
    return util::error_json(error);
  }
  control->set_theme(theme);
  scene_dirty_tracker::mark_scene_modified();

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["control_path"] = mcp::JsonValue(control_path);
  r["theme_path"] = mcp::JsonValue(theme_path);
  r["note"] = mcp::JsonValue(kSceneNotSavedNote);
  return r;
}

mcp::JsonValue handle_set_control_anchor_preset(const mcp::JsonValue &args) {
  std::string control_path;
  if (!need_string(args, "control_path", control_path)) {
    return util::error_json("missing required parameter: control_path");
  }
  auto *it_preset = args.Find("preset");
  if (!it_preset || !it_preset->IsInt()) {
    return util::error_json(
        "missing or invalid required parameter: preset (Control LayoutPreset "
        "integer)");
  }
  const int preset = static_cast<int>(it_preset->GetInt());
  if (preset < kMinAnchorPreset || preset > kMaxAnchorPreset) {
    return util::error_json(
        "invalid preset: " + std::to_string(preset) +
        " — LayoutPreset values are 0..15 (top_left=0, bottom_right=3, "
        "center=8, left_wide=9..bottom_wide=12, vcenter_wide=13, "
        "hcenter_wide=14, full_rect=15)");
  }
  bool keep_offsets = false;
  auto *it_keep = args.Find("keep_offsets");
  if (it_keep && it_keep->IsBool()) {
    keep_offsets = it_keep->GetBool();
  }

  std::string error;
  auto *control = resolve_control_or_error(control_path, error);
  if (!control) {
    return util::error_json(error);
  }
  control->set_anchors_preset(static_cast<godot::Control::LayoutPreset>(preset),
                              keep_offsets);
  scene_dirty_tracker::mark_scene_modified();

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["control_path"] = mcp::JsonValue(control_path);
  r["preset"] = mcp::JsonValue(static_cast<int64_t>(preset));
  r["keep_offsets"] = mcp::JsonValue(keep_offsets);
  r["note"] = mcp::JsonValue(kSceneNotSavedNote);
  return r;
}

mcp::JsonValue handle_set_control_layout(const mcp::JsonValue &args) {
  std::string path;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  auto *it_preset = args.Find("preset");
  auto *it_anchors = args.Find("anchors");
  auto *it_offsets = args.Find("offsets");
  auto *it_size_h = args.Find("size_flags_h");
  auto *it_size_v = args.Find("size_flags_v");
  auto *it_grow_h = args.Find("grow_h");
  auto *it_grow_v = args.Find("grow_v");
  auto *it_min = args.Find("min_size");
  bool has_preset = it_preset != nullptr;
  bool has_anchors = it_anchors != nullptr;
  bool has_offsets = it_offsets != nullptr;
  bool has_size_h = it_size_h != nullptr;
  bool has_size_v = it_size_v != nullptr;
  bool has_grow_h = it_grow_h != nullptr;
  bool has_grow_v = it_grow_v != nullptr;
  bool has_min = it_min != nullptr;
  if (has_preset && has_anchors) {
    return util::error_json("preset and anchors are mutually exclusive — pass only one of them");
  }
  int preset = 0;
  if (has_preset) {
    if (!it_preset->IsInt()) {
      return util::error_json("missing or invalid parameter: preset (Control LayoutPreset integer 0..15)");
    }
    preset = static_cast<int>(it_preset->GetInt());
    if (preset < kMinAnchorPreset || preset > kMaxAnchorPreset) {
      return util::error_json("invalid preset: " + std::to_string(preset) + " — LayoutPreset values are 0..15 (top_left=0, bottom_right=3, center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15)");
    }
  }
  bool keep_offsets = false;
  auto *it_keep = args.Find("keep_offsets");
  if (it_keep) {
    if (!it_keep->IsBool()) {
      return util::error_json("invalid keep_offsets: expected a boolean");
    }
    keep_offsets = it_keep->GetBool();
  }
  double anchor_vals[4] = {0.0, 0.0, 0.0, 0.0};
  bool anchor_set[4] = {false, false, false, false};
  if (has_anchors) {
    if (!it_anchors->IsObject()) {
      return util::error_json("invalid anchors: expected an object with optional left, top, right, bottom floats");
    }
    for (const auto &entry : it_anchors->GetObject()) {
      int idx = -1;
      if (entry.first == "left") {
        idx = 0;
      } else if (entry.first == "top") {
        idx = 1;
      } else if (entry.first == "right") {
        idx = 2;
      } else if (entry.first == "bottom") {
        idx = 3;
      } else {
        return util::error_json("unknown anchors key: " + entry.first + " — expected left, top, right or bottom");
      }
      if (!entry.second.IsNumber()) {
        return util::error_json("invalid anchors." + entry.first + ": expected a number");
      }
      anchor_vals[idx] = entry.second.GetDouble();
      anchor_set[idx] = true;
    }
  }
  double offset_vals[4] = {0.0, 0.0, 0.0, 0.0};
  bool offset_set[4] = {false, false, false, false};
  if (has_offsets) {
    if (!it_offsets->IsObject()) {
      return util::error_json("invalid offsets: expected an object with optional left, top, right, bottom numbers");
    }
    for (const auto &entry : it_offsets->GetObject()) {
      int idx = -1;
      if (entry.first == "left") {
        idx = 0;
      } else if (entry.first == "top") {
        idx = 1;
      } else if (entry.first == "right") {
        idx = 2;
      } else if (entry.first == "bottom") {
        idx = 3;
      } else {
        return util::error_json("unknown offsets key: " + entry.first + " — expected left, top, right or bottom");
      }
      if (!entry.second.IsNumber()) {
        return util::error_json("invalid offsets." + entry.first + ": expected a number");
      }
      offset_vals[idx] = entry.second.GetDouble();
      offset_set[idx] = true;
    }
  }
  int size_h = 0;
  int size_v = 0;
  if (has_size_h) {
    if (!it_size_h->IsInt()) {
      return util::error_json("invalid size_flags_h: expected an integer");
    }
    size_h = static_cast<int>(it_size_h->GetInt());
  }
  if (has_size_v) {
    if (!it_size_v->IsInt()) {
      return util::error_json("invalid size_flags_v: expected an integer");
    }
    size_v = static_cast<int>(it_size_v->GetInt());
  }
  int grow_h = 0;
  int grow_v = 0;
  if (has_grow_h) {
    if (!it_grow_h->IsInt()) {
      return util::error_json("invalid grow_h: expected an integer");
    }
    grow_h = static_cast<int>(it_grow_h->GetInt());
  }
  if (has_grow_v) {
    if (!it_grow_v->IsInt()) {
      return util::error_json("invalid grow_v: expected an integer");
    }
    grow_v = static_cast<int>(it_grow_v->GetInt());
  }
  double min_x = 0.0;
  double min_y = 0.0;
  bool min_set_x = false;
  bool min_set_y = false;
  if (has_min) {
    if (!it_min->IsObject()) {
      return util::error_json("invalid min_size: expected an object with optional x, y numbers");
    }
    for (const auto &entry : it_min->GetObject()) {
      if (entry.first != "x" && entry.first != "y") {
        return util::error_json("unknown min_size key: " + entry.first + " — expected x or y");
      }
      if (!entry.second.IsNumber()) {
        return util::error_json("invalid min_size." + entry.first + ": expected a number");
      }
      double v = entry.second.GetDouble();
      if (v < 0.0) {
        return util::error_json("invalid min_size." + entry.first + ": expected a non-negative number");
      }
      if (entry.first == "x") {
        min_x = v;
        min_set_x = true;
      } else {
        min_y = v;
        min_set_y = true;
      }
    }
    if (!min_set_x && !min_set_y) {
      return util::error_json("invalid min_size: expected at least one of x or y");
    }
  }
  if (!has_preset && !has_anchors && !has_offsets && !has_size_h && !has_size_v && !has_grow_h && !has_grow_v && !has_min) {
    return util::error_json("no layout change requested: pass preset, anchors, offsets, size_flags_h, size_flags_v, grow_h, grow_v or min_size");
  }
  std::string error;
  auto *control = resolve_control_or_error(path, error);
  if (!control) {
    return util::error_json(error);
  }
  godot::Variant old_vals[13];
  old_vals[0] = control->get(godot::StringName("anchor_left"));
  old_vals[1] = control->get(godot::StringName("anchor_top"));
  old_vals[2] = control->get(godot::StringName("anchor_right"));
  old_vals[3] = control->get(godot::StringName("anchor_bottom"));
  old_vals[4] = control->get(godot::StringName("offset_left"));
  old_vals[5] = control->get(godot::StringName("offset_top"));
  old_vals[6] = control->get(godot::StringName("offset_right"));
  old_vals[7] = control->get(godot::StringName("offset_bottom"));
  old_vals[8] = control->get(godot::StringName("size_flags_horizontal"));
  old_vals[9] = control->get(godot::StringName("size_flags_vertical"));
  old_vals[10] = control->get(godot::StringName("grow_horizontal"));
  old_vals[11] = control->get(godot::StringName("grow_vertical"));
  old_vals[12] = control->get(godot::StringName("custom_minimum_size"));
  if (has_preset) {
    control->set_anchors_preset(static_cast<godot::Control::LayoutPreset>(preset), keep_offsets);
  }
  if (has_anchors) {
    if (anchor_set[0]) {
      control->set(godot::StringName("anchor_left"), godot::Variant(anchor_vals[0]));
    }
    if (anchor_set[1]) {
      control->set(godot::StringName("anchor_top"), godot::Variant(anchor_vals[1]));
    }
    if (anchor_set[2]) {
      control->set(godot::StringName("anchor_right"), godot::Variant(anchor_vals[2]));
    }
    if (anchor_set[3]) {
      control->set(godot::StringName("anchor_bottom"), godot::Variant(anchor_vals[3]));
    }
  }
  if (has_offsets) {
    if (offset_set[0]) {
      control->set(godot::StringName("offset_left"), godot::Variant(offset_vals[0]));
    }
    if (offset_set[1]) {
      control->set(godot::StringName("offset_top"), godot::Variant(offset_vals[1]));
    }
    if (offset_set[2]) {
      control->set(godot::StringName("offset_right"), godot::Variant(offset_vals[2]));
    }
    if (offset_set[3]) {
      control->set(godot::StringName("offset_bottom"), godot::Variant(offset_vals[3]));
    }
  }
  if (has_size_h) {
    control->set(godot::StringName("size_flags_horizontal"), godot::Variant(static_cast<int64_t>(size_h)));
  }
  if (has_size_v) {
    control->set(godot::StringName("size_flags_vertical"), godot::Variant(static_cast<int64_t>(size_v)));
  }
  if (has_grow_h) {
    control->set(godot::StringName("grow_horizontal"), godot::Variant(static_cast<int64_t>(grow_h)));
  }
  if (has_grow_v) {
    control->set(godot::StringName("grow_vertical"), godot::Variant(static_cast<int64_t>(grow_v)));
  }
  if (has_min) {
    godot::Variant cur_var = control->get(godot::StringName("custom_minimum_size"));
    godot::Vector2 cur = static_cast<godot::Vector2>(cur_var);
    double nx = min_set_x ? min_x : static_cast<double>(cur.x);
    double ny = min_set_y ? min_y : static_cast<double>(cur.y);
    control->set(godot::StringName("custom_minimum_size"), godot::Variant(godot::Vector2(static_cast<float>(nx), static_cast<float>(ny))));
  }
  godot::Variant new_vals[13];
  new_vals[0] = control->get(godot::StringName("anchor_left"));
  new_vals[1] = control->get(godot::StringName("anchor_top"));
  new_vals[2] = control->get(godot::StringName("anchor_right"));
  new_vals[3] = control->get(godot::StringName("anchor_bottom"));
  new_vals[4] = control->get(godot::StringName("offset_left"));
  new_vals[5] = control->get(godot::StringName("offset_top"));
  new_vals[6] = control->get(godot::StringName("offset_right"));
  new_vals[7] = control->get(godot::StringName("offset_bottom"));
  new_vals[8] = control->get(godot::StringName("size_flags_horizontal"));
  new_vals[9] = control->get(godot::StringName("size_flags_vertical"));
  new_vals[10] = control->get(godot::StringName("grow_horizontal"));
  new_vals[11] = control->get(godot::StringName("grow_vertical"));
  new_vals[12] = control->get(godot::StringName("custom_minimum_size"));
  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  bool undoable = undo_redo != nullptr;
  if (undoable) {
    bool any = false;
    for (int i = 0; i < 13; i++) {
      if (old_vals[i] != new_vals[i]) {
        any = true;
        break;
      }
    }
    if (any) {
      undo_redo->create_action(godot::String("Set Control Layout"));
      if (old_vals[0] != new_vals[0]) {
        undo_redo->add_do_property(control, godot::StringName("anchor_left"), new_vals[0]);
        undo_redo->add_undo_property(control, godot::StringName("anchor_left"), old_vals[0]);
      }
      if (old_vals[1] != new_vals[1]) {
        undo_redo->add_do_property(control, godot::StringName("anchor_top"), new_vals[1]);
        undo_redo->add_undo_property(control, godot::StringName("anchor_top"), old_vals[1]);
      }
      if (old_vals[2] != new_vals[2]) {
        undo_redo->add_do_property(control, godot::StringName("anchor_right"), new_vals[2]);
        undo_redo->add_undo_property(control, godot::StringName("anchor_right"), old_vals[2]);
      }
      if (old_vals[3] != new_vals[3]) {
        undo_redo->add_do_property(control, godot::StringName("anchor_bottom"), new_vals[3]);
        undo_redo->add_undo_property(control, godot::StringName("anchor_bottom"), old_vals[3]);
      }
      if (old_vals[4] != new_vals[4]) {
        undo_redo->add_do_property(control, godot::StringName("offset_left"), new_vals[4]);
        undo_redo->add_undo_property(control, godot::StringName("offset_left"), old_vals[4]);
      }
      if (old_vals[5] != new_vals[5]) {
        undo_redo->add_do_property(control, godot::StringName("offset_top"), new_vals[5]);
        undo_redo->add_undo_property(control, godot::StringName("offset_top"), old_vals[5]);
      }
      if (old_vals[6] != new_vals[6]) {
        undo_redo->add_do_property(control, godot::StringName("offset_right"), new_vals[6]);
        undo_redo->add_undo_property(control, godot::StringName("offset_right"), old_vals[6]);
      }
      if (old_vals[7] != new_vals[7]) {
        undo_redo->add_do_property(control, godot::StringName("offset_bottom"), new_vals[7]);
        undo_redo->add_undo_property(control, godot::StringName("offset_bottom"), old_vals[7]);
      }
      if (old_vals[8] != new_vals[8]) {
        undo_redo->add_do_property(control, godot::StringName("size_flags_horizontal"), new_vals[8]);
        undo_redo->add_undo_property(control, godot::StringName("size_flags_horizontal"), old_vals[8]);
      }
      if (old_vals[9] != new_vals[9]) {
        undo_redo->add_do_property(control, godot::StringName("size_flags_vertical"), new_vals[9]);
        undo_redo->add_undo_property(control, godot::StringName("size_flags_vertical"), old_vals[9]);
      }
      if (old_vals[10] != new_vals[10]) {
        undo_redo->add_do_property(control, godot::StringName("grow_horizontal"), new_vals[10]);
        undo_redo->add_undo_property(control, godot::StringName("grow_horizontal"), old_vals[10]);
      }
      if (old_vals[11] != new_vals[11]) {
        undo_redo->add_do_property(control, godot::StringName("grow_vertical"), new_vals[11]);
        undo_redo->add_undo_property(control, godot::StringName("grow_vertical"), old_vals[11]);
      }
      if (old_vals[12] != new_vals[12]) {
        undo_redo->add_do_property(control, godot::StringName("custom_minimum_size"), new_vals[12]);
        undo_redo->add_undo_property(control, godot::StringName("custom_minimum_size"), old_vals[12]);
      }
      undo_redo->commit_action(false);
    }
  }
  scene_dirty_tracker::mark_scene_modified();
  mcp::JsonValue out_anchors(mcp::JsonValue::object_tag);
  out_anchors["left"] = mcp::JsonValue(static_cast<double>(new_vals[0]));
  out_anchors["top"] = mcp::JsonValue(static_cast<double>(new_vals[1]));
  out_anchors["right"] = mcp::JsonValue(static_cast<double>(new_vals[2]));
  out_anchors["bottom"] = mcp::JsonValue(static_cast<double>(new_vals[3]));
  mcp::JsonValue out_offsets(mcp::JsonValue::object_tag);
  out_offsets["left"] = mcp::JsonValue(static_cast<double>(new_vals[4]));
  out_offsets["top"] = mcp::JsonValue(static_cast<double>(new_vals[5]));
  out_offsets["right"] = mcp::JsonValue(static_cast<double>(new_vals[6]));
  out_offsets["bottom"] = mcp::JsonValue(static_cast<double>(new_vals[7]));
  godot::Vector2 final_min = static_cast<godot::Vector2>(new_vals[12]);
  mcp::JsonValue out_min(mcp::JsonValue::object_tag);
  out_min["x"] = mcp::JsonValue(static_cast<double>(final_min.x));
  out_min["y"] = mcp::JsonValue(static_cast<double>(final_min.y));
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  out["path"] = mcp::JsonValue(path);
  if (has_preset) {
    out["preset"] = mcp::JsonValue(static_cast<int64_t>(preset));
    out["keep_offsets"] = mcp::JsonValue(keep_offsets);
  }
  out["anchors"] = std::move(out_anchors);
  out["offsets"] = std::move(out_offsets);
  out["size_flags_h"] = mcp::JsonValue(static_cast<int64_t>(new_vals[8]));
  out["size_flags_v"] = mcp::JsonValue(static_cast<int64_t>(new_vals[9]));
  out["grow_h"] = mcp::JsonValue(static_cast<int64_t>(new_vals[10]));
  out["grow_v"] = mcp::JsonValue(static_cast<int64_t>(new_vals[11]));
  out["min_size"] = std::move(out_min);
  out["undoable"] = mcp::JsonValue(undoable);
  if (!undoable) {
    out["undo_skip_reason"] = mcp::JsonValue("EditorUndoRedoManager not available");
  }
  out["note"] = mcp::JsonValue(kSceneNotSavedNote);
  return out;
}

mcp::JsonValue handle_set_theme_item_bundle(const mcp::JsonValue &args) {
  std::string path;
  if (!need_string(args, "path", path)) {
    return util::error_json("missing required parameter: path");
  }
  auto *it_colors = args.Find("colors");
  auto *it_constants = args.Find("constants");
  auto *it_font_sizes = args.Find("font_sizes");
  auto *it_styleboxes = args.Find("styleboxes");
  bool has_colors = it_colors != nullptr;
  bool has_constants = it_constants != nullptr;
  bool has_font_sizes = it_font_sizes != nullptr;
  bool has_styleboxes = it_styleboxes != nullptr;
  if (!has_colors && !has_constants && !has_font_sizes && !has_styleboxes) {
    return util::error_json(
        "no theme item change requested: pass colors, constants, font_sizes "
        "or styleboxes");
  }
  struct StagedColor {
    std::string theme_type;
    std::string varname;
    godot::Color color;
  };
  struct StagedInt {
    std::string theme_type;
    std::string varname;
    int32_t value;
  };
  struct StagedBox {
    std::string theme_type;
    std::string varname;
    godot::Ref<godot::StyleBoxFlat> box;
  };
  std::vector<StagedColor> staged_colors;
  std::vector<StagedInt> staged_constants;
  std::vector<StagedInt> staged_font_sizes;
  std::vector<StagedBox> staged_boxes;
  std::string error;
  if (has_colors) {
    if (!it_colors->IsObject()) {
      return util::error_json(
          "invalid colors: expected an object mapping theme_type to {varname: "
          "color} objects");
    }
    for (const auto &type_entry : it_colors->GetObject()) {
      const std::string theme_type = type_entry.first;
      if (theme_type.empty()) {
        return util::error_json(
            "invalid colors: theme_type must be a non-empty string");
      }
      if (!type_entry.second.IsObject()) {
        return util::error_json("invalid colors." + theme_type +
                                ": expected an object mapping varname to "
                                "color values");
      }
      for (const auto &var_entry : type_entry.second.GetObject()) {
        const std::string varname = var_entry.first;
        if (varname.empty()) {
          return util::error_json("invalid colors." + theme_type +
                                  ": varname must be a non-empty string");
        }
        godot::Color color;
        if (!parse_color_value(var_entry.second, color, error)) {
          return util::error_json("colors." + theme_type + "." + varname +
                                  ": " + error);
        }
        StagedColor item;
        item.theme_type = theme_type;
        item.varname = varname;
        item.color = color;
        staged_colors.push_back(item);
      }
    }
  }
  if (has_constants) {
    if (!it_constants->IsObject()) {
      return util::error_json(
          "invalid constants: expected an object mapping theme_type to "
          "{varname: integer} objects");
    }
    for (const auto &type_entry : it_constants->GetObject()) {
      const std::string theme_type = type_entry.first;
      if (theme_type.empty()) {
        return util::error_json(
            "invalid constants: theme_type must be a non-empty string");
      }
      if (!type_entry.second.IsObject()) {
        return util::error_json(
            "invalid constants." + theme_type +
            ": expected an object mapping varname to integer values");
      }
      for (const auto &var_entry : type_entry.second.GetObject()) {
        const std::string varname = var_entry.first;
        if (varname.empty()) {
          return util::error_json("invalid constants." + theme_type +
                                  ": varname must be a non-empty string");
        }
        if (!var_entry.second.IsInt()) {
          return util::error_json("invalid constants." + theme_type + "." +
                                  varname + ": expected an integer");
        }
        StagedInt item;
        item.theme_type = theme_type;
        item.varname = varname;
        item.value = static_cast<int32_t>(var_entry.second.GetInt());
        staged_constants.push_back(item);
      }
    }
  }
  if (has_font_sizes) {
    if (!it_font_sizes->IsObject()) {
      return util::error_json(
          "invalid font_sizes: expected an object mapping theme_type to "
          "{varname: integer} objects");
    }
    for (const auto &type_entry : it_font_sizes->GetObject()) {
      const std::string theme_type = type_entry.first;
      if (theme_type.empty()) {
        return util::error_json(
            "invalid font_sizes: theme_type must be a non-empty string");
      }
      if (!type_entry.second.IsObject()) {
        return util::error_json(
            "invalid font_sizes." + theme_type +
            ": expected an object mapping varname to integer values");
      }
      for (const auto &var_entry : type_entry.second.GetObject()) {
        const std::string varname = var_entry.first;
        if (varname.empty()) {
          return util::error_json("invalid font_sizes." + theme_type +
                                  ": varname must be a non-empty string");
        }
        if (!var_entry.second.IsInt()) {
          return util::error_json("invalid font_sizes." + theme_type + "." +
                                  varname + ": expected an integer");
        }
        StagedInt item;
        item.theme_type = theme_type;
        item.varname = varname;
        item.value = static_cast<int32_t>(var_entry.second.GetInt());
        staged_font_sizes.push_back(item);
      }
    }
  }
  if (has_styleboxes) {
    if (!it_styleboxes->IsObject()) {
      return util::error_json(
          "invalid styleboxes: expected an object mapping theme_type to "
          "{varname: stylebox} objects");
    }
    for (const auto &type_entry : it_styleboxes->GetObject()) {
      const std::string theme_type = type_entry.first;
      if (theme_type.empty()) {
        return util::error_json(
            "invalid styleboxes: theme_type must be a non-empty string");
      }
      if (!type_entry.second.IsObject()) {
        return util::error_json(
            "invalid styleboxes." + theme_type +
            ": expected an object mapping varname to stylebox objects");
      }
      for (const auto &var_entry : type_entry.second.GetObject()) {
        const std::string varname = var_entry.first;
        if (varname.empty()) {
          return util::error_json("invalid styleboxes." + theme_type +
                                  ": varname must be a non-empty string");
        }
        if (!var_entry.second.IsObject()) {
          return util::error_json(
              "invalid styleboxes." + theme_type + "." + varname +
              ": expected an object with optional bg_color, border_color, "
              "border_width, corner_radius, content_margin");
        }
        const std::string item_prefix =
            "styleboxes." + theme_type + "." + varname;
        godot::Ref<godot::StyleBoxFlat> box;
        box.instantiate();
        if (box.is_null()) {
          return util::error_json("failed to create StyleBoxFlat instance");
        }
        auto *it_bg = var_entry.second.Find("bg_color");
        if (it_bg) {
          godot::Color color;
          if (!parse_color_value(*it_bg, color, error)) {
            return util::error_json(item_prefix + ".bg_color: " + error);
          }
          box->set_bg_color(color);
        }
        auto *it_border = var_entry.second.Find("border_color");
        if (it_border) {
          godot::Color color;
          if (!parse_color_value(*it_border, color, error)) {
            return util::error_json(item_prefix + ".border_color: " + error);
          }
          box->set_border_color(color);
        }
        auto *it_width = var_entry.second.Find("border_width");
        if (it_width) {
          const std::string width_name = item_prefix + ".border_width";
          if (!apply_per_side(
                  *it_width,
                  [&box](double v) {
                    box->set_border_width_all(static_cast<int32_t>(v));
                  },
                  [&box](int side, double v) {
                    box->set_border_width(static_cast<godot::Side>(side),
                                          static_cast<int32_t>(v));
                  },
                  width_name.c_str(), kSideKeys, error)) {
            return util::error_json(error);
          }
        }
        auto *it_radius = var_entry.second.Find("corner_radius");
        if (it_radius) {
          const std::string radius_name = item_prefix + ".corner_radius";
          if (!apply_per_side(
                  *it_radius,
                  [&box](double v) {
                    box->set_corner_radius_all(static_cast<int32_t>(v));
                  },
                  [&box](int corner, double v) {
                    box->set_corner_radius(static_cast<godot::Corner>(corner),
                                           static_cast<int32_t>(v));
                  },
                  radius_name.c_str(), kCornerKeys, error)) {
            return util::error_json(error);
          }
        }
        auto *it_margin = var_entry.second.Find("content_margin");
        if (it_margin) {
          const std::string margin_name = item_prefix + ".content_margin";
          if (!apply_per_side(
                  *it_margin,
                  [&box](double v) {
                    box->set_content_margin_all(static_cast<float>(v));
                  },
                  [&box](int side, double v) {
                    box->set_content_margin(static_cast<godot::Side>(side),
                                            static_cast<float>(v));
                  },
                  margin_name.c_str(), kSideKeys, error)) {
            return util::error_json(error);
          }
        }
        StagedBox item;
        item.theme_type = theme_type;
        item.varname = varname;
        item.box = box;
        staged_boxes.push_back(item);
      }
    }
  }
  if (staged_colors.empty() && staged_constants.empty() &&
      staged_font_sizes.empty() && staged_boxes.empty()) {
    return util::error_json(
        "no theme item change requested: pass colors, constants, font_sizes "
        "or styleboxes with at least one item");
  }
  godot::Ref<godot::Theme> theme;
  if (!load_theme_or_error(path, theme, error)) {
    return util::error_json(error);
  }
  for (const auto &item : staged_colors) {
    theme->set_color(godot::StringName(item.varname.c_str()),
                     godot::StringName(item.theme_type.c_str()), item.color);
  }
  for (const auto &item : staged_constants) {
    theme->set_constant(godot::StringName(item.varname.c_str()),
                        godot::StringName(item.theme_type.c_str()),
                        item.value);
  }
  for (const auto &item : staged_font_sizes) {
    theme->set_font_size(godot::StringName(item.varname.c_str()),
                         godot::StringName(item.theme_type.c_str()),
                         item.value);
  }
  for (const auto &item : staged_boxes) {
    theme->set_stylebox(godot::StringName(item.varname.c_str()),
                        godot::StringName(item.theme_type.c_str()), item.box);
  }
  bool verified = false;
  if (!persist_theme(theme, path, error, verified)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["saved"] = mcp::JsonValue(true);
  r["verified"] = mcp::JsonValue(verified);
  r["colors_applied"] =
      mcp::JsonValue(static_cast<int64_t>(staged_colors.size()));
  r["constants_applied"] =
      mcp::JsonValue(static_cast<int64_t>(staged_constants.size()));
  r["font_sizes_applied"] =
      mcp::JsonValue(static_cast<int64_t>(staged_font_sizes.size()));
  r["styleboxes_applied"] =
      mcp::JsonValue(static_cast<int64_t>(staged_boxes.size()));
  return r;
}

} // namespace theme_ops
} // namespace godot_autopilot
