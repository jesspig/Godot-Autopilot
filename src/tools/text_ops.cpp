#include "text_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace text_ops {

using JV = mcp::JsonValue;

namespace {

JV ok_json() {
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

struct RidStore {
  std::unordered_map<int64_t, godot::RID> map;

  int64_t store(const godot::RID &rid) {
    int64_t id = rid.get_id();
    map[id] = rid;
    return id;
  }

  godot::RID get(int64_t id) {
    auto it = map.find(id);
    if (it != map.end())
      return it->second;
    return godot::RID();
  }
};

RidStore &rid_store() {
  static RidStore s;
  return s;
}

godot::RID resolve_rid(const JV &args, const char *key) {
  auto *it = args.Find(key);
  if (!it || !it->IsNumber())
    return godot::RID();
  return rid_store().get(it->GetInt());
}

godot::Ref<godot::TextServer> get_ts() {
  auto *mgr = godot::TextServerManager::get_singleton();
  if (!mgr)
    return godot::Ref<godot::TextServer>();
  return mgr->get_primary_interface();
}

} // namespace

JV handle_create_font(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_text_font called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID rid = ts->create_font();
  int64_t id = rid_store().store(rid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_text_font completed");
  JV r(JV::object_tag);
  r["result"] = JV(id);
  return r;
}

JV handle_create_shaped_text(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_shaped_text called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  int direction = 0;
  auto *d = args.Find("direction");
  if (d && d->IsInt())
    direction = d->GetInt();
  int orientation = 0;
  auto *o = args.Find("orientation");
  if (o && o->IsInt())
    orientation = o->GetInt();
  godot::RID rid = ts->create_shaped_text(
      static_cast<godot::TextServer::Direction>(direction),
      static_cast<godot::TextServer::Orientation>(orientation));
  int64_t id = rid_store().store(rid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_shaped_text completed");
  JV r(JV::object_tag);
  r["result"] = JV(id);
  return r;
}

JV handle_font_set_antialiasing(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_antialiasing called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID font_rid = resolve_rid(args, "font_rid");
  if (!font_rid.is_valid())
    return util::error_json("missing or invalid parameter: font_rid");
  auto *a = args.Find("antialiasing");
  if (!a || !a->IsInt())
    return util::error_json("missing required parameter: antialiasing");
  ts->font_set_antialiasing(
      font_rid, static_cast<godot::TextServer::FontAntialiasing>(a->GetInt()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_antialiasing completed");
  return ok_json();
}

JV handle_font_set_data(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_data called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID font_rid = resolve_rid(args, "font_rid");
  if (!font_rid.is_valid())
    return util::error_json("missing or invalid parameter: font_rid");
  auto *dp = args.Find("data");
  if (!dp || !dp->IsString())
    return util::error_json("missing required parameter: data");
  std::string data_path = dp->GetString();
  godot::PackedByteArray bytes =
      godot::FileAccess::get_file_as_bytes(godot::String(data_path.c_str()));
  ts->font_set_data(font_rid, bytes);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_data completed");
  return ok_json();
}

JV handle_font_set_hinting(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_hinting called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID font_rid = resolve_rid(args, "font_rid");
  if (!font_rid.is_valid())
    return util::error_json("missing or invalid parameter: font_rid");
  auto *h = args.Find("hinting");
  if (!h || !h->IsInt())
    return util::error_json("missing required parameter: hinting");
  ts->font_set_hinting(font_rid,
                       static_cast<godot::TextServer::Hinting>(h->GetInt()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_text_font_hinting completed");
  return ok_json();
}

JV handle_get_system_font_path(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_text_font_system_path called");
  auto *fn = args.Find("font_name");
  if (!fn || !fn->IsString())
    return util::error_json("missing required parameter: font_name");
  std::string font_name = fn->GetString();
  int weight = 400;
  auto *w = args.Find("weight");
  if (w && w->IsInt())
    weight = w->GetInt();
  int stretch = 100;
  auto *s = args.Find("stretch");
  if (s && s->IsInt())
    stretch = s->GetInt();
  bool italic = false;
  auto *it = args.Find("italic");
  if (it && it->IsBool())
    italic = it->GetBool();
  auto *os = godot::OS::get_singleton();
  if (!os)
    return util::error_json("OS singleton not available");
  godot::String path = os->get_system_font_path(
      godot::String(font_name.c_str()), weight, stretch, italic);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_text_font_system_path completed");
  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(path));
  return r;
}

JV handle_has_feature(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_text_feature called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  auto *f = args.Find("feature");
  if (!f || !f->IsInt())
    return util::error_json("missing required parameter: feature");
  bool result =
      ts->has_feature(static_cast<godot::TextServer::Feature>(f->GetInt()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_text_feature completed");
  JV r(JV::object_tag);
  r["result"] = JV(result);
  return r;
}

JV handle_is_locale_right_to_left(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "is_text_locale_right_to_left called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  auto *l = args.Find("locale");
  if (!l || !l->IsString())
    return util::error_json("missing required parameter: locale");
  bool result =
      ts->is_locale_right_to_left(godot::String(l->GetString().c_str()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "is_text_locale_right_to_left completed");
  JV r(JV::object_tag);
  r["result"] = JV(result);
  return r;
}

JV handle_shaped_text_add_string(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_shaped_text_string called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID shaped_rid = resolve_rid(args, "shaped_rid");
  if (!shaped_rid.is_valid())
    return util::error_json("missing or invalid parameter: shaped_rid");
  auto *tp = args.Find("text");
  if (!tp || !tp->IsString())
    return util::error_json("missing required parameter: text");
  godot::RID font_rid = resolve_rid(args, "font_rid");
  if (!font_rid.is_valid())
    return util::error_json("missing or invalid parameter: font_rid");
  auto *sp = args.Find("size");
  if (!sp || !sp->IsInt())
    return util::error_json("missing required parameter: size");
  std::string language;
  auto *lp = args.Find("language");
  if (lp && lp->IsString())
    language = lp->GetString();
  godot::TypedArray<godot::RID> fonts;
  fonts.append(font_rid);
  bool result = ts->shaped_text_add_string(
      shaped_rid, godot::String(tp->GetString().c_str()), fonts,
      static_cast<int64_t>(sp->GetInt()), godot::Dictionary(),
      godot::String(language.c_str()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_shaped_text_string completed");
  JV r(JV::object_tag);
  r["result"] = JV(result);
  return r;
}

JV handle_shaped_text_get_size(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_shaped_text_size called");
  auto ts = get_ts();
  if (ts.is_null())
    return util::error_json("TextServer not available");
  godot::RID shaped_rid = resolve_rid(args, "shaped_rid");
  if (!shaped_rid.is_valid())
    return util::error_json("missing or invalid parameter: shaped_rid");
  godot::Vector2 size = ts->shaped_text_get_size(shaped_rid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_shaped_text_size completed");
  JV r(JV::object_tag);
  JV inner(JV::object_tag);
  inner["x"] = JV(static_cast<double>(size.x));
  inner["y"] = JV(static_cast<double>(size.y));
  r["result"] = std::move(inner);
  return r;
}

JV handle_file_write(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "write_file called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString())
    return util::error_json("missing required parameter: path");
  auto *cp = args.Find("content");
  if (!cp || !cp->IsString())
    return util::error_json("missing required parameter: content");
  std::string mode = "WRITE";
  auto *mp = args.Find("mode");
  if (mp && mp->IsString())
    mode = mp->GetString();
  godot::FileAccess::ModeFlags flag = godot::FileAccess::WRITE;
  if (mode == "APPEND")
    flag = godot::FileAccess::READ_WRITE;
  auto file =
      godot::FileAccess::open(godot::String(pp->GetString().c_str()), flag);
  if (file.is_null())
    return util::error_json("failed to open file: " + pp->GetString());
  if (mode == "APPEND")
    file->seek_end();
  file->store_string(godot::String(cp->GetString().c_str()));
  file->close();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "write_file completed");
  return ok_json();
}

namespace {

std::string ascii_lower(const std::string &s) {
  std::string out = s;
  for (char &c : out) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  }
  return out;
}

bool matches_extension(const std::string &name,
                       const std::vector<std::string> &exts,
                       bool case_sensitive) {
  size_t dot = name.rfind('.');
  if (dot == std::string::npos)
    return false;
  std::string ext = name.substr(dot + 1);
  for (const auto &e : exts) {
    if (case_sensitive) {
      if (ext == e)
        return true;
    } else if (ascii_lower(ext) == ascii_lower(e)) {
      return true;
    }
  }
  return false;
}

int count_occurrences(const std::string &text, const std::string &query,
                      bool case_sensitive) {
  if (query.empty())
    return 0;
  if (case_sensitive) {
    int n = 0;
    size_t pos = 0;
    while ((pos = text.find(query, pos)) != std::string::npos) {
      ++n;
      pos += query.size();
    }
    return n;
  }
  const std::string lower_text = ascii_lower(text);
  const std::string lower_query = ascii_lower(query);
  int n = 0;
  size_t pos = 0;
  while ((pos = lower_text.find(lower_query, pos)) != std::string::npos) {
    ++n;
    pos += lower_query.size();
  }
  return n;
}

std::string join_search_path(const std::string &dir,
                             const std::string &name) {
  return dir + (dir.empty() || dir.back() == '/' ? std::string() : "/") + name;
}

void find_in_files_recursive(const std::string &dir,
                             const std::vector<std::string> &exts,
                             const std::string &query,
                             bool case_sensitive, int max_results,
                             std::vector<std::pair<std::string, int>> &hits,
                             bool &truncated) {
  if (truncated || static_cast<int>(hits.size()) >= max_results)
    return;
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) {
    return;
  }
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (truncated || static_cast<int>(hits.size()) >= max_results)
      break;
    if (entry != "." && entry != "..") {
      std::string name = util::to_std(entry);
      std::string fpath = join_search_path(dir, name);
      if (da->current_is_dir()) {
        find_in_files_recursive(fpath, exts, query, case_sensitive, max_results,
                                hits, truncated);
      } else if (matches_extension(name, exts, case_sensitive)) {
        auto file = godot::FileAccess::open(godot::String(fpath.c_str()),
                                            godot::FileAccess::READ);
        if (file.is_null())
          continue;
        std::string text = util::to_std(file->get_as_text());
        file->close();
        int n = count_occurrences(text, query, case_sensitive);
        if (n > 0) {
          hits.emplace_back(fpath, n);
          if (static_cast<int>(hits.size()) >= max_results)
            truncated = true;
        }
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

} // namespace

JV handle_file_read(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "read_file called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString())
    return util::error_json("missing required parameter: path");
  auto file = godot::FileAccess::open(godot::String(pp->GetString().c_str()),
                                      godot::FileAccess::READ);
  if (file.is_null())
    return util::error_json("failed to open file: " + pp->GetString());
  std::string content = util::to_std(file->get_as_text());
  file->close();
  JV inner(JV::object_tag);
  inner["path"] = JV(pp->GetString());
  inner["content"] = JV(content);
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "read_file completed");
  return r;
}

JV handle_find_in_files(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "find_in_files called");
  auto *qp = args.Find("query");
  if (!qp || !qp->IsString() || qp->GetString().empty())
    return util::error_json("missing required parameter: query");
  std::string query = qp->GetString();

  std::string dir = "res://";
  auto *dp = args.Find("dir");
  if (dp && dp->IsString())
    dir = dp->GetString();

  std::vector<std::string> exts = {"gd", "tscn", "tres", "cs",
                                   "md", "json", "h",  "cpp"};
  auto *ep = args.Find("extensions");
  if (ep && ep->IsArray() && !ep->GetArray().empty()) {
    exts.clear();
    for (const auto &e : ep->GetArray()) {
      if (e.IsString())
        exts.push_back(e.GetString());
    }
  }

  bool case_sensitive = false;
  auto *cp = args.Find("case_sensitive");
  if (cp && cp->IsBool())
    case_sensitive = cp->GetBool();

  int max_results = 500;
  auto *mp = args.Find("max_results");
  if (mp && mp->IsInt())
    max_results = static_cast<int>(mp->GetInt());
  if (max_results < 1)
    max_results = 1;

  std::vector<std::pair<std::string, int>> hits;
  bool truncated = false;
  find_in_files_recursive(dir, exts, query, case_sensitive, max_results, hits,
                          truncated);

  JV files(JV::array_tag);
  for (const auto &h : hits) {
    JV item(JV::object_tag);
    item["file"] = JV(h.first);
    item["matches"] = JV(static_cast<int64_t>(h.second));
    files.PushBack(std::move(item));
  }
  JV inner(JV::object_tag);
  inner["files"] = std::move(files);
  inner["truncated"] = JV(truncated);
  inner["total"] = JV(static_cast<int64_t>(hits.size()));
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "find_in_files completed");
  return r;
}

} // namespace text_ops
} // namespace godot_autopilot
