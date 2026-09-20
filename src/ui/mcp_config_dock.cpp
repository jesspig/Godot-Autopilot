#include "mcp_config_dock.hpp"

#include <string>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_separator.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "core/monitor.hpp"
#include "core/plugin_config.hpp"
#include "core/sanitize_policy.hpp"
#include "tools/authorization.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kDockTitle = "MCP Config";
constexpr const char *kSkillsDir = "res://.agents/skills";
constexpr const char *kSkillDirPrefix = "godot-autopilot-";

godot::Color theme_color(const char *name, const godot::Color &fallback) {
  godot::Ref<godot::Theme> theme =
      godot::EditorInterface::get_singleton()->get_editor_theme();
  if (theme.is_valid() && theme->has_color(name, "Editor")) {
    return theme->get_color(name, "Editor");
  }
  return fallback;
}

bool write_file(const godot::String &path, const std::string &content) {
  godot::DirAccess::make_dir_recursive_absolute(path.get_base_dir());
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

void remove_dir_recursive(const godot::String &path) {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(path);
  if (dir.is_null()) {
    return;
  }
  dir->list_dir_begin();
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != "..") {
      const godot::String child = path.path_join(entry);
      if (dir->current_is_dir()) {
        remove_dir_recursive(child);
      } else {
        dir->remove(child);
      }
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();
  godot::DirAccess::remove_absolute(path);
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

  auto *allow_row = memnew(godot::HBoxContainer);
  root->add_child(allow_row);
  auto *allow_label = memnew(godot::Label);
  allow_label->set_text("Allow code_execute:");
  allow_row->add_child(allow_label);
  allow_code_execute_check = memnew(godot::CheckBox);
  allow_code_execute_check->set_pressed(authorization::allow_list_contains(
      PluginConfig::load_allow(), "code_execute"));
  allow_code_execute_check->set_tooltip_text(
      "Allow code_execute (and the execute_script tool): read on the next "
      "tool call, no restart needed. When the GODOT_AUTOPILOT_ALLOW "
      "environment variable is set it wins over this setting.");
  allow_row->add_child(allow_code_execute_check);
  allow_code_execute_check->connect(
      "toggled",
      callable_mp(this, &McpConfigDock::_on_allow_code_execute_toggled));

  auto *game_runtime_row = memnew(godot::HBoxContainer);
  root->add_child(game_runtime_row);
  auto *game_runtime_label = memnew(godot::Label);
  game_runtime_label->set_text("Allow game_runtime:");
  game_runtime_row->add_child(game_runtime_label);
  allow_game_runtime_check = memnew(godot::CheckBox);
  allow_game_runtime_check->set_pressed(authorization::allow_list_contains(
      PluginConfig::load_allow(), "game_runtime"));
  allow_game_runtime_check->set_tooltip_text(
      "Allow game_runtime (and the game runtime tools): read on the next "
      "tool call, no restart needed. When the GODOT_AUTOPILOT_ALLOW "
      "environment variable is set it wins over this setting.");
  game_runtime_row->add_child(allow_game_runtime_check);
  allow_game_runtime_check->connect(
      "toggled",
      callable_mp(this, &McpConfigDock::_on_allow_game_runtime_toggled));

  auto *user_tools_row = memnew(godot::HBoxContainer);
  root->add_child(user_tools_row);
  auto *user_tools_label = memnew(godot::Label);
  user_tools_label->set_text("Allow user tools:");
  user_tools_row->add_child(user_tools_label);
  allow_user_tools_check = memnew(godot::CheckBox);
  allow_user_tools_check->set_pressed(
      authorization::capability_enabled("user_tools"));
  allow_user_tools_check->set_tooltip_text(
      "Allow user tools registered from scripts through the AutopilotTools "
      "engine singleton; read on the next tool call, no restart needed. When "
      "the GODOT_AUTOPILOT_ALLOW environment variable is set it wins over "
      "this setting.");
  user_tools_row->add_child(allow_user_tools_check);
  allow_user_tools_check->connect(
      "toggled", callable_mp(this, &McpConfigDock::_on_allow_user_tools_toggled));

  auto *desensitize_row = memnew(godot::HBoxContainer);
  root->add_child(desensitize_row);
  auto *desensitize_label = memnew(godot::Label);
  desensitize_label->set_text("Desensitize data:");
  desensitize_row->add_child(desensitize_label);
  desensitize_check = memnew(godot::CheckBox);
  const bool desensitize = PluginConfig::load_desensitize();
  desensitize_check->set_pressed(desensitize);
  sanitize_policy::set_enabled(desensitize);
  desensitize_check->set_tooltip_text(
      "Strip sensitive key values from recorded data; read on the next "
      "tool call, no restart needed.");
  desensitize_row->add_child(desensitize_check);
  desensitize_check->connect(
      "toggled", callable_mp(this, &McpConfigDock::_on_desensitize_toggled));

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

  generate_skills_button = memnew(godot::Button);
  generate_skills_button->connect(
      "pressed", callable_mp(this, &McpConfigDock::_on_generate_skills));
  root->add_child(generate_skills_button);

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

void McpConfigDock::_on_allow_code_execute_toggled(bool checked) {
  monitor::ui_action(
      "config_allow_toggle",
      monitor::build_attrs(
          {{"capability", "code_execute"}, {"enabled", checked ? "true" : "false"}}));
  monitor::security(
      "allow_change",
      monitor::build_attrs(
          {{"capability", "code_execute"}, {"enabled", checked ? "true" : "false"}}));
  _on_allow_toggled("code_execute", allow_code_execute_check, checked);
}

void McpConfigDock::_on_allow_game_runtime_toggled(bool checked) {
  monitor::ui_action(
      "config_allow_toggle",
      monitor::build_attrs(
          {{"capability", "game_runtime"}, {"enabled", checked ? "true" : "false"}}));
  monitor::security(
      "allow_change",
      monitor::build_attrs(
          {{"capability", "game_runtime"}, {"enabled", checked ? "true" : "false"}}));
  _on_allow_toggled("game_runtime", allow_game_runtime_check, checked);
}

void McpConfigDock::_on_allow_user_tools_toggled(bool checked) {
  monitor::ui_action(
      "config_allow_toggle",
      monitor::build_attrs(
          {{"capability", "user_tools"}, {"enabled", checked ? "true" : "false"}}));
  monitor::security(
      "allow_change",
      monitor::build_attrs(
          {{"capability", "user_tools"}, {"enabled", checked ? "true" : "false"}}));
  _on_allow_toggled("user_tools", allow_user_tools_check, checked);
}

void McpConfigDock::_on_desensitize_toggled(bool checked) {
  PluginConfig::save_desensitize(checked);
  sanitize_policy::set_enabled(checked);
  monitor::ui_action(
      "config_desensitize_toggle",
      monitor::build_attrs({{"enabled", checked ? "true" : "false"}}));
  monitor::security("desensitize",
                    monitor::build_attrs({{"enabled", checked ? "true" : "false"}}));
}

void McpConfigDock::_on_allow_toggled(const char *capability,
                                      godot::CheckBox *box, bool checked) {
  const std::string allow = PluginConfig::load_allow();
  PluginConfig::save_allow(
      checked ? authorization::allow_list_add(allow, capability)
              : authorization::allow_list_remove(allow, capability));
  box->set_pressed_no_signal(authorization::allow_list_contains(
      PluginConfig::load_allow(), capability));
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
  _refresh_generate_skills_button();
}

void McpConfigDock::_refresh_generate_skills_button() {
  generate_skills_button->set_text(has_existing_skills() ? "Update Skills"
                                                         : "Generate Skills");
  generate_skills_button->set_tooltip_text(
      "Write the 8 godot-autopilot skills to .agents/skills/ "
      "(updates existing entries)");
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
  monitor::ui_action("config_apply_port",
                     monitor::build_attrs({{"port", std::to_string(port)}}));
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
  monitor::ui_action(
      "config_generate",
      monitor::build_attrs({{"client", std::to_string(static_cast<int>(id))},
                            {"port", std::to_string(port)}}));
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
  if (uses_toml(id)) {
    TomlMergeResult result = merge_toml_config(id, port, existing_std);
    if (result.status == TomlMergeResult::Status::AlreadyConfigured) {
      _report("Skipped " + godot::String(rel_path) +
                  ": MCP servers already present (existing config kept)",
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

bool McpConfigDock::has_existing_skills() {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(kSkillsDir);
  if (dir.is_null()) {
    return false;
  }
  dir->list_dir_begin();
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && dir->current_is_dir() &&
        entry.begins_with(kSkillDirPrefix)) {
      dir->list_dir_end();
      return true;
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();
  return false;
}

void McpConfigDock::remove_legacy_skill_dirs() {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(kSkillsDir);
  if (dir.is_null()) {
    return;
  }
  dir->list_dir_begin();
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && dir->current_is_dir() &&
        entry.begins_with(kSkillDirPrefix)) {
      remove_dir_recursive(godot::String(kSkillsDir).path_join(entry));
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();
}

void McpConfigDock::_on_generate_skills() {
  const bool updating = has_existing_skills();
  monitor::ui_action(
      "config_generate_skills",
      monitor::build_attrs({{"updating", updating ? "true" : "false"}}));
  if (updating) {
    remove_legacy_skill_dirs();
  }

  godot::String root =
      godot::ProjectSettings::get_singleton()->globalize_path("res://");

  int generated_skills = 0;
  int generated_references = 0;
  std::string failures;

  for (const skill_gen::SkillSpec &spec : skill_gen::all_skills()) {
    bool all_ok = true;
    for (const skill_gen::SkillFile &file : spec.files) {
      const std::string rel_path =
          skill_gen::skill_file_path(spec.name, file.relative_path);
      const auto slash = rel_path.rfind('/');
      if (slash != std::string::npos) {
        godot::DirAccess::make_dir_recursive_absolute(
            root.path_join(godot::String(rel_path.substr(0, slash).c_str())));
      }
      const std::string content = file.relative_path == "SKILL.md"
                                      ? skill_gen::render_skill_md(spec)
                                      : file.body;
      if (write_file(root.path_join(godot::String(rel_path.c_str())),
                     content)) {
        if (file.relative_path != "SKILL.md") {
          ++generated_references;
        }
      } else {
        all_ok = false;
        failures += rel_path + "\n";
      }
    }
    if (all_ok) {
      ++generated_skills;
    }
  }

  if (failures.empty()) {
    if (updating) {
      _report("Updated " + godot::String::num_int64(generated_skills) +
                  " skills in .agents/skills/ (" +
                  godot::String::num_int64(generated_references) +
                  " reference files)",
              theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
    } else {
      _report("Generated " + godot::String::num_int64(generated_skills) +
                  " skills in .agents/skills/ (" +
                  godot::String::num_int64(generated_references) +
                  " reference files)",
              theme_color("success_color", godot::Color(0.4f, 0.9f, 0.4f)));
    }
  } else {
    _report("Skill generation failed for:\n" + godot::String(failures.c_str()),
            theme_color("error_color", godot::Color(0.95f, 0.4f, 0.4f)));
  }

  _refresh_generate_skills_button();
}

} // namespace godot_autopilot