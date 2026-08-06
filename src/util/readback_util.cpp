#include "readback_util.hpp"
#include "variant_json.hpp"

namespace godot_self_driving {
namespace util {

ReadbackStatus check_readback(const godot::Variant &expected_value,
                              const godot::Variant &old_value,
                              const godot::Variant &actual_value,
                              std::string &out_detail) {
  std::string expected_dump = VariantJson::serialize(expected_value).Dump();
  std::string actual_dump = VariantJson::serialize(actual_value).Dump();

  if (expected_value.get_type() != godot::Variant::NIL &&
      actual_value.get_type() == godot::Variant::NIL) {
    out_detail =
        "property rejected: expected " + expected_dump + " but readback is nil";
    return ReadbackStatus::REJECTED;
  }

  if (expected_dump == actual_dump) {
    out_detail = "value matches expected";
    return ReadbackStatus::MATCHED;
  }

  if (expected_dump == VariantJson::serialize(old_value).Dump()) {
    out_detail = "value already at target (default)";
    return ReadbackStatus::NOOP;
  }

  out_detail = "engine adjusted value: " + expected_dump + " -> " + actual_dump;
  return ReadbackStatus::CONVERTED;
}

} // namespace util
} // namespace godot_self_driving
