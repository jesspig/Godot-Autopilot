#ifndef GODOT_SELF_DRIVING_READBACK_UTIL_HPP
#define GODOT_SELF_DRIVING_READBACK_UTIL_HPP

#include <string>
#include <godot_cpp/variant/variant.hpp>

namespace godot_self_driving {
namespace util {

enum class ReadbackStatus { MATCHED, REJECTED, CONVERTED, NOOP };

ReadbackStatus check_readback(const godot::Variant& expected_value,
                              const godot::Variant& old_value,
                              const godot::Variant& actual_value,
                              std::string& out_detail);

} // namespace util
} // namespace godot_self_driving

#endif
