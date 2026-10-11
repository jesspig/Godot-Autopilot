#include "file_dir_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace file_dir_ops {

using JV = mcp::JsonValue;

namespace {

std::string join_file_path(const std::string &dir, const std::string &name) {
  return dir + (dir.empty() || dir.back() == '/' ? std::string() : "/") + name;
}

bool is_hidden_entry(const std::string &name) {
  return !name.empty() && name[0] == '.';
}

} // namespace

JV handle_list_directory(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "list_directory called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString())
    return util::error_json("missing required parameter: path");
  bool include_hidden = false;
  auto *hp = args.Find("include_hidden");
  if (hp && hp->IsBool())
    include_hidden = hp->GetBool();
  int max_entries = 500;
  auto *mp = args.Find("max_entries");
  if (mp && mp->IsInt())
    max_entries = static_cast<int>(mp->GetInt());
  if (max_entries < 1)
    max_entries = 1;
  const auto checked = util::normalize_project_path(pp->GetString(), true);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "list only inside the project namespaces");
  const std::string dir = checked.value;
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null())
    return util::error_json("failed to open directory: " + pp->GetString());
  std::vector<std::pair<std::string, bool>> collected;
  bool truncated = false;
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != "..") {
      std::string name = util::to_std(entry);
      bool hidden = is_hidden_entry(name);
      if (!hidden || include_hidden) {
        if (static_cast<int>(collected.size()) >= max_entries) {
          truncated = true;
          break;
        }
        collected.emplace_back(name, da->current_is_dir());
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
  JV entries(JV::array_tag);
  for (const auto &item : collected) {
    JV obj(JV::object_tag);
    obj["name"] = JV(item.first);
    obj["path"] = JV(join_file_path(dir, item.first));
    obj["is_dir"] = JV(item.second);
    entries.PushBack(std::move(obj));
  }
  JV inner(JV::object_tag);
  inner["path"] = JV(dir);
  inner["entries"] = std::move(entries);
  inner["total"] = JV(static_cast<int64_t>(collected.size()));
  inner["truncated"] = JV(truncated);
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "list_directory completed");
  return r;
}

JV handle_get_file_info(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_file_info called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString())
    return util::error_json("missing required parameter: path");
  const auto checked = util::normalize_project_path(pp->GetString(), true);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "use a res:// or user:// project path");
  const std::string path = checked.value;
  godot::String path_gs(path.c_str());
  bool is_dir = godot::DirAccess::dir_exists_absolute(path_gs);
  bool is_file = false;
  if (!is_dir)
    is_file = godot::FileAccess::file_exists(path_gs);
  bool exists = is_dir || is_file;
  int64_t size = 0;
  int64_t modified_time = 0;
  if (is_file) {
    auto file = godot::FileAccess::open(path_gs, godot::FileAccess::READ);
    if (!file.is_null()) {
      size = file->get_length();
      file->close();
    }
    modified_time =
        static_cast<int64_t>(godot::FileAccess::get_modified_time(path_gs));
  }
  JV inner(JV::object_tag);
  inner["path"] = JV(path);
  inner["exists"] = JV(exists);
  inner["is_dir"] = JV(is_dir);
  inner["is_file"] = JV(is_file);
  inner["size"] = JV(size);
  inner["modified_time"] = JV(modified_time);
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_file_info completed");
  return r;
}

JV handle_copy_file_generic(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "copy_file called");
  auto *sp = args.Find("path");
  if (!sp || !sp->IsString())
    return util::error_json("missing required parameter: path");
  auto *dp = args.Find("dest_path");
  if (!dp || !dp->IsString())
    return util::error_json("missing required parameter: dest_path");
  const auto checked_src =
      util::normalize_project_path(sp->GetString(), true, false);
  if (!checked_src.valid())
    return util::error_detail("path rejected", "path", checked_src.error,
                              "copy only inside the project namespaces");
  const auto checked_dst =
      util::normalize_project_path(dp->GetString(), true, false);
  if (!checked_dst.valid())
    return util::error_detail("path rejected", "dest_path", checked_dst.error,
                              "copy only inside the project namespaces");
  const std::string src = checked_src.value;
  const std::string dest = checked_dst.value;
  if (src == dest)
    return util::error_json("source and destination are the same file: " +
                            src);
  godot::String src_gs(src.c_str());
  godot::String dest_gs(dest.c_str());
  if (!godot::FileAccess::file_exists(src_gs))
    return util::error_json("source does not exist: " + src);
  if (godot::DirAccess::dir_exists_absolute(dest_gs))
    return util::error_json("destination is a directory: " + dest);
  bool overwritten = godot::FileAccess::file_exists(dest_gs);
  const std::string parent = resource_fs::parent_directory_of(dest);
  if (!resource_fs::is_resource_root_dir(parent)) {
    godot::String parent_gs(parent.c_str());
    if (!godot::DirAccess::dir_exists_absolute(parent_gs)) {
      godot::Error mk_err =
          godot::DirAccess::make_dir_recursive_absolute(parent_gs);
      if (mk_err != godot::OK)
        return util::error_json("failed to create parent directory: " +
                                parent + " (error " +
                                std::to_string(static_cast<int>(mk_err)) + ")");
    }
  }
  godot::Error err = godot::DirAccess::copy_absolute(src_gs, dest_gs);
  if (err != godot::OK)
    return util::error_json("failed to copy file: " + src + " -> " + dest +
                            " (error " + std::to_string(static_cast<int>(err)) +
                            ")");
  JV r(JV::object_tag);
  r["result"] = JV("copied");
  r["from"] = JV(src);
  r["to"] = JV(dest);
  r["overwritten"] = JV(overwritten);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "copy_file completed");
  return r;
}

JV handle_ensure_directory(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "ensure_directory called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString())
    return util::error_json("missing required parameter: path");
  const auto checked =
      util::normalize_project_path(pp->GetString(), true, false);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "create directories only below the namespaces");
  const std::string dir = checked.value;
  godot::String dir_gs(dir.c_str());
  bool already_existed = godot::DirAccess::dir_exists_absolute(dir_gs);
  if (!already_existed) {
    godot::Error err = godot::DirAccess::make_dir_recursive_absolute(dir_gs);
    if (err != godot::OK)
      return util::error_json("failed to create directory: " + dir +
                              " (error " +
                              std::to_string(static_cast<int>(err)) + ")");
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["path"] = JV(dir);
  r["already_existed"] = JV(already_existed);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "ensure_directory completed");
  return r;
}

} // namespace file_dir_ops
} // namespace godot_autopilot
