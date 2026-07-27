#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "core/mode_detector.hpp"
#include "ui/mcp_status_bar.hpp"

#ifdef _WIN32
#define GSD_EXPORT __declspec(dllexport)
#else
#define GSD_EXPORT
#endif

class GodotSelfDrivingPlugin : public godot::EditorPlugin {
    GDCLASS(GodotSelfDrivingPlugin, godot::EditorPlugin)

    godot_self_driving::McpStatusBar* status_bar;

protected:
    static void _bind_methods() {}

public:
    GodotSelfDrivingPlugin() : status_bar(nullptr) {}

    void _enter_tree() override;
    void _exit_tree() override;
};

void GodotSelfDrivingPlugin::_enter_tree() {
    bool is_editor = godot_self_driving::ModeDetector::is_editor();
    (void)is_editor;

    status_bar = memnew(godot_self_driving::McpStatusBar);
    status_bar->set_status_text("GSD: offline");
    add_control_to_container(godot::EditorPlugin::CONTAINER_TOOLBAR, status_bar);
}

void GodotSelfDrivingPlugin::_exit_tree() {
    if (status_bar) {
        remove_control_from_container(godot::EditorPlugin::CONTAINER_TOOLBAR, status_bar);
        memdelete(status_bar);
        status_bar = nullptr;
    }
}

extern "C" {
GSD_EXPORT GDExtensionBool GDExtensionEntryPoint(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization)
{
    godot::GDExtensionBinding::InitObject init(p_get_proc_address, p_library, r_initialization);

    init.register_initializer([](godot::ModuleInitializationLevel p_level) {
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
            godot::ClassDB::register_class<godot_self_driving::McpStatusBar>();
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            godot::ClassDB::register_class<GodotSelfDrivingPlugin>();
            godot::EditorPlugins::add_by_type<GodotSelfDrivingPlugin>();
        }
    });

    init.register_terminator([](godot::ModuleInitializationLevel p_level) {
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
            // Scene-level cleanup
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            // Editor-level cleanup
        }
    });

    return init.init();
}
}
