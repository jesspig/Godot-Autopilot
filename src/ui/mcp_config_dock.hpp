#ifndef GODOT_AUTOPILOT_MCP_CONFIG_DOCK_HPP
#define GODOT_AUTOPILOT_MCP_CONFIG_DOCK_HPP

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_dock.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/spin_box.hpp>

#include "../core/server_context.hpp"
#include "../util/client_config_gen.hpp"
#include "../util/skill_gen.hpp"
#include "ui/mcp_log_dock.hpp"

namespace godot_autopilot {

class McpConfigDock : public godot::EditorDock {
  GDCLASS(McpConfigDock, godot::EditorDock)

  godot::SpinBox *port_spin;
  godot::Button *apply_button;
  godot::Label *status_label;
  godot::OptionButton *client_select;
  godot::Button *generate_button;
  godot::Button *generate_skills_button;
  godot::Label *result_label;

  godot::CheckBox *show_time_check = nullptr;
  McpLogDock *log_dock_ = nullptr;

  ServerContext *server_ctx = nullptr;

protected:
  static void _bind_methods();

public:
  McpConfigDock();

  void set_server_context(ServerContext *ctx);
  void set_log_dock(McpLogDock *dock);

private:
  void _on_apply_port();
  void _on_generate();
  void _on_generate_skills();
  bool has_existing_skills();
  void remove_legacy_skill_dirs();
  void _refresh_status();
  void _refresh_generate_skills_button();
  void _report(const godot::String &text, const godot::Color &color);
  void _on_show_time_toggled(bool checked);
};

} // namespace godot_autopilot

#endif