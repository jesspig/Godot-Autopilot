#ifndef GODOT_AUTOPILOT_EDITOR_UI_OPS_HPP
#define GODOT_AUTOPILOT_EDITOR_UI_OPS_HPP

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/window.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace editor_ui_ops {

struct Element {
  int id = 0;
  std::string path;
  std::string type;
  std::string name;
  std::string text;
  std::string tooltip;
  std::string accessibility_name;
  std::string placeholder;
  bool enabled = true;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  int window_id = 0;
};

struct WindowMapping {
  double scale_x = 1.0;
  double scale_y = 1.0;
  double offset_x = 0.0;
  double offset_y = 0.0;
};

struct MarkRect {
  int id = 0;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
};

std::vector<Element> collect_elements(const std::string &query, const std::string &type_filter,
                                      bool interactive_only, int max_elements, bool *out_truncated);
bool find_element_by_path(const std::string &path, Element *out);
godot::Control *find_control_by_path(const std::string &path);
godot::Window *window_for_id(int window_id);
std::vector<Element> hit_test(double window_x, double window_y, int window_id, int max_results);
bool editor_viewport_window_mapping(const std::string &which, int index, WindowMapping *out,
                                    std::string *error);
void draw_annotations(godot::Ref<godot::Image> image, const std::vector<MarkRect> &marks);
mcp::JsonValue handle_get_editor_ui_elements(const mcp::JsonValue &args);
mcp::JsonValue handle_hit_test_editor_point(const mcp::JsonValue &args);
mcp::JsonValue handle_get_editor_viewport_geometry(const mcp::JsonValue &args);

} // namespace editor_ui_ops
} // namespace godot_autopilot

#endif
