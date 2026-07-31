#include "capture_ops.hpp"
#include "core/log_system.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <string>

namespace godot_self_driving {
namespace capture_ops {

namespace {

mcp::JsonValue error_response(const std::string& msg) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(msg);
    return e;
}

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string base64_encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        int n = (i + 2 < len) ? 3 : (len - i);
        unsigned int val = static_cast<unsigned int>(data[i]) << 16;
        if (n > 1) val |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (n > 2) val |= static_cast<unsigned int>(data[i + 2]);
        result += b64_chars[(val >> 18) & 0x3F];
        result += b64_chars[(val >> 12) & 0x3F];
        if (n > 1) result += b64_chars[(val >> 6) & 0x3F];
        else result += '=';
        if (n > 2) result += b64_chars[val & 0x3F];
        else result += '=';
    }
    return result;
}

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "capture_viewport called");

    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) return error_response("EditorInterface not available");

    // Godot 4 中所有 Control 共享窗口的 Viewport，get_base_control() 返回 Control*（完整类型）
    auto* base = editor->get_base_control();
    if (!base) return error_response("Editor base control not available");

    auto* viewport_node = base->get_viewport();
    if (!viewport_node) return error_response("Viewport not available");

    auto texture = viewport_node->get_texture();
    if (texture.is_null()) return error_response("Could not get viewport texture");

    auto img = texture->get_image();
    if (img.is_null()) return error_response("Could not get image from texture");

    godot::PackedByteArray png_buffer = img->save_png_to_buffer();
    if (png_buffer.size() == 0) return error_response("PNG encoding returned empty buffer");

    std::string b64 = base64_encode(png_buffer.ptr(), static_cast<size_t>(png_buffer.size()));

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["data"] = mcp::JsonValue(b64);
    inner["format"] = mcp::JsonValue("png");
    inner["width"] = mcp::JsonValue(img->get_width());
    inner["height"] = mcp::JsonValue(img->get_height());
    r["result"] = std::move(inner);

    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        "capture_viewport completed: " + std::to_string(img->get_width()) + "x" + std::to_string(img->get_height()));
    return r;
}

} // namespace capture_ops
} // namespace godot_self_driving
