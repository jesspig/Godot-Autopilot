#include "theme_ops.hpp"

#include "core/scene_dirty_tracker.hpp"
#include "util/error_util.hpp"
#include "util/scene_path.hpp"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

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
                   const std::string &path, std::string &out_error) {
  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver) {
    out_error = "ResourceSaver not available";
    return false;
  }
  std::string dir_path = path;
  size_t last_slash = dir_path.find_last_of('/');
  if (last_slash != std::string::npos) {
    dir_path = dir_path.substr(0, last_slash);
    godot::String dir_gs(dir_path.c_str());
    if (!godot::DirAccess::dir_exists_absolute(dir_gs)) {
      godot::Error mk_err =
          godot::DirAccess::make_dir_recursive_absolute(dir_gs);
      if (mk_err != godot::Error::OK) {
        out_error = "failed to create directory: " + dir_path + " (error " +
                    std::to_string(static_cast<int>(mk_err)) + ")";
        return false;
      }
    }
  }
  godot::Error err = saver->save(theme, godot::String(path.c_str()));
  if (err != godot::OK) {
    out_error = "failed to save theme, error code: " +
                std::to_string(static_cast<int>(err));
    return false;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(godot::String(path.c_str()));
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
  if (!persist_theme(theme, path, save_error)) {
    return util::error_json(save_error);
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["saved"] = mcp::JsonValue(true);
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

  if (!persist_theme(theme, path, error)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
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

  if (!persist_theme(theme, path, error)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
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

  if (!persist_theme(theme, path, error)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
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

  if (!persist_theme(theme, path, error)) {
    return util::error_json(error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["theme_type"] = mcp::JsonValue(theme_type);
  r["varname"] = mcp::JsonValue(varname);
  r["saved"] = mcp::JsonValue(true);
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

} // namespace theme_ops
} // namespace godot_autopilot
