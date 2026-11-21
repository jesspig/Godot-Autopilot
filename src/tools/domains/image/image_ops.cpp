#include "tools/domains/image/image_ops.hpp"
#include "core/resource_registry.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/json_godot.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture1_d.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/noise_texture2d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#ifdef GetObject
#undef GetObject
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace image_ops {

namespace {

constexpr int kMaxDimension = 4096;
constexpr int kDefaultDimension = 256;

mcp::JsonValue make_error(const std::string &message) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(message);
  return e;
}

mcp::JsonValue make_result(mcp::JsonValue value) {
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(value);
  return r;
}

std::string action_of(const mcp::JsonValue &args) {
  const mcp::JsonValue *value = args.Find("action");
  if (value == nullptr || !value->IsString()) {
    return std::string();
  }
  return value->GetString();
}

std::string missing_param(const std::string &action, const char *name) {
  return "missing required parameter for action '" + action + "': " + name;
}

std::string invalid_param(const char *name, const std::string &expected) {
  return std::string("invalid parameter: ") + name + " must be " + expected;
}

std::string lowercase(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c + ('a' - 'A'));
    }
  }
  return text;
}

bool is_digits(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  return true;
}

bool read_required_string(const mcp::JsonValue &args, const char *key,
                          const std::string &action, std::string &out,
                          std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = missing_param(action, key);
    return false;
  }
  if (!value->IsString() || value->GetString().empty()) {
    error = invalid_param(key, "a non-empty string");
    return false;
  }
  out = value->GetString();
  return true;
}

bool read_optional_string(const mcp::JsonValue &args, const char *key,
                          std::string &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    return true;
  }
  if (!value->IsString()) {
    error = invalid_param(key, "a string");
    return false;
  }
  out = value->GetString();
  return true;
}

bool read_required_int(const mcp::JsonValue &args, const char *key,
                       const std::string &action, int64_t &out,
                       std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = missing_param(action, key);
    return false;
  }
  if (!value->IsNumber()) {
    error = invalid_param(key, "a number");
    return false;
  }
  out = value->IsInt() ? value->GetInt()
                       : static_cast<int64_t>(value->GetDouble());
  return true;
}

bool read_int_or(const mcp::JsonValue &args, const char *key, int64_t fallback,
                 int64_t &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    out = fallback;
    return true;
  }
  if (!value->IsNumber()) {
    error = invalid_param(key, "a number");
    return false;
  }
  out = value->IsInt() ? value->GetInt()
                       : static_cast<int64_t>(value->GetDouble());
  return true;
}

bool read_double_or(const mcp::JsonValue &args, const char *key,
                    double fallback, double &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    out = fallback;
    return true;
  }
  if (!value->IsNumber()) {
    error = invalid_param(key, "a number");
    return false;
  }
  out = value->IsInt() ? static_cast<double>(value->GetInt())
                       : value->GetDouble();
  return true;
}

bool read_bool_or(const mcp::JsonValue &args, const char *key, bool fallback,
                  bool &out, std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    out = fallback;
    return true;
  }
  if (!value->IsBool()) {
    error = invalid_param(key, "a boolean");
    return false;
  }
  out = value->GetBool();
  return true;
}

bool check_dimension(int64_t value, const char *name, std::string &error) {
  if (value < 1 || value > kMaxDimension) {
    error = invalid_param(name, "between 1 and 4096");
    return false;
  }
  return true;
}

bool read_color(const mcp::JsonValue &value, const char *name,
                godot::Color &out, std::string &error) {
  if (!value.IsObject()) {
    error = invalid_param(name, "an object {r, g, b, a}");
    return false;
  }
  const char *channels[4] = {"r", "g", "b", "a"};
  for (const auto &entry : value.GetObject()) {
    bool known = false;
    for (const char *channel : channels) {
      if (entry.first == channel) {
        known = true;
        break;
      }
    }
    if (!known) {
      error = std::string("invalid parameter: ") + name + " has unknown field '" +
              entry.first + "' (allowed: r, g, b, a)";
      return false;
    }
  }
  for (const char *channel : channels) {
    const mcp::JsonValue *component = value.Find(channel);
    if (component != nullptr && !component->IsNumber()) {
      error = std::string("invalid parameter: ") + name + "." + channel +
              " must be a number";
      return false;
    }
  }
  out = util::json_to_color(value);
  return true;
}

bool read_optional_color(const mcp::JsonValue &args, const char *key,
                         godot::Color &out, bool &present,
                         std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  present = value != nullptr;
  if (value == nullptr) {
    return true;
  }
  return read_color(*value, key, out, error);
}

mcp::JsonValue color_to_json(const godot::Color &color) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["r"] = mcp::JsonValue(static_cast<double>(color.r));
  j["g"] = mcp::JsonValue(static_cast<double>(color.g));
  j["b"] = mcp::JsonValue(static_cast<double>(color.b));
  j["a"] = mcp::JsonValue(static_cast<double>(color.a));
  return j;
}

mcp::JsonValue rect2i_to_json(const godot::Rect2i &rect) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["x"] = mcp::JsonValue(static_cast<int64_t>(rect.position.x));
  j["y"] = mcp::JsonValue(static_cast<int64_t>(rect.position.y));
  j["width"] = mcp::JsonValue(static_cast<int64_t>(rect.size.x));
  j["height"] = mcp::JsonValue(static_cast<int64_t>(rect.size.y));
  return j;
}

struct FormatName {
  const char *name;
  godot::Image::Format format;
};

const FormatName kFormatNames[] = {
    {"l8", godot::Image::FORMAT_L8},
    {"la8", godot::Image::FORMAT_LA8},
    {"r8", godot::Image::FORMAT_R8},
    {"rg8", godot::Image::FORMAT_RG8},
    {"rgb8", godot::Image::FORMAT_RGB8},
    {"rgba8", godot::Image::FORMAT_RGBA8},
    {"rgba4444", godot::Image::FORMAT_RGBA4444},
    {"rgb565", godot::Image::FORMAT_RGB565},
    {"rf", godot::Image::FORMAT_RF},
    {"rgf", godot::Image::FORMAT_RGF},
    {"rgbf", godot::Image::FORMAT_RGBF},
    {"rgbaf", godot::Image::FORMAT_RGBAF},
    {"rh", godot::Image::FORMAT_RH},
    {"rgh", godot::Image::FORMAT_RGH},
    {"rgbh", godot::Image::FORMAT_RGBH},
    {"rgbah", godot::Image::FORMAT_RGBAH},
    {"rgbe9995", godot::Image::FORMAT_RGBE9995},
    {"dxt1", godot::Image::FORMAT_DXT1},
    {"dxt3", godot::Image::FORMAT_DXT3},
    {"dxt5", godot::Image::FORMAT_DXT5},
    {"rgtc_r", godot::Image::FORMAT_RGTC_R},
    {"rgtc_rg", godot::Image::FORMAT_RGTC_RG},
    {"bptc_rgba", godot::Image::FORMAT_BPTC_RGBA},
    {"bptc_rgbf", godot::Image::FORMAT_BPTC_RGBF},
    {"bptc_rgbfu", godot::Image::FORMAT_BPTC_RGBFU},
    {"etc", godot::Image::FORMAT_ETC},
    {"etc2_r11", godot::Image::FORMAT_ETC2_R11},
    {"etc2_r11s", godot::Image::FORMAT_ETC2_R11S},
    {"etc2_rg11", godot::Image::FORMAT_ETC2_RG11},
    {"etc2_rg11s", godot::Image::FORMAT_ETC2_RG11S},
    {"etc2_rgb8", godot::Image::FORMAT_ETC2_RGB8},
    {"etc2_rgba8", godot::Image::FORMAT_ETC2_RGBA8},
    {"etc2_rgb8a1", godot::Image::FORMAT_ETC2_RGB8A1},
    {"etc2_ra_as_rg", godot::Image::FORMAT_ETC2_RA_AS_RG},
    {"dxt5_ra_as_rg", godot::Image::FORMAT_DXT5_RA_AS_RG},
    {"astc_4x4", godot::Image::FORMAT_ASTC_4x4},
    {"astc_4x4_hdr", godot::Image::FORMAT_ASTC_4x4_HDR},
    {"astc_8x8", godot::Image::FORMAT_ASTC_8x8},
    {"astc_8x8_hdr", godot::Image::FORMAT_ASTC_8x8_HDR},
    {"r16", godot::Image::FORMAT_R16},
    {"rg16", godot::Image::FORMAT_RG16},
    {"rgb16", godot::Image::FORMAT_RGB16},
    {"rgba16", godot::Image::FORMAT_RGBA16},
    {"r16i", godot::Image::FORMAT_R16I},
    {"rg16i", godot::Image::FORMAT_RG16I},
    {"rgb16i", godot::Image::FORMAT_RGB16I},
    {"rgba16i", godot::Image::FORMAT_RGBA16I},
};

std::string format_name_of(godot::Image::Format format) {
  for (const FormatName &entry : kFormatNames) {
    if (entry.format == format) {
      return entry.name;
    }
  }
  return "unknown";
}

bool parse_format(const std::string &name, godot::Image::Format &out) {
  const std::string key = lowercase(name);
  for (const FormatName &entry : kFormatNames) {
    if (key == entry.name) {
      out = entry.format;
      return true;
    }
  }
  return false;
}

struct InterpolationName {
  const char *name;
  godot::Image::Interpolation interpolation;
};

const InterpolationName kInterpolations[] = {
    {"nearest", godot::Image::INTERPOLATE_NEAREST},
    {"bilinear", godot::Image::INTERPOLATE_BILINEAR},
    {"cubic", godot::Image::INTERPOLATE_CUBIC},
    {"trilinear", godot::Image::INTERPOLATE_TRILINEAR},
    {"lanczos", godot::Image::INTERPOLATE_LANCZOS},
};

bool parse_interpolation(const std::string &name,
                         godot::Image::Interpolation &out) {
  const std::string key = lowercase(name);
  for (const InterpolationName &entry : kInterpolations) {
    if (key == entry.name) {
      out = entry.interpolation;
      return true;
    }
  }
  return false;
}

struct CompressModeName {
  const char *name;
  godot::Image::CompressMode mode;
};

const CompressModeName kCompressModes[] = {
    {"s3tc", godot::Image::COMPRESS_S3TC},
    {"etc", godot::Image::COMPRESS_ETC},
    {"etc2", godot::Image::COMPRESS_ETC2},
    {"bptc", godot::Image::COMPRESS_BPTC},
    {"astc", godot::Image::COMPRESS_ASTC},
};

bool parse_compress_mode(const std::string &name,
                         godot::Image::CompressMode &out) {
  const std::string key = lowercase(name);
  for (const CompressModeName &entry : kCompressModes) {
    if (key == entry.name) {
      out = entry.mode;
      return true;
    }
  }
  return false;
}

struct CompressSourceName {
  const char *name;
  godot::Image::CompressSource source;
};

const CompressSourceName kCompressSources[] = {
    {"generic", godot::Image::COMPRESS_SOURCE_GENERIC},
    {"srgb", godot::Image::COMPRESS_SOURCE_SRGB},
    {"normal", godot::Image::COMPRESS_SOURCE_NORMAL},
};

bool parse_compress_source(const std::string &name,
                           godot::Image::CompressSource &out) {
  const std::string key = lowercase(name);
  for (const CompressSourceName &entry : kCompressSources) {
    if (key == entry.name) {
      out = entry.source;
      return true;
    }
  }
  return false;
}

bool parse_astc_format(const std::string &name,
                       godot::Image::ASTCFormat &out) {
  const std::string key = lowercase(name);
  if (key == "4x4") {
    out = godot::Image::ASTC_FORMAT_4x4;
    return true;
  }
  if (key == "8x8") {
    out = godot::Image::ASTC_FORMAT_8x8;
    return true;
  }
  return false;
}

godot::Ref<godot::Image> as_image(const godot::Ref<godot::Resource> &res) {
  if (res.is_null()) {
    return godot::Ref<godot::Image>();
  }
  auto *image = godot::Object::cast_to<godot::Image>(res.ptr());
  if (image == nullptr) {
    return godot::Ref<godot::Image>();
  }
  return godot::Ref<godot::Image>(image);
}

bool load_image_file(const std::string &path, godot::Ref<godot::Image> &out,
                     std::string &normalized, std::string &error) {
  const util::ProjectPath project_path =
      util::normalize_project_path(path, false, false);
  if (!project_path.valid()) {
    error = "invalid image path: " + project_path.error;
    return false;
  }
  normalized = project_path.value;
  out = godot::Image::load_from_file(godot::String(normalized.c_str()));
  if (out.is_null() || out->is_empty()) {
    error = "failed to load image file: " + normalized +
            " (missing file or unsupported format)";
    return false;
  }
  if (out->get_width() > kMaxDimension || out->get_height() > kMaxDimension) {
    error = "image file " + normalized + " is " +
            std::to_string(out->get_width()) + "x" +
            std::to_string(out->get_height()) +
            ", above the 4096x4096 limit";
    out = godot::Ref<godot::Image>();
    return false;
  }
  return true;
}

bool resolve_image_locator(const mcp::JsonValue &value,
                           godot::Ref<godot::Image> &out,
                           std::string &error) {
  std::string name;
  std::string path;
  bool has_object_id = false;
  int64_t object_id = 0;
  std::string other_class;
  std::string descriptor = "locator";
  if (value.IsString()) {
    const std::string text = value.GetString();
    descriptor = text;
    if (text.rfind("memory://", 0) == 0) {
      name = text.substr(9);
    } else if (text.rfind("res://", 0) == 0 ||
               text.rfind("user://", 0) == 0) {
      path = text;
    } else if (is_digits(text)) {
      if (!util::parse_decimal_int64(text, object_id)) {
        error = "invalid image locator: object_id is out of range";
        return false;
      }
      has_object_id = true;
    } else if (!text.empty()) {
      name = text;
    }
  } else if (value.IsObject()) {
    for (const auto &entry : value.GetObject()) {
      if (entry.first != "name" && entry.first != "path" &&
          entry.first != "object_id" && entry.first != "object_id_str") {
        error = "invalid image locator: unknown field '" + entry.first +
                "' (allowed: name, path, object_id, object_id_str)";
        return false;
      }
    }
    const mcp::JsonValue *name_p = value.Find("name");
    if (name_p != nullptr) {
      if (!name_p->IsString()) {
        error = invalid_param("image.name", "a string");
        return false;
      }
      name = name_p->GetString();
    }
    const mcp::JsonValue *path_p = value.Find("path");
    if (path_p != nullptr) {
      if (!path_p->IsString()) {
        error = invalid_param("image.path", "a string");
        return false;
      }
      path = path_p->GetString();
    }
    const mcp::JsonValue *oid_p = value.Find("object_id");
    if (oid_p != nullptr) {
      if (!oid_p->IsNumber()) {
        error = invalid_param("image.object_id", "a number");
        return false;
      }
      has_object_id = true;
      object_id = oid_p->IsInt() ? oid_p->GetInt()
                                 : static_cast<int64_t>(oid_p->GetDouble());
    }
    const mcp::JsonValue *oid_str_p = value.Find("object_id_str");
    if (oid_str_p != nullptr) {
      if (!oid_str_p->IsString() || !is_digits(oid_str_p->GetString())) {
        error = invalid_param("image.object_id_str", "a decimal integer string");
        return false;
      }
      if (!util::parse_decimal_int64(oid_str_p->GetString(), object_id)) {
        error = "invalid image locator: object_id_str is out of range";
        return false;
      }
      has_object_id = true;
    }
  } else {
    error = "invalid image locator: expected a string or object";
    return false;
  }

  if (has_object_id) {
    godot::Object *object =
        godot::ObjectDB::get_instance(static_cast<uint64_t>(object_id));
    if (object != nullptr) {
      auto *image = godot::Object::cast_to<godot::Image>(object);
      if (image != nullptr) {
        out = godot::Ref<godot::Image>(image);
      } else {
        other_class = util::to_std(object->get_class());
      }
    }
  }
  if (out.is_null() && !name.empty()) {
    const godot::Ref<godot::Resource> found =
        resource_registry::lookup_memory(name);
    out = as_image(found);
    if (out.is_null() && found.is_valid()) {
      other_class = util::to_std(found->get_class());
    }
  }
  if (out.is_null() && !path.empty()) {
    out = as_image(resource_registry::lookup_memory(path));
    if (out.is_null()) {
      std::string normalized;
      if (!load_image_file(path, out, normalized, error)) {
        return false;
      }
      resource_registry::register_resource(out, normalized);
    }
  }
  if (out.is_null()) {
    if (!other_class.empty()) {
      error = "resource '" + descriptor + "' is " + other_class +
              ", not an Image — pass an Image handle such as the preview "
              "image returned by texture_gen";
      return false;
    }
    error = "image not found: " + descriptor +
            " — create it with image_edit action='create' or load it with "
            "image_edit action='load'";
    return false;
  }
  return true;
}

bool resolve_required_image(const mcp::JsonValue &args,
                            const std::string &action, const char *key,
                            godot::Ref<godot::Image> &out,
                            std::string &error) {
  const mcp::JsonValue *value = args.Find(key);
  if (value == nullptr) {
    error = missing_param(action, key);
    return false;
  }
  return resolve_image_locator(*value, out, error);
}

void register_named_resource(const godot::Ref<godot::Resource> &res,
                             const std::string &name) {
  if (res.is_null()) {
    return;
  }
  if (!name.empty()) {
    res->set_name(godot::String(name.c_str()));
    res->set_path(godot::String(("memory://" + name).c_str()));
  }
  resource_registry::register_resource(res, name);
}

std::string effective_name(const godot::Ref<godot::Resource> &res,
                           const std::string &name) {
  if (!name.empty()) {
    return name;
  }
  return util::to_std(res->get_name());
}

mcp::JsonValue image_to_json(const godot::Ref<godot::Image> &image,
                             const std::string &name) {
  const std::string effective = effective_name(image, name);
  const int64_t object_id =
      static_cast<int64_t>(image->get_instance_id());
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue("Image");
  j["object_id"] = mcp::JsonValue(object_id);
  j["object_id_str"] = mcp::JsonValue(std::to_string(object_id));
  j["name"] = mcp::JsonValue(effective);
  j["path"] = mcp::JsonValue(util::to_std(image->get_path()));
  j["width"] = mcp::JsonValue(static_cast<int64_t>(image->get_width()));
  j["height"] = mcp::JsonValue(static_cast<int64_t>(image->get_height()));
  j["format"] = mcp::JsonValue(format_name_of(image->get_format()));
  j["image_ref"] = mcp::JsonValue(
      effective.empty() ? std::to_string(object_id) : "memory://" + effective);
  return j;
}

mcp::JsonValue texture_to_json(const godot::Ref<godot::Texture2D> &texture,
                               const std::string &name) {
  const std::string effective = effective_name(texture, name);
  const int64_t object_id =
      static_cast<int64_t>(texture->get_instance_id());
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(util::to_std(texture->get_class()));
  j["object_id"] = mcp::JsonValue(object_id);
  j["object_id_str"] = mcp::JsonValue(std::to_string(object_id));
  j["name"] = mcp::JsonValue(effective);
  j["path"] = mcp::JsonValue(util::to_std(texture->get_path()));
  j["width"] = mcp::JsonValue(static_cast<int64_t>(texture->get_width()));
  j["height"] = mcp::JsonValue(static_cast<int64_t>(texture->get_height()));
  j["texture_ref"] = mcp::JsonValue(
      effective.empty() ? std::to_string(object_id) : "memory://" + effective);
  return j;
}

uint8_t color_channel(float value) {
  if (value <= 0.0f) {
    return 0;
  }
  if (value >= 1.0f) {
    return 255;
  }
  return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

void write_color(uint8_t *target, const godot::Color &color) {
  target[0] = color_channel(color.r);
  target[1] = color_channel(color.g);
  target[2] = color_channel(color.b);
  target[3] = color_channel(color.a);
}

godot::Ref<godot::Image> render_gradient_image(
    const godot::Ref<godot::Gradient> &gradient, int width, int height,
    const std::string &axis) {
  godot::PackedByteArray data;
  data.resize(static_cast<int64_t>(width) * static_cast<int64_t>(height) * 4);
  uint8_t *pixels = data.ptrw();
  if (axis == "vertical") {
    for (int y = 0; y < height; ++y) {
      const float t = height > 1
                          ? static_cast<float>(y) /
                                static_cast<float>(height - 1)
                          : 0.0f;
      const godot::Color color = gradient->sample(t);
      uint8_t rgba[4];
      write_color(rgba, color);
      for (int x = 0; x < width; ++x) {
        std::memcpy(pixels + (static_cast<int64_t>(y) * width + x) * 4, rgba,
                    4);
      }
    }
  } else {
    for (int x = 0; x < width; ++x) {
      const float t = width > 1
                          ? static_cast<float>(x) /
                                static_cast<float>(width - 1)
                          : 0.0f;
      const godot::Color color = gradient->sample(t);
      uint8_t rgba[4];
      write_color(rgba, color);
      for (int y = 0; y < height; ++y) {
        std::memcpy(pixels + (static_cast<int64_t>(y) * width + x) * 4, rgba,
                    4);
      }
    }
  }
  return godot::Image::create_from_data(width, height, false,
                                        godot::Image::FORMAT_RGBA8, data);
}

float apply_repeat_mode(float t, const std::string &repeat) {
  if (repeat == "repeat") {
    return t - std::floor(t);
  }
  if (repeat == "mirror") {
    float value = std::fmod(t, 2.0f);
    if (value < 0.0f) {
      value += 2.0f;
    }
    return value > 1.0f ? 2.0f - value : value;
  }
  return std::min(1.0f, std::max(0.0f, t));
}

godot::Ref<godot::Image> apply_color_ramp(
    const godot::Ref<godot::Image> &image,
    const godot::Ref<godot::Gradient> &ramp) {
  if (ramp.is_null() || image.is_null()) {
    return image;
  }
  godot::Ref<godot::Image> mapped = image->duplicate();
  if (mapped.is_null()) {
    return image;
  }
  if (mapped->get_format() != godot::Image::FORMAT_RGBA8) {
    mapped->convert(godot::Image::FORMAT_RGBA8);
  }
  for (int y = 0; y < mapped->get_height(); ++y) {
    for (int x = 0; x < mapped->get_width(); ++x) {
      const godot::Color source = mapped->get_pixel(x, y);
      mapped->set_pixel(x, y, ramp->sample(source.r));
    }
  }
  return mapped;
}

godot::Ref<godot::Image> render_fill_image(
    const godot::Ref<godot::Gradient> &gradient, int width, int height,
    const std::string &fill, const godot::Vector2 &from,
    const godot::Vector2 &to, const std::string &repeat) {
  godot::PackedByteArray data;
  data.resize(static_cast<int64_t>(width) * static_cast<int64_t>(height) * 4);
  uint8_t *pixels = data.ptrw();
  const godot::Vector2 direction = to - from;
  const float direction_length_sq = direction.length_squared();
  const float direction_length = std::sqrt(direction_length_sq);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const godot::Vector2 uv(static_cast<float>(x) / static_cast<float>(width),
                              static_cast<float>(y) /
                                  static_cast<float>(height));
      const godot::Vector2 relative = uv - from;
      float t = 0.0f;
      if (fill == "radial") {
        t = direction_length > 0.0f ? relative.length() / direction_length
                                     : 0.0f;
      } else if (fill == "square") {
        float ratio_x = direction.x != 0.0f ? relative.x / direction.x : 0.0f;
        float ratio_y = direction.y != 0.0f ? relative.y / direction.y : 0.0f;
        t = std::max(std::fabs(ratio_x), std::fabs(ratio_y));
      } else if (fill == "conic") {
        const float dot = relative.dot(direction);
        const float cross = direction.x * relative.y - direction.y * relative.x;
        const float angle = std::atan2(cross, dot);
        t = (angle + static_cast<float>(3.14159265358979323846)) /
            static_cast<float>(6.28318530717958647692);
      } else {
        t = direction_length_sq > 0.0f ? relative.dot(direction) /
                                             direction_length_sq
                                       : 0.0f;
      }
      const godot::Color color = gradient->sample(apply_repeat_mode(t, repeat));
      write_color(pixels + (static_cast<int64_t>(y) * width + x) * 4, color);
    }
  }
  return godot::Image::create_from_data(width, height, false,
                                        godot::Image::FORMAT_RGBA8, data);
}

struct NoiseTypeName {
  const char *name;
  godot::FastNoiseLite::NoiseType type;
};

const NoiseTypeName kNoiseTypes[] = {
    {"simplex", godot::FastNoiseLite::TYPE_SIMPLEX},
    {"simplex_smooth", godot::FastNoiseLite::TYPE_SIMPLEX_SMOOTH},
    {"cellular", godot::FastNoiseLite::TYPE_CELLULAR},
    {"perlin", godot::FastNoiseLite::TYPE_PERLIN},
    {"value_cubic", godot::FastNoiseLite::TYPE_VALUE_CUBIC},
    {"value", godot::FastNoiseLite::TYPE_VALUE},
};

struct FractalTypeName {
  const char *name;
  godot::FastNoiseLite::FractalType type;
};

const FractalTypeName kFractalTypes[] = {
    {"none", godot::FastNoiseLite::FRACTAL_NONE},
    {"fbm", godot::FastNoiseLite::FRACTAL_FBM},
    {"ridged", godot::FastNoiseLite::FRACTAL_RIDGED},
    {"ping_pong", godot::FastNoiseLite::FRACTAL_PING_PONG},
};

std::string noise_type_name_of(godot::FastNoiseLite::NoiseType type) {
  for (const NoiseTypeName &entry : kNoiseTypes) {
    if (entry.type == type) {
      return entry.name;
    }
  }
  return "unknown";
}

std::string fractal_type_name_of(godot::FastNoiseLite::FractalType type) {
  for (const FractalTypeName &entry : kFractalTypes) {
    if (entry.type == type) {
      return entry.name;
    }
  }
  return "unknown";
}

bool build_noise(const mcp::JsonValue &args,
                 godot::Ref<godot::FastNoiseLite> &out, std::string &error) {
  out.instantiate();
  int64_t seed = 0;
  if (!read_int_or(args, "seed", 0, seed, error)) {
    return false;
  }
  out->set_seed(static_cast<int32_t>(seed));
  double frequency = 0.0;
  if (!read_double_or(args, "frequency", 0.0, frequency, error)) {
    return false;
  }
  if (args.Find("frequency") != nullptr) {
    out->set_frequency(static_cast<float>(frequency));
  }
  std::string noise_type;
  if (!read_optional_string(args, "noise_type", noise_type, error)) {
    return false;
  }
  if (!noise_type.empty()) {
    const std::string key = lowercase(noise_type);
    bool found = false;
    for (const NoiseTypeName &entry : kNoiseTypes) {
      if (key == entry.name) {
        out->set_noise_type(entry.type);
        found = true;
        break;
      }
    }
    if (!found) {
      error =
          "invalid parameter: noise_type must be simplex, simplex_smooth, "
          "cellular, perlin, value_cubic or value";
      return false;
    }
  }
  std::string fractal_type;
  if (!read_optional_string(args, "fractal_type", fractal_type, error)) {
    return false;
  }
  if (!fractal_type.empty()) {
    const std::string key = lowercase(fractal_type);
    bool found = false;
    for (const FractalTypeName &entry : kFractalTypes) {
      if (key == entry.name) {
        out->set_fractal_type(entry.type);
        found = true;
        break;
      }
    }
    if (!found) {
      error =
          "invalid parameter: fractal_type must be none, fbm, ridged or "
          "ping_pong";
      return false;
    }
  }
  int64_t octaves = 0;
  if (!read_int_or(args, "fractal_octaves", 0, octaves, error)) {
    return false;
  }
  if (args.Find("fractal_octaves") != nullptr) {
    if (octaves < 1) {
      error = invalid_param("fractal_octaves", "at least 1");
      return false;
    }
    out->set_fractal_octaves(static_cast<int32_t>(octaves));
  }
  return true;
}

bool build_gradient(const mcp::JsonValue &args, bool default_preset,
                    godot::Ref<godot::Gradient> &out, std::string &error) {
  const mcp::JsonValue *points = args.Find("gradient");
  if (points == nullptr && !default_preset) {
    out = godot::Ref<godot::Gradient>();
    return true;
  }
  out.instantiate();
  if (points == nullptr) {
    out->add_point(0.0f, godot::Color(0.0f, 0.0f, 0.0f, 1.0f));
    out->add_point(1.0f, godot::Color(1.0f, 1.0f, 1.0f, 1.0f));
  } else {
    if (!points->IsArray() || points->GetArray().empty()) {
      error = invalid_param(
          "gradient", "a non-empty array of {offset, color} objects");
      return false;
    }
    for (const mcp::JsonValue &entry : points->GetArray()) {
      if (!entry.IsObject()) {
        error = invalid_param("gradient entry", "an object {offset, color}");
        return false;
      }
      for (const auto &field : entry.GetObject()) {
        if (field.first != "offset" && field.first != "color") {
          error = "invalid parameter: gradient entry has unknown field '" +
                  field.first + "' (allowed: offset, color)";
          return false;
        }
      }
      const mcp::JsonValue *offset = entry.Find("offset");
      if (offset == nullptr || !offset->IsNumber()) {
        error = invalid_param("gradient entry.offset", "a number in 0..1");
        return false;
      }
      const double offset_value = offset->IsInt()
                                      ? static_cast<double>(offset->GetInt())
                                      : offset->GetDouble();
      if (offset_value < 0.0 || offset_value > 1.0) {
        error = invalid_param("gradient entry.offset", "a number in 0..1");
        return false;
      }
      const mcp::JsonValue *color = entry.Find("color");
      if (color == nullptr) {
        error = "missing required parameter for gradient entry: color";
        return false;
      }
      godot::Color value;
      if (!read_color(*color, "gradient entry.color", value, error)) {
        return false;
      }
      out->add_point(static_cast<float>(offset_value), value);
    }
  }
  std::string interpolation;
  if (!read_optional_string(args, "interpolation", interpolation, error)) {
    return false;
  }
  if (!interpolation.empty()) {
    const std::string key = lowercase(interpolation);
    if (key == "linear") {
      out->set_interpolation_mode(godot::Gradient::GRADIENT_INTERPOLATE_LINEAR);
    } else if (key == "constant") {
      out->set_interpolation_mode(
          godot::Gradient::GRADIENT_INTERPOLATE_CONSTANT);
    } else if (key == "cubic") {
      out->set_interpolation_mode(godot::Gradient::GRADIENT_INTERPOLATE_CUBIC);
    } else {
      error = "invalid parameter: interpolation must be linear, constant or "
              "cubic";
      return false;
    }
  }
  return true;
}

godot::Ref<godot::Image> render_noise_image(
    const godot::Ref<godot::FastNoiseLite> &noise, int width, int height,
    bool seamless, bool invert, bool normalize, float skirt) {
  if (seamless) {
    return noise->get_seamless_image(width, height, invert, false, skirt,
                                     normalize);
  }
  return noise->get_image(width, height, invert, false, normalize);
}

bool resolve_generation_size(const mcp::JsonValue &args, int &width,
                             int &height, std::string &error) {
  int64_t width_value = kDefaultDimension;
  int64_t height_value = kDefaultDimension;
  if (!read_int_or(args, "width", kDefaultDimension, width_value, error)) {
    return false;
  }
  if (!check_dimension(width_value, "width", error)) {
    return false;
  }
  if (!read_int_or(args, "height", kDefaultDimension, height_value, error)) {
    return false;
  }
  if (!check_dimension(height_value, "height", error)) {
    return false;
  }
  width = static_cast<int>(width_value);
  height = static_cast<int>(height_value);
  return true;
}

mcp::JsonValue handle_noise_image(const mcp::JsonValue &args,
                                  bool as_texture) {
  std::string error;
  int width = kDefaultDimension;
  int height = kDefaultDimension;
  if (!resolve_generation_size(args, width, height, error)) {
    return make_error(error);
  }
  godot::Ref<godot::FastNoiseLite> noise;
  if (!build_noise(args, noise, error)) {
    return make_error(error);
  }
  bool seamless = false;
  bool normalize = true;
  bool invert = false;
  if (!read_bool_or(args, "seamless", false, seamless, error)) {
    return make_error(error);
  }
  if (!read_bool_or(args, "normalize", true, normalize, error)) {
    return make_error(error);
  }
  if (!read_bool_or(args, "invert", false, invert, error)) {
    return make_error(error);
  }
  double skirt = 0.1;
  if (!read_double_or(args, "skirt", 0.1, skirt, error)) {
    return make_error(error);
  }
  std::string name;
  if (!read_optional_string(args, "name", name, error)) {
    return make_error(error);
  }
  std::string image_name;
  if (!read_optional_string(args, "image_name", image_name, error)) {
    return make_error(error);
  }
  if (!name.empty() && image_name.empty()) {
    image_name = name + "_image";
  }
  godot::Ref<godot::Gradient> color_ramp;
  if (!build_gradient(args, false, color_ramp, error)) {
    return make_error(error);
  }
  godot::Ref<godot::Image> image =
      render_noise_image(noise, width, height, seamless, invert, normalize,
                         static_cast<float>(skirt));
  if (image.is_null() || image->is_empty()) {
    return make_error("failed to render noise image at " +
                      std::to_string(width) + "x" + std::to_string(height));
  }
  if (as_texture) {
    image = apply_color_ramp(image, color_ramp);
  }
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["mode"] = mcp::JsonValue(as_texture ? "noise_texture" : "noise");
  result["width"] = mcp::JsonValue(width);
  result["height"] = mcp::JsonValue(height);
  result["seed"] = mcp::JsonValue(static_cast<int64_t>(noise->get_seed()));
  result["frequency"] =
      mcp::JsonValue(static_cast<double>(noise->get_frequency()));
  result["noise_type"] =
      mcp::JsonValue(noise_type_name_of(noise->get_noise_type()));
  result["fractal_type"] =
      mcp::JsonValue(fractal_type_name_of(noise->get_fractal_type()));
  result["seamless"] = mcp::JsonValue(seamless);
  result["normalize"] = mcp::JsonValue(normalize);
  result["invert"] = mcp::JsonValue(invert);
  result["skirt"] = mcp::JsonValue(skirt);
  if (as_texture) {
    godot::Ref<godot::NoiseTexture2D> texture;
    texture.instantiate();
    texture->set_width(width);
    texture->set_height(height);
    texture->set_noise(noise);
    if (color_ramp.is_valid()) {
      texture->set_color_ramp(color_ramp);
    }
    texture->set_seamless(seamless);
    texture->set_normalize(normalize);
    texture->set_invert(invert);
    if (seamless) {
      texture->set_seamless_blend_skirt(static_cast<float>(skirt));
    }
    register_named_resource(texture, name);
    register_named_resource(image, image_name);
    result["texture"] = texture_to_json(texture, name);
    result["image"] = image_to_json(image, image_name);
  } else {
    register_named_resource(image, name);
    result["image"] = image_to_json(image, name);
  }
  return make_result(std::move(result));
}

mcp::JsonValue handle_gradient_image(const mcp::JsonValue &args) {
  std::string error;
  int width = kDefaultDimension;
  int height = kDefaultDimension;
  if (!resolve_generation_size(args, width, height, error)) {
    return make_error(error);
  }
  godot::Ref<godot::Gradient> gradient;
  if (!build_gradient(args, true, gradient, error)) {
    return make_error(error);
  }
  std::string axis = "horizontal";
  if (!read_optional_string(args, "axis", axis, error)) {
    return make_error(error);
  }
  axis = lowercase(axis);
  if (axis != "horizontal" && axis != "vertical") {
    return make_error(
        "invalid parameter: axis must be horizontal or vertical");
  }
  std::string name;
  if (!read_optional_string(args, "name", name, error)) {
    return make_error(error);
  }
  godot::Ref<godot::Image> image =
      render_gradient_image(gradient, width, height, axis);
  if (image.is_null() || image->is_empty()) {
    return make_error("failed to render gradient image at " +
                      std::to_string(width) + "x" + std::to_string(height));
  }
  register_named_resource(image, name);
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["mode"] = mcp::JsonValue("gradient");
  result["axis"] = mcp::JsonValue(axis);
  result["width"] = mcp::JsonValue(width);
  result["height"] = mcp::JsonValue(height);
  result["image"] = image_to_json(image, name);
  return make_result(std::move(result));
}

mcp::JsonValue handle_gradient_texture(const mcp::JsonValue &args) {
  std::string error;
  int width = kDefaultDimension;
  int height = kDefaultDimension;
  if (!resolve_generation_size(args, width, height, error)) {
    return make_error(error);
  }
  godot::Ref<godot::Gradient> gradient;
  if (!build_gradient(args, true, gradient, error)) {
    return make_error(error);
  }
  std::string dimension = "2d";
  if (!read_optional_string(args, "dimension", dimension, error)) {
    return make_error(error);
  }
  dimension = lowercase(dimension);
  if (dimension != "1d" && dimension != "2d") {
    return make_error("invalid parameter: dimension must be 1d or 2d");
  }
  std::string name;
  if (!read_optional_string(args, "name", name, error)) {
    return make_error(error);
  }
  std::string image_name;
  if (!read_optional_string(args, "image_name", image_name, error)) {
    return make_error(error);
  }
  if (!name.empty() && image_name.empty()) {
    image_name = name + "_image";
  }
  godot::Ref<godot::Image> image;
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["mode"] = mcp::JsonValue("gradient_texture");
  result["dimension"] = mcp::JsonValue(dimension);
  result["width"] = mcp::JsonValue(width);
  if (dimension == "1d") {
    godot::Ref<godot::GradientTexture1D> texture;
    texture.instantiate();
    texture->set_gradient(gradient);
    texture->set_width(width);
    register_named_resource(texture, name);
    image = render_gradient_image(gradient, width, 1, "horizontal");
    result["height"] = mcp::JsonValue(1);
    result["texture"] = texture_to_json(texture, name);
  } else {
    std::string fill = "linear";
    std::string repeat = "none";
    if (!read_optional_string(args, "fill", fill, error)) {
      return make_error(error);
    }
    if (!read_optional_string(args, "repeat", repeat, error)) {
      return make_error(error);
    }
    fill = lowercase(fill);
    repeat = lowercase(repeat);
    if (fill != "linear" && fill != "radial" && fill != "square" &&
        fill != "conic") {
      return make_error(
          "invalid parameter: fill must be linear, radial, square or conic");
    }
    if (repeat != "none" && repeat != "repeat" && repeat != "mirror") {
      return make_error(
          "invalid parameter: repeat must be none, repeat or mirror");
    }
    godot::Vector2 fill_from(0.0f, 0.0f);
    godot::Vector2 fill_to(1.0f, 0.0f);
    const mcp::JsonValue *from_p = args.Find("fill_from");
    if (from_p != nullptr) {
      if (!from_p->IsObject()) {
        error = invalid_param("fill_from", "an object {x, y}");
        return make_error(error);
      }
      fill_from = util::json_to_vec2(*from_p);
    }
    const mcp::JsonValue *to_p = args.Find("fill_to");
    if (to_p != nullptr) {
      if (!to_p->IsObject()) {
        error = invalid_param("fill_to", "an object {x, y}");
        return make_error(error);
      }
      fill_to = util::json_to_vec2(*to_p);
    }
    godot::Ref<godot::GradientTexture2D> texture;
    texture.instantiate();
    texture->set_gradient(gradient);
    texture->set_width(width);
    texture->set_height(height);
    if (fill == "radial") {
      texture->set_fill(godot::GradientTexture2D::FILL_RADIAL);
    } else if (fill == "square") {
      texture->set_fill(godot::GradientTexture2D::FILL_SQUARE);
    } else if (fill == "conic") {
      texture->set_fill(godot::GradientTexture2D::FILL_CONIC);
    } else {
      texture->set_fill(godot::GradientTexture2D::FILL_LINEAR);
    }
    if (repeat == "repeat") {
      texture->set_repeat(godot::GradientTexture2D::REPEAT);
    } else if (repeat == "mirror") {
      texture->set_repeat(godot::GradientTexture2D::REPEAT_MIRROR);
    } else {
      texture->set_repeat(godot::GradientTexture2D::REPEAT_NONE);
    }
    texture->set_fill_from(fill_from);
    texture->set_fill_to(fill_to);
    register_named_resource(texture, name);
    image = render_fill_image(gradient, width, height, fill, fill_from,
                              fill_to, repeat);
    result["height"] = mcp::JsonValue(height);
    result["fill"] = mcp::JsonValue(fill);
    result["repeat"] = mcp::JsonValue(repeat);
    result["texture"] = texture_to_json(texture, name);
  }
  if (image.is_null() || image->is_empty()) {
    return make_error("failed to render gradient preview image");
  }
  register_named_resource(image, image_name);
  result["image"] = image_to_json(image, image_name);
  return make_result(std::move(result));
}

} // namespace

mcp::JsonValue handle_edit_action(const mcp::JsonValue &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return make_error("missing required parameter: action");
  }
  std::string error;
  if (action == "create") {
    int64_t width = 0;
    int64_t height = 0;
    if (!read_required_int(args, "width", action, width, error) ||
        !read_required_int(args, "height", action, height, error) ||
        !check_dimension(width, "width", error) ||
        !check_dimension(height, "height", error)) {
      return make_error(error);
    }
    godot::Image::Format format = godot::Image::FORMAT_RGBA8;
    std::string format_text;
    if (!read_optional_string(args, "format", format_text, error)) {
      return make_error(error);
    }
    if (!format_text.empty()) {
      if (!parse_format(format_text, format)) {
        return make_error("invalid parameter: format '" + format_text +
                          "' is not a known image format");
      }
      if (format >= godot::Image::FORMAT_DXT1 &&
          format <= godot::Image::FORMAT_ASTC_8x8_HDR) {
        return make_error(
            "invalid parameter: format must be an uncompressed format");
      }
    }
    godot::Ref<godot::Image> image = godot::Image::create(
        static_cast<int32_t>(width), static_cast<int32_t>(height), false,
        format);
    if (image.is_null() || image->is_empty()) {
      return make_error("failed to create image at " + std::to_string(width) +
                        "x" + std::to_string(height));
    }
    godot::Color color;
    bool has_color = false;
    if (!read_optional_color(args, "color", color, has_color, error)) {
      return make_error(error);
    }
    if (has_color) {
      image->fill(color);
    }
    std::string name;
    if (!read_optional_string(args, "name", name, error)) {
      return make_error(error);
    }
    register_named_resource(image, name);
    return make_result(image_to_json(image, name));
  }
  if (action == "load") {
    std::string path;
    if (!read_required_string(args, "path", action, path, error)) {
      return make_error(error);
    }
    godot::Ref<godot::Image> image;
    std::string normalized;
    if (!load_image_file(path, image, normalized, error)) {
      return make_error(error);
    }
    std::string name;
    if (!read_optional_string(args, "name", name, error)) {
      return make_error(error);
    }
    register_named_resource(image, name);
    resource_registry::register_resource(image, normalized);
    mcp::JsonValue result = image_to_json(image, name);
    result["source_path"] = mcp::JsonValue(normalized);
    return make_result(std::move(result));
  }
  if (action == "info") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    mcp::JsonValue result = image_to_json(image, "");
    result["data_size"] =
        mcp::JsonValue(static_cast<int64_t>(image->get_data_size()));
    result["is_empty"] = mcp::JsonValue(image->is_empty());
    result["is_compressed"] = mcp::JsonValue(image->is_compressed());
    result["has_mipmaps"] = mcp::JsonValue(image->has_mipmaps());
    result["used_rect"] = rect2i_to_json(image->get_used_rect());
    return make_result(std::move(result));
  }
  if (action == "resize") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    int64_t width = 0;
    int64_t height = 0;
    if (!read_required_int(args, "width", action, width, error) ||
        !read_required_int(args, "height", action, height, error) ||
        !check_dimension(width, "width", error) ||
        !check_dimension(height, "height", error)) {
      return make_error(error);
    }
    godot::Image::Interpolation interpolation =
        godot::Image::INTERPOLATE_BILINEAR;
    std::string interpolation_text;
    if (!read_optional_string(args, "interpolation", interpolation_text,
                              error)) {
      return make_error(error);
    }
    if (!interpolation_text.empty() &&
        !parse_interpolation(interpolation_text, interpolation)) {
      return make_error("invalid parameter: interpolation must be nearest, "
                        "bilinear, cubic, trilinear or lanczos");
    }
    image->resize(static_cast<int32_t>(width), static_cast<int32_t>(height),
                  interpolation);
    return make_result(image_to_json(image, ""));
  }
  if (action == "crop") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    int64_t width = 0;
    int64_t height = 0;
    if (!read_required_int(args, "width", action, width, error) ||
        !read_required_int(args, "height", action, height, error)) {
      return make_error(error);
    }
    int64_t x = 0;
    int64_t y = 0;
    if (!read_int_or(args, "x", 0, x, error) ||
        !read_int_or(args, "y", 0, y, error)) {
      return make_error(error);
    }
    if (x < 0 || y < 0 || width < 1 || height < 1 ||
        x + width > image->get_width() || y + height > image->get_height()) {
      return make_error(
          "invalid parameter: crop region is outside the image bounds");
    }
    if (x == 0 && y == 0) {
      image->crop(static_cast<int32_t>(width), static_cast<int32_t>(height));
    } else {
      godot::Ref<godot::Image> region = image->get_region(
          godot::Rect2i(static_cast<int32_t>(x), static_cast<int32_t>(y),
                        static_cast<int32_t>(width),
                        static_cast<int32_t>(height)));
      if (region.is_null() || region->is_empty()) {
        return make_error("failed to crop image region");
      }
      image->copy_from(region);
    }
    return make_result(image_to_json(image, ""));
  }
  if (action == "flip") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    std::string axis = "y";
    if (!read_optional_string(args, "axis", axis, error)) {
      return make_error(error);
    }
    axis = lowercase(axis);
    if (axis == "x") {
      image->flip_x();
    } else if (axis == "y") {
      image->flip_y();
    } else if (axis == "both") {
      image->flip_x();
      image->flip_y();
    } else {
      return make_error("invalid parameter: axis must be x, y or both");
    }
    return make_result(image_to_json(image, ""));
  }
  if (action == "rotate") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    int64_t degrees = 0;
    if (!read_required_int(args, "degrees", action, degrees, error)) {
      return make_error(error);
    }
    if (degrees != 90 && degrees != 180 && degrees != 270) {
      return make_error("invalid parameter: degrees must be 90, 180 or 270");
    }
    std::string direction = "clockwise";
    if (!read_optional_string(args, "direction", direction, error)) {
      return make_error(error);
    }
    direction = lowercase(direction);
    if (direction != "clockwise" && direction != "counterclockwise") {
      return make_error(
          "invalid parameter: direction must be clockwise or counterclockwise");
    }
    const int clockwise = direction == "clockwise" ? 0 : 1;
    if (degrees == 90) {
      image->rotate_90(static_cast<godot::ClockDirection>(clockwise));
    } else if (degrees == 180) {
      image->rotate_180();
    } else {
      image->rotate_90(static_cast<godot::ClockDirection>(1 - clockwise));
    }
    return make_result(image_to_json(image, ""));
  }
  if (action == "fill") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    const mcp::JsonValue *color_p = args.Find("color");
    if (color_p == nullptr) {
      return make_error(missing_param(action, "color"));
    }
    godot::Color color;
    if (!read_color(*color_p, "color", color, error)) {
      return make_error(error);
    }
    image->fill(color);
    return make_result(image_to_json(image, ""));
  }
  return make_error("unknown action: " + action);
}

mcp::JsonValue handle_pixel_action(const mcp::JsonValue &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return make_error("missing required parameter: action");
  }
  std::string error;
  if (action == "blit" || action == "blend") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    godot::Ref<godot::Image> source;
    if (!resolve_required_image(args, action, "src", source, error)) {
      return make_error(error);
    }
    int64_t x = 0;
    int64_t y = 0;
    int64_t src_x = 0;
    int64_t src_y = 0;
    int64_t src_width = source->get_width();
    int64_t src_height = source->get_height();
    if (!read_int_or(args, "x", 0, x, error) ||
        !read_int_or(args, "y", 0, y, error) ||
        !read_int_or(args, "src_x", 0, src_x, error) ||
        !read_int_or(args, "src_y", 0, src_y, error) ||
        !read_int_or(args, "src_width", src_width, src_width, error) ||
        !read_int_or(args, "src_height", src_height, src_height, error)) {
      return make_error(error);
    }
    if (src_x < 0 || src_y < 0 || src_width < 1 || src_height < 1 ||
        src_x + src_width > source->get_width() ||
        src_y + src_height > source->get_height()) {
      return make_error(
          "invalid parameter: source region is outside the source image "
          "bounds");
    }
    godot::Ref<godot::Image> prepared = source;
    if (source->get_format() != image->get_format()) {
      prepared = source->duplicate();
      if (prepared.is_null()) {
        return make_error("failed to duplicate source image for format "
                          "conversion");
      }
      prepared->convert(image->get_format());
    }
    const godot::Rect2i src_rect(static_cast<int32_t>(src_x),
                                 static_cast<int32_t>(src_y),
                                 static_cast<int32_t>(src_width),
                                 static_cast<int32_t>(src_height));
    const godot::Vector2i dest(static_cast<int32_t>(x),
                               static_cast<int32_t>(y));
    if (action == "blend") {
      image->blend_rect(prepared, src_rect, dest);
    } else {
      image->blit_rect(prepared, src_rect, dest);
    }
    mcp::JsonValue result = image_to_json(image, "");
    result["operation"] = mcp::JsonValue(action);
    result["src_rect"] = rect2i_to_json(src_rect);
    result["dest"] = mcp::JsonValue(
        mcp::JsonValue::Object{{"x", mcp::JsonValue(static_cast<int64_t>(x))},
                               {"y", mcp::JsonValue(static_cast<int64_t>(y))}});
    return make_result(std::move(result));
  }
  if (action == "get_pixel" || action == "set_pixel") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    int64_t x = 0;
    int64_t y = 0;
    if (!read_required_int(args, "x", action, x, error) ||
        !read_required_int(args, "y", action, y, error)) {
      return make_error(error);
    }
    if (x < 0 || y < 0 || x >= image->get_width() ||
        y >= image->get_height()) {
      return make_error("pixel out of bounds: (" + std::to_string(x) + ", " +
                        std::to_string(y) + ") outside " +
                        std::to_string(image->get_width()) + "x" +
                        std::to_string(image->get_height()));
    }
    if (action == "get_pixel") {
      const godot::Color color = image->get_pixel(static_cast<int32_t>(x),
                                                  static_cast<int32_t>(y));
      mcp::JsonValue result(mcp::JsonValue::object_tag);
      result["x"] = mcp::JsonValue(x);
      result["y"] = mcp::JsonValue(y);
      result["color"] = color_to_json(color);
      return make_result(std::move(result));
    }
    const mcp::JsonValue *color_p = args.Find("color");
    if (color_p == nullptr) {
      return make_error(missing_param(action, "color"));
    }
    godot::Color color;
    if (!read_color(*color_p, "color", color, error)) {
      return make_error(error);
    }
    image->set_pixel(static_cast<int32_t>(x), static_cast<int32_t>(y), color);
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    result["x"] = mcp::JsonValue(x);
    result["y"] = mcp::JsonValue(y);
    result["color"] = color_to_json(color);
    return make_result(std::move(result));
  }
  if (action == "compress") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    std::string mode_text = "s3tc";
    std::string source_text = "generic";
    std::string astc_text = "4x4";
    if (!read_optional_string(args, "mode", mode_text, error) ||
        !read_optional_string(args, "source", source_text, error) ||
        !read_optional_string(args, "astc_format", astc_text, error)) {
      return make_error(error);
    }
    godot::Image::CompressMode mode = godot::Image::COMPRESS_S3TC;
    if (!parse_compress_mode(mode_text, mode)) {
      return make_error(
          "invalid parameter: mode must be s3tc, etc, etc2, bptc or astc");
    }
    godot::Image::CompressSource source =
        godot::Image::COMPRESS_SOURCE_GENERIC;
    if (!parse_compress_source(source_text, source)) {
      return make_error(
          "invalid parameter: source must be generic, srgb or normal");
    }
    godot::Image::ASTCFormat astc_format = godot::Image::ASTC_FORMAT_4x4;
    if (!parse_astc_format(astc_text, astc_format)) {
      return make_error("invalid parameter: astc_format must be 4x4 or 8x8");
    }
    const godot::Error save_error = image->compress(mode, source, astc_format);
    if (save_error != godot::OK) {
      return make_error("compress failed with error code " +
                        std::to_string(static_cast<int>(save_error)) +
                        " (the image may already be compressed)");
    }
    mcp::JsonValue result = image_to_json(image, "");
    result["compressed"] = mcp::JsonValue(true);
    return make_result(std::move(result));
  }
  if (action == "decompress") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    const godot::Error decompress_error = image->decompress();
    if (decompress_error != godot::OK) {
      return make_error("decompress failed with error code " +
                        std::to_string(static_cast<int>(decompress_error)) +
                        " (the image may not be compressed)");
    }
    mcp::JsonValue result = image_to_json(image, "");
    result["compressed"] = mcp::JsonValue(false);
    return make_result(std::move(result));
  }
  if (action == "to_texture") {
    godot::Ref<godot::Image> image;
    if (!resolve_required_image(args, action, "image", image, error)) {
      return make_error(error);
    }
    std::string name;
    if (!read_optional_string(args, "name", name, error)) {
      return make_error(error);
    }
    godot::Ref<godot::ImageTexture> texture =
        godot::ImageTexture::create_from_image(image);
    if (texture.is_null()) {
      return make_error("failed to create ImageTexture from image");
    }
    register_named_resource(texture, name);
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    result["texture"] = texture_to_json(texture, name);
    result["image"] = image_to_json(image, "");
    return make_result(std::move(result));
  }
  return make_error("unknown action: " + action);
}

mcp::JsonValue handle_save_action(const mcp::JsonValue &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return make_error("missing required parameter: action");
  }
  if (action != "save") {
    return make_error("unknown action: " + action);
  }
  std::string error;
  godot::Ref<godot::Image> image;
  if (!resolve_required_image(args, action, "image", image, error)) {
    return make_error(error);
  }
  std::string path;
  if (!read_required_string(args, "path", action, path, error)) {
    return make_error(error);
  }
  const util::ProjectPath project_path =
      util::normalize_project_path(path, false, false);
  if (!project_path.valid()) {
    return make_error("invalid path: " + project_path.error);
  }
  if (lowercase(project_path.value).size() < 4 ||
      lowercase(project_path.value).substr(lowercase(project_path.value).size() -
                                            4) != ".png") {
    return make_error("invalid parameter: path must end with .png");
  }
  std::string dir_created;
  std::string dir_error;
  if (!resource_fs::ensure_parent_directory(project_path.value, dir_created,
                                            dir_error)) {
    return make_error(dir_error);
  }
  const godot::String target(project_path.value.c_str());
  const godot::Error save_error = image->save_png(target);
  if (save_error != godot::OK) {
    return make_error("save_png failed with error code " +
                      std::to_string(static_cast<int>(save_error)) +
                      " for " + project_path.value);
  }
  const bool verified = godot::FileAccess::file_exists(target);
  int64_t bytes = 0;
  if (verified) {
    const godot::PackedByteArray data =
        godot::FileAccess::get_file_as_bytes(target);
    bytes = data.size();
  }
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["path"] = mcp::JsonValue(project_path.value);
  result["bytes"] = mcp::JsonValue(bytes);
  result["width"] = mcp::JsonValue(static_cast<int64_t>(image->get_width()));
  result["height"] = mcp::JsonValue(static_cast<int64_t>(image->get_height()));
  result["verified"] = mcp::JsonValue(verified);
  if (!dir_created.empty()) {
    result["directories_created"] = mcp::JsonValue(dir_created);
  }
  return make_result(std::move(result));
}

mcp::JsonValue handle_texture_gen_action(const mcp::JsonValue &args) {
  const std::string action = action_of(args);
  if (action.empty()) {
    return make_error("missing required parameter: action");
  }
  if (action == "noise" || action == "noise_texture") {
    return handle_noise_image(args, action == "noise_texture");
  }
  if (action == "gradient") {
    return handle_gradient_image(args);
  }
  if (action == "gradient_texture") {
    return handle_gradient_texture(args);
  }
  return make_error("unknown action: " + action);
}

} // namespace image_ops
} // namespace godot_autopilot
