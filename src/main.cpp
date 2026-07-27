#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>

#include "core/log_system.hpp"
#include "core/mode_detector.hpp"
#include "ui/mcp_status_bar.hpp"

static godot_self_driving::LogSystem& get_log_system() {
    return godot_self_driving::LogSystem::instance();
}

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
    using godot_self_driving::LogLevel;
    using godot_self_driving::LogCategory;

    get_log_system().log(LogLevel::Info, LogCategory::System, "==== Godot Self-Driving plugin starting ====");
    get_log_system().log(LogLevel::Info, LogCategory::System,
        std::string("Runtime mode: ") + (godot_self_driving::ModeDetector::is_editor() ? "Editor" : "Runtime"));

    status_bar = memnew(godot_self_driving::McpStatusBar);
    status_bar->set_status_text("GSD: offline");
    add_control_to_container(godot::EditorPlugin::CONTAINER_TOOLBAR, status_bar);
    get_log_system().log(LogLevel::Debug, LogCategory::System, "Toolbar status bar attached");

    get_log_system().log(LogLevel::Info, LogCategory::System, "Plugin ready");
}

void GodotSelfDrivingPlugin::_exit_tree() {
    if (status_bar) {
        remove_control_from_container(godot::EditorPlugin::CONTAINER_TOOLBAR, status_bar);
        memdelete(status_bar);
        status_bar = nullptr;
    }
    get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Editor plugin exited");
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
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Scene level initialized");
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Editor level initialized");

            godot::ClassDB::register_class<GodotSelfDrivingPlugin>();
            godot::EditorPlugins::add_by_type<GodotSelfDrivingPlugin>();
        }
    });

    init.register_terminator([](godot::ModuleInitializationLevel p_level) {
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Editor level terminated");
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Scene level terminated");
        }
    });

    return init.init();
}
}
