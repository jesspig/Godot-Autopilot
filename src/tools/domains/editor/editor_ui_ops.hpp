#ifndef GODOT_AUTOPILOT_EDITOR_UI_OPS_HPP
#define GODOT_AUTOPILOT_EDITOR_UI_OPS_HPP

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
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

struct SceneTreeRow {
  std::string path;
  std::string name;
  std::string type;
  int depth = 0;
  bool selected = false;
  bool collapsed = false;
  bool visible = false;
  bool has_rect = false;
  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
};

struct SceneNodeHit {
  std::string path;
  std::string name;
  std::string type;
};

std::string lower_ascii(std::string text);
int clamp_scene_tree_max_items(int value);
bool scene_tree_row_matches(const std::string &path, const std::string &name,
                            const std::string &filter, bool selected,
                            bool selected_only);
std::string scene_tree_relative_path(const std::string &parent_path,
                                     const std::string &name);

godot::Tree *find_scene_tree(std::string *error);
bool collect_scene_tree_rows(godot::Tree *tree, const std::string &filter,
                             bool selected_only, int max_items,
                             std::vector<SceneTreeRow> *out, bool *out_truncated,
                             int *out_window_id, std::string *error);
bool find_scene_tree_item(godot::Tree *tree, godot::Node *node,
                          godot::TreeItem **out_item);
bool scene_tree_row_at_point(double window_x, double window_y, int window_id,
                             SceneTreeRow *out, std::string *error);
std::vector<SceneNodeHit> pick_scene_nodes_2d(double window_x, double window_y,
                                              int window_id, int max_results,
                                              std::string *error);
mcp::JsonValue scene_tree_row_to_json(const SceneTreeRow &row);

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
mcp::JsonValue handle_scene_tree_items(const mcp::JsonValue &args);
mcp::JsonValue handle_select_scene_tree_node(const mcp::JsonValue &args);

} // namespace editor_ui_ops
} // namespace godot_autopilot

#endif
