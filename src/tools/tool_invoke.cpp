#include "tools/tool_invoke.hpp"

#include "core/export_guard.hpp"
#include "tools/dispatch.hpp"
#include "util/error_util.hpp"

namespace godot_autopilot {
namespace tools {

namespace {

thread_local int g_depth = 0;

struct DepthGuard {
  DepthGuard() { ++g_depth; }
  ~DepthGuard() { --g_depth; }
};

} // namespace

int invoke_depth() { return g_depth; }

mcp::JsonValue invoke_tool(const std::string &name,
                           const mcp::JsonValue &args) {
  if (ExportGuard::is_exporting()) {
    return util::error_json(
        "editor is exporting; retry after export completes");
  }
  if (g_depth > kMaxInvokeDepth) {
    return util::error_json("tool invoke depth exceeded (max 8): '" + name +
                            "'");
  }
  DepthGuard guard;
  mcp::JsonValue result = dispatch::call_handler(name, args);
  if (result.IsObject()) {
    const mcp::JsonValue *pending = result.Find("__gda_pending");
    if (pending != nullptr && pending->IsInt()) {
      return util::error_json("cannot invoke async tool '" + name +
                              "' from within another tool on the main thread");
    }
  }
  return result;
}

} // namespace tools
} // namespace godot_autopilot
