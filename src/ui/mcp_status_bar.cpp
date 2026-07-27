#include "mcp_status_bar.hpp"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot_self_driving {

McpStatusBar::McpStatusBar() {
    set_custom_minimum_size(godot::Vector2(0, 24));
    add_theme_constant_override("margin_left", 8);
    add_theme_constant_override("margin_right", 8);

    icon = memnew(godot::TextureRect);
    icon->set_stretch_mode(godot::TextureRect::STRETCH_KEEP_CENTERED);
    icon->set_custom_minimum_size(godot::Vector2(16, 16));
    add_child(icon);

    if (godot::Engine::get_singleton()->is_editor_hint()) {
        auto* editor = godot::EditorInterface::get_singleton();
        if (editor) {
            auto* base = editor->get_base_control();
            if (base) {
                auto theme = base->get_theme();
                if (theme.is_valid() && theme->has_icon("Node", "EditorIcons")) {
                    icon->set_texture(theme->get_icon("Node", "EditorIcons"));
                }
            }
        }
    }

    status_label = memnew(godot::Label);
    status_label->set_text("GSD: initializing");
    add_child(status_label);
}

void McpStatusBar::set_status_text(const godot::String& text) {
    status_label->set_text(text);
}

void McpStatusBar::set_status_ok(bool ok) {
    if (ok) {
        icon->set_modulate(godot::Color(0.3, 0.9, 0.3));
    } else {
        icon->set_modulate(godot::Color(0.9, 0.3, 0.3));
    }
}

void McpStatusBar::_bind_methods() {
}

} // namespace godot_self_driving
