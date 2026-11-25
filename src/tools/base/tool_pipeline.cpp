#include <tools/base/tool_pipeline.hpp>

#include <tools/domains/capture/capture_ops.hpp>
#include <tools/base/tool_spec.hpp>
#include <util/error_util.hpp>
#include <util/scene_path.hpp>

namespace godot_autopilot {
namespace pipeline {

namespace {

void merge_observe_result(godot_autopilot::util::JsonValue &inner) {
  godot_autopilot::util::JsonValue capture = capture_ops::handle_capture_viewport(
      godot_autopilot::util::JsonValue::Parse(R"({"target":"editor"})"));
  const godot_autopilot::util::JsonValue *capture_result = util::find_result_or_data(capture);
  if (capture_result && capture_result->IsObject()) {
    const char *keys[] = {"data", "format", "width", "height", "path"};
    for (const char *key : keys) {
      if (const godot_autopilot::util::JsonValue *value = capture_result->Find(key)) {
        inner[key] = *value;
      }
    }
    return;
  }
  const godot_autopilot::util::JsonValue *error = capture.Find("error");
  if (error && error->IsString()) {
    inner["observe_error"] = *error;
  } else {
    inner["observe_error"] = godot_autopilot::util::JsonValue("capture failed");
  }
}

} // namespace

godot_autopilot::util::JsonValue run_post(const ToolSpec &spec, const godot_autopilot::util::JsonValue &args,
                        godot_autopilot::util::JsonValue result) {
  if ((spec.flags & tool_flags::kObserve) != 0 && args.IsObject() &&
      result.IsObject()) {
    const godot_autopilot::util::JsonValue *observe = args.Find("observe");
    if (observe && observe->IsBool() && observe->GetBool()) {
      godot_autopilot::util::JsonValue *target = util::find_result_or_data(result);
      if (!target || !target->IsObject()) {
        target = &result;
      }
      merge_observe_result(*target);
      godot_autopilot::util::JsonValue *mirror = nullptr;
      if (target == result.Find("result")) {
        mirror = result.Find("data");
      } else if (target == result.Find("data")) {
        mirror = result.Find("result");
      }
      if (mirror && mirror->IsObject()) {
        const char *keys[] = {"data",         "format", "width",
                              "height",       "path",   "observe_error"};
        for (const char *key : keys) {
          if (const godot_autopilot::util::JsonValue *value = target->Find(key)) {
            (*mirror)[key] = *value;
          }
        }
      }
    }
  }
  if ((spec.flags & tool_flags::kSceneTarget) != 0 && result.IsObject() &&
      result.Find("error") == nullptr) {
    const char *envelopes[] = {"result", "data"};
    for (const char *key : envelopes) {
      godot_autopilot::util::JsonValue *target = result.Find(key);
      if (target && target->IsObject() &&
          target->Find("scene_path") == nullptr) {
        util::add_scene_info_fields(*target, util::edited_scene_info());
      }
    }
  }
  return result;
}

} // namespace pipeline
} // namespace godot_autopilot
