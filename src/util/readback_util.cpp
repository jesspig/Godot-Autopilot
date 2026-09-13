#include "readback_util.hpp"
#include "variant_json.hpp"
#include <cmath>
#include <godot_cpp/classes/object.hpp>

namespace godot_autopilot {
namespace util {

namespace {

bool is_readback_value_type(godot::Variant::Type type) {
  switch (type) {
  case godot::Variant::VECTOR2:
  case godot::Variant::VECTOR2I:
  case godot::Variant::RECT2:
  case godot::Variant::RECT2I:
  case godot::Variant::VECTOR3:
  case godot::Variant::VECTOR3I:
  case godot::Variant::TRANSFORM2D:
  case godot::Variant::VECTOR4:
  case godot::Variant::VECTOR4I:
  case godot::Variant::PLANE:
  case godot::Variant::QUATERNION:
  case godot::Variant::AABB:
  case godot::Variant::BASIS:
  case godot::Variant::TRANSFORM3D:
  case godot::Variant::PROJECTION:
  case godot::Variant::COLOR:
    return true;
  default:
    return false;
  }
}

template <typename T> bool approx_equal(T a, T b) {
  double da = static_cast<double>(a);
  double db = static_cast<double>(b);
  double abs_a = std::abs(da);
  double abs_b = std::abs(db);
  double scale = abs_a > abs_b ? abs_a : abs_b;
  double tolerance = 1e-5 * scale > 1e-6 ? 1e-5 * scale : 1e-6;
  return std::abs(da - db) <= tolerance;
}

bool value_components_approx_equal(const godot::Variant &expected_value,
                                   const godot::Variant &actual_value) {
  switch (expected_value.get_type()) {
  case godot::Variant::VECTOR2: {
    auto a = static_cast<godot::Vector2>(expected_value);
    auto b = static_cast<godot::Vector2>(actual_value);
    return approx_equal(a.x, b.x) && approx_equal(a.y, b.y);
  }
  case godot::Variant::VECTOR2I: {
    auto a = static_cast<godot::Vector2i>(expected_value);
    auto b = static_cast<godot::Vector2i>(actual_value);
    return a.x == b.x && a.y == b.y;
  }
  case godot::Variant::RECT2: {
    auto a = static_cast<godot::Rect2>(expected_value);
    auto b = static_cast<godot::Rect2>(actual_value);
    return approx_equal(a.position.x, b.position.x) &&
           approx_equal(a.position.y, b.position.y) &&
           approx_equal(a.size.x, b.size.x) && approx_equal(a.size.y, b.size.y);
  }
  case godot::Variant::RECT2I: {
    auto a = static_cast<godot::Rect2i>(expected_value);
    auto b = static_cast<godot::Rect2i>(actual_value);
    return a.position.x == b.position.x && a.position.y == b.position.y &&
           a.size.x == b.size.x && a.size.y == b.size.y;
  }
  case godot::Variant::VECTOR3: {
    auto a = static_cast<godot::Vector3>(expected_value);
    auto b = static_cast<godot::Vector3>(actual_value);
    return approx_equal(a.x, b.x) && approx_equal(a.y, b.y) &&
           approx_equal(a.z, b.z);
  }
  case godot::Variant::VECTOR3I: {
    auto a = static_cast<godot::Vector3i>(expected_value);
    auto b = static_cast<godot::Vector3i>(actual_value);
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }
  case godot::Variant::TRANSFORM2D: {
    auto a = static_cast<godot::Transform2D>(expected_value);
    auto b = static_cast<godot::Transform2D>(actual_value);
    for (int i = 0; i < 3; i++) {
      if (!approx_equal(a.columns[i].x, b.columns[i].x) ||
          !approx_equal(a.columns[i].y, b.columns[i].y)) {
        return false;
      }
    }
    return true;
  }
  case godot::Variant::VECTOR4: {
    auto a = static_cast<godot::Vector4>(expected_value);
    auto b = static_cast<godot::Vector4>(actual_value);
    return approx_equal(a.x, b.x) && approx_equal(a.y, b.y) &&
           approx_equal(a.z, b.z) && approx_equal(a.w, b.w);
  }
  case godot::Variant::VECTOR4I: {
    auto a = static_cast<godot::Vector4i>(expected_value);
    auto b = static_cast<godot::Vector4i>(actual_value);
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
  }
  case godot::Variant::PLANE: {
    auto a = static_cast<godot::Plane>(expected_value);
    auto b = static_cast<godot::Plane>(actual_value);
    return approx_equal(a.normal.x, b.normal.x) &&
           approx_equal(a.normal.y, b.normal.y) &&
           approx_equal(a.normal.z, b.normal.z) && approx_equal(a.d, b.d);
  }
  case godot::Variant::QUATERNION: {
    auto a = static_cast<godot::Quaternion>(expected_value);
    auto b = static_cast<godot::Quaternion>(actual_value);
    return approx_equal(a.x, b.x) && approx_equal(a.y, b.y) &&
           approx_equal(a.z, b.z) && approx_equal(a.w, b.w);
  }
  case godot::Variant::AABB: {
    auto a = static_cast<godot::AABB>(expected_value);
    auto b = static_cast<godot::AABB>(actual_value);
    return approx_equal(a.position.x, b.position.x) &&
           approx_equal(a.position.y, b.position.y) &&
           approx_equal(a.position.z, b.position.z) &&
           approx_equal(a.size.x, b.size.x) &&
           approx_equal(a.size.y, b.size.y) &&
           approx_equal(a.size.z, b.size.z);
  }
  case godot::Variant::BASIS: {
    auto a = static_cast<godot::Basis>(expected_value);
    auto b = static_cast<godot::Basis>(actual_value);
    for (int i = 0; i < 3; i++) {
      if (!approx_equal(a.rows[i].x, b.rows[i].x) ||
          !approx_equal(a.rows[i].y, b.rows[i].y) ||
          !approx_equal(a.rows[i].z, b.rows[i].z)) {
        return false;
      }
    }
    return true;
  }
  case godot::Variant::TRANSFORM3D: {
    auto a = static_cast<godot::Transform3D>(expected_value);
    auto b = static_cast<godot::Transform3D>(actual_value);
    for (int i = 0; i < 3; i++) {
      if (!approx_equal(a.basis.rows[i].x, b.basis.rows[i].x) ||
          !approx_equal(a.basis.rows[i].y, b.basis.rows[i].y) ||
          !approx_equal(a.basis.rows[i].z, b.basis.rows[i].z)) {
        return false;
      }
    }
    return approx_equal(a.origin.x, b.origin.x) &&
           approx_equal(a.origin.y, b.origin.y) &&
           approx_equal(a.origin.z, b.origin.z);
  }
  case godot::Variant::PROJECTION: {
    auto a = static_cast<godot::Projection>(expected_value);
    auto b = static_cast<godot::Projection>(actual_value);
    for (int i = 0; i < 4; i++) {
      if (!approx_equal(a.columns[i].x, b.columns[i].x) ||
          !approx_equal(a.columns[i].y, b.columns[i].y) ||
          !approx_equal(a.columns[i].z, b.columns[i].z) ||
          !approx_equal(a.columns[i].w, b.columns[i].w)) {
        return false;
      }
    }
    return true;
  }
  case godot::Variant::COLOR: {
    auto a = static_cast<godot::Color>(expected_value);
    auto b = static_cast<godot::Color>(actual_value);
    return approx_equal(a.r, b.r) && approx_equal(a.g, b.g) &&
           approx_equal(a.b, b.b) && approx_equal(a.a, b.a);
  }
  default:
    return false;
  }
}

} // namespace

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
    godot::Variant::Type expected_type = expected_value.get_type();
    if (is_readback_value_type(expected_type)) {
      if (actual_value.get_type() != expected_type) {
        out_detail = "property rejected: value not applied (type mismatch: "
                     "expected " +
                     expected_dump + " but readback is " + actual_dump + ")";
        return ReadbackStatus::REJECTED;
      }
      if (value_components_approx_equal(expected_value, actual_value)) {
        out_detail = "value matches expected";
        return ReadbackStatus::MATCHED;
      }
      if (actual_dump == VariantJson::serialize(old_value).Dump()) {
        out_detail =
            "value not applied: property stayed at old value (expected " +
            expected_dump + ", readback " + actual_dump + ")";
        return ReadbackStatus::REJECTED;
      }
      out_detail =
          "engine adjusted value: " + expected_dump + " -> " + actual_dump;
      return ReadbackStatus::CONVERTED;
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
