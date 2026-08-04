#ifndef GODOT_SELF_DRIVING_SCENE_DIRTY_TRACKER_HPP
#define GODOT_SELF_DRIVING_SCENE_DIRTY_TRACKER_HPP

namespace godot_self_driving {
namespace scene_dirty_tracker {

void mark_scene_modified();
void clear_scene_modified();
bool is_current_scene_dirty();

} // namespace scene_dirty_tracker
} // namespace godot_self_driving

#endif
