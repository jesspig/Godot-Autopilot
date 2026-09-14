#ifndef GODOT_AUTOPILOT_TYPE_HINT_HPP
#define GODOT_AUTOPILOT_TYPE_HINT_HPP

#include "error_util.hpp"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <optional>
#include <string>

namespace godot_autopilot {
namespace util {

inline bool parse_type_hint_int(const std::string &text, int &out_value) {
  if (text.empty()) {
    return false;
  }
  int value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + (c - '0');
    if (value > 100000) {
      return false;
    }
  }
  out_value = value;
  return true;
}

struct ArrayElementHint {
  bool known = false;
  godot::Variant::Type type = godot::Variant::Type::NIL;
  int hint = 0;
  std::string hint_string;
};

inline std::optional<ArrayElementHint>
parse_array_element_hint(const godot::Dictionary &prop_info) {
  if (!prop_info.has("hint") || !prop_info.has("hint_string")) {
    return std::nullopt;
  }
  int hint_val = static_cast<int>(prop_info["hint"]);
  if (hint_val != godot::PROPERTY_HINT_TYPE_STRING) {
    return std::nullopt;
  }
  std::string raw = to_std(prop_info["hint_string"].operator godot::String());
  std::size_t slash = raw.find('/');
  if (slash == std::string::npos) {
    return std::nullopt;
  }
  std::size_t colon = raw.find(':', slash + 1);
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  int element_type_id = 0;
  int element_hint = 0;
  if (!parse_type_hint_int(raw.substr(0, slash), element_type_id) ||
      !parse_type_hint_int(raw.substr(slash + 1, colon - slash - 1),
                           element_hint)) {
    return std::nullopt;
  }
  ArrayElementHint result;
  result.known =
      element_type_id < static_cast<int>(godot::Variant::VARIANT_MAX);
  result.type = static_cast<godot::Variant::Type>(element_type_id);
  result.hint = element_hint;
  result.hint_string = raw.substr(colon + 1);
  return result;
}

inline std::string infer_type_hint(const godot::Dictionary &dict,
                                   std::string type_hint) {
  if (!type_hint.empty() || !dict.has("type")) {
    return type_hint;
  }
  int type_id = static_cast<int>(dict["type"]);
  int hint_val = 0;
  if (dict.has("hint")) {
    hint_val = static_cast<int>(dict["hint"]);
  }
  bool is_object_type =
      static_cast<godot::Variant::Type>(type_id) == godot::Variant::OBJECT;
  bool is_resource_hint = hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE;
  if ((is_object_type || is_resource_hint) && dict.has("hint_string")) {
    std::string hint_str = to_std(dict["hint_string"].operator godot::String());
    if (!hint_str.empty()) {
      type_hint = hint_str;
    }
  }
  if (type_hint.empty()) {
    type_hint = to_std(godot::Variant::get_type_name(
        static_cast<godot::Variant::Type>(type_id)));
  }
  return type_hint;
}

} // namespace util
} // namespace godot_autopilot

#endif
