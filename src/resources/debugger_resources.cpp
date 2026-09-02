#include "resources/debugger_resources.hpp"
#include "core/log_system.hpp"
#include "tools/debugger_ops.hpp"

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

  server.RegisterResource(
      "editor-output-log", "godot://editor/output-log",
      ResourceOptions{}.Description(
          "Recent editor output log (print/printerr/script errors)"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_log_text(200);
        }));
      });

  server.RegisterResource(
      "debugger-errors", "godot://debugger/errors",
      ResourceOptions{}.Description(
          "Runtime errors and warnings from the running game"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_errors_text(50);
        }));
      });

  server.RegisterResource("debugger-output", "godot://debugger/output",
                          ResourceOptions{}.Description(
                              "Runtime print() output from the running game"),
                          [make_result, &queue](const std::string &uri) {
                            return make_result(uri, queue.execute_sync([] {
                              return capture_get_game_output_text(200);
                            }));
                          });

  server.RegisterResource(
      "debugger-stack-dump", "godot://debugger/stack-dump",
      ResourceOptions{}.Description(
          "Current stack trace when debugger is paused on a breakpoint"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_stack_dump_text();
        }));
      });

  server.RegisterResource(
      "debugger-scene-tree", "godot://debugger/scene-tree",
      ResourceOptions{}.Description("Remote scene tree of the running game"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_scene_tree_text();
        }));
      });

  server.RegisterResource(
      "debugger-monitors", "godot://debugger/monitors",
      ResourceOptions{}.Description(
          "Latest performance monitor data from the running game"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_monitors_text(5);
        }));
      });

  server.RegisterResource(
      "debugger-session", "godot://debugger/session",
      ResourceOptions{}.Description(
          "Current debugger session state (active/breaked/running)"),
      [make_result, &queue](const std::string &uri) {
        return make_result(uri, queue.execute_sync([] {
          return capture_get_session_info_text();
        }));
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Resources,
                            "Registered debugger resources: 7 URIs");
}

} // namespace godot_autopilot
