#include "debugger_ops.hpp"
#include "core/log_system.hpp"
#include "runtime_ops.hpp"
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <vector>
#include <string>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace godot_self_driving {
namespace debugger_ops {

namespace {

// ── Data structures ──
struct LogEntry {
    std::string text;
    bool is_error;
    uint64_t timestamp_ms;
};

struct ErrorEntry {
    int hr, min, sec, msec;
    std::string source_file, source_func;
    int source_line;
    std::string error, error_descr;
    bool is_warning;
    std::vector<std::string> stack_files;
    std::vector<std::string> stack_funcs;
    std::vector<int> stack_lines;
};

struct GameOutputEntry {
    std::string text;
    int type;
    uint64_t timestamp_ms;
};

struct StackFrame {
    std::string file;
    int line;
    std::string func;
};

struct MonitorFrame {
    std::vector<double> values;
    uint64_t timestamp_ms;
};

// ── Singleton capture buffer ──
class DebuggerCapture {
public:
    static DebuggerCapture& instance() {
        static DebuggerCapture inst;
        return inst;
    }

    void add_log_entry(const std::string& text, bool is_error) {
        std::lock_guard<std::mutex> lock(mtx_);
        log_buffer_.push_back({text, is_error, now_ms()});
        if (log_buffer_.size() > MAX_LOG) log_buffer_.erase(log_buffer_.begin());
    }

    void add_error(int hr, int min, int sec, int msec,
        const std::string& source_file, const std::string& source_func, int source_line,
        const std::string& error, const std::string& error_descr, bool is_warning,
        const std::vector<std::string>& stack_files,
        const std::vector<std::string>& stack_funcs,
        const std::vector<int>& stack_lines) {
        std::lock_guard<std::mutex> lock(mtx_);
        errors_.push_back({hr, min, sec, msec, source_file, source_func, source_line,
            error, error_descr, is_warning, stack_files, stack_funcs, stack_lines});
        if (errors_.size() > MAX_ERRORS) errors_.erase(errors_.begin());
    }

    void add_game_output(const std::string& text, int type) {
        std::lock_guard<std::mutex> lock(mtx_);
        game_output_.push_back({text, type, now_ms()});
        if (game_output_.size() > MAX_GAME_OUTPUT) game_output_.erase(game_output_.begin());
    }

    void set_stack_dump(const std::vector<StackFrame>& frames) {
        std::lock_guard<std::mutex> lock(mtx_);
        stack_ = frames;
    }

    void set_scene_tree_raw(const std::vector<std::string>& node_lines) {
        std::lock_guard<std::mutex> lock(mtx_);
        scene_tree_lines_ = node_lines;
    }

    void add_monitor_frame(const std::vector<double>& values) {
        std::lock_guard<std::mutex> lock(mtx_);
        monitors_.push_back({values, now_ms()});
        if (monitors_.size() > MAX_MONITORS) monitors_.erase(monitors_.begin());
    }

    void set_session_state(bool active, bool breaked) {
        std::lock_guard<std::mutex> lock(mtx_);
        session_active_ = active;
        session_breaked_ = breaked;
    }

    std::string get_log_text(size_t limit) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        size_t start = (limit >= log_buffer_.size()) ? 0 : log_buffer_.size() - limit;
        for (size_t i = start; i < log_buffer_.size(); i++) {
            auto& e = log_buffer_[i];
            oss << "[" << ms_to_time(e.timestamp_ms) << "] "
                << (e.is_error ? "[ERROR] " : "[INFO] ")
                << e.text << "\n";
        }
        return oss.str();
    }

    size_t log_count() {
        std::lock_guard<std::mutex> lock(mtx_);
        return log_buffer_.size();
    }

    std::string get_log_text_since(size_t since_count, bool errors_only) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        size_t start = (since_count >= log_buffer_.size()) ? log_buffer_.size() : since_count;
        for (size_t i = start; i < log_buffer_.size(); i++) {
            auto& e = log_buffer_[i];
            if (errors_only && !e.is_error) continue;
            oss << "[" << ms_to_time(e.timestamp_ms) << "] "
                << (e.is_error ? "[ERROR] " : "[INFO] ")
                << e.text << "\n";
        }
        return oss.str();
    }

    std::string get_errors_text(size_t limit) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        size_t start = (limit >= errors_.size()) ? 0 : errors_.size() - limit;
        for (size_t i = start; i < errors_.size(); i++) {
            auto& e = errors_[i];
            oss << "[" << (e.is_warning ? "WARNING" : "ERROR") << "] "
                << std::setfill('0') << std::setw(2) << e.hr << ":"
                << std::setfill('0') << std::setw(2) << e.min << ":"
                << std::setfill('0') << std::setw(2) << e.sec
                << " - " << e.source_file << ":" << e.source_line
                << " @ " << e.source_func << "(): " << e.error << "\n";
            if (!e.error_descr.empty()) oss << "  " << e.error_descr << "\n";
            if (!e.stack_files.empty()) {
                oss << "  Stack:\n";
                for (size_t j = 0; j < e.stack_files.size(); j++)
                    oss << "    " << e.stack_files[j] << ":" << e.stack_lines[j]
                        << " @ " << e.stack_funcs[j] << "()\n";
            }
            oss << "  ───\n\n";
        }
        return oss.str();
    }

    std::string get_game_output_text(size_t limit) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        size_t start = (limit >= game_output_.size()) ? 0 : game_output_.size() - limit;
        for (size_t i = start; i < game_output_.size(); i++) {
            auto& e = game_output_[i];
            std::string pfx = e.type == 0 ? "[STD] " : (e.type == 1 ? "[ERR] " : "[RICH] ");
            oss << pfx << e.text << "\n";
        }
        return oss.str();
    }

    std::string get_stack_dump_text() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        oss << "Stack Trace (" << stack_.size() << " frames):\n";
        for (size_t i = 0; i < stack_.size(); i++)
            oss << "  #" << i << " " << stack_[i].file << ":" << stack_[i].line
                << " @ " << stack_[i].func << "()\n";
        return oss.str();
    }

    std::string get_scene_tree_text() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::ostringstream oss;
        for (auto& line : scene_tree_lines_) oss << line << "\n";
        return oss.str();
    }

    std::string get_monitors_text(size_t count) {
        std::lock_guard<std::mutex> lock(mtx_);
        static const char* names[] = {
            "FPS", "Time Process (ms)", "Time Physics (ms)", "Time Navigation (ms)",
            "Static Memory (MB)", "Static Memory Max (MB)", "Message Buffer Max (MB)",
            "Objects", "Resources", "Nodes", "Orphan Nodes",
            "Render Objects/Frame", "Render Primitives/Frame", "Render Draw Calls/Frame",
            "Video Mem Used (MB)", "Texture Mem Used (MB)", "Buffer Mem Used (MB)",
            "Physics 2D Active Objects", "Physics 2D Collision Pairs", "Physics 2D Islands",
            "Physics 3D Active Objects", "Physics 3D Collision Pairs", "Physics 3D Islands",
            "Audio Output Latency (ms)", "Nav Active Maps", "Nav Regions", "Nav Agents",
            "Nav Links", "Nav Polygons", "Nav Edges", "Nav Edge Merges",
            "Nav Edge Connections", "Nav Edge Frees", "Nav Obstacles"
        };
        std::ostringstream oss;
        size_t start = (count >= monitors_.size()) ? 0 : monitors_.size() - count;
        for (size_t i = start; i < monitors_.size(); i++) {
            oss << "Monitor Frame #" << (i + 1) << ":\n";
            size_t n = std::min(monitors_[i].values.size(), sizeof(names)/sizeof(names[0]));
            for (size_t j = 0; j < n; j++)
                oss << "  " << names[j] << ": " << monitors_[i].values[j] << "\n";
        }
        return oss.str();
    }

    std::string get_session_info_text() {
        std::lock_guard<std::mutex> lock(mtx_);
        bool active = capture_session_active();
        bool breaked = capture_session_breaked();
        size_t session_count = 0;
        auto* plugin = ::godot::DebugCapturePlugin::get_instance();
        if (plugin) session_count = plugin->session_ids_.size();
        std::ostringstream oss;
        oss << "Debug Session:\n";
        oss << "  Active: " << (active ? "true" : "false") << "\n";
        oss << "  Breaked: " << (breaked ? "true" : "false") << "\n";
        oss << "  Running: " << (active && !breaked ? "true" : "false") << "\n";
        oss << "  Sessions: " << session_count << "\n";
        return oss.str();
    }

private:
    DebuggerCapture() = default;
    static uint64_t now_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }
    static std::string ms_to_time(uint64_t ms) {
        time_t sec = static_cast<time_t>(ms / 1000);
        struct tm* ti = localtime(&sec);
        if (!ti) return "??:??:??";
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
        return std::string(buf);
    }

    std::vector<LogEntry> log_buffer_;
    std::vector<ErrorEntry> errors_;
    std::vector<GameOutputEntry> game_output_;
    std::vector<StackFrame> stack_;
    std::vector<std::string> scene_tree_lines_;
    std::vector<MonitorFrame> monitors_;
    bool session_active_ = false;
    bool session_breaked_ = false;
    std::mutex mtx_;
    static constexpr size_t MAX_LOG = 2000;
    static constexpr size_t MAX_ERRORS = 500;
    static constexpr size_t MAX_GAME_OUTPUT = 2000;
    static constexpr size_t MAX_MONITORS = 500;
};

// ── Godot 3 → 4 renamed property hints ──
// 依据 Godot 官方迁移表 renames_map_3_to_4.cpp，且旧名在 Godot 4
// 引擎 ADD_PROPERTY 全局核查中已无同名属性（frames 为任务指定项）。
struct RenameHint {
    const char* old_name;
    const char* new_name;
};

const RenameHint RENAME_HINTS[] = {
    { "frames", "sprite_frames" },                  // AnimatedSprite2D/3D
    { "cast_to", "target_position" },               // RayCast2D/3D, ShapeCast2D/3D
    { "rect_position", "position" },                // Control
    { "rect_global_position", "global_position" },  // Control
    { "rect_size", "size" },                        // Control
    { "rect_min_size", "custom_minimum_size" },     // Control
    { "rect_rotation", "rotation" },                // Control
    { "rect_scale", "scale" },                      // Control
    { "rect_pivot_offset", "pivot_offset" },        // Control
    { "translation", "position" },                  // Node3D
};

constexpr const char INVALID_ACCESS_MARKER[] = "Invalid access to property or key '";

std::string append_rename_hint(const std::string& error_text) {
    size_t marker_pos = error_text.find(INVALID_ACCESS_MARKER);
    if (marker_pos == std::string::npos) {
        return error_text;
    }
    size_t key_start = marker_pos + sizeof(INVALID_ACCESS_MARKER) - 1;
    size_t key_end = error_text.find('\'', key_start);
    if (key_end == std::string::npos) {
        return error_text;
    }
    std::string key = error_text.substr(key_start, key_end - key_start);
    constexpr const char* BASE_TYPE_SUFFIX = " on a base object of type '";
    size_t suffix_pos = error_text.find(BASE_TYPE_SUFFIX, key_end);
    if (suffix_pos != std::string::npos) {
        size_t type_start = suffix_pos + sizeof(BASE_TYPE_SUFFIX) - 1;
        size_t type_end = error_text.find('\'', type_start);
        if (type_end != std::string::npos) {
            std::string base_type = error_text.substr(type_start, type_end - type_start);
            bool is_packed_array = base_type.compare(0, 6, "Packed") == 0 &&
                base_type.size() >= 11 && base_type.compare(base_type.size() - 5, 5, "Array") == 0;
            if (base_type == "Dictionary" || base_type == "Array" || is_packed_array) {
                return error_text;
            }
        }
    }
    for (const RenameHint& hint : RENAME_HINTS) {
        if (key != hint.old_name) {
            continue;
        }
        std::string hinted = error_text;
        if (!hinted.empty() && hinted.back() != '\n') {
            hinted += '\n';
        }
        hinted += "Hint: did you mean '";
        hinted += hint.new_name;
        hinted += "'? The old Godot 3 name '";
        hinted += hint.old_name;
        hinted += "' was renamed in Godot 4.";
        return hinted;
    }
    return error_text;
}

} // namespace

// ── DebuggerCapture delegation functions ──
void capture_add_log_entry(const std::string& text, bool is_error) { DebuggerCapture::instance().add_log_entry(text, is_error); }
std::string capture_get_log_text(size_t limit) { return DebuggerCapture::instance().get_log_text(limit); }
std::string capture_get_errors_text(size_t limit) { return DebuggerCapture::instance().get_errors_text(limit); }
std::string capture_get_game_output_text(size_t limit) { return DebuggerCapture::instance().get_game_output_text(limit); }
std::string capture_get_stack_dump_text() { return DebuggerCapture::instance().get_stack_dump_text(); }
std::string capture_get_scene_tree_text() { return DebuggerCapture::instance().get_scene_tree_text(); }
std::string capture_get_monitors_text(size_t count) { return DebuggerCapture::instance().get_monitors_text(count); }
std::string capture_get_session_info_text() { return DebuggerCapture::instance().get_session_info_text(); }
bool capture_session_active() {
    auto* plugin = ::godot::DebugCapturePlugin::get_instance();
    if (!plugin) return false;
    for (int32_t id : plugin->session_ids_) {
        auto s = plugin->get_session(id);
        if (s.is_valid() && s->is_active()) return true;
    }
    return false;
}
bool capture_session_breaked() {
    auto* plugin = ::godot::DebugCapturePlugin::get_instance();
    if (!plugin) return false;
    for (int32_t id : plugin->session_ids_) {
        auto s = plugin->get_session(id);
        if (s.is_valid() && s->is_breaked()) return true;
    }
    return false;
}
size_t capture_log_count() { return DebuggerCapture::instance().log_count(); }
std::string capture_new_error_text(size_t since_count) {
    return append_rename_hint(DebuggerCapture::instance().get_log_text_since(since_count, true));
}

} // namespace debugger_ops
} // namespace godot_self_driving

// ── OutputCaptureLogger + DebugCapturePlugin implementations ──
namespace godot {

DebugCapturePlugin* DebugCapturePlugin::s_instance = nullptr;

void OutputCaptureLogger::_log_error(const String& p_function, const String& p_file,
    int32_t p_line, const String& p_code, const String& p_rationale,
    bool, int32_t, const TypedArray<Ref<ScriptBacktrace>>&) {
    std::string text(p_file.utf8().ptr());
    text += ":";
    text += std::to_string(p_line);
    text += " - ";
    text += p_rationale.utf8().ptr();
    if (p_code.length() > 0) {
        text += " (";
        text += p_code.utf8().ptr();
        text += ")";
    }
    godot_self_driving::debugger_ops::capture_add_log_entry(text, true);
}

void OutputCaptureLogger::_log_message(const String& p_message, bool p_error) {
    godot_self_driving::debugger_ops::capture_add_log_entry(p_message.utf8().ptr(), p_error);
}

bool DebugCapturePlugin::_has_capture(const String& p_name) const {
    std::string n = p_name.utf8().ptr();
    return n == "scene" || n == "performance" || n == "visual"
        || n == "servers" || n == "window" || n == "filesystem" || n == "gsd";
}

void DebugCapturePlugin::_setup_session(int32_t p_session_id) {
    session_ids_.push_back(p_session_id);
    session_ = get_session(p_session_id);
    if (session_.is_valid()) {
        godot_self_driving::debugger_ops::capture_add_log_entry("Debug session started", false);
    }
}

bool DebugCapturePlugin::_capture(const String& p_message, const Array& p_data, int32_t) {
    std::string msg = p_message.utf8().ptr();
    using dc = godot_self_driving::debugger_ops::DebuggerCapture;

    if (msg == "error" && p_data.size() >= 11) {
        int hr = static_cast<int>(p_data[0]), min = static_cast<int>(p_data[1]);
        int sec = static_cast<int>(p_data[2]), msec = static_cast<int>(p_data[3]);
        String src_file = p_data[4], src_func = p_data[5];
        int src_line = static_cast<int>(p_data[6]);
        String err = p_data[7], err_descr = p_data[8];
        bool is_warning = p_data[9];
        int stack_size = static_cast<int>(p_data[10]);
        std::vector<std::string> s_files, s_funcs;
        std::vector<int> s_lines;
        for (int i = 0; i < stack_size; i++) {
            int base = 11 + i * 3;
            if (base + 2 < static_cast<int>(p_data.size())) {
                s_files.push_back(String(p_data[base]).utf8().ptr());
                s_funcs.push_back(String(p_data[base + 1]).utf8().ptr());
                s_lines.push_back(static_cast<int>(p_data[base + 2]));
            }
        }
        dc::instance().add_error(hr, min, sec, msec,
            src_file.utf8().ptr(), src_func.utf8().ptr(), src_line,
            err.utf8().ptr(), err_descr.utf8().ptr(), is_warning,
            s_files, s_funcs, s_lines);
    }
    else if (msg == "output" && p_data.size() >= 2) {
        PackedStringArray strings = p_data[0];
        PackedInt32Array types = p_data[1];
        for (int i = 0; i < strings.size() && i < types.size(); i++)
            dc::instance().add_game_output(std::string(strings[i].utf8().ptr()), static_cast<int>(types[i]));
    }
    else if (msg == "stack_dump") {
        std::vector<godot_self_driving::debugger_ops::StackFrame> frames;
        for (int i = 0; i + 2 < static_cast<int>(p_data.size()); i += 3) {
            godot_self_driving::debugger_ops::StackFrame f;
            f.file = String(p_data[i]).utf8().ptr();
            f.line = static_cast<int>(p_data[i + 1]);
            f.func = String(p_data[i + 2]).utf8().ptr();
            frames.push_back(std::move(f));
        }
        dc::instance().set_stack_dump(frames);
    }
    else if (msg == "debug_enter") {
        dc::instance().set_session_state(true, true);
    }
    else if (msg == "debug_exit") {
        dc::instance().set_session_state(true, false);
    }
    else if (msg == "scene:scene_tree") {
        std::vector<std::string> lines;
        int idx = 0;
        while (idx + 5 < static_cast<int>(p_data.size())) {
            String name = p_data[idx + 1];
            String type_name = p_data[idx + 2];
            idx += 6;
            lines.push_back(std::string(name.utf8().ptr()) + " (" + type_name.utf8().ptr() + ")");
        }
        dc::instance().set_scene_tree_raw(lines);
    }
    else if (msg == "performance:profile_frame") {
        std::vector<double> values;
        for (int i = 0; i < static_cast<int>(p_data.size()); i++)
            values.push_back(static_cast<double>(p_data[i]));
        dc::instance().add_monitor_frame(values);
    }
    else if (msg == "gsd:response" && p_data.size() >= 1) {
        String payload = p_data[0];
        godot_self_driving::runtime_ops::handle_game_response(std::string(payload.utf8().ptr()));
    }

    return false;
}

} // namespace godot

namespace godot_self_driving {
namespace debugger_ops {

namespace {

mcp::JsonValue capture_note_for_empty_result() {
    return mcp::JsonValue(capture_session_active()
        ? "session active but no data captured — debugger data is only available while a game is running through the editor debugger; the editor's built-in debug handlers consume debug messages before editor plugins can see them (engine limitation); use log_get_game_entries to read the game process log file instead"
        : "no active debug session — debugger data is only available while a game is running through the editor debugger; start the game with editor_play_current_scene");
}

} // namespace

// ── MCP tool handlers ──
mcp::JsonValue handle_output_get_log(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "output_get_log called");
    size_t limit = 50;
    if (auto* l = args.Find("limit")) { if (l->IsInt()) limit = static_cast<size_t>(l->GetInt()); }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(DebuggerCapture::instance().get_log_text(limit));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "output_get_log completed");
    return r;
}

mcp::JsonValue handle_debugger_get_errors(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_errors called");
    size_t limit = 20;
    if (auto* l = args.Find("limit")) { if (l->IsInt()) limit = static_cast<size_t>(l->GetInt()); }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    std::string errors_text = DebuggerCapture::instance().get_errors_text(limit);
    r["result"] = mcp::JsonValue(errors_text);
    if (errors_text.empty()) r["note"] = capture_note_for_empty_result();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_errors completed");
    return r;
}

mcp::JsonValue handle_debugger_get_output(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_output called");
    size_t limit = 50;
    if (auto* l = args.Find("limit")) { if (l->IsInt()) limit = static_cast<size_t>(l->GetInt()); }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    std::string output_text = DebuggerCapture::instance().get_game_output_text(limit);
    r["result"] = mcp::JsonValue(output_text);
    if (output_text.empty()) r["note"] = capture_note_for_empty_result();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_output completed");
    return r;
}

mcp::JsonValue handle_debugger_get_stack_dump(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_stack_dump called");
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(DebuggerCapture::instance().get_stack_dump_text());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_stack_dump completed");
    return r;
}

mcp::JsonValue handle_debugger_get_scene_tree(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_scene_tree called");
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    std::string scene_tree_text = DebuggerCapture::instance().get_scene_tree_text();
    r["result"] = mcp::JsonValue(scene_tree_text);
    if (scene_tree_text.empty()) r["note"] = capture_note_for_empty_result();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_scene_tree completed");
    return r;
}

mcp::JsonValue handle_debugger_get_monitors(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_monitors called");
    size_t count = 1;
    if (auto* c = args.Find("count")) { if (c->IsInt()) count = static_cast<size_t>(c->GetInt()); }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    std::string monitors_text = DebuggerCapture::instance().get_monitors_text(count);
    r["result"] = mcp::JsonValue(monitors_text);
    if (monitors_text.empty()) r["note"] = capture_note_for_empty_result();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_monitors completed");
    return r;
}

mcp::JsonValue handle_debugger_get_session_info(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_session_info called");
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(DebuggerCapture::instance().get_session_info_text());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "debugger_get_session_info completed");
    return r;
}

void register_classes() {
    godot::ClassDB::register_class<::godot::OutputCaptureLogger>();
    godot::ClassDB::register_class<::godot::DebugCapturePlugin>();
}

::godot::Ref<::godot::OutputCaptureLogger> create_output_logger() {
    ::godot::Ref<::godot::OutputCaptureLogger> logger;
    logger.instantiate();
    return logger;
}

::godot::Ref<::godot::DebugCapturePlugin> create_debug_plugin() {
    ::godot::Ref<::godot::DebugCapturePlugin> plugin;
    plugin.instantiate();
    ::godot::DebugCapturePlugin::s_instance = plugin.ptr();
    return plugin;
}

} // namespace debugger_ops
} // namespace godot_self_driving