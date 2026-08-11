#ifndef GODOT_AUTOPILOT_MCP_STATUS_BAR_HPP
#define GODOT_AUTOPILOT_MCP_STATUS_BAR_HPP

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/texture_rect.hpp>

namespace godot_autopilot {

class McpStatusBar : public godot::HBoxContainer {
  GDCLASS(McpStatusBar, godot::HBoxContainer)

  godot::Label *status_label;
  godot::TextureRect *icon;

protected:
  static void _bind_methods();

public:
  McpStatusBar();
  ~McpStatusBar() = default;

  void set_status_text(const godot::String &text);
  void set_status_ok(bool ok);
};

} // namespace godot_autopilot

#endif
