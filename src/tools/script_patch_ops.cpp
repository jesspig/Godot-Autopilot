#include "script_patch_ops.hpp"

#include "tools/script_ops.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/variant/string.hpp>
#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace script_patch_ops {

namespace {

constexpr size_t CONTEXT_RADIUS = 240;
constexpr size_t ANCHOR_ECHO_LIMIT = 120;

bool get_flag(const mcp::JsonValue &args, const char *name) {
  const mcp::JsonValue *flag = args.Find(name);
  return flag != nullptr && flag->IsBool() && flag->GetBool();
}

size_t count_occurrences(const std::string &haystack,
                         const std::string &needle) {
  if (needle.empty())
    return 0;
  size_t count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

std::string truncate_echo(const std::string &text, size_t limit) {
  if (text.size() <= limit)
    return text;
  return text.substr(0, limit) + "...(truncated, total " +
         std::to_string(text.size()) + " bytes)";
}

std::string context_around(const std::string &text, size_t pos, size_t len) {
  size_t begin = pos > CONTEXT_RADIUS ? pos - CONTEXT_RADIUS : 0;
  size_t end = pos + len + CONTEXT_RADIUS;
  if (end > text.size())
    end = text.size();
  std::string out;
  if (begin > 0)
    out += "...";
  out += text.substr(begin, end - begin);
  if (end < text.size())
    out += "...";
  return out;
}

bool read_disk_text(const std::string &path, std::string &out_text,
                    std::string &error_out) {
  auto file = godot::FileAccess::open(godot::String(path.c_str()),
                                      godot::FileAccess::READ);
  if (file.is_null() || !file->is_open()) {
    error_out = "failed to open script for reading: " + path;
    return false;
  }
  out_text = util::to_std(file->get_as_text());
  godot::Error read_err = file->get_error();
  if (read_err != godot::OK && read_err != godot::ERR_FILE_EOF) {
    error_out = "failed to read script: " + path;
    return false;
  }
  return true;
}

bool restore_disk_text(const std::string &path, const std::string &original,
                       std::string &error_out) {
  auto writer = godot::FileAccess::open(godot::String(path.c_str()),
                                        godot::FileAccess::WRITE);
  if (writer.is_null() || !writer->is_open()) {
    error_out = "rollback failed: cannot reopen file for writing: " + path;
    return false;
  }
  if (!writer->store_string(godot::String::utf8(original.c_str()))) {
    error_out = "rollback failed: could not restore original content: " + path;
    return false;
  }
  writer->close();
  if (writer->get_error() != godot::OK) {
    error_out = "rollback failed: error after restoring content: " + path;
    return false;
  }
  return true;
}

bool save_patched_script(const std::string &path, const std::string &patched,
                         std::string &error_out) {
  godot::Ref<godot::GDScript> script;
  script.instantiate();
  if (script.is_null()) {
    error_out = "failed to create GDScript";
    return false;
  }
  script->set_source_code(godot::String::utf8(patched.c_str()));
  script->set_path_cache(godot::String(path.c_str()));
  if (script->reload() != godot::OK) {
    error_out = "script compilation failed on save; no file was written";
    return false;
  }
  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver) {
    error_out = "ResourceSaver not available; no file was written";
    return false;
  }
  godot::Error err = saver->save(script, godot::String(path.c_str()));
  if (err != godot::OK) {
    error_out = "failed to save script, error code: " +
                std::to_string(static_cast<int>(err));
    return false;
  }
  return true;
}

void notify_editors(const std::string &path) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(godot::String(path.c_str()));
    }
  }
}

} // namespace

mcp::JsonValue handle_patch(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString() || it_path->GetString().empty()) {
    return util::error_json("missing required parameter: path");
  }
  auto *it_anchor = args.Find("anchor");
  if (!it_anchor || !it_anchor->IsString() || it_anchor->GetString().empty()) {
    return util::error_json("missing required parameter: anchor "
                            "(non-empty literal text to locate in the file)");
  }
  auto *it_replacement = args.Find("replacement");
  if (!it_replacement || !it_replacement->IsString()) {
    return util::error_json("missing required parameter: replacement "
                            "(new text; empty string deletes the anchor "
                            "in replace mode)");
  }
  std::string path = it_path->GetString();
  std::string anchor = it_anchor->GetString();
  std::string replacement = it_replacement->GetString();

  std::string mode = "replace";
  auto *it_mode = args.Find("mode");
  if (it_mode && it_mode->IsString())
    mode = it_mode->GetString();
  bool is_replace = mode == "replace";
  bool is_insert = mode == "insert";
  if (!is_replace && !is_insert) {
    return util::error_json("invalid mode: '" + mode +
                            "' — expected 'replace' or 'insert'");
  }

  bool preview = get_flag(args, "preview");
  bool dry_run = get_flag(args, "dry_run");
  bool preview_only = preview || dry_run;

  if (!godot::FileAccess::file_exists(godot::String(path.c_str()))) {
    return util::error_json("script file not found: " + path +
                            " — patch_script only patches existing files; "
                            "use create_script to create new files. "
                            "No file was written.");
  }

  std::string original;
  std::string read_error;
  if (!read_disk_text(path, original, read_error)) {
    return util::error_json(read_error + ". No file was written.");
  }

  size_t anchor_pos = original.find(anchor);
  size_t anchor_count = count_occurrences(original, anchor);
  if (anchor_pos == std::string::npos) {
    return util::error_json(
        "anchor not found in " + path + " (0 occurrences in " +
        std::to_string(original.size()) + " bytes; anchor starts with '" +
        truncate_echo(anchor, ANCHOR_ECHO_LIMIT) +
        "'). No file was written.");
  }

  std::string patched;
  size_t edit_pos = anchor_pos;
  if (is_replace) {
    patched = original.substr(0, anchor_pos) + replacement +
              original.substr(anchor_pos + anchor.size());
  } else {
    edit_pos = anchor_pos + anchor.size();
    patched = original.substr(0, edit_pos) + replacement +
              original.substr(edit_pos);
  }

  if (patched == original) {
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["path"] = mcp::JsonValue(path);
    j["mode"] = mcp::JsonValue(mode);
    j["anchor_found"] = mcp::JsonValue(true);
    j["anchor_index"] = mcp::JsonValue(static_cast<int64_t>(anchor_pos));
    j["anchor_occurrences"] =
        mcp::JsonValue(static_cast<int64_t>(anchor_count));
    j["unchanged"] = mcp::JsonValue(true);
    j["disk_write"] = mcp::JsonValue(false);
    j["verified"] = mcp::JsonValue(true);
    j["readback"] = mcp::JsonValue(true);
    j["disk_bytes"] =
        mcp::JsonValue(static_cast<int64_t>(original.size()));
    j["content_hash"] = mcp::JsonValue(script_ops::fnv1a64_hex(original));
    j["content_hash_algo"] =
        mcp::JsonValue(script_ops::content_hash_algo_name());
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    r["note"] =
        mcp::JsonValue("patched text is identical to the file on disk; "
                       "nothing was written");
    return r;
  }

  if (preview_only) {
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["path"] = mcp::JsonValue(path);
    j["mode"] = mcp::JsonValue(mode);
    j["anchor_found"] = mcp::JsonValue(true);
    j["anchor_index"] = mcp::JsonValue(static_cast<int64_t>(anchor_pos));
    j["anchor_occurrences"] =
        mcp::JsonValue(static_cast<int64_t>(anchor_count));
    j["preview"] = mcp::JsonValue(preview);
    j["dry_run"] = mcp::JsonValue(dry_run);
    j["disk_write"] = mcp::JsonValue(false);
    j["original_bytes"] =
        mcp::JsonValue(static_cast<int64_t>(original.size()));
    j["patched_bytes"] =
        mcp::JsonValue(static_cast<int64_t>(patched.size()));
    j["before_context"] =
        mcp::JsonValue(context_around(original, anchor_pos, anchor.size()));
    j["after_context"] = mcp::JsonValue(
        context_around(patched, edit_pos, replacement.size()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    r["note"] = mcp::JsonValue(
        "preview only — nothing was written and the patched text was not "
        "compiled; the formal write compiles before saving and aborts with "
        "zero disk writes on compilation failure");
    return r;
  }

  std::string compile_error;
  if (!script_ops::try_compile_source(patched, path, compile_error)) {
    return util::error_json(compile_error + " No file was written.");
  }

  std::string save_error;
  if (!save_patched_script(path, patched, save_error)) {
    std::string rollback_error;
    bool rolled_back = restore_disk_text(path, original, rollback_error);
    std::string message = save_error + " Original content " +
                          (rolled_back ? "was restored."
                                       : "RESTORE FAILED: " + rollback_error);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(message);
    e["disk_write"] = mcp::JsonValue(true);
    e["rolled_back"] = mcp::JsonValue(rolled_back);
    return e;
  }

  bool verified = false;
  bool readback = false;
  std::string read_back;
  std::string write_issue;
  std::string write_warning;
  auto readback_file = godot::FileAccess::open(
      godot::String(path.c_str()), godot::FileAccess::READ);
  if (readback_file.is_valid() && readback_file->is_open()) {
    readback = true;
    read_back = util::to_std(readback_file->get_as_text());
    godot::Error read_err = readback_file->get_error();
    if (read_err != godot::OK && read_err != godot::ERR_FILE_EOF) {
      write_issue = "readback read error";
    } else if (read_back == patched) {
      verified = true;
    } else {
      write_issue = "readback mismatch (patched " +
                    std::to_string(patched.size()) + " bytes, read back " +
                    std::to_string(read_back.size()) + " bytes)";
      write_warning =
          "file may not have been updated on disk; retry or check file locks "
          "(e.g. the running game or antivirus)";
    }
  } else {
    write_issue = "readback open failed";
  }

  bool rolled_back = false;
  std::string rollback_error;
  if (!verified) {
    rolled_back = restore_disk_text(path, original, rollback_error);
  }

  notify_editors(path);
  std::string cache_refresh_error;
  bool cache_refreshed = script_ops::refresh_script_cache(path, cache_refresh_error);

  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["path"] = mcp::JsonValue(path);
  j["mode"] = mcp::JsonValue(mode);
  j["anchor_found"] = mcp::JsonValue(true);
  j["anchor_index"] = mcp::JsonValue(static_cast<int64_t>(anchor_pos));
  j["anchor_occurrences"] =
      mcp::JsonValue(static_cast<int64_t>(anchor_count));
  j["disk_write"] = mcp::JsonValue(true);
  j["verified"] = mcp::JsonValue(verified);
  j["readback"] = mcp::JsonValue(readback);
  if (readback) {
    j["disk_bytes"] = mcp::JsonValue(static_cast<int64_t>(read_back.size()));
    j["content_hash"] = mcp::JsonValue(script_ops::fnv1a64_hex(read_back));
    j["content_hash_algo"] =
        mcp::JsonValue(script_ops::content_hash_algo_name());
  }
  if (!write_issue.empty()) {
    j["write_issue"] = mcp::JsonValue(write_issue);
  }
  if (!write_warning.empty()) {
    j["warning"] = mcp::JsonValue(write_warning);
  }
  j["rolled_back"] = mcp::JsonValue(rolled_back);
  if (!verified && !rolled_back && !rollback_error.empty()) {
    j["rollback_error"] = mcp::JsonValue(rollback_error);
  }
  j["cache_refreshed"] = mcp::JsonValue(cache_refreshed);
  if (!cache_refreshed) {
    j["cache_refresh_error"] = mcp::JsonValue(cache_refresh_error);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  r["note"] = mcp::JsonValue(
      "editor cache was refreshed for this file; if a game is running with the "
      "old script, call reload_game_scripts first and then "
      "reload_current_scene (or retry the operation) — reloading the scene "
      "alone does not guarantee the on-disk version is re-read "
      "(CACHE_MODE_REUSE)");
  return r;
}

} // namespace script_patch_ops
} // namespace godot_autopilot
