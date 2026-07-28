#ifndef GODOT_SELF_DRIVING_VARIANT_JSON_HPP
#define GODOT_SELF_DRIVING_VARIANT_JSON_HPP

#include <mcp/JsonValue.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot_self_driving {

struct VariantJson {
    static mcp::JsonValue serialize(const godot::Variant& v);
    static godot::Variant deserialize(const mcp::JsonValue& j, const std::string& type_hint = "");
};

} // namespace godot_self_driving

#endif
