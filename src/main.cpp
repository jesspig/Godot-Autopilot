#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref.hpp>

#include "core/command_queue.hpp"
#include "core/log_system.hpp"
#include "core/mode_detector.hpp"
#include "core/server_context.hpp"
#include "runtime/game_bridge.hpp"
#include "tools/debugger_ops.hpp"
#include "ui/mcp_log_dock.hpp"
#include "ui/mcp_status_bar.hpp"

static godot_self_driving::LogSystem& get_log_system() {
    return godot_self_driving::LogSystem::instance();
}

static godot_self_driving::ServerContext* g_server_ctx = nullptr;

static godot_self_driving::ServerContext* get_server_ctx() {
    return g_server_ctx;
}

#ifdef _WIN32
#define GSD_EXPORT __declspec(dllexport)
#else
#define GSD_EXPORT
#endif

class GodotSelfDrivingPlugin : public godot::EditorPlugin {
    GDCLASS(GodotSelfDrivingPlugin, godot::EditorPlugin)

    godot_self_driving::McpStatusBar* status_bar;
    godot_self_driving::McpLogDock* log_dock;
    godot::Ref<::godot::OutputCaptureLogger> output_logger_;
    godot::Ref<::godot::DebugCapturePlugin> debug_plugin_;
    static godot_self_driving::CommandQueue s_queue;

protected:
    static void _bind_methods() {}

public:
    GodotSelfDrivingPlugin() : status_bar(nullptr), log_dock(nullptr) {}

    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;

    static godot_self_driving::CommandQueue& queue() { return s_queue; }
};

godot_self_driving::CommandQueue GodotSelfDrivingPlugin::s_queue;

namespace godot_self_driving {

// Forward-declared in tools/runtime_ops.hpp; lets the runtime tools dispatch
// their send_request work onto the editor main thread.
CommandQueue& get_editor_queue() {
    return GodotSelfDrivingPlugin::queue();
}

} // namespace godot_self_driving

void GodotSelfDrivingPlugin::_enter_tree() {
    using godot_self_driving::LogLevel;
    using godot_self_driving::LogCategory;

    get_log_system().log(LogLevel::Info, LogCategory::System, "==== Godot Self-Driving plugin starting ====");
    get_log_system().log(LogLevel::Info, LogCategory::System,
        std::string("Runtime mode: ") + (godot_self_driving::ModeDetector::is_editor() ? "Editor" : "Runtime"));

    status_bar = memnew(godot_self_driving::McpStatusBar);
    if (get_server_ctx() && get_server_ctx()->is_running()) {
        auto port = get_server_ctx()->get_port();
        status_bar->set_status_text("GSD: 127.0.0.1:" + godot::String::num_int64(port));
    } else {
        status_bar->set_status_text("GSD: offline");
    }
    add_control_to_container(godot::EditorPlugin::CONTAINER_TOOLBAR, status_bar);
    get_log_system().log(LogLevel::Debug, LogCategory::System, "Toolbar status bar attached");

    log_dock = memnew(godot_self_driving::McpLogDock);
    log_dock->set_title("MCP Log");
    add_dock(log_dock);
    get_log_system().log(LogLevel::Debug, LogCategory::System, "Bottom log dock registered");

    // ── Register output capture logger ──
    output_logger_ = godot_self_driving::debugger_ops::create_output_logger();
    if (output_logger_.is_valid()) {
        auto* os = godot::OS::get_singleton();
        if (os) {
            os->add_logger(output_logger_);
            get_log_system().log(LogLevel::Info, LogCategory::System, "Output capture logger registered");
        }
    }

    // ── Register debugger capture plugin ──
    debug_plugin_ = godot_self_driving::debugger_ops::create_debug_plugin();
    if (debug_plugin_.is_valid()) {
        add_debugger_plugin(debug_plugin_);
        get_log_system().log(LogLevel::Info, LogCategory::System, "Debugger capture plugin registered");
    }

    get_log_system().log(LogLevel::Info, LogCategory::System, "Plugin ready");
}

void GodotSelfDrivingPlugin::_process(double) {
    s_queue.drain();
    if (log_dock) {
        log_dock->poll_new_entries();
    }
}

void GodotSelfDrivingPlugin::_exit_tree() {
    // ── Unregister debugger capture plugin ──
    if (debug_plugin_.is_valid()) {
        remove_debugger_plugin(debug_plugin_);
        debug_plugin_.unref();
    }

    // ── Unregister output capture logger ──
    if (output_logger_.is_valid()) {
        auto* os = godot::OS::get_singleton();
        if (os) {
            os->remove_logger(output_logger_);
        }
        output_logger_.unref();
    }

    if (log_dock) {
        remove_dock(log_dock);
        memdelete(log_dock);
        log_dock = nullptr;
    }
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
            if (!godot::Engine::get_singleton()->is_editor_hint()) {
                godot_self_driving::runtime::game_bridge::register_listener();
            }
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Scene level initialized");
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Editor level initialized");

            godot_self_driving::debugger_ops::register_classes();

            g_server_ctx = new (std::nothrow) godot_self_driving::ServerContext(GodotSelfDrivingPlugin::queue());
            if (g_server_ctx) {
                g_server_ctx->start();
                auto port = g_server_ctx->get_port();
                get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::Transport,
                    "MCP server listening on 127.0.0.1:" + std::to_string(port));
            }

            godot::ClassDB::register_class<godot_self_driving::McpLogDock>();
            godot::ClassDB::register_class<GodotSelfDrivingPlugin>();
            godot::EditorPlugins::add_by_type<GodotSelfDrivingPlugin>();
        }
    });

    init.register_terminator([](godot::ModuleInitializationLevel p_level) {
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
            delete g_server_ctx;
            g_server_ctx = nullptr;
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Editor level terminated");
        }
        if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
            godot_self_driving::runtime::game_bridge::unregister_listener();
            get_log_system().log(godot_self_driving::LogLevel::Info, godot_self_driving::LogCategory::System, "Scene level terminated");
        }
    });

    return init.init();
}
}
