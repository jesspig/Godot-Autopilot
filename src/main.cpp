#include <cstdlib>
#include <string>
#include <typeinfo>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "core/log_system.hpp"
#include "core/mode_detector.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "core/server_context.hpp"
#include "runtime/game_bridge.hpp"
#include "tools/debugger_ops.hpp"
#include "tools/runtime_ops.hpp"
#include "ui/mcp_log_dock.hpp"
#include "ui/mcp_status_bar.hpp"

static godot_self_driving::LogSystem &get_log_system() {
  return godot_self_driving::LogSystem::instance();
}

static godot_self_driving::ServerContext *g_server_ctx = nullptr;

static godot_self_driving::ServerContext *get_server_ctx() {
  return g_server_ctx;
}

static bool gsd_cmdline_mode() {
  if (const char *f = std::getenv("GSD_FORCE_HEADLESS")) {
    if (std::string(f) == "1") return false;
  }
  auto *engine = godot::Engine::get_singleton();
  auto *ds = godot::DisplayServer::get_singleton();
  if (!engine || !engine->is_editor_hint() || !ds) return false;
  return !ds->window_can_draw();
}

#ifdef _WIN32
#define GSD_EXPORT __declspec(dllexport)
#else
#define GSD_EXPORT
#endif

class GodotSelfDrivingPlugin : public godot::EditorPlugin {
  GDCLASS(GodotSelfDrivingPlugin, godot::EditorPlugin)

  godot_self_driving::McpStatusBar *status_bar;
  godot_self_driving::McpLogDock *log_dock;
  godot::Ref<godot_self_driving::debugger_ops::OutputCaptureLogger>
      output_logger_;
  godot::Ref<godot_self_driving::debugger_ops::DebugCapturePlugin>
      debug_plugin_;
  godot::Ref<godot_self_driving::ExportGuard> export_guard_;
  static godot_self_driving::CommandQueue s_queue;

protected:
  static void _bind_methods() {}

public:
  GodotSelfDrivingPlugin() : status_bar(nullptr), log_dock(nullptr) {}

  void _enter_tree() override;
  void _exit_tree() override;
  void _process(double delta) override;
  godot::String
  _get_unsaved_status(const godot::String &p_for_scene) const override;

  static godot_self_driving::CommandQueue &queue() { return s_queue; }
};

godot_self_driving::CommandQueue GodotSelfDrivingPlugin::s_queue;

godot::String GodotSelfDrivingPlugin::_get_unsaved_status(
    const godot::String &p_for_scene) const {
  if (godot_self_driving::scene_dirty_tracker::is_current_scene_dirty()) {
    return p_for_scene;
  }
  return godot::String();
}

void GodotSelfDrivingPlugin::_enter_tree() {
  using godot_self_driving::LogCategory;
  using godot_self_driving::LogLevel;

  get_log_system().log(LogLevel::Info, LogCategory::System,
                       "==== Godot Self-Driving plugin starting ====");
  godot_self_driving::runtime_ops::set_editor_queue(
      &GodotSelfDrivingPlugin::queue());
  get_log_system().log(LogLevel::Info, LogCategory::System,
                       std::string("Runtime mode: ") +
                           (godot_self_driving::ModeDetector::is_editor()
                                ? "Editor"
                                : "Runtime"));

  if (gsd_cmdline_mode()) {
    get_log_system().log(LogLevel::Info, LogCategory::System,
                         "cmdline mode: plugin UI/server disabled");
    return;
  }

  try {
    status_bar = memnew(godot_self_driving::McpStatusBar);
    status_bar->set_status_text("GSD: starting...");
    add_control_to_container(godot::EditorPlugin::CONTAINER_TOOLBAR,
                             status_bar);
    get_log_system().log(LogLevel::Debug, LogCategory::System,
                         "Toolbar status bar attached");
  } catch (const std::exception &e) {
    get_log_system().log(
        LogLevel::Error, LogCategory::System,
        "plugin setup step failed (status bar): " + std::string(e.what()) +
            " (type=" + typeid(e).name() + ")");
  } catch (...) {
    get_log_system().log(LogLevel::Error, LogCategory::System,
                         "plugin setup step failed (status bar): "
                         "unknown exception");
  }

  try {
    log_dock = memnew(godot_self_driving::McpLogDock);
    log_dock->set_title("MCP Log");
    add_dock(log_dock);
    get_log_system().log(LogLevel::Debug, LogCategory::System,
                         "Bottom log dock registered");
  } catch (const std::exception &e) {
    get_log_system().log(
        LogLevel::Error, LogCategory::System,
        "plugin setup step failed (log dock): " + std::string(e.what()) +
            " (type=" + typeid(e).name() + ")");
  } catch (...) {
    get_log_system().log(LogLevel::Error, LogCategory::System,
                         "plugin setup step failed (log dock): "
                         "unknown exception");
  }

  try {
    output_logger_ = godot_self_driving::debugger_ops::create_output_logger();
    if (output_logger_.is_valid()) {
      auto *os = godot::OS::get_singleton();
      if (os) {
        os->add_logger(output_logger_);
        get_log_system().log(LogLevel::Info, LogCategory::System,
                             "Output capture logger registered");
      }
    }
  } catch (const std::exception &e) {
    get_log_system().log(
        LogLevel::Error, LogCategory::System,
        "plugin setup step failed (output logger): " +
            std::string(e.what()) + " (type=" + typeid(e).name() + ")");
  } catch (...) {
    get_log_system().log(LogLevel::Error, LogCategory::System,
                         "plugin setup step failed (output logger): "
                         "unknown exception");
  }

  try {
    debug_plugin_ = godot_self_driving::debugger_ops::create_debug_plugin();
    if (debug_plugin_.is_valid()) {
      add_debugger_plugin(debug_plugin_);
      get_log_system().log(LogLevel::Info, LogCategory::System,
                           "Debugger capture plugin registered");
    }
  } catch (const std::exception &e) {
    get_log_system().log(
        LogLevel::Error, LogCategory::System,
        "plugin setup step failed (debugger plugin): " +
            std::string(e.what()) + " (type=" + typeid(e).name() + ")");
  } catch (...) {
    get_log_system().log(LogLevel::Error, LogCategory::System,
                         "plugin setup step failed (debugger plugin): "
                         "unknown exception");
  }

  g_server_ctx = new (std::nothrow)
      godot_self_driving::ServerContext(GodotSelfDrivingPlugin::queue());
  if (g_server_ctx) {
    bool started = g_server_ctx->start();
    if (started) {
      auto port = g_server_ctx->get_port();
      if (status_bar) {
        status_bar->set_status_text("GSD: 0.0.0.0:" +
                                    godot::String::num_int64(port));
      }
      get_log_system().log(LogLevel::Info, LogCategory::Transport,
                           "MCP server listening on 0.0.0.0:" +
                               std::to_string(port));
    } else {
      if (status_bar) {
        status_bar->set_status_text("GSD: offline");
      }
      get_log_system().log(LogLevel::Error, LogCategory::Transport,
                           "MCP server start failed: " +
                               g_server_ctx->last_error());
    }
  } else {
    if (status_bar) {
      status_bar->set_status_text("GSD: offline");
    }
  }

  export_guard_.instantiate();
  add_export_plugin(export_guard_);

  get_log_system().log(LogLevel::Info, LogCategory::System, "Plugin ready");
}

void GodotSelfDrivingPlugin::_process(double) {
  s_queue.drain();
  if (log_dock) {
    log_dock->poll_new_entries();
  }
}

void GodotSelfDrivingPlugin::_exit_tree() {
  try {
    if (g_server_ctx) {
      g_server_ctx->stop();
      delete g_server_ctx;
      g_server_ctx = nullptr;
    }

    if (export_guard_.is_valid()) {
      remove_export_plugin(export_guard_);
      export_guard_.unref();
    }

    if (debug_plugin_.is_valid()) {
      remove_debugger_plugin(debug_plugin_);
      debug_plugin_.unref();
    }

    if (output_logger_.is_valid()) {
      auto *os = godot::OS::get_singleton();
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
      remove_control_from_container(godot::EditorPlugin::CONTAINER_TOOLBAR,
                                    status_bar);
      memdelete(status_bar);
      status_bar = nullptr;
    }
    get_log_system().log(godot_self_driving::LogLevel::Info,
                         godot_self_driving::LogCategory::System,
                         "Editor plugin exited");
  } catch (const std::exception &e) {
    get_log_system().log(godot_self_driving::LogLevel::Error,
                         godot_self_driving::LogCategory::System,
                         "exit tree exception: " + std::string(e.what()) +
                             " (type=" + typeid(e).name() + ")");
  } catch (...) {
    get_log_system().log(godot_self_driving::LogLevel::Error,
                         godot_self_driving::LogCategory::System,
                         "exit tree exception: unknown exception");
  }
}

extern "C" {
GSD_EXPORT GDExtensionBool
GDExtensionEntryPoint(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                      GDExtensionClassLibraryPtr p_library,
                      GDExtensionInitialization *r_initialization) {
  godot::GDExtensionBinding::InitObject init(p_get_proc_address, p_library,
                                             r_initialization);

  init.register_initializer([](godot::ModuleInitializationLevel p_level) {
    try {
      if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        if (!godot::Engine::get_singleton()->is_editor_hint()) {
          godot_self_driving::runtime::game_bridge::register_listener();
        }
        get_log_system().log(godot_self_driving::LogLevel::Info,
                             godot_self_driving::LogCategory::System,
                             "Scene level initialized");
      }
      if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
        get_log_system().log(godot_self_driving::LogLevel::Info,
                             godot_self_driving::LogCategory::System,
                             "Editor level initialized");

        godot_self_driving::debugger_ops::register_classes();

        godot::ClassDB::register_class<godot_self_driving::McpLogDock>();
        godot::ClassDB::register_class<godot_self_driving::McpStatusBar>();
        godot::ClassDB::register_class<godot_self_driving::ExportGuard>();
        godot::ClassDB::register_class<GodotSelfDrivingPlugin>();
        godot::EditorPlugins::add_by_type<GodotSelfDrivingPlugin>();
      }
    } catch (const std::exception &e) {
      get_log_system().log(godot_self_driving::LogLevel::Error,
                           godot_self_driving::LogCategory::System,
                           "initializer exception: " + std::string(e.what()));
    } catch (...) {
      get_log_system().log(godot_self_driving::LogLevel::Error,
                           godot_self_driving::LogCategory::System,
                           "initializer unknown exception");
    }
  });

  init.register_terminator([](godot::ModuleInitializationLevel p_level) {
    try {
      if (p_level == godot::MODULE_INITIALIZATION_LEVEL_EDITOR) {
        get_log_system().log(godot_self_driving::LogLevel::Info,
                             godot_self_driving::LogCategory::System,
                             "Editor level terminated");
      }
      if (p_level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        godot_self_driving::runtime::game_bridge::unregister_listener();
        get_log_system().log(godot_self_driving::LogLevel::Info,
                             godot_self_driving::LogCategory::System,
                             "Scene level terminated");
      }
    } catch (const std::exception &e) {
      get_log_system().log(godot_self_driving::LogLevel::Error,
                           godot_self_driving::LogCategory::System,
                           "terminator exception: " + std::string(e.what()));
    } catch (...) {
      get_log_system().log(godot_self_driving::LogLevel::Error,
                           godot_self_driving::LogCategory::System,
                           "terminator unknown exception");
    }
  });

  return init.init();
}
}
