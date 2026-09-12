#include "text_ops.hpp"
#include "core/editor_readiness.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/project_path.hpp"
#include "util/rid_registry.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <set>
#include <string>
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

struct TextRidDomain {};

struct ScanBudget {
  size_t files = 0;
  size_t total_bytes = 0;
  std::string reason;

  bool take_file(size_t file_bytes, size_t depth) {
    if (depth > GDA_SCAN_MAX_DEPTH) {
      reason = "directory_depth";
      return false;
    }
    if (files >= GDA_SCAN_MAX_FILES) {
      reason = "file_count";
      return false;
    }
    if (file_bytes > GDA_SCAN_MAX_FILE_BYTES) {
      reason = "single_file_bytes";
      return false;
    }
    if (total_bytes > GDA_SCAN_MAX_TOTAL_BYTES - file_bytes) {
      reason = "total_bytes";
      return false;
    }
    ++files;
    total_bytes += file_bytes;
    return true;
  }
};

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
  int64_t id = util::rid_store<TextRidDomain>().store(rid);
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
  int64_t id = util::rid_store<TextRidDomain>().store(rid);
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
  godot::RID font_rid = util::resolve_rid<TextRidDomain>(args, "font_rid");
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
  godot::RID font_rid = util::resolve_rid<TextRidDomain>(args, "font_rid");
  if (!font_rid.is_valid())
    return util::error_json("missing or invalid parameter: font_rid");
  auto *dp = args.Find("data");
  if (!dp || !dp->IsString())
    return util::error_json("missing required parameter: data");
  const auto checked = util::normalize_project_path(dp->GetString(), true);
  if (!checked.valid())
    return util::error_detail("path rejected", "data", checked.error,
                              "use a res:// or user:// project path");
  godot::PackedByteArray bytes =
      godot::FileAccess::get_file_as_bytes(godot::String(checked.value.c_str()));
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
  godot::RID font_rid = util::resolve_rid<TextRidDomain>(args, "font_rid");
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
  godot::RID shaped_rid = util::resolve_rid<TextRidDomain>(args, "shaped_rid");
  if (!shaped_rid.is_valid())
    return util::error_json("missing or invalid parameter: shaped_rid");
  auto *tp = args.Find("text");
  if (!tp || !tp->IsString())
    return util::error_json("missing required parameter: text");
  godot::RID font_rid = util::resolve_rid<TextRidDomain>(args, "font_rid");
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
  godot::RID shaped_rid = util::resolve_rid<TextRidDomain>(args, "shaped_rid");
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

namespace {

std::string lowercase_extension_of(const std::string &path) {
  size_t slash = path.find_last_of('/');
  std::string name =
      slash == std::string::npos ? path : path.substr(slash + 1);
  size_t dot = name.rfind('.');
  if (dot == std::string::npos || dot + 1 >= name.size())
    return std::string();
  std::string ext = name.substr(dot + 1);
  for (char &c : ext) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  }
  return ext;
}

bool is_import_type_extension(const std::string &ext) {
  static const std::set<std::string> kImportExtensions = {
      "bmp", "cut", "dds", "exr", "hdr", "jpg", "jpeg", "ktx",
      "png", "pnm", "svg", "svgz", "tga", "webp", "wav", "mp3",
      "ogg", "ttf", "otf", "woff", "woff2", "pfb", "fnt", "dae",
      "obj", "glb", "gltf", "escn", "fbx", "blend", "csv", "po"};
  return kImportExtensions.count(ext) > 0;
}

bool is_script_extension(const std::string &ext) {
  static const std::set<std::string> kScriptExtensions = {"gd", "cs",
                                                          "gdshader",
                                                          "gdshaderinc"};
  return kScriptExtensions.count(ext) > 0;
}

JV build_script_diagnostics(const godot::String &gs_path,
                            const std::string &ext) {
  JV diag(JV::object_tag);
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    diag["ok"] = JV(false);
    diag["hint"] =
        JV("ResourceLoader not available; open the file in the editor script "
           "panel to inspect errors");
    return diag;
  }
  if (loader->has_cached(gs_path)) {
    auto cached = loader->get_cached_ref(gs_path);
    if (cached.is_valid())
      cached->set_path(godot::String());
  }
  godot::Ref<godot::Resource> loaded = loader->load(gs_path);
  if (loaded.is_valid()) {
    diag["ok"] = JV(true);
    return diag;
  }
  diag["ok"] = JV(false);
  if (ext == "cs") {
    diag["hint"] =
        JV("the C# assembly may not be built yet; run a project build before "
           "expecting this script to load");
  } else {
    diag["hint"] =
        JV("the script failed to load after write (parse or compile error); "
           "line-level details are only available in the editor script panel");
  }
  return diag;
}

} // namespace

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
  const auto checked = util::normalize_project_path(pp->GetString(), true,
                                                    false);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "write only inside the project namespaces");
  const std::string normalized_path = checked.value;
  godot::String gs_path(normalized_path.c_str());
  auto file = godot::FileAccess::open(gs_path, flag);
  if (file.is_null())
    return util::error_json("failed to open file: " + pp->GetString());
  if (mode == "APPEND")
    file->seek_end();
  file->store_string(godot::String(cp->GetString().c_str()));
  file->close();

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["engine_managed"] = JV(false);
  bool in_project = normalized_path.rfind("res://", 0) == 0;
  auto *engine = godot::Engine::get_singleton();
  auto *editor =
      in_project ? godot::EditorInterface::get_singleton() : nullptr;
  if (editor && engine && engine->is_editor_hint()) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      r["engine_managed"] = JV(true);
      std::string ext = lowercase_extension_of(normalized_path);
      bool imported = godot::FileAccess::file_exists(
                          gs_path + godot::String(".import")) ||
                      is_import_type_extension(ext);
      if (imported) {
        if (is_import_in_progress())
          return busy_error();
        godot::PackedStringArray files;
        files.append(gs_path);
        efs->reimport_files(files);
        r["action"] = JV("reimport");
      } else {
        efs->update_file(gs_path);
        r["action"] = JV("update_file");
      }
      if (is_script_extension(ext))
        r["diagnostics"] = build_script_diagnostics(gs_path, ext);
    }
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "write_file completed");
  return r;
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
                              bool &truncated, ScanBudget &budget,
                              size_t depth) {
  if (truncated || static_cast<int>(hits.size()) >= max_results)
    return;
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) {
    return;
  }
  da->list_dir_begin();
  if (depth > GDA_SCAN_MAX_DEPTH) {
    budget.reason = "directory_depth";
    truncated = true;
    da->list_dir_end();
    return;
  }
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (truncated || static_cast<int>(hits.size()) >= max_results)
      break;
    if (entry != "." && entry != "..") {
      std::string name = util::to_std(entry);
      std::string fpath = join_search_path(dir, name);
      if (da->current_is_dir()) {
        find_in_files_recursive(fpath, exts, query, case_sensitive, max_results,
                                hits, truncated, budget, depth + 1);
      } else if (matches_extension(name, exts, case_sensitive)) {
        auto file = godot::FileAccess::open(godot::String(fpath.c_str()),
                                            godot::FileAccess::READ);
        if (file.is_null())
          continue;
        const int64_t length = file->get_length();
        if (length < 0 || !budget.take_file(static_cast<size_t>(length), depth)) {
          truncated = true;
          break;
        }
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
  const auto checked = util::normalize_project_path(pp->GetString(), true,
                                                    false);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "use a res:// or user:// project path");
  auto file = godot::FileAccess::open(godot::String(checked.value.c_str()),
                                      godot::FileAccess::READ);
  if (file.is_null())
    return util::error_json("failed to open file: " + pp->GetString());
  std::string content = util::to_std(file->get_as_text());
  file->close();
  JV inner(JV::object_tag);
  inner["path"] = JV(checked.value);
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

  const auto checked_dir = util::normalize_project_path(dir, true);
  if (!checked_dir.valid())
    return util::error_detail("path rejected", "dir", checked_dir.error,
                              "search only inside the project namespaces");
  dir = checked_dir.value;

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
  ScanBudget budget;
  find_in_files_recursive(dir, exts, query, case_sensitive, max_results, hits,
                          truncated, budget, 0);

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
  JV limit(JV::object_tag);
  limit["files_scanned"] = JV(static_cast<int64_t>(budget.files));
  limit["bytes_scanned"] = JV(static_cast<int64_t>(budget.total_bytes));
  limit["max_files"] = JV(static_cast<int64_t>(GDA_SCAN_MAX_FILES));
  limit["max_file_bytes"] = JV(static_cast<int64_t>(GDA_SCAN_MAX_FILE_BYTES));
  limit["max_total_bytes"] = JV(static_cast<int64_t>(GDA_SCAN_MAX_TOTAL_BYTES));
  limit["max_depth"] = JV(static_cast<int64_t>(GDA_SCAN_MAX_DEPTH));
  if (!budget.reason.empty())
    limit["reason"] = JV(budget.reason);
  inner["limit"] = std::move(limit);
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "find_in_files completed");
  return r;
}

} // namespace text_ops
} // namespace godot_autopilot
