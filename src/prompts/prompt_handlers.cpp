#include "prompts/prompt_handlers.hpp"
#include "core/config.hpp"
#include "core/export_guard.hpp"
#include "core/log_system.hpp"
#include "prompts/prompt_create_3d_scene.hpp"
#include "prompts/prompt_debug_physics.hpp"
#include "prompts/prompt_keycode_reference.hpp"
#include "prompts/prompt_setup_character.hpp"
#include "prompts/prompt_setup_gui.hpp"
#include "prompts/prompt_setup_input_map.hpp"
#include "prompts/prompt_tool_usage.hpp"
#include "tools/dispatch.hpp"
#include <mcp/Content.hpp>
#include <mcp/McpError.hpp>
#include <memory>
#include <string>
#include <utility>

namespace godot_autopilot {

namespace {

mcp::GetPromptResult make_result(const std::string &content) {
  mcp::GetPromptResult r;
  mcp::PromptMessage pm;
  pm.role = "assistant";
  pm.content = mcp::TextContent{"text", content};
  r.messages = {std::move(pm)};
  return r;
}

template <typename Fn>
mcp::GetPromptResult run_prompt_task(CommandQueue &queue,
                                     const std::string &label, Fn task) {
  if (godot_autopilot::ExportGuard::is_exporting()) {
    throw mcp::McpError(mcp::McpErrorCode::HandlerError,
                        dispatch::export_blocked_error_text());
  }
  auto state = std::make_shared<std::string>();
  dispatch::BoundedWaitOutcome outcome = dispatch::run_bounded_on_main_thread(
      queue, label, state, std::move(task), GDA_DISPATCH_WAIT_DEFAULT_MS);
  if (outcome.timed_out) {
    throw mcp::McpError(
        mcp::McpErrorCode::RequestTimeout,
        dispatch::main_thread_timeout_message(label, outcome.waited_ms,
                                              outcome.cancelled));
  }
  if (!outcome.ok) {
    throw mcp::McpError(mcp::McpErrorCode::InternalError, outcome.error);
  }
  return make_result(*state);
}

} // namespace

void register_all_prompts(mcp::McpServer &server, CommandQueue &queue) {
  server.RegisterPrompt(
      "create-3d-scene",
      mcp::PromptOptions{}.Description("Guide to create a basic 3D scene with "
                                       "camera, lighting, and a test object"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt create-3d-scene",
            [](std::string &content) { content = prompt_create_3d_scene(); });
      });

  server.RegisterPrompt(
      "setup-character",
      mcp::PromptOptions{}.Description(
          "Guide to set up a 3D character controller with CharacterBody3D, "
          "collision, and movement script"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt setup-character",
            [](std::string &content) { content = prompt_setup_character(); });
      });

  server.RegisterPrompt(
      "debug-physics",
      mcp::PromptOptions{}.Description(
          "Guide to use physics debugging tools including ray casts, shape "
          "casts, and performance monitors"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt debug-physics",
            [](std::string &content) { content = prompt_debug_physics(); });
      });

  server.RegisterPrompt(
      "setup-input-map",
      mcp::PromptOptions{}.Description(
          "Guide to configure input actions in Project Settings and test them"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt setup-input-map",
            [](std::string &content) { content = prompt_setup_input_map(); });
      });

  server.RegisterPrompt(
      "setup-gui",
      mcp::PromptOptions{}.Description(
          "Guide to create a simple GUI with CanvasLayer, containers, buttons, "
          "and labels"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt setup-gui",
            [](std::string &content) { content = prompt_setup_gui(); });
      });

  server.RegisterPrompt(
      "tool-usage",
      mcp::PromptOptions{}.Description(
          "Usage examples for the 17 most commonly used MCP tools with JSON "
          "input/output and gotchas"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt tool-usage",
            [](std::string &content) { content = prompt_tool_usage(); });
      });

  server.RegisterPrompt(
      "keycode-reference",
      mcp::PromptOptions{}.Description(
          "Godot keycode reference for InputEventKey and Variant type JSON "
          "mapping for property operations"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        return run_prompt_task(
            queue, "prompt keycode-reference",
            [](std::string &content) { content = prompt_keycode_reference(); });
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Prompts,
                            "7 prompt templates registered");
}

} // namespace godot_autopilot
