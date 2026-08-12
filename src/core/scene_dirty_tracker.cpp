#include "scene_dirty_tracker.hpp"

#include <mutex>

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>

namespace godot_autopilot {
namespace scene_dirty_tracker {

namespace {

std::mutex g_mutex;
bool g_dirty = false;
int64_t g_scene_root_instance_id = 0;

} // namespace

void mark_scene_modified() {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto *editor = godot::EditorInterface::get_singleton();
  auto *root = editor ? editor->get_edited_scene_root() : nullptr;
  g_dirty = root != nullptr;
  g_scene_root_instance_id =
      root ? static_cast<int64_t>(root->get_instance_id()) : 0;
}

void clear_scene_modified() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_dirty = false;
  g_scene_root_instance_id = 0;
}

bool is_current_scene_dirty() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_dirty)
    return false;
  auto *editor = godot::EditorInterface::get_singleton();
  auto *root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!root)
    return false;
  return static_cast<int64_t>(root->get_instance_id()) ==
         g_scene_root_instance_id;
}

} // namespace scene_dirty_tracker
} // namespace godot_autopilot
