#include "error_util.hpp"

namespace godot_autopilot {
namespace util {

mcp::JsonValue error_json(const std::string &msg) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(msg);
  return e;
}

mcp::JsonValue ok_result(mcp::JsonValue value) {
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(value);
  return r;
}

std::string to_std(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

mcp::JsonValue error_detail(const std::string &fact,
                            const std::string &position,
                            const std::string &expected,
                            const std::string &action) {
  return error_json(fact + " \xe2\x80\x94 position: " + position +
                    " \xe2\x80\x94 expected: " + expected +
                    " \xe2\x80\x94 action: " + action);
}

} // namespace util
} // namespace godot_autopilot
