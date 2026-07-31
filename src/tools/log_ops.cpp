#include "log_ops.hpp"
#include "core/log_system.hpp"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <string>
#include <vector>

namespace godot_self_driving {
namespace log_ops {

using JV = mcp::JsonValue;

namespace {

constexpr char RUN_HINT[] = "game logs are written by the running game process (start it with editor_play_current_scene)";

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

JV error_json(const std::string& msg) {
    JV e(JV::object_tag);
    e["error"] = JV(msg);
    return e;
}

constexpr int64_t DEFAULT_LIMIT = 50;
constexpr int64_t MAX_LIMIT = 500;

std::string logs_dir_diagnostic(const godot::String& logs_dir) {
    auto dir = godot::DirAccess::open(logs_dir);
    if (!dir.is_valid()) {
        return "logs directory does not exist: \"" + to_std(logs_dir) + "\"";
    }
    godot::PackedStringArray names;
    dir->list_dir_begin();
    godot::String name = dir->get_next();
    while (!name.is_empty()) {
        if (name != "." && name != "..") {
            names.push_back(name);
        }
        name = dir->get_next();
    }
    dir->list_dir_end();
    if (names.size() == 0) {
        return "logs directory is empty: \"" + to_std(logs_dir) + "\"";
    }
    std::string joined = "directory contains: ";
    for (int i = 0; i < names.size(); i++) {
        if (i > 0) joined += ", ";
        joined += to_std(names[i]);
    }
    return joined;
}

} // namespace

JV handle_log_get_game_entries(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "log_get_game_entries called");
    int64_t limit = DEFAULT_LIMIT;
    auto* lp = args.Find("limit");
    if (lp) {
        if (!lp->IsInt()) return error_json("invalid parameter: limit must be an integer");
        limit = lp->GetInt();
        if (limit > MAX_LIMIT) limit = MAX_LIMIT;
        if (limit < 0) limit = 0;
    }
    auto* os = godot::OS::get_singleton();
    if (!os) return error_json("OS singleton not available");
    godot::String logs_dir = os->get_user_data_dir() + "/logs";
    godot::String path = logs_dir + "/godot.log";
    if (!godot::FileAccess::file_exists(path)) {
        return error_json("game log file not found: \"" + to_std(path) + "\" — " +
            logs_dir_diagnostic(logs_dir) + " — " + RUN_HINT);
    }
    auto file = godot::FileAccess::open(path, godot::FileAccess::READ);
    if (file.is_null()) {
        os->delay_usec(100000);
        file = godot::FileAccess::open(path, godot::FileAccess::READ);
        if (file.is_null()) {
            return error_json("failed to open game log file (retried once after delay): \"" + to_std(path) + "\" — open error code " + std::to_string(static_cast<int>(godot::FileAccess::get_open_error())) + " — " +
                logs_dir_diagnostic(logs_dir) + " — " + RUN_HINT);
        }
    }
    std::vector<std::string> tail;
    int64_t total_lines = 0;
    while (!file->eof_reached()) {
        std::string line = to_std(file->get_line());
        total_lines++;
        if (limit == 0) continue;
        if (static_cast<int64_t>(tail.size()) < limit) {
            tail.push_back(std::move(line));
        } else {
            tail.erase(tail.begin());
            tail.push_back(std::move(line));
        }
    }
    file->close();
    JV entries(JV::array_tag);
    for (const std::string& line : tail) {
        entries.PushBack(JV(line));
    }
    JV result(JV::object_tag);
    result["path"] = JV(to_std(path));
    result["entries"] = std::move(entries);
    result["total_lines"] = JV(total_lines);
    JV r(JV::object_tag);
    r["result"] = std::move(result);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "log_get_game_entries completed");
    return r;
}

} // namespace log_ops
} // namespace godot_self_driving
