#ifndef GODOT_AUTOPILOT_MCP_CONFIG_DOCK_HPP
#define GODOT_AUTOPILOT_MCP_CONFIG_DOCK_HPP

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_dock.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/spin_box.hpp>

#include "../core/server_context.hpp"
#include "../util/client_config_gen.hpp"

namespace godot_autopilot {

class McpConfigDock : public godot::EditorDock {
  GDCLASS(McpConfigDock, godot::EditorDock)

  godot::SpinBox *port_spin;
  godot::Button *apply_button;
  godot::Label *status_label;
  godot::OptionButton *client_select;
  godot::Button *generate_button;
  godot::Label *result_label;

  ServerContext *server_ctx = nullptr;

protected:
  static void _bind_methods();

public:
  McpConfigDock();

  void set_server_context(ServerContext *ctx);

private:
  void _on_apply_port();
  void _on_generate();
  void _refresh_status();
  void _report(const godot::String &text, const godot::Color &color);
};

} // namespace godot_autopilot

#endif