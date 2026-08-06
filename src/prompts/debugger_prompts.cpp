#include "prompts/debugger_prompts.hpp"
#include "core/log_system.hpp"
#include <mcp/Content.hpp>

namespace godot_self_driving {

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

void register_debugger_prompts(mcp::McpServer &server) {
  using mcp::PromptOptions;

  server.RegisterPrompt(
      "debug-analyze-error",
      PromptOptions{}.Description(
          "Analyze a runtime error with full context. Use output_get_log and "
          "debugger_get_errors first to gather data."),
      [](const std::string &name, const std::optional<mcp::JsonValue> &args) {
        std::string text =
            R"TEMPLATE(You have encountered a runtime error in the Godot game. Follow these steps to diagnose:

1. **Read error data**: Use `debugger_get_errors` to retrieve structured errors from the running game, or
   use `output_get_log` to check editor output for compilation/script errors.

2. **Identify the error type**:
   - GDScript runtime error (division by zero, null instance, etc.)
   - Engine error (resource loading failure, node path not found)
   - Script compilation error (syntax, type mismatch)

3. **Trace the source**: For each error found, note:
   - File and line number
   - Function name where it occurred
   - The error message and description

4. **Analyze the stack trace**:
   - Which function called which?
   - Is the error in game logic or engine interaction?

5. **Propose a fix**:
   - Check for null values before use
   - Validate node paths before access
   - Ensure resources are loaded before use
)TEMPLATE";

        if (args && args->IsObject()) {
          auto *err = args->Find("error_text");
          if (err && err->IsString()) {
            text += "\n## Error to analyze:\n" + err->GetString() + "\n";
          }
        }
        return make_result(text);
      });

  server.RegisterPrompt(
      "debug-analyze-breakpoint",
      PromptOptions{}.Description(
          "Analyze the current breakpoint context: stack trace, scene tree, "
          "and variable state."),
      [](const std::string &name,
         const std::optional<mcp::JsonValue> &args) -> mcp::GetPromptResult {
        std::string text =
            R"TEMPLATE(The debugger has paused at a breakpoint. Follow these steps:

1. **Read the stack**: Use `debugger_get_stack_dump` to see the call stack.
2. **Read the scene**: Use `debugger_get_scene_tree` to see the remote scene tree.
3. **Understand the context**:
   - Which function is the execution paused in?
   - What's the call path leading here?
   - Which nodes are currently active in the scene?

4. **Check session state**: Use `debugger_get_session_info` to confirm debugging state.

5. **Determine the next action**:
   - If investigating a bug: compare expected vs actual values
   - If inspecting flow: check if the call hierarchy is as expected
)TEMPLATE";
        return make_result(text);
      });

  server.RegisterPrompt(
      "debug-review-output",
      PromptOptions{}.Description("Review the game's output log for issues, "
                                  "errors, and unexpected behavior."),
      [](const std::string &name,
         const std::optional<mcp::JsonValue> &args) -> mcp::GetPromptResult {
        std::string text =
            R"TEMPLATE(Review the game output to understand the current state:

1. **Read outputs**:
   - Use `output_get_log` for editor-side output (script compilation results, tool scripts)
   - Use `debugger_get_output` for runtime print() output from the running game

2. **Look for patterns**:
   - Error messages: RED/RED with stack traces
   - Warnings: YELLOW annotations
   - Performance data: frame time, memory usage

3. **Correlate with game state**:
   - Do errors occur on specific actions (loading, collision, input)?
   - Are there repeated error patterns suggesting a loop issue?
   - Is the output silent when it shouldn't be?

4. **Synthesize findings**:
   - What is the health of the running game?
   - What should be fixed or investigated next?
)TEMPLATE";

        if (args && args->IsObject()) {
          auto *s = args->Find("since");
          if (s && s->IsString()) {
            text += "\n## Context: Reviewing output since \"" + s->GetString() +
                    "\"\n";
          }
        }
        return make_result(text);
      });

  server.RegisterPrompt(
      "debug-review-performance",
      PromptOptions{}.Description(
          "Review performance monitor data to identify bottlenecks."),
      [](const std::string &name,
         const std::optional<mcp::JsonValue> &args) -> mcp::GetPromptResult {
        std::string text = R"TEMPLATE(Review game performance:

1. **Read monitor data**: Use `debugger_get_monitors` to retrieve the latest performance frame.

2. **Key metrics to check**:
   - FPS: target 60 (or your project's target)
   - Time Process: CPU time spent in _process() — should be < 16ms for 60 FPS
   - Time Physics: CPU time in _physics_process() — should be < 16ms
   - Static Memory: should be stable, no growth over time
   - Nodes: object count
   - Draw calls: lower is better for 2D

3. **Identify anomalies**:
   - Sudden FPS drops
   - Memory leaks (growing memory over time)
   - Excessive node counts

4. **Performance advice**:
   - Too many nodes? Consider pooling or Object pooling
   - High draw calls? Check for unnecessary CanvasItem updates
   - High physics time? Simplify collision shapes
)TEMPLATE";
        return make_result(text);
      });

  server.RegisterPrompt(
      "debug-session-status",
      PromptOptions{}.Description(
          "Quick summary of the current debug session state."),
      [](const std::string &name,
         const std::optional<mcp::JsonValue> &args) -> mcp::GetPromptResult {
        std::string text = R"TEMPLATE(Check the current debug session:

1. Use `debugger_get_session_info` to see if a game is running and if the debugger is paused.
2. If active and not breaked: the game is running normally. Check `debugger_get_errors` for any runtime errors.
3. If breaked: the debugger is paused at a breakpoint. Use `debugger_get_stack_dump` to see where.
4. If inactive: no game is currently running in the editor.

Based on the state, I will:
- Running with errors → diagnose the errors
- Breaked → analyze the breakpoint context
- Running clean → report that the game is running normally
- No session → report that no game is running
)TEMPLATE";
        return make_result(text);
      });

  LogSystem::instance().log(LogLevel::Info, LogCategory::Prompts,
                            "Registered debugger prompts: 5 templates");
}

} // namespace godot_self_driving
