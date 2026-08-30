#include "mcp_config_dock.hpp"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "core/plugin_config.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kDockTitle = "MCP Config";

godot::Color theme_color(const char *name, const godot::Color &fallback) {
  godot::Ref<godot::Theme> theme =
      godot::EditorInterface::get_singleton()->get_editor_theme();
  if (theme.is_valid() && theme->has_color(name, "Editor")) {
    return theme->get_color(name, "Editor");
  }
  return fallback;
}

bool write_file(const godot::String &path, const std::string &content) {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(path, godot::FileAccess::WRITE);
  if (file.is_null()) {
    return false;
  }
  file->store_string(godot::String(content.c_str()));
  file->close();
  return true;
}

godot::String read_file(const godot::String &path) {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(path, godot::FileAccess::READ);
  if (file.is_null()) {
    return godot::String();
  }
  godot::String content = file->get_as_text();
  file->close();
  return content;
}

} // namespace

McpConfigDock::McpConfigDock() : port_spin(nullptr), apply_button(nullptr) {
  set_title(kDockTitle);
  set_default_slot(EditorDock::DOCK_SLOT_RIGHT_UR);
  set_closable(true);

  auto *root = memnew(godot::VBoxContainer);
  add_child(root);

  auto *server_heading = memnew(godot::Label);
  server_heading->set_text("MCP Server");
  root->add_child(server_heading);

  auto *port_row = memnew(godot::HBoxContainer);
  root->add_child(port_row);

  auto *port_label = memnew(godot::Label);
  port_label->set_text("Port:");
  port_row->add_child(port_label);

  port_spin = memnew(godot::SpinBox);
  port_spin->set_min(1);
  port_spin->set_max(65535);
  port_spin->set_value(9527);
  port_spin->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
  port_row->add_child(port_spin);

  apply_button = memnew(godot::Button);
  apply_button->set_text("Apply");
  apply_button->connect("pressed",
                        callable_mp(this, &McpConfigDock::_on_apply_port));
  port_row->add_child(apply_button);

  auto *time_row = memnew(godot::HBoxContainer);
  root->add_child(time_row);
  auto *time_label = memnew(godot::Label);
  time_label->set_text("Show timestamps:");
  time_row->add_child(time_label);
  show_time_check = memnew(godot::CheckBox);
  show_time_check->set_pressed(PluginConfig::load_show_time());
  show_time_check->set_tooltip_text("Prefix each log line with HH:MM:SS; collapsed (merged) lines always show latest time.");
  time_row->add_child(show_time_check);
  show_time_check->connect("toggled", callable_mp(this, &McpConfigDock::_on_show_time_toggled));

  status_label = memnew(godot::Label);
  root->add_child(status_label);

  auto *separator = memnew(godot::HSeparator);
  root->add_child(separator);

  auto *configs_heading = memnew(godot::Label);
  configs_heading->set_text("Client Configs");
  root->add_child(configs_heading);

  auto *target_label = memnew(godot::Label);
  target_label->set_text("Generated in project root (res://)");
  target_label->set_modulate(theme_color("font_disabled_color",
                                         godot::Color(0.6f, 0.6f, 0.6f)));
  root->add_child(target_label);

  client_select = memnew(godot::OptionButton);
  for (int i = 0; i < static_cast<int>(client_config_gen::ClientId::COUNT);
       ++i) {
    auto id = static_cast<client_config_gen::ClientId>(i);
    client_select->add_item(
        godot::String(client_config_gen::display_name(id)) + "  (" +
            godot::String(client_config_gen::file_path(id)) + ")",
        i);
  }
  client_select->select(0);
  root->add_child(client_select);

  generate_button = memnew(godot::Button);
  generate_button->set_text("Generate");
  generate_button->connect("pressed",
                           callable_mp(this, &McpConfigDock::_on_generate));
  root->add_child(generate_button);

  result_label = memnew(godot::Label);
  result_label->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
  root->add_child(result_label);

  auto *note_label = memnew(godot::Label);
  note_label->set_text(
      "Notes: Claude Code asks for approval of .mcp.json on first use; "
      "Trae / Qoder / WorkBuddy need their project-level MCP switch enabled; "
      "Codex loads project config only for trusted projects.");
  note_label->set_autowrap_mode(godot::TextServer::AUTOWRAP_WORD_SMART);
  note_label->set_modulate(theme_color("font_disabled_color",
                                       godot::Color(0.6f, 0.6f, 0.6f)));
  root->add_child(note_label);

  _refresh_status();
}

void McpConfigDock::_bind_methods() {}

void McpConfigDock::set_log_dock(McpLogDock *dock) { log_dock_ = dock; }

void McpConfigDock::_on_show_time_toggled(bool checked) {
  PluginConfig::save_show_time(checked);
  if (log_dock_) log_dock_->set_show_time(checked);
}

void McpConfigDock::set_server_context(ServerContext *ctx) {
  server_ctx = ctx;
  if (port_spin != nullptr) {
    port_spin->set_value(ctx != nullptr ? ctx->get_port() : 9527);
  }
  apply_button->set_disabled(ctx == nullptr);
  _refresh_status();
}

void McpConfigDock::_refresh_status() {
  if (status_label == nullptr) {
    return;
  }
  if (server_ctx != nullptr && server_ctx->is_running()) {
    status_label->set_text("Running on port " +
                           godot::String::num_int64(server_ctx->get_port()));
    status_label->set_modulate(
        theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
  } else {
    status_label->set_text("Server offline");
    status_label->set_modulate(
        theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
  }
}

void McpConfigDock::_report(const godot::String &text,
                            const godot::Color &color) {
  result_label->set_text(text);
  result_label->set_modulate(color);
}

void McpConfigDock::_on_apply_port() {
  if (server_ctx == nullptr) {
    _report("Server not available", theme_color("error_color",
                                                godot::Color(0.95f, 0.4f,
                                                             0.4f)));
    return;
  }
  int port = static_cast<int>(port_spin->get_value());
  if (server_ctx->restart(static_cast<uint16_t>(port))) {
    PluginConfig::save_port(port);
    _refresh_status();
    _report("Port applied: " + godot::String::num_int64(port),
            theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
  } else {
    _refresh_status();
    _report("Restart failed: " + godot::String(server_ctx->last_error().c_str()),
            theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
  }
}

void McpConfigDock::_on_generate() {
  using namespace client_config_gen;

  int port = static_cast<int>(port_spin->get_value());
  auto id = static_cast<ClientId>(client_select->get_selected_id());
  godot::String root =
      godot::ProjectSettings::get_singleton()->globalize_path("res://");
  godot::String abs_path = root.path_join(godot::String(file_path(id)));
  const char *rel_path = file_path(id);

  if (!godot::FileAccess::file_exists(abs_path)) {
    if (write_file(abs_path, render_config(id, port))) {
      _report("Created " + godot::String(rel_path) + " on port " +
                  godot::String::num_int64(port),
              theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
    } else {
      _report("Write failed: " + godot::String(rel_path),
              theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
    }
    return;
  }

  godot::String existing = read_file(abs_path);
  std::string existing_std = existing.utf8().get_data();
  if (id == ClientId::Codex) {
    TomlMergeResult result = merge_toml_config(port, existing_std);
    if (result.status == TomlMergeResult::Status::AlreadyConfigured) {
      _report("Skipped " + godot::String(rel_path) +
                  ": mcp_servers already present (existing config kept)",
              theme_color("warning_color", godot::Color(0.95f, 0.8f, 0.4f)));
    } else if (write_file(abs_path, result.content)) {
      _report("Updated " + godot::String(rel_path) + " on port " +
                  godot::String::num_int64(port) +
                  " (existing config kept)",
              theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
    } else {
      _report("Write failed: " + godot::String(rel_path),
              theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
    }
    return;
  }

  MergeResult result = merge_json_config(id, port, existing_std);
  if (result.status == MergeResult::Status::Unparsable) {
    _report("Skipped " + godot::String(rel_path) +
                ": existing file is not valid JSON (not overwritten)",
            theme_color("warning_color", godot::Color(0.95f, 0.8f, 0.4f)));
  } else if (write_file(abs_path, result.content)) {
    _report("Updated " + godot::String(rel_path) + " on port " +
                godot::String::num_int64(port) + " (existing config kept)",
            theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
  } else {
    _report("Write failed: " + godot::String(rel_path),
            theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
  }
}

} // namespace godot_autopilot