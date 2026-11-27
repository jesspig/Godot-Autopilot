#include "error_util.hpp"

namespace godot_autopilot {
namespace util {

godot_autopilot::util::JsonValue error_json(const std::string &msg) {
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(msg);
  return e;
}

godot_autopilot::util::JsonValue ok_result(godot_autopilot::util::JsonValue value) {
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(value);
  return r;
}

const godot_autopilot::util::JsonValue *find_result_or_data(const godot_autopilot::util::JsonValue &body) {
  if (const godot_autopilot::util::JsonValue *v = body.Find("result")) {
    return v;
  }
  return body.Find("data");
}

godot_autopilot::util::JsonValue *find_result_or_data(godot_autopilot::util::JsonValue &body) {
  if (godot_autopilot::util::JsonValue *v = body.Find("result")) {
    return v;
  }
  return body.Find("data");
}

std::string to_std(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

godot_autopilot::util::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action) {
  return error_json(fact + " \xe2\x80\x94 position: " + position +
                    " \xe2\x80\x94 expected: " + expected +
                    " \xe2\x80\x94 action: " + action);
}

} // namespace util
} // namespace godot_autopilot
