#include "text_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <string>
#include <unordered_map>

namespace godot_self_driving {
namespace text_ops {

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

JV ok_json() {
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    return r;
}

struct RidStore {
    std::unordered_map<int64_t, godot::RID> map;

    int64_t store(const godot::RID& rid) {
        int64_t id = rid.get_id();
        map[id] = rid;
        return id;
    }

    godot::RID get(int64_t id) {
        auto it = map.find(id);
        if (it != map.end()) return it->second;
        return godot::RID();
    }
};

RidStore& rid_store() {
    static RidStore s;
    return s;
}

godot::RID resolve_rid(const JV& args, const char* key) {
    auto* it = args.Find(key);
    if (!it || !it->IsNumber()) return godot::RID();
    return rid_store().get(it->GetInt());
}

godot::Ref<godot::TextServer> get_ts() {
    auto* mgr = godot::TextServerManager::get_singleton();
    if (!mgr) return godot::Ref<godot::TextServer>();
    return mgr->get_primary_interface();
}

} // namespace

JV handle_create_font(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_create_font called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID rid = ts->create_font();
    int64_t id = rid_store().store(rid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_create_font completed");
    JV r(JV::object_tag);
    r["result"] = JV(id);
    return r;
}

JV handle_create_shaped_text(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_create_shaped_text called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    int direction = 0;
    auto* d = args.Find("direction");
    if (d && d->IsInt()) direction = d->GetInt();
    int orientation = 0;
    auto* o = args.Find("orientation");
    if (o && o->IsInt()) orientation = o->GetInt();
    godot::RID rid = ts->create_shaped_text(
        static_cast<godot::TextServer::Direction>(direction),
        static_cast<godot::TextServer::Orientation>(orientation));
    int64_t id = rid_store().store(rid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_create_shaped_text completed");
    JV r(JV::object_tag);
    r["result"] = JV(id);
    return r;
}

JV handle_font_set_antialiasing(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_antialiasing called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID font_rid = resolve_rid(args, "font_rid");
    if (!font_rid.is_valid()) return error_json("missing or invalid parameter: font_rid");
    auto* a = args.Find("antialiasing");
    if (!a || !a->IsInt()) return error_json("missing required parameter: antialiasing");
    ts->font_set_antialiasing(font_rid, static_cast<godot::TextServer::FontAntialiasing>(a->GetInt()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_antialiasing completed");
    return ok_json();
}

JV handle_font_set_data(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_data called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID font_rid = resolve_rid(args, "font_rid");
    if (!font_rid.is_valid()) return error_json("missing or invalid parameter: font_rid");
    auto* dp = args.Find("data");
    if (!dp || !dp->IsString()) return error_json("missing required parameter: data");
    std::string data_path = dp->GetString();
    godot::PackedByteArray bytes = godot::FileAccess::get_file_as_bytes(godot::String(data_path.c_str()));
    ts->font_set_data(font_rid, bytes);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_data completed");
    return ok_json();
}

JV handle_font_set_hinting(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_hinting called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID font_rid = resolve_rid(args, "font_rid");
    if (!font_rid.is_valid()) return error_json("missing or invalid parameter: font_rid");
    auto* h = args.Find("hinting");
    if (!h || !h->IsInt()) return error_json("missing required parameter: hinting");
    ts->font_set_hinting(font_rid, static_cast<godot::TextServer::Hinting>(h->GetInt()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_font_set_hinting completed");
    return ok_json();
}

JV handle_get_system_font_path(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_get_system_font_path called");
    auto* fn = args.Find("font_name");
    if (!fn || !fn->IsString()) return error_json("missing required parameter: font_name");
    std::string font_name = fn->GetString();
    int weight = 400;
    auto* w = args.Find("weight");
    if (w && w->IsInt()) weight = w->GetInt();
    int stretch = 100;
    auto* s = args.Find("stretch");
    if (s && s->IsInt()) stretch = s->GetInt();
    bool italic = false;
    auto* it = args.Find("italic");
    if (it && it->IsBool()) italic = it->GetBool();
    auto* os = godot::OS::get_singleton();
    if (!os) return error_json("OS singleton not available");
    godot::String path = os->get_system_font_path(
        godot::String(font_name.c_str()), weight, stretch, italic);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_get_system_font_path completed");
    JV r(JV::object_tag);
    r["result"] = JV(to_std(path));
    return r;
}

JV handle_has_feature(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_has_feature called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    auto* f = args.Find("feature");
    if (!f || !f->IsInt()) return error_json("missing required parameter: feature");
    bool result = ts->has_feature(static_cast<godot::TextServer::Feature>(f->GetInt()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_has_feature completed");
    JV r(JV::object_tag);
    r["result"] = JV(result);
    return r;
}

JV handle_is_locale_right_to_left(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_is_locale_right_to_left called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    auto* l = args.Find("locale");
    if (!l || !l->IsString()) return error_json("missing required parameter: locale");
    bool result = ts->is_locale_right_to_left(godot::String(l->GetString().c_str()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_is_locale_right_to_left completed");
    JV r(JV::object_tag);
    r["result"] = JV(result);
    return r;
}

JV handle_shaped_text_add_string(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_shaped_text_add_string called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID shaped_rid = resolve_rid(args, "shaped_rid");
    if (!shaped_rid.is_valid()) return error_json("missing or invalid parameter: shaped_rid");
    auto* tp = args.Find("text");
    if (!tp || !tp->IsString()) return error_json("missing required parameter: text");
    godot::RID font_rid = resolve_rid(args, "font_rid");
    if (!font_rid.is_valid()) return error_json("missing or invalid parameter: font_rid");
    auto* sp = args.Find("size");
    if (!sp || !sp->IsInt()) return error_json("missing required parameter: size");
    std::string language;
    auto* lp = args.Find("language");
    if (lp && lp->IsString()) language = lp->GetString();
    godot::TypedArray<godot::RID> fonts;
    fonts.append(font_rid);
    bool result = ts->shaped_text_add_string(
        shaped_rid,
        godot::String(tp->GetString().c_str()),
        fonts,
        static_cast<int64_t>(sp->GetInt()),
        godot::Dictionary(),
        godot::String(language.c_str()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_shaped_text_add_string completed");
    JV r(JV::object_tag);
    r["result"] = JV(result);
    return r;
}

JV handle_shaped_text_get_size(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_shaped_text_get_size called");
    auto ts = get_ts();
    if (ts.is_null()) return error_json("TextServer not available");
    godot::RID shaped_rid = resolve_rid(args, "shaped_rid");
    if (!shaped_rid.is_valid()) return error_json("missing or invalid parameter: shaped_rid");
    godot::Vector2 size = ts->shaped_text_get_size(shaped_rid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "text_shaped_text_get_size completed");
    JV r(JV::object_tag);
    JV inner(JV::object_tag);
    inner["x"] = JV(static_cast<double>(size.x));
    inner["y"] = JV(static_cast<double>(size.y));
    r["result"] = std::move(inner);
    return r;
}

JV handle_file_write(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "file_write called");
    auto* pp = args.Find("path");
    if (!pp || !pp->IsString()) return error_json("missing required parameter: path");
    auto* cp = args.Find("content");
    if (!cp || !cp->IsString()) return error_json("missing required parameter: content");
    std::string mode = "WRITE";
    auto* mp = args.Find("mode");
    if (mp && mp->IsString()) mode = mp->GetString();
    godot::FileAccess::ModeFlags flag = godot::FileAccess::WRITE;
    if (mode == "APPEND") flag = godot::FileAccess::READ_WRITE;
    auto file = godot::FileAccess::open(godot::String(pp->GetString().c_str()), flag);
    if (file.is_null()) return error_json("failed to open file: " + pp->GetString());
    if (mode == "APPEND") file->seek_end();
    file->store_string(godot::String(cp->GetString().c_str()));
    file->close();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "file_write completed");
    return ok_json();
}

} // namespace text_ops
} // namespace godot_self_driving
