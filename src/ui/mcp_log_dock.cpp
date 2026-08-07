#include "mcp_log_dock.hpp"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/h_flow_container.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot_self_driving {

static const char *LEVEL_ICON_NAMES[4] = {"Debug", "Popup", "StatusWarning",
                                          "StatusError"};

static const char *LEVEL_TOOLTIPS[4] = {"Toggle visibility of debug messages.",
                                        "Toggle visibility of info messages.",
                                        "Toggle visibility of warnings.",
                                        "Toggle visibility of errors."};

McpLogDock::McpLogDock() : log_system(&LogSystem::instance()) {
  set_title("MCP Log");
  set_icon_name("Node");
  set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
  set_closable(true);
  set_global(false);
  set_custom_minimum_size(godot::Vector2(0, DEFAULT_DOCK_HEIGHT));

  auto *main_vbox = memnew(godot::VBoxContainer);
  main_vbox->set_anchors_and_offsets_preset(godot::Control::PRESET_FULL_RECT);
  add_child(main_vbox);

  log_display = memnew(godot::RichTextLabel);
  log_display->set_threaded(true);
  log_display->set_use_bbcode(true);
  log_display->set_scroll_follow(true);
  log_display->set_selection_enabled(true);
  log_display->set_context_menu_enabled(true);
  log_display->set_focus_mode(godot::Control::FOCUS_CLICK);
  log_display->set_v_size_flags(godot::Control::SIZE_EXPAND_FILL);
  log_display->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
  log_display->set_deselect_on_focus_loss_enabled(false);
  main_vbox->add_child(log_display);

  auto *bottom_hf = memnew(godot::HFlowContainer);
  bottom_hf->set_alignment(godot::FlowContainer::ALIGNMENT_END);
  main_vbox->add_child(bottom_hf);

  clear_button = memnew(godot::Button);
  clear_button->set_theme_type_variation("BottomPanelButton");
  clear_button->set_focus_mode(godot::Control::FOCUS_ACCESSIBILITY);
  clear_button->set_tooltip_text("Clear all messages");
  bottom_hf->add_child(clear_button);

  collapse_button = memnew(godot::Button);
  collapse_button->set_theme_type_variation("BottomPanelButton");
  collapse_button->set_focus_mode(godot::Control::FOCUS_ACCESSIBILITY);
  collapse_button->set_toggle_mode(true);
  collapse_button->set_tooltip_text(
      "Collapse consecutive identical messages into one entry. Shows number of "
      "occurrences.");
  bottom_hf->add_child(collapse_button);

  search_box = memnew(godot::LineEdit);
  search_box->set_custom_minimum_size(godot::Vector2(150, 0));
  search_box->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
  search_box->set_placeholder("Filter Messages");
  search_box->set_clear_button_enabled(true);
  bottom_hf->add_child(search_box);

  category_filter = memnew(godot::OptionButton);
  category_filter->add_item("All");
  category_filter->add_item("System");
  category_filter->add_item("Transport");
  category_filter->add_item("Tools");
  category_filter->add_item("Resources");
  category_filter->add_item("Prompts");
  category_filter->select(0);
  bottom_hf->add_child(category_filter);

  auto *filter_box = memnew(godot::HBoxContainer);
  bottom_hf->add_child(filter_box);
  for (int i = 0; i < 4; i++) {
    filter_buttons[i] = memnew(godot::Button);
    filter_buttons[i]->set_toggle_mode(true);
    filter_buttons[i]->set_pressed(true);
    filter_buttons[i]->set_text("0");
    filter_buttons[i]->set_tooltip_text(LEVEL_TOOLTIPS[i]);
    filter_buttons[i]->set_focus_mode(godot::Control::FOCUS_ACCESSIBILITY);
    filter_buttons[i]->set_theme_type_variation("EditorLogFilterButton");
    filter_box->add_child(filter_buttons[i]);
  }

  search_box->connect("text_changed",
                      callable_mp(this, &McpLogDock::_on_search_changed));
  for (int i = 0; i < 4; i++) {
    filter_buttons[i]->connect(
        "toggled", callable_mp(this, &McpLogDock::_on_filter_toggled).bind(i));
  }
  category_filter->connect(
      "item_selected", callable_mp(this, &McpLogDock::_on_category_changed));
  clear_button->connect("pressed", callable_mp(this, &McpLogDock::_on_clear));
  collapse_button->connect(
      "toggled", callable_mp(this, &McpLogDock::_on_collapse_toggled));

  _update_theme();
  _rebuild_log();
}

McpLogDock::~McpLogDock() {}

void McpLogDock::_bind_methods() {}

void McpLogDock::_notification(int p_what) {
  switch (p_what) {
  case NOTIFICATION_ENTER_TREE:
  case NOTIFICATION_THEME_CHANGED:
    _update_theme();
    break;
  }
}

void McpLogDock::_update_theme() {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return;
  auto theme = editor->get_editor_theme();
  if (theme.is_null())
    return;

  const godot::StringName editor_icons("EditorIcons");
  const godot::StringName editor_fonts("EditorFonts");
  const godot::StringName editor_type("Editor");

  theme_cache.error_color = theme->has_color("error_color", editor_type)
                                ? theme->get_color("error_color", editor_type)
                                : godot::Color(1, 0.3, 0.3);
  theme_cache.warning_color =
      theme->has_color("warning_color", editor_type)
          ? theme->get_color("warning_color", editor_type)
          : godot::Color(1, 0.85, 0.4);
  theme_cache.info_color = theme->has_color("font_color", editor_type)
                               ? theme->get_color("font_color", editor_type)
                               : godot::Color(0.9, 0.9, 0.9);
  theme_cache.debug_color = theme_cache.info_color * godot::Color(1, 1, 1, 0.6);

  if (theme->has_font("output_source", editor_fonts))
    log_display->add_theme_font_override(
        "normal_font", theme->get_font("output_source", editor_fonts));
  if (theme->has_font("output_source_bold", editor_fonts))
    log_display->add_theme_font_override(
        "bold_font", theme->get_font("output_source_bold", editor_fonts));
  if (theme->has_font("output_source_italic", editor_fonts))
    log_display->add_theme_font_override(
        "italics_font", theme->get_font("output_source_italic", editor_fonts));
  if (theme->has_font("output_source_mono", editor_fonts))
    log_display->add_theme_font_override(
        "mono_font", theme->get_font("output_source_mono", editor_fonts));
  if (theme->has_font_size("output_source_size", editor_fonts)) {
    int fs = theme->get_font_size("output_source_size", editor_fonts);
    log_display->add_theme_font_size_override("normal_font_size", fs);
    log_display->add_theme_font_size_override("bold_font_size", fs);
    log_display->add_theme_font_size_override("italics_font_size", fs);
    log_display->add_theme_font_size_override("mono_font_size", fs);
  }
  log_display->add_theme_constant_override("text_highlight_h_padding", 0);
  log_display->add_theme_constant_override("text_highlight_v_padding", 0);

  if (theme->has_icon("Clear", editor_icons))
    clear_button->set_button_icon(theme->get_icon("Clear", editor_icons));
  if (theme->has_icon("CombineLines", editor_icons))
    collapse_button->set_button_icon(
        theme->get_icon("CombineLines", editor_icons));
  if (theme->has_icon("Search", editor_icons))
    search_box->set_right_icon(theme->get_icon("Search", editor_icons));

  for (int i = 0; i < 4; i++) {
    if (theme->has_icon(LEVEL_ICON_NAMES[i], editor_icons))
      filter_buttons[i]->set_button_icon(
          theme->get_icon(LEVEL_ICON_NAMES[i], editor_icons));
  }
}

void McpLogDock::refresh() { _rebuild_log(); }

void McpLogDock::poll_new_entries() {
  auto entries = log_system->query({});
  if (entries.size() <= last_shown_count_)
    return;

  if (!collapse) {
    for (size_t i = last_shown_count_; i < entries.size(); i++) {
      _add_log_line(*entries[i]);
    }
    last_shown_count_ = entries.size();
    _update_filter_counts();
    return;
  }

  last_shown_count_ = entries.size();
  _rebuild_log();
}

void McpLogDock::_update_filter_counts() {
  auto entries = log_system->query({});
  int counts[4] = {0, 0, 0, 0};
  for (auto *e : entries) {
    counts[static_cast<int>(e->level)]++;
  }
  for (int i = 0; i < 4; i++) {
    filter_buttons[i]->set_text(godot::String::num_int64(counts[i]));
  }
}

bool McpLogDock::_add_log_line(const LogEntry &entry, int count) {
  if (!_check_display(entry))
    return false;

  _apply_entry_style(entry.level);
  if (collapse && count > 1) {
    log_display->add_text("(" + godot::String::num_int64(count) + ")" +
                          entry.message.c_str());
  } else {
    log_display->add_text(entry.message.c_str());
  }
  log_display->newline();
  log_display->pop();

  if (log_display->get_paragraph_count() > LINE_LIMIT) {
    log_display->remove_paragraph(0);
  }
  return true;
}

void McpLogDock::_apply_entry_style(LogLevel level) {
  switch (level) {
  case LogLevel::Error:
    log_display->push_color(theme_cache.error_color);
    break;
  case LogLevel::Warning:
    log_display->push_color(theme_cache.warning_color);
    break;
  case LogLevel::Info:
    log_display->push_color(theme_cache.info_color);
    break;
  default:
    log_display->push_color(theme_cache.debug_color);
    break;
  }
}

bool McpLogDock::_check_display(const LogEntry &entry) const {
  int level_idx = static_cast<int>(entry.level);
  if (!filter_buttons[level_idx]->is_pressed())
    return false;

  if (category_filter->get_selected() > 0) {
    LogCategory selected_cat =
        static_cast<LogCategory>(category_filter->get_selected() - 1);
    if (entry.category != selected_cat)
      return false;
  }

  if (!search_box->get_text().is_empty()) {
    godot::String filter = search_box->get_text();
    godot::String msg = entry.message.c_str();
    if (msg.findn(filter) == -1)
      return false;
  }

  return true;
}

void McpLogDock::_rebuild_log() {
  log_display->clear();
  last_shown_count_ = 0;

  auto entries = log_system->query({});
  last_shown_count_ = entries.size();

  if (!collapse) {
    for (const auto *e : entries) {
      _add_log_line(*e);
    }
    _update_filter_counts();
    return;
  }

  std::unordered_map<std::string, LogLevel> msg_level;
  std::unordered_map<std::string, LogCategory> msg_cat;
  for (const auto *e : entries) {
    if (!msg_level.count(e->message)) {
      msg_level[e->message] = e->level;
      msg_cat[e->message] = e->category;
    }
  }

  {
    std::unordered_map<std::string, int> freq;
    for (const auto *e : entries) {
      if (!_check_display(*e))
        continue;
      freq[e->message]++;
    }

    std::unordered_map<std::string, int> seg;
    for (const auto *e : entries) {
      if (!_check_display(*e))
        continue;

      if (freq[e->message] == 1) {
        for (auto &[msg, cnt] : seg) {
          if (cnt == 0)
            continue;
          auto le = LogEntry{{}, msg_level[msg], msg_cat[msg], msg};
          _add_log_line(le, cnt);
          cnt = 0;
        }
        auto le = LogEntry{
            {}, msg_level[e->message], msg_cat[e->message], e->message};
        _add_log_line(le, 1);
        continue;
      }

      seg[e->message]++;
    }

    for (auto &[msg, cnt] : seg) {
      if (cnt == 0)
        continue;
      auto le = LogEntry{{}, msg_level[msg], msg_cat[msg], msg};
      _add_log_line(le, cnt);
    }
  }

  _update_filter_counts();
}

void McpLogDock::_on_search_changed(const godot::String &text) {
  _rebuild_log();
}

void McpLogDock::_on_filter_toggled(bool active, int level_idx) {
  _rebuild_log();
}

void McpLogDock::_on_category_changed(int index) { _rebuild_log(); }

void McpLogDock::_on_clear() {
  log_display->clear();
  last_shown_count_ = log_system->query({}).size();
}

void McpLogDock::_on_collapse_toggled(bool enabled) {
  collapse = enabled;
  _rebuild_log();
}

} // namespace godot_self_driving
