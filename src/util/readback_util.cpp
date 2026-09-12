#include "readback_util.hpp"
#include "variant_json.hpp"
#include <godot_cpp/classes/object.hpp>

namespace godot_autopilot {
namespace util {

ReadbackStatus check_readback(const godot::Variant &expected_value,
                              const godot::Variant &old_value,
                              const godot::Variant &actual_value,
                              std::string &out_detail, bool type_sensitive) {
  std::string expected_dump = VariantJson::serialize(expected_value).Dump();
  std::string actual_dump = VariantJson::serialize(actual_value).Dump();

  if (type_sensitive) {
    if (expected_value.get_type() == godot::Variant::OBJECT &&
        expected_value.operator godot::Object *() != nullptr) {
      bool actual_is_object =
          actual_value.get_type() == godot::Variant::OBJECT &&
          actual_value.operator godot::Object *() != nullptr;
      if (!actual_is_object) {
        out_detail = "property rejected: value not applied (type mismatch: "
                     "expected object " +
                     expected_dump + " but readback is " + actual_dump + ")";
        return ReadbackStatus::REJECTED;
      }
    }
    if (expected_value.get_type() == godot::Variant::ARRAY &&
        actual_value.get_type() == godot::Variant::ARRAY) {
      godot::Array expected_array = expected_value.operator godot::Array();
      godot::Array actual_array = actual_value.operator godot::Array();
      if (expected_array.size() > 0 && actual_array.size() == 0) {
        out_detail = "property rejected: value not applied (expected " +
                     expected_dump + " but readback array is empty)";
        return ReadbackStatus::REJECTED;
      }
      int64_t count = expected_array.size() < actual_array.size()
                          ? expected_array.size()
                          : actual_array.size();
      for (int64_t i = 0; i < count; i++) {
        if (expected_array[i].get_type() != godot::Variant::NIL &&
            actual_array[i].get_type() == godot::Variant::NIL) {
          out_detail = "property rejected: value not applied (array element " +
                       std::to_string(i) + " is null, expected " +
                       VariantJson::serialize(expected_array[i]).Dump() + ")";
          return ReadbackStatus::REJECTED;
        }
      }
    }
  }

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
} // namespace godot_autopilot
