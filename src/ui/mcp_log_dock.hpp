#ifndef GODOT_SELF_DRIVING_MCP_LOG_DOCK_HPP
#define GODOT_SELF_DRIVING_MCP_LOG_DOCK_HPP

#include <godot_cpp/classes/editor_dock.hpp>
#include <godot_cpp/classes/rich_text_label.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/classes/button.hpp>

#include "../core/log_system.hpp"

namespace godot_self_driving {

class McpLogDock : public godot::EditorDock {
    GDCLASS(McpLogDock, godot::EditorDock)

    godot::RichTextLabel* log_display;
    godot::LineEdit* search_box;
    godot::Button* filter_buttons[4];
    godot::OptionButton* category_filter;
    godot::Button* clear_button;
    godot::Button* collapse_button;

    struct ThemeCache {
        godot::Color error_color;
        godot::Color warning_color;
        godot::Color info_color;
        godot::Color debug_color;
    };
    ThemeCache theme_cache;

    LogSystem* log_system;
    bool collapse = false;
    static constexpr int LINE_LIMIT = 5000;
    size_t last_shown_count_ = 0;

protected:
    static void _bind_methods();

public:
    McpLogDock();
    ~McpLogDock();

    void refresh();
    void poll_new_entries();

    void _notification(int p_what);

private:
    void _update_theme();
    bool _add_log_line(const LogEntry& entry, int count = 1);
    void _apply_entry_style(LogLevel level);
    void _update_filter_counts();
    bool _check_display(const LogEntry& entry) const;
    void _rebuild_log();
    void _on_search_changed(const godot::String& text);
    void _on_filter_toggled(bool active, int level_idx);
    void _on_category_changed(int index);
    void _on_clear();
    void _on_collapse_toggled(bool enabled);
};

} // namespace godot_self_driving

#endif
