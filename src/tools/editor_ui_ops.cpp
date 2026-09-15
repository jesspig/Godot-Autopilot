#include "editor_ui_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/scene_path.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace editor_ui_ops {

namespace {

int clampi(int value, int low, int high) {
  if (value < low)
    return low;
  if (value > high)
    return high;
  return value;
}

} // namespace

std::string lower_ascii(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
  }
  return text;
}

int clamp_scene_tree_max_items(int value) { return clampi(value, 1, 1000); }

bool scene_tree_row_matches(const std::string &path, const std::string &name,
                            const std::string &filter, bool selected,
                            bool selected_only) {
  if (selected_only && !selected)
    return false;
  if (filter.empty())
    return true;
  const std::string needle = lower_ascii(filter);
  if (lower_ascii(path).find(needle) != std::string::npos)
    return true;
  return lower_ascii(name).find(needle) != std::string::npos;
}

std::string scene_tree_relative_path(const std::string &parent_path,
                                     const std::string &name) {
  if (name.empty())
    return parent_path;
  if (parent_path.empty())
    return name;
  return parent_path + "/" + name;
}

namespace {

std::string read_string_prop(const godot::Object *object, const godot::StringName &name) {
  const godot::Variant value = object->get(name);
  if (value.get_type() != godot::Variant::STRING)
    return std::string();
  return util::to_std(static_cast<godot::String>(value));
}

bool read_bool_prop(const godot::Object *object, const godot::StringName &name, bool fallback) {
  const godot::Variant value = object->get(name);
  if (value.get_type() != godot::Variant::BOOL)
    return fallback;
  return static_cast<bool>(value);
}

godot::Window *window_of(const godot::Node *node) {
  return godot::Object::cast_to<godot::Window>(node->get_viewport());
}

Element make_element(godot::Control *control, int id) {
  Element element;
  element.id = id;
  element.path = util::to_std(godot::String(control->get_path()));
  element.type = util::to_std(control->get_class());
  element.name = util::to_std(godot::String(control->get_name()));
  element.text = read_string_prop(control, "text");
  if (element.text.empty())
    element.text = read_string_prop(control, "title");
  element.tooltip = util::to_std(control->get_tooltip_text());
  element.accessibility_name = util::to_std(control->get_accessibility_name());
  element.placeholder = read_string_prop(control, "placeholder_text");
  element.enabled = !read_bool_prop(control, "disabled", false);

  godot::Window *window = window_of(control);
  godot::Vector2 window_position;
  if (window) {
    const godot::Vector2i window_pos = window->get_position();
    window_position = godot::Vector2(static_cast<float>(window_pos.x),
                                     static_cast<float>(window_pos.y));
    element.window_id = window->get_window_id();
  }
  const godot::Vector2 client = control->get_screen_position() - window_position;
  const godot::Vector2 size = control->get_size();
  element.x = client.x;
  element.y = client.y;
  element.w = size.x;
  element.h = size.y;
  return element;
}

bool is_interactive_control(const godot::Control *control) {
  static const char *kInteractiveClasses[] = {
      "BaseButton", "LineEdit", "TextEdit", "CodeEdit", "TabBar", "TabContainer",
      "Tree", "ItemList", "SpinBox", "Slider", "ColorPickerButton", "ColorPicker",
      "MenuBar", "ScrollBar"};
  for (const char *class_name : kInteractiveClasses) {
    if (control->is_class(class_name))
      return true;
  }
  return false;
}

struct CollectFilter {
  std::string query_lower;
  std::string type_filter;
  bool interactive_only = false;
};

bool element_matches(const Element &element, const CollectFilter &filter) {
  if (filter.query_lower.empty())
    return true;
  const std::string fields[] = {element.path, element.name, element.text,
                                element.tooltip, element.placeholder};
  for (const std::string &field : fields) {
    if (lower_ascii(field).find(filter.query_lower) != std::string::npos)
      return true;
  }
  return false;
}

void collect_walk(godot::Node *node, const CollectFilter &filter, int max_elements,
                  std::vector<Element> *out, int *next_id, bool *truncated) {
  const int child_count = node->get_child_count(true);
  for (int i = 0; i < child_count; ++i) {
    godot::Node *child = node->get_child(i, true);
    if (!child)
      continue;
    if (auto *control = godot::Object::cast_to<godot::Control>(child)) {
      if (!control->is_visible_in_tree())
        continue;
      bool pre_match = true;
      if (!filter.type_filter.empty() &&
          util::to_std(control->get_class()) != filter.type_filter)
        pre_match = false;
      if (pre_match && filter.interactive_only && !is_interactive_control(control))
        pre_match = false;
      if (pre_match) {
        Element element = make_element(control, 0);
        if (element_matches(element, filter)) {
          if (max_elements > 0 && static_cast<int>(out->size()) >= max_elements) {
            *truncated = true;
            return;
          }
          element.id = (*next_id)++;
          out->push_back(std::move(element));
        }
      }
      collect_walk(control, filter, max_elements, out, next_id, truncated);
      if (*truncated)
        return;
    } else {
      collect_walk(child, filter, max_elements, out, next_id, truncated);
      if (*truncated)
        return;
    }
  }
}

// Approximation of Viewport::_gui_find_control_at_pos: reverse child order,
// recursing into children before testing the control itself.
godot::Control *find_hit_control(godot::Node *node, const godot::Vector2 &point) {
  const int child_count = node->get_child_count(true);
  for (int i = child_count - 1; i >= 0; --i) {
    godot::Node *child = node->get_child(i, true);
    if (!child)
      continue;
    if (auto *control = godot::Object::cast_to<godot::Control>(child)) {
      if (!control->is_visible_in_tree())
        continue;
      const godot::Transform2D global = control->get_global_transform();
      const godot::Vector2 local = global.affine_inverse().xform(point);
      const godot::Rect2 local_rect(godot::Vector2(), control->get_size());
      const bool inside = local_rect.has_point(local);
      if (control->is_clipping_contents() && !inside)
        continue;
      if (godot::Control *hit = find_hit_control(control, point))
        return hit;
      if (!inside)
        continue;
      if (control->get_mouse_filter() == godot::Control::MOUSE_FILTER_IGNORE)
        continue;
      return control;
    }
    if (godot::Object::cast_to<godot::Window>(child))
      continue;
    if (godot::Control *hit = find_hit_control(child, point))
      return hit;
  }
  return nullptr;
}

godot::Control *find_control_walk(godot::Node *node, const std::string &path) {
  const int child_count = node->get_child_count(true);
  for (int i = 0; i < child_count; ++i) {
    godot::Node *child = node->get_child(i, true);
    if (!child)
      continue;
    if (auto *control = godot::Object::cast_to<godot::Control>(child)) {
      if (!control->is_visible_in_tree())
        continue;
      if (util::to_std(godot::String(control->get_path())) == path)
        return control;
      if (godot::Control *found = find_control_walk(control, path))
        return found;
    } else {
      if (godot::Control *found = find_control_walk(child, path))
        return found;
    }
  }
  return nullptr;
}

bool node_is_self_or_descendant(const godot::Node *node,
                                const godot::Node *ancestor) {
  if (!node || !ancestor)
    return false;
  for (const godot::Node *current = node; current; current = current->get_parent()) {
    if (current == ancestor)
      return true;
  }
  return false;
}

godot::Node *scene_tree_metadata_node(godot::TreeItem *item,
                                      godot::Node *scene_root) {
  if (!item || !scene_root)
    return nullptr;
  const godot::Variant metadata = item->get_metadata(0);
  if (metadata.get_type() != godot::Variant::NODE_PATH)
    return nullptr;
  const godot::NodePath node_path = static_cast<godot::NodePath>(metadata);
  if (node_path.is_empty())
    return nullptr;
  godot::Node *node = scene_root->get_node_or_null(node_path);
  if (!node || !node_is_self_or_descendant(node, scene_root))
    return nullptr;
  return node;
}

void collect_tree_controls(godot::Node *node,
                           std::vector<godot::Tree *> *out) {
  if (auto *tree = godot::Object::cast_to<godot::Tree>(node))
    out->push_back(tree);
  const int child_count = node->get_child_count(true);
  for (int i = 0; i < child_count; ++i) {
    godot::Node *child = node->get_child(i, true);
    if (child)
      collect_tree_controls(child, out);
  }
}

int score_scene_tree_candidate(godot::Tree *tree, godot::Node *scene_root) {
  godot::TreeItem *root = tree->get_root();
  if (!root)
    return 0;
  if (scene_tree_metadata_node(root, scene_root) == scene_root)
    return 3;
  godot::TreeItem *child = root->get_first_child();
  for (int guard = 0; child && guard < 64; child = child->get_next(), ++guard) {
    if (scene_tree_metadata_node(child, scene_root))
      return 2;
  }
  if (!root->get_text(0).is_empty() &&
      root->get_text(0) == godot::String(scene_root->get_name()))
    return 1;
  return 0;
}

godot::Tree *find_scene_tree_walk(godot::Node *base, godot::Node *scene_root) {
  std::vector<godot::Tree *> trees;
  collect_tree_controls(base, &trees);
  godot::Tree *best = nullptr;
  int best_score = 0;
  for (godot::Tree *tree : trees) {
    const int score = score_scene_tree_candidate(tree, scene_root);
    if (score > best_score) {
      best_score = score;
      best = tree;
    }
    if (best_score >= 3)
      break;
  }
  return best;
}

godot::Tree *ancestor_tree_of(godot::Node *node) {
  for (godot::Node *current = node; current; current = current->get_parent()) {
    if (auto *tree = godot::Object::cast_to<godot::Tree>(current))
      return tree;
  }
  return nullptr;
}

int tree_item_depth(godot::TreeItem *item) {
  int depth = 0;
  for (godot::TreeItem *parent = item->get_parent(); parent;
       parent = parent->get_parent()) {
    ++depth;
  }
  return depth;
}

std::string node_scene_path(godot::Node *node, godot::Node *scene_root) {
  if (!node || !scene_root)
    return std::string();
  if (node == scene_root)
    return util::to_std(node->get_name());
  const godot::NodePath relative = scene_root->get_path_to(node);
  const std::string path = util::to_std(godot::String(relative));
  if (path.empty() || path.compare(0, 2, "..") == 0)
    return std::string();
  return path;
}

godot::Vector2 tree_client_origin(godot::Control *tree, int *out_window_id) {
  if (out_window_id)
    *out_window_id = 0;
  if (!tree)
    return godot::Vector2();
  godot::Window *window = window_of(tree);
  if (!window)
    return tree->get_screen_position();
  if (out_window_id)
    *out_window_id = window->get_window_id();
  const godot::Vector2i window_pos = window->get_position();
  return tree->get_screen_position() -
         godot::Vector2(static_cast<float>(window_pos.x),
                        static_cast<float>(window_pos.y));
}

void fill_scene_tree_row(godot::Tree *tree, godot::TreeItem *item,
                         godot::Node *node, const std::string &path,
                         const std::string &name, int depth,
                         const godot::Vector2 &tree_origin,
                         SceneTreeRow *out) {
  out->path = path;
  out->name = name;
  out->depth = depth;
  out->selected = item->is_selected(0);
  out->collapsed = item->is_collapsed();
  out->visible = item->is_visible_in_tree();
  if (node)
    out->type = util::to_std(node->get_class());
  if (out->visible) {
    const godot::Rect2 rect = tree->get_item_area_rect(item, 0);
    out->has_rect = true;
    out->x = static_cast<double>(tree_origin.x + rect.position.x);
    out->y = static_cast<double>(tree_origin.y + rect.position.y);
    out->w = static_cast<double>(rect.size.x);
    out->h = static_cast<double>(rect.size.y);
  }
}

struct SceneTreeWalk {
  godot::Tree *tree = nullptr;
  godot::Node *scene_root = nullptr;
  godot::Vector2 tree_origin;
  std::string filter;
  bool selected_only = false;
  int max_items = 200;
  std::vector<SceneTreeRow> *rows = nullptr;
  bool *truncated = nullptr;
};

void walk_scene_tree_rows(godot::TreeItem *item, const std::string &parent_path,
                          int depth, SceneTreeWalk *walk) {
  for (godot::TreeItem *child = item->get_first_child(); child;
       child = child->get_next()) {
    godot::Node *node = scene_tree_metadata_node(child, walk->scene_root);
    std::string name = util::to_std(child->get_text(0));
    if (node)
      name = util::to_std(node->get_name());
    std::string path = node ? node_scene_path(node, walk->scene_root)
                            : std::string();
    if (path.empty())
      path = scene_tree_relative_path(parent_path, name);
    if (scene_tree_row_matches(path, name, walk->filter, child->is_selected(0),
                               walk->selected_only)) {
      if (static_cast<int>(walk->rows->size()) >= walk->max_items) {
        *walk->truncated = true;
        return;
      }
      SceneTreeRow row;
      fill_scene_tree_row(walk->tree, child, node, path, name, depth,
                          walk->tree_origin, &row);
      walk->rows->push_back(std::move(row));
    }
    walk_scene_tree_rows(child, path, depth + 1, walk);
    if (*walk->truncated)
      return;
  }
}

bool canvas_item_client_rect(godot::CanvasItem *item,
                             const godot::Transform2D &canvas_to_client,
                             godot::Rect2 *out) {
  godot::Rect2 local;
  if (auto *control = godot::Object::cast_to<godot::Control>(item)) {
    local = control->get_rect();
  } else if (auto *sprite = godot::Object::cast_to<godot::Sprite2D>(item)) {
    local = sprite->get_rect();
  } else {
    return false;
  }
  const godot::Transform2D global = item->get_global_transform();
  const godot::Vector2 corner_a = canvas_to_client.xform(global.xform(local.position));
  const godot::Vector2 corner_b =
      canvas_to_client.xform(global.xform(local.position + local.size));
  out->position = godot::Vector2(std::min(corner_a.x, corner_b.x),
                                 std::min(corner_a.y, corner_b.y));
  out->size = godot::Vector2(std::fabs(corner_b.x - corner_a.x),
                             std::fabs(corner_b.y - corner_a.y));
  return true;
}

void collect_scene_node_hits(godot::Node *node,
                             const godot::Transform2D &canvas_to_client,
                             const godot::Vector2 &point,
                             std::vector<godot::Node *> *hits) {
  const int child_count = node->get_child_count(true);
  for (int i = 0; i < child_count; ++i) {
    godot::Node *child = node->get_child(i, true);
    if (!child)
      continue;
    if (auto *canvas_item = godot::Object::cast_to<godot::CanvasItem>(child)) {
      if (canvas_item->is_visible_in_tree()) {
        godot::Rect2 rect;
        if (canvas_item_client_rect(canvas_item, canvas_to_client, &rect) &&
            rect.has_point(point)) {
          hits->push_back(child);
        }
      }
    }
    collect_scene_node_hits(child, canvas_to_client, point, hits);
  }
}

godot::SubViewport *resolve_editor_viewport(const std::string &which, int index,
                                            std::string *error) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    if (error)
      *error = "EditorInterface not available";
    return nullptr;
  }
  if (which == "2d")
    return editor->get_editor_viewport_2d();
  if (which == "3d") {
    if (index < 0 || index > 3) {
      if (error)
        *error = "invalid index for 3d viewport: must be between 0 and 3";
      return nullptr;
    }
    return editor->get_editor_viewport_3d(index);
  }
  if (error)
    *error = "invalid viewport: must be '2d' or '3d'";
  return nullptr;
}

int viewport_image_dimension(godot::SubViewport *viewport, bool width) {
  if (godot::Ref<godot::ViewportTexture> texture = viewport->get_texture(); !texture.is_null()) {
    if (godot::Ref<godot::Image> image = texture->get_image(); !image.is_null())
      return width ? image->get_width() : image->get_height();
  }
  const godot::Vector2 visible = viewport->get_visible_rect().size;
  const double value = width ? visible.x : visible.y;
  return static_cast<int>(std::lround(value));
}

mcp::JsonValue element_to_json(const Element &element) {
  mcp::JsonValue object(mcp::JsonValue::object_tag);
  object["id"] = mcp::JsonValue(element.id);
  if (!element.path.empty())
    object["path"] = mcp::JsonValue(element.path);
  if (!element.type.empty())
    object["type"] = mcp::JsonValue(element.type);
  if (!element.name.empty())
    object["name"] = mcp::JsonValue(element.name);
  if (!element.text.empty())
    object["text"] = mcp::JsonValue(element.text);
  if (!element.tooltip.empty())
    object["tooltip"] = mcp::JsonValue(element.tooltip);
  if (!element.accessibility_name.empty())
    object["accessibility_name"] = mcp::JsonValue(element.accessibility_name);
  if (!element.placeholder.empty())
    object["placeholder"] = mcp::JsonValue(element.placeholder);
  object["enabled"] = mcp::JsonValue(element.enabled);
  mcp::JsonValue position(mcp::JsonValue::object_tag);
  position["x"] = mcp::JsonValue(element.x);
  position["y"] = mcp::JsonValue(element.y);
  object["position"] = std::move(position);
  mcp::JsonValue size(mcp::JsonValue::object_tag);
  size["x"] = mcp::JsonValue(element.w);
  size["y"] = mcp::JsonValue(element.h);
  object["size"] = std::move(size);
  object["window_id"] = mcp::JsonValue(element.window_id);
  return object;
}

bool read_int_param(const mcp::JsonValue &args, const char *name, int default_value, int *out,
                    std::string *error) {
  *out = default_value;
  const mcp::JsonValue *value = args.Find(name);
  if (!value)
    return true;
  if (!value->IsNumber()) {
    *error = std::string("invalid parameter: ") + name + " must be an integer";
    return false;
  }
  const double raw = value->IsInt() ? static_cast<double>(value->GetInt()) : value->GetDouble();
  if (raw >= static_cast<double>(INT32_MAX))
    *out = INT32_MAX;
  else if (raw <= static_cast<double>(INT32_MIN))
    *out = INT32_MIN;
  else
    *out = static_cast<int>(raw);
  return true;
}

bool read_bool_param(const mcp::JsonValue &args, const char *name, bool default_value, bool *out,
                     std::string *error) {
  *out = default_value;
  const mcp::JsonValue *value = args.Find(name);
  if (!value)
    return true;
  if (!value->IsBool()) {
    *error = std::string("invalid parameter: ") + name + " must be a boolean";
    return false;
  }
  *out = value->GetBool();
  return true;
}

bool read_string_param(const mcp::JsonValue &args, const char *name, std::string *out,
                       std::string *error) {
  const mcp::JsonValue *value = args.Find(name);
  if (!value)
    return true;
  if (!value->IsString()) {
    *error = std::string("invalid parameter: ") + name + " must be a string";
    return false;
  }
  *out = value->GetString();
  return true;
}

bool extract_vec2(const mcp::JsonValue &object, godot::Vector2 *out) {
  const mcp::JsonValue *x = object.Find("x");
  const mcp::JsonValue *y = object.Find("y");
  if (!x || !x->IsNumber() || !y || !y->IsNumber())
    return false;
  const double xv = x->IsInt() ? static_cast<double>(x->GetInt()) : x->GetDouble();
  const double yv = y->IsInt() ? static_cast<double>(y->GetInt()) : y->GetDouble();
  *out = godot::Vector2(static_cast<float>(xv), static_cast<float>(yv));
  return true;
}

const uint8_t kDigitGlyphs[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7}, {0x7, 0x1, 0x7, 0x4, 0x7},
    {0x7, 0x1, 0x7, 0x1, 0x7}, {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
    {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x2, 0x2, 0x2}, {0x7, 0x5, 0x7, 0x5, 0x7},
    {0x7, 0x5, 0x7, 0x1, 0x7},
};

void fill_rect_clamped(const godot::Ref<godot::Image> &image, int image_width, int image_height,
                       int x, int y, int w, int h, const godot::Color &color) {
  const int x0 = clampi(x, 0, image_width);
  const int y0 = clampi(y, 0, image_height);
  const int x1 = clampi(x + w, 0, image_width);
  const int y1 = clampi(y + h, 0, image_height);
  if (x1 <= x0 || y1 <= y0)
    return;
  image->fill_rect(godot::Rect2i(x0, y0, x1 - x0, y1 - y0), color);
}

} // namespace

std::vector<Element> collect_elements(const std::string &query, const std::string &type_filter,
                                      bool interactive_only, int max_elements,
                                      bool *out_truncated) {
  std::vector<Element> elements;
  bool truncated = false;
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    if (godot::Control *base = editor->get_base_control()) {
      CollectFilter filter;
      filter.query_lower = lower_ascii(query);
      filter.type_filter = type_filter;
      filter.interactive_only = interactive_only;
      int next_id = 1;
      collect_walk(base, filter, max_elements, &elements, &next_id, &truncated);
    }
  }
  if (out_truncated)
    *out_truncated = truncated;
  return elements;
}

bool find_element_by_path(const std::string &path, Element *out) {
  if (!out)
    return false;
  const std::vector<Element> elements =
      collect_elements(std::string(), std::string(), false, 0, nullptr);
  for (const Element &element : elements) {
    if (element.path == path) {
      *out = element;
      return true;
    }
  }
  return false;
}

godot::Control *find_control_by_path(const std::string &path) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  godot::Control *base = editor->get_base_control();
  if (!base)
    return nullptr;
  return find_control_walk(base, path);
}

godot::Window *window_for_id(int window_id) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  godot::Control *base = editor->get_base_control();
  if (!base)
    return nullptr;
  godot::Window *root = base->get_window();
  if (!root)
    return nullptr;

  std::vector<godot::Node *> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    godot::Node *node = stack.back();
    stack.pop_back();
    if (auto *window = godot::Object::cast_to<godot::Window>(node)) {
      if (window->get_window_id() == window_id)
        return window;
    }
    const int child_count = node->get_child_count(true);
    for (int i = 0; i < child_count; ++i) {
      if (godot::Node *child = node->get_child(i, true))
        stack.push_back(child);
    }
  }
  return nullptr;
}

godot::Tree *find_scene_tree(std::string *error) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    if (error)
      *error = "EditorInterface not available";
    return nullptr;
  }
  godot::Node *scene_root = editor->get_edited_scene_root();
  if (!scene_root) {
    if (error)
      *error = "no scene currently open in the editor";
    return nullptr;
  }
  godot::Control *base = editor->get_base_control();
  if (!base) {
    if (error)
      *error = "editor base control not available";
    return nullptr;
  }
  if (godot::Tree *tree = find_scene_tree_walk(base, scene_root))
    return tree;
  if (error)
    *error = "editor scene tree control not found";
  return nullptr;
}

bool collect_scene_tree_rows(godot::Tree *tree, const std::string &filter,
                             bool selected_only, int max_items,
                             std::vector<SceneTreeRow> *out, bool *out_truncated,
                             int *out_window_id, std::string *error) {
  if (out)
    out->clear();
  if (out_truncated)
    *out_truncated = false;
  if (out_window_id)
    *out_window_id = 0;
  if (!tree || !out) {
    if (error)
      *error = "editor scene tree control not available";
    return false;
  }
  godot::TreeItem *root = tree->get_root();
  if (!root) {
    if (error)
      *error = "editor scene tree has no root item";
    return false;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root) {
    if (error)
      *error = "no scene currently open in the editor";
    return false;
  }

  int window_id = 0;
  const godot::Vector2 tree_origin = tree_client_origin(tree, &window_id);
  if (out_window_id)
    *out_window_id = window_id;

  bool truncated = false;
  const std::string root_name = util::to_std(scene_root->get_name());
  if (scene_tree_row_matches(root_name, root_name, filter, root->is_selected(0),
                             selected_only)) {
    if (max_items <= 0) {
      truncated = true;
    } else {
      SceneTreeRow row;
      fill_scene_tree_row(tree, root, scene_root, root_name, root_name, 0,
                          tree_origin, &row);
      out->push_back(std::move(row));
    }
  }
  if (!truncated) {
    SceneTreeWalk walk;
    walk.tree = tree;
    walk.scene_root = scene_root;
    walk.tree_origin = tree_origin;
    walk.filter = filter;
    walk.selected_only = selected_only;
    walk.max_items = max_items;
    walk.rows = out;
    walk.truncated = &truncated;
    walk_scene_tree_rows(root, root_name, 1, &walk);
  }
  if (out_truncated)
    *out_truncated = truncated;
  return true;
}

bool find_scene_tree_item(godot::Tree *tree, godot::Node *node,
                          godot::TreeItem **out_item) {
  if (out_item)
    *out_item = nullptr;
  if (!tree || !node)
    return false;
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root || !node_is_self_or_descendant(node, scene_root))
    return false;
  godot::TreeItem *root = tree->get_root();
  if (!root)
    return false;

  const godot::NodePath expected = scene_root->get_path_to(node);
  std::vector<godot::TreeItem *> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    godot::TreeItem *item = stack.back();
    stack.pop_back();
    const godot::Variant metadata = item->get_metadata(0);
    if (metadata.get_type() == godot::Variant::NODE_PATH &&
        static_cast<godot::NodePath>(metadata) == expected) {
      if (out_item)
        *out_item = item;
      return true;
    }
    for (godot::TreeItem *child = item->get_first_child(); child;
         child = child->get_next()) {
      stack.push_back(child);
    }
  }
  return false;
}

bool scene_tree_row_at_point(double window_x, double window_y, int window_id,
                             SceneTreeRow *out, std::string *error) {
  if (out)
    *out = SceneTreeRow();
  godot::Window *window = window_for_id(window_id);
  if (!window) {
    if (error)
      *error = "window not found: id " + std::to_string(window_id);
    return false;
  }
  const godot::Vector2 point(static_cast<float>(window_x),
                             static_cast<float>(window_y));
  godot::Control *hit = find_hit_control(window, point);
  if (!hit)
    return false;
  godot::Tree *hit_tree = ancestor_tree_of(hit);
  if (!hit_tree)
    return false;

  std::string tree_error;
  godot::Tree *scene_tree = find_scene_tree(&tree_error);
  if (!scene_tree) {
    if (error)
      *error = tree_error;
    return false;
  }
  if (hit_tree != scene_tree)
    return false;

  const godot::Vector2 local =
      scene_tree->get_global_transform().affine_inverse().xform(point);
  godot::TreeItem *item = scene_tree->get_item_at_position(local);
  if (!item)
    return false;
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  godot::Node *node = scene_tree_metadata_node(item, scene_root);
  if (!node)
    return false;
  const std::string path = node_scene_path(node, scene_root);
  if (path.empty())
    return false;

  int ignored_window_id = 0;
  const godot::Vector2 origin =
      tree_client_origin(scene_tree, &ignored_window_id);
  int depth = tree_item_depth(item);
  if (scene_tree->is_root_hidden() && depth > 0)
    --depth;
  SceneTreeRow row;
  fill_scene_tree_row(scene_tree, item, node, path,
                      util::to_std(node->get_name()), depth, origin, &row);
  if (out)
    *out = std::move(row);
  return true;
}

std::vector<SceneNodeHit> pick_scene_nodes_2d(double window_x, double window_y,
                                              int window_id, int max_results,
                                              std::string *error) {
  std::vector<SceneNodeHit> hits;
  if (max_results <= 0)
    return hits;
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    if (error)
      *error = "EditorInterface not available";
    return hits;
  }
  godot::Node *scene_root = editor->get_edited_scene_root();
  if (!scene_root)
    return hits;
  godot::SubViewport *viewport = editor->get_editor_viewport_2d();
  if (!viewport) {
    if (error)
      *error = "2D editor viewport not available";
    return hits;
  }
  godot::Window *viewport_window = viewport->get_window();
  if (!viewport_window || viewport_window->get_window_id() != window_id)
    return hits;
  if (auto *container =
          godot::Object::cast_to<godot::Control>(viewport->get_parent())) {
    if (!container->is_visible_in_tree())
      return hits;
  }

  const godot::Vector2 point(static_cast<float>(window_x),
                             static_cast<float>(window_y));
  const godot::Transform2D screen_transform = viewport->get_screen_transform();
  const godot::Vector2 viewport_point =
      screen_transform.affine_inverse().xform(point);
  if (!viewport->get_visible_rect().has_point(viewport_point))
    return hits;

  const godot::Transform2D canvas_to_client =
      screen_transform * viewport->get_global_canvas_transform();
  std::vector<godot::Node *> candidates;
  collect_scene_node_hits(scene_root, canvas_to_client, point, &candidates);
  for (auto it = candidates.rbegin();
       it != candidates.rend() && static_cast<int>(hits.size()) < max_results;
       ++it) {
    SceneNodeHit hit;
    hit.path = node_scene_path(*it, scene_root);
    if (hit.path.empty())
      continue;
    hit.name = util::to_std((*it)->get_name());
    hit.type = util::to_std((*it)->get_class());
    hits.push_back(std::move(hit));
  }
  return hits;
}

mcp::JsonValue scene_tree_row_to_json(const SceneTreeRow &row) {
  mcp::JsonValue object(mcp::JsonValue::object_tag);
  object["path"] = mcp::JsonValue(row.path);
  object["name"] = mcp::JsonValue(row.name);
  if (!row.type.empty())
    object["type"] = mcp::JsonValue(row.type);
  object["depth"] = mcp::JsonValue(row.depth);
  object["selected"] = mcp::JsonValue(row.selected);
  object["collapsed"] = mcp::JsonValue(row.collapsed);
  object["visible"] = mcp::JsonValue(row.visible);
  if (row.has_rect) {
    mcp::JsonValue rect(mcp::JsonValue::object_tag);
    mcp::JsonValue position(mcp::JsonValue::object_tag);
    position["x"] = mcp::JsonValue(row.x);
    position["y"] = mcp::JsonValue(row.y);
    mcp::JsonValue size(mcp::JsonValue::object_tag);
    size["x"] = mcp::JsonValue(row.w);
    size["y"] = mcp::JsonValue(row.h);
    rect["position"] = std::move(position);
    rect["size"] = std::move(size);
    object["rect"] = std::move(rect);
  } else {
    object["rect"] = mcp::JsonValue(nullptr);
  }
  return object;
}

std::vector<Element> hit_test(double window_x, double window_y, int window_id, int max_results) {
  std::vector<Element> hits;
  if (max_results <= 0)
    return hits;
  godot::Window *window = window_for_id(window_id);
  if (!window)
    return hits;
  const godot::Vector2 point(static_cast<float>(window_x), static_cast<float>(window_y));
  godot::Control *hit = find_hit_control(window, point);
  if (!hit)
    return hits;

  godot::Node *node = hit;
  while (node && node != window && static_cast<int>(hits.size()) < max_results) {
    if (auto *control = godot::Object::cast_to<godot::Control>(node))
      hits.push_back(make_element(control, static_cast<int>(hits.size()) + 1));
    node = node->get_parent();
  }
  return hits;
}

bool editor_viewport_window_mapping(const std::string &which, int index, WindowMapping *out,
                                    std::string *error) {
  if (!out) {
    if (error)
      *error = "internal error: null output";
    return false;
  }
  godot::SubViewport *viewport = resolve_editor_viewport(which, index, error);
  if (!viewport) {
    if (error && error->empty())
      *error = "editor viewport not available";
    return false;
  }
  const godot::Transform2D screen_transform = viewport->get_screen_transform();
  const godot::Vector2 scale = screen_transform.get_scale();
  const godot::Vector2 origin = screen_transform.get_origin();
  out->scale_x = scale.x;
  out->scale_y = scale.y;
  out->offset_x = origin.x;
  out->offset_y = origin.y;
  return true;
}

void draw_annotations(godot::Ref<godot::Image> image, const std::vector<MarkRect> &marks) {
  if (image.is_null() || marks.empty())
    return;
  const godot::Image::Format format = image->get_format();
  if (format != godot::Image::FORMAT_RGBA8 && format != godot::Image::FORMAT_RGB8)
    return;

  const int image_width = image->get_width();
  const int image_height = image->get_height();
  if (image_width <= 0 || image_height <= 0)
    return;

  const godot::Color box_color(1.0f, 0.0f, 0.0f, 1.0f);
  const godot::Color label_background(0.1f, 0.1f, 0.1f, 1.0f);
  const godot::Color digit_color(1.0f, 1.0f, 1.0f, 1.0f);
  const int scale = 2;

  for (const MarkRect &mark : marks) {
    const int rx = static_cast<int>(std::lround(mark.x));
    const int ry = static_cast<int>(std::lround(mark.y));
    const int rw = static_cast<int>(std::lround(mark.w));
    const int rh = static_cast<int>(std::lround(mark.h));
    if (rw <= 0 || rh <= 0)
      continue;

    fill_rect_clamped(image, image_width, image_height, rx, ry, rw, 1, box_color);
    fill_rect_clamped(image, image_width, image_height, rx, ry + rh - 1, rw, 1, box_color);
    fill_rect_clamped(image, image_width, image_height, rx, ry, 1, rh, box_color);
    fill_rect_clamped(image, image_width, image_height, rx + rw - 1, ry, 1, rh, box_color);

    const std::string label = std::to_string(mark.id);
    const int digits = static_cast<int>(label.size());
    if (digits == 0)
      continue;
    const int label_x = rx + 2;
    const int label_y = ry + 2;
    fill_rect_clamped(image, image_width, image_height, label_x, label_y, digits * 4 * scale + 2,
                      5 * scale + 2, label_background);
    for (int d = 0; d < digits; ++d) {
      const char character = label[static_cast<size_t>(d)];
      if (character < '0' || character > '9')
        continue;
      const int digit = character - '0';
      const int origin_x = label_x + 1 + d * 4 * scale;
      const int origin_y = label_y + 1;
      for (int row = 0; row < 5; ++row) {
        const uint8_t bits = kDigitGlyphs[digit][row];
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

mcp::JsonValue handle_get_editor_ui_elements(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_editor_ui_elements called");

  std::string error;
  std::string query;
  if (!read_string_param(args, "query", &query, &error))
    return util::error_json(error);
  std::string type_filter;
  if (!read_string_param(args, "type_filter", &type_filter, &error))
    return util::error_json(error);
  bool interactive_only = false;
  if (!read_bool_param(args, "interactive_only", false, &interactive_only, &error))
    return util::error_json(error);
  int max_elements = 100;
  if (!read_int_param(args, "max_elements", 100, &max_elements, &error))
    return util::error_json(error);
  max_elements = clampi(max_elements, 1, 1000);

  bool truncated = false;
  const std::vector<Element> elements =
      collect_elements(query, type_filter, interactive_only, max_elements, &truncated);

  mcp::JsonValue array(mcp::JsonValue::array_tag);
  for (const Element &element : elements)
    array.PushBack(element_to_json(element));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["elements"] = std::move(array);
  inner["count"] = mcp::JsonValue(static_cast<int>(elements.size()));
  inner["truncated"] = mcp::JsonValue(truncated);
  inner["space"] = mcp::JsonValue("window");
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_hit_test_editor_point(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "hit_test_editor_point called");

  const mcp::JsonValue *position = args.Find("position");
  if (!position || !position->IsObject())
    return util::error_json("missing required parameter: position");
  godot::Vector2 point;
  if (!extract_vec2(*position, &point))
    return util::error_json("position must have numeric x and y fields");

  std::string error;
  int window_id = 0;
  if (!read_int_param(args, "window_id", 0, &window_id, &error))
    return util::error_json(error);
  int max_results = 10;
  if (!read_int_param(args, "max_results", 10, &max_results, &error))
    return util::error_json(error);
  max_results = clampi(max_results, 1, 64);
  bool include_scene_nodes = false;
  if (!read_bool_param(args, "include_scene_nodes", false, &include_scene_nodes,
                       &error))
    return util::error_json(error);

  const std::vector<Element> hits =
      hit_test(point.x, point.y, window_id, max_results);

  mcp::JsonValue array(mcp::JsonValue::array_tag);
  for (const Element &element : hits)
    array.PushBack(element_to_json(element));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["hits"] = std::move(array);
  inner["count"] = mcp::JsonValue(static_cast<int>(hits.size()));
  inner["space"] = mcp::JsonValue("window");

  if (include_scene_nodes) {
    SceneTreeRow row;
    std::string tree_error;
    if (scene_tree_row_at_point(point.x, point.y, window_id, &row, &tree_error)) {
      inner["scene_tree_item"] = scene_tree_row_to_json(row);
    } else if (!tree_error.empty()) {
      inner["scene_tree_error"] = mcp::JsonValue(tree_error);
    }

    std::string pick_error;
    const std::vector<SceneNodeHit> scene_nodes =
        pick_scene_nodes_2d(point.x, point.y, window_id, 8, &pick_error);
    mcp::JsonValue scene_nodes_json(mcp::JsonValue::array_tag);
    for (const SceneNodeHit &node : scene_nodes) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["path"] = mcp::JsonValue(node.path);
      item["name"] = mcp::JsonValue(node.name);
      item["type"] = mcp::JsonValue(node.type);
      scene_nodes_json.PushBack(std::move(item));
    }
    inner["scene_nodes"] = std::move(scene_nodes_json);
    inner["scene_node_count"] = mcp::JsonValue(static_cast<int>(scene_nodes.size()));
    if (!pick_error.empty())
      inner["scene_nodes_error"] = mcp::JsonValue(pick_error);
  }
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_scene_tree_items(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "scene_tree_items called");

  std::string error;
  std::string filter;
  if (!read_string_param(args, "filter", &filter, &error))
    return util::error_json(error);
  bool selected_only = false;
  if (!read_bool_param(args, "selected_only", false, &selected_only, &error))
    return util::error_json(error);
  int max_items = 200;
  if (!read_int_param(args, "max_items", 200, &max_items, &error))
    return util::error_json(error);
  max_items = clamp_scene_tree_max_items(max_items);

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");
  if (!editor->get_edited_scene_root()) {
    return util::error_detail("no scene currently open in the editor",
                              "scene_tree_items", "an edited scene",
                              "open or create a scene first (create_editor_scene)");
  }

  std::string tree_error;
  godot::Tree *tree = find_scene_tree(&tree_error);
  if (!tree) {
    return util::error_detail(
        tree_error, "scene_tree_items",
        "an editor Tree control whose first row metadata resolves to a node of "
        "the edited scene",
        "use get_scene_tree for the scene model, or retry once the editor UI is "
        "ready");
  }

  std::vector<SceneTreeRow> rows;
  bool truncated = false;
  int window_id = 0;
  if (!collect_scene_tree_rows(tree, filter, selected_only, max_items, &rows,
                               &truncated, &window_id, &error)) {
    return util::error_detail(error, "scene_tree_items",
                              "a populated editor scene tree",
                              "use get_scene_tree for the scene model");
  }

  mcp::JsonValue array(mcp::JsonValue::array_tag);
  for (const SceneTreeRow &row : rows)
    array.PushBack(scene_tree_row_to_json(row));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["rows"] = std::move(array);
  inner["count"] = mcp::JsonValue(static_cast<int>(rows.size()));
  inner["truncated"] = mcp::JsonValue(truncated);
  inner["window_id"] = mcp::JsonValue(window_id);
  inner["space"] = mcp::JsonValue("window");
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_select_scene_tree_node(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "select_scene_tree_node called");

  std::string error;
  std::string path;
  if (!read_string_param(args, "path", &path, &error))
    return util::error_json(error);
  if (path.empty())
    return util::error_json("missing required parameter: path");
  bool add = false;
  if (!read_bool_param(args, "add", false, &add, &error))
    return util::error_json(error);
  bool inspect = true;
  if (!read_bool_param(args, "inspect", true, &inspect, &error))
    return util::error_json(error);
  bool focus = true;
  if (!read_bool_param(args, "focus", true, &focus, &error))
    return util::error_json(error);

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");
  godot::Node *scene_root = editor->get_edited_scene_root();
  if (!scene_root) {
    return util::error_detail("no scene currently open in the editor",
                              "select_scene_tree_node", "an edited scene",
                              "open or create a scene first (create_editor_scene)");
  }

  std::string hint;
  godot::Node *node = util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    return util::error_detail(
        "scene node not found: " + path, "select_scene_tree_node",
        "an existing node in the edited scene root \"" +
            util::to_std(scene_root->get_name()) + "\"",
        "call scene_tree_items or get_scene_tree to list valid node paths");
  }

  auto *selection = editor->get_selection();
  if (!selection)
    return util::error_json("EditorSelection not available");

  std::vector<godot::Node *> previous;
  if (add) {
    const godot::TypedArray<godot::Node> existing =
        selection->get_selected_nodes();
    previous.reserve(static_cast<size_t>(existing.size()));
    for (int i = 0; i < existing.size(); ++i) {
      if (godot::Node *selected = godot::Object::cast_to<godot::Node>(existing[i]))
        previous.push_back(selected);
    }
  }

  selection->clear();
  selection->add_node(node);

  bool inspected = false;
  if (inspect) {
    editor->edit_node(node);
    inspected = true;
  }

  if (add && inspected) {
    for (godot::Node *selected : previous) {
      if (selected->is_inside_tree())
        selection->add_node(selected);
    }
  }

  bool tree_found = false;
  bool focused = false;
  std::string tree_error;
  godot::Tree *tree = find_scene_tree(&tree_error);
  if (tree) {
    tree_found = true;
    godot::TreeItem *item = nullptr;
    if (find_scene_tree_item(tree, node, &item) && item && focus) {
      tree->scroll_to_item(item, true);
      focused = true;
    }
  }

  const int selected_count =
      static_cast<int>(selection->get_selected_nodes().size());

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  inner["path"] = mcp::JsonValue(node_scene_path(node, scene_root));
  inner["selected_count"] = mcp::JsonValue(selected_count);
  inner["inspected"] = mcp::JsonValue(inspected);
  inner["focused"] = mcp::JsonValue(focused);
  inner["tree_found"] = mcp::JsonValue(tree_found);
  if (!tree_found)
    inner["note"] = mcp::JsonValue(
        "editor scene tree control not found, so the row was not scrolled into "
        "view; selection and inspector still applied");
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_get_editor_viewport_geometry(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_editor_viewport_geometry called");

  std::string error;
  std::string which = "2d";
  if (!read_string_param(args, "viewport", &which, &error))
    return util::error_json(error);
  int index = 0;
  if (!read_int_param(args, "index", 0, &index, &error))
    return util::error_json(error);

  godot::SubViewport *viewport = resolve_editor_viewport(which, index, &error);
  if (!viewport)
    return util::error_json(error.empty() ? "editor viewport not available" : error);

  WindowMapping mapping;
  if (!editor_viewport_window_mapping(which, index, &mapping, &error))
    return util::error_json(error);

  const int image_width = viewport_image_dimension(viewport, true);
  const int image_height = viewport_image_dimension(viewport, false);
  godot::Window *window = viewport->get_window();

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["viewport"] = mcp::JsonValue(which);
  inner["window_id"] = mcp::JsonValue(window ? window->get_window_id() : 0);
  inner["space"] = mcp::JsonValue("window");
  inner["image_width"] = mcp::JsonValue(image_width);
  inner["image_height"] = mcp::JsonValue(image_height);
  mcp::JsonValue viewport_size(mcp::JsonValue::object_tag);
  viewport_size["x"] = mcp::JsonValue(image_width);
  viewport_size["y"] = mcp::JsonValue(image_height);
  inner["viewport_size"] = std::move(viewport_size);

  mcp::JsonValue mapping_json(mcp::JsonValue::object_tag);
  mapping_json["scale_x"] = mcp::JsonValue(mapping.scale_x);
  mapping_json["scale_y"] = mcp::JsonValue(mapping.scale_y);
  mapping_json["offset_x"] = mcp::JsonValue(mapping.offset_x);
  mapping_json["offset_y"] = mcp::JsonValue(mapping.offset_y);
  inner["mapping"] = std::move(mapping_json);

  if (which == "2d") {
    const godot::Transform2D canvas_transform = viewport->get_global_canvas_transform();
    const godot::Vector2 canvas_origin = canvas_transform.get_origin();
    const godot::Vector2 canvas_zoom = canvas_transform.get_scale();
    mcp::JsonValue canvas(mcp::JsonValue::object_tag);
    mcp::JsonValue origin(mcp::JsonValue::object_tag);
    origin["x"] = mcp::JsonValue(static_cast<double>(canvas_origin.x));
    origin["y"] = mcp::JsonValue(static_cast<double>(canvas_origin.y));
    canvas["origin"] = std::move(origin);
    mcp::JsonValue zoom(mcp::JsonValue::object_tag);
    zoom["x"] = mcp::JsonValue(static_cast<double>(canvas_zoom.x));
    zoom["y"] = mcp::JsonValue(static_cast<double>(canvas_zoom.y));
    canvas["zoom"] = std::move(zoom);
    inner["canvas"] = std::move(canvas);
  }

  return util::ok_result(std::move(inner));
}

} // namespace editor_ui_ops
} // namespace godot_autopilot
