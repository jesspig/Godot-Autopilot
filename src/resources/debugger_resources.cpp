#include "resources/debugger_resources.hpp"
#include "core/log_system.hpp"
#include "core/monitor.hpp"
#include "tools/debugger_ops.hpp"
#include "tools/tool_invoke.hpp"

namespace godot_autopilot {

void register_debugger_resources(mcp::McpServer &server, CommandQueue &queue) {
  using namespace debugger_ops;
  using namespace mcp;

  auto make_result = [](const std::string &uri, const std::string &text) {
    TextResourceContents trc;
    trc.uri = uri;
    trc.text = text;
    trc.mime_type = "text/plain";
    ReadResourceResult rr;
    rr.contents = {ResourceContents{trc}};
    return rr;
  };

  monitor::lifecycle(
      "debugger_resource_editor_output_log",
      monitor::build_attrs({{"uri", "godot://editor/output-log"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "editor-output-log", "godot://editor/output-log",
      ResourceOptions{}.Description(
          "Recent editor output log (print/printerr/script errors)"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_log_text(200);
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  monitor::lifecycle(
      "debugger_resource_errors",
      monitor::build_attrs({{"uri", "godot://debugger/errors"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "debugger-errors", "godot://debugger/errors",
      ResourceOptions{}.Description(
          "Runtime errors and warnings from the running game"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_errors_text(50);
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  monitor::lifecycle(
      "debugger_resource_output",
      monitor::build_attrs({{"uri", "godot://debugger/output"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource("debugger-output", "godot://debugger/output",
                          ResourceOptions{}.Description(
                              "Runtime print() output from the running game"),
                          [make_result, &queue](const std::string &uri) {
                            const tools::TraceContext ctx =
                                tools::capture_trace_context();
                            const std::string text = queue.execute_sync([ctx] {
                              tools::ScopedTraceContext restore(ctx);
                              return capture_get_game_output_text(200);
                            });
                            monitor::data_flow(
                                "resource_read",
                                static_cast<int64_t>(text.size()),
                                monitor::build_attrs(
                                    {{"uri", uri},
                                     {"request_id",
                                      monitor::current_request_id()}}));
                            return make_result(uri, text);
                          });

  monitor::lifecycle(
      "debugger_resource_stack_dump",
      monitor::build_attrs({{"uri", "godot://debugger/stack-dump"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "debugger-stack-dump", "godot://debugger/stack-dump",
      ResourceOptions{}.Description(
          "Current stack trace when debugger is paused on a breakpoint"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_stack_dump_text();
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  monitor::lifecycle(
      "debugger_resource_scene_tree",
      monitor::build_attrs({{"uri", "godot://debugger/scene-tree"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "debugger-scene-tree", "godot://debugger/scene-tree",
      ResourceOptions{}.Description("Remote scene tree of the running game"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_scene_tree_text();
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  monitor::lifecycle(
      "debugger_resource_monitors",
      monitor::build_attrs({{"uri", "godot://debugger/monitors"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "debugger-monitors", "godot://debugger/monitors",
      ResourceOptions{}.Description(
          "Latest performance monitor data from the running game"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_monitors_text(5);
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  monitor::lifecycle(
      "debugger_resource_session",
      monitor::build_attrs({{"uri", "godot://debugger/session"},
                            {"request_id", monitor::current_request_id()}}));
  server.RegisterResource(
      "debugger-session", "godot://debugger/session",
      ResourceOptions{}.Description(
          "Current debugger session state (active/breaked/running)"),
      [make_result, &queue](const std::string &uri) {
        const tools::TraceContext ctx = tools::capture_trace_context();
        const std::string text = queue.execute_sync([ctx] {
          tools::ScopedTraceContext restore(ctx);
          return capture_get_session_info_text();
        });
        monitor::data_flow(
            "resource_read", static_cast<int64_t>(text.size()),
            monitor::build_attrs({{"uri", uri},
                                  {"request_id", monitor::current_request_id()}}));
        return make_result(uri, text);
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Resources,
                            "Registered debugger resources: 7 URIs");
}

} // namespace godot_autopilot
