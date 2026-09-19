#include <tools/tool_pipeline.hpp>

#include <tools/capture_ops.hpp>
#include <tools/tool_spec.hpp>
#include <util/scene_path.hpp>

namespace godot_autopilot {
namespace pipeline {

namespace {

void merge_observe_result(mcp::JsonValue &inner) {
  mcp::JsonValue capture = capture_ops::handle_capture_viewport(
      mcp::JsonValue::Parse(R"({"target":"editor"})"));
  const mcp::JsonValue *capture_result = capture.Find("result");
  if (capture_result && capture_result->IsObject()) {
    const char *keys[] = {"data", "format", "width", "height", "path"};
    for (const char *key : keys) {
      if (const mcp::JsonValue *value = capture_result->Find(key)) {
        inner[key] = *value;
      }
    }
    return;
  }
  const mcp::JsonValue *error = capture.Find("error");
  if (error && error->IsString()) {
    inner["observe_error"] = *error;
  } else {
    inner["observe_error"] = mcp::JsonValue("capture failed");
  }
}

} // namespace

mcp::JsonValue run_post(const ToolSpec &spec, const mcp::JsonValue &args,
                        mcp::JsonValue result) {
  if ((spec.flags & tool_flags::kObserve) != 0 && args.IsObject() &&
      result.IsObject()) {
    const mcp::JsonValue *observe = args.Find("observe");
    if (observe && observe->IsBool() && observe->GetBool()) {
      mcp::JsonValue *target = result.Find("result");
      if (!target || !target->IsObject()) {
        target = &result;
      }
      merge_observe_result(*target);
    }
  }
  if ((spec.flags & tool_flags::kSceneTarget) != 0 && result.IsObject() &&
      result.Find("error") == nullptr) {
    mcp::JsonValue *target = result.Find("result");
    if (target && target->IsObject() &&
        target->Find("scene_path") == nullptr) {
      util::add_scene_info_fields(*target, util::edited_scene_info());
    }
  }
  return result;
}

} // namespace pipeline
} // namespace godot_autopilot
