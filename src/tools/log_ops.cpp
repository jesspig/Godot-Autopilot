#include "log_ops.hpp"
#include "core/log_system.hpp"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <vector>

namespace godot_self_driving {
namespace log_ops {

using JV = mcp::JsonValue;

namespace {

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
    godot::String path = os->get_user_data_dir() + "/logs/godot.log";
    if (!godot::FileAccess::file_exists(path)) {
        return error_json("game log file not found at " + to_std(path) + " — run the game first (editor_play_current_scene)");
    }
    auto file = godot::FileAccess::open(path, godot::FileAccess::READ);
    if (file.is_null()) return error_json("failed to open game log file at " + to_std(path));
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
