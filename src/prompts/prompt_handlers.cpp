#include "prompts/prompt_handlers.hpp"
#include "core/log_system.hpp"
#include "prompts/prompt_create_3d_scene.hpp"
#include "prompts/prompt_debug_physics.hpp"
#include "prompts/prompt_keycode_reference.hpp"
#include "prompts/prompt_setup_character.hpp"
#include "prompts/prompt_setup_gui.hpp"
#include "prompts/prompt_setup_input_map.hpp"
#include "prompts/prompt_tool_usage.hpp"
#include <mcp/Content.hpp>
#include <string>

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

} // namespace

void register_all_prompts(mcp::McpServer &server, CommandQueue &queue) {
  server.RegisterPrompt(
      "create-3d-scene",
      mcp::PromptOptions{}.Description("Guide to create a basic 3D scene with "
                                       "camera, lighting, and a test object"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_create_3d_scene(); })
            .get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "setup-character",
      mcp::PromptOptions{}.Description(
          "Guide to set up a 3D character controller with CharacterBody3D, "
          "collision, and movement script"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_setup_character(); })
            .get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "debug-physics",
      mcp::PromptOptions{}.Description(
          "Guide to use physics debugging tools including ray casts, shape "
          "casts, and performance monitors"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_debug_physics(); }).get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "setup-input-map",
      mcp::PromptOptions{}.Description(
          "Guide to configure input actions in Project Settings and test them"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_setup_input_map(); })
            .get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "setup-gui",
      mcp::PromptOptions{}.Description(
          "Guide to create a simple GUI with CanvasLayer, containers, buttons, "
          "and labels"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_setup_gui(); }).get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "tool-usage",
      mcp::PromptOptions{}.Description(
          "Usage examples for the 17 most commonly used MCP tools with JSON "
          "input/output and gotchas"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_tool_usage(); }).get();
        return make_result(content);
      });

  server.RegisterPrompt(
      "keycode-reference",
      mcp::PromptOptions{}.Description(
          "Godot keycode reference for InputEventKey and Variant type JSON "
          "mapping for property operations"),
      [&queue](const std::string &,
               const std::optional<mcp::JsonValue> &) -> mcp::GetPromptResult {
        std::string content;
        queue.submit([&content]() { content = prompt_keycode_reference(); })
            .get();
        return make_result(content);
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Prompts,
                            "7 prompt templates registered");
}

} // namespace godot_autopilot
