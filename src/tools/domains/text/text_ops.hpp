#ifndef GODOT_AUTOPILOT_TEXT_OPS_HPP
#define GODOT_AUTOPILOT_TEXT_OPS_HPP

#include "util/json_value.hpp"

#include <string>
#include <vector>

namespace godot_autopilot {
namespace resource_fs {
struct TextWriteResult;
}

namespace text_ops {

struct CSharpClassInfo {
  bool found = false;
  std::string name;
  bool is_partial = false;
  std::string base;
};

std::vector<CSharpClassInfo> parse_csharp_classes(const std::string &source);

godot_autopilot::util::JsonValue script_load_diagnostics(bool loaded, bool instantiable);

godot_autopilot::util::JsonValue file_write_failure_json(
    const std::string &path, const std::string &mode,
    const resource_fs::TextWriteResult &result);

godot_autopilot::util::JsonValue handle_create_font(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_shaped_text(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_font_set_antialiasing(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_font_set_data(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_font_set_hinting(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_system_font_path(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_has_feature(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_is_locale_right_to_left(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shaped_text_add_string(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shaped_text_get_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_font(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shaped_text(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_text_query(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_file_write(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_csharp_script(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_file_read(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_find_in_files(const godot_autopilot::util::JsonValue &args);

} // namespace text_ops
} // namespace godot_autopilot

#endif
