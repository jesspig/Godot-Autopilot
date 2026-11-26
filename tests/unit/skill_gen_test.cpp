#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include "tools/base/tool_catalog.hpp"
#include "util/skill_gen.hpp"
#include <version.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace godot_autopilot::skill_gen;

constexpr size_t kSkillCount = 1;
constexpr size_t kSkillFileCount = 24;
const char *const kExpectedSkillNames[kSkillCount] = {
    "godot-autopilot"};

const char *const kManualWhitelist[] = {
    "new_errors_since_last_call", "retry_after_ms", "memory", "user", "res",
    "godot", "at_frame", "stop_on_error", "rollback_on_error", "parent_path",
    "max_depth", "include_properties", "source_code", "function_name",
    "timeout_ms", "auto_owner", "SceneRoot", "target", "retryable", "isError",
    "true", "false", "theme_type",
    "godot_autopilot", "editor_readiness", "resource_ops", "record_error",
    "writes_file", "writes_scene", "is_error", "confirm_required",
    "already_playing", "physics_stalled", "node_count", "scan_truncated",
    "just_pressed", "global_rect", "auth_mode", "authorization_mode",
    "check_equal", "make_current", "close_scene", "get_tile_data",
    "target_position", "sprite_frames", "custom_minimum_size", "motion_mode",
    "cone_twist", "generic_6dof", "axis_lock", "collision_exception",
    "game_", "create_",
    "add_render_canvas_item_", "api_type", "call_method",
    "can_instantiate", "cast_motion", "cast_to", "check_almost_equal",
    "collide_shape", "collider_id", "create_render_", "dependency_count",
    "elapsed_ms", "from_archive", "gda_test", "get_node", "get_property",
    "get_rest_info", "get_singleton", "get_world_3d", "hint_string", "ignored_params",
    "instance_id", "instantiate_failed", "intersect_point", "intersect_ray",
    "intersect_shape", "is_playing", "is_valid", "just_released",
    "keep_state", "last_activity_ms", "load_failed", "missing_dependencies",
    "missing_dependency", "mouse_button", "mouse_motion", "parent_class",
    "physics_frame", "recent_engine_errors", "running_over_budget",
    "set_property", "total_lines", "unresolved_uids",
    "action_press", "action_release", "alternative_tile", "anchor_",
    "anchors_preset", "applied_properties", "button_pressed", "call_deferred", "can_process",
    "converted_node_paths", "create_tile",
    "create_timer", "current_physics_frame", "dest_files", "dest_md5",
    "exact_match", "ext_resource", "failed_property", "find_track", "from_uid_path",
    "full_rect", "game_runtime", "gd_resource", "gd_scene",
    "generator_parameters", "get_bus_index", "group_file", "grow_",
    "import_sidecar_warning", "import_sidecar_warnings", "importer_defaults",
    "importer_name", "importer_version", "inline_resources", "instance_placeholder",
    "is_action_just_pressed", "is_action_just_pressed_by_event",
    "is_action_pressed", "is_editor_hint", "is_hovered", "is_input_action_",
    "is_instance_valid", "key_label", "layout_mode", "live_", "load_steps",
    "map_get_iteration_id", "mark_unsaved", "max_errors_per_second",
    "max_polyphony", "max_queued_messages", "max_warnings_per_second",
    "modifies_window", "node_paths", "offset_", "owner_uid_path",
    "parent_id_path", "parse_input_event", "physical_keycode", "press_input_",
    "pressed_event_id", "process_events", "process_frame", "process_owner",
    "property_warnings", "queue_free", "release_input_", "runtime_node_select_", "scanning_changes",
    "scripting_enabled", "send_error", "send_message", "set_active",
    "set_anchors_and_offsets_preset", "set_anchors_preset", "set_cell",
    "set_custom_mouse_cursor", "shows_alert", "side_effect", "source_file",
    "source_md5", "sub_resource", "tile_map_data", "tile_set", "to_uid_path",
    "track_insert_key", "tree_exited", "uid_cache", "unique_id",
    "use_multiple_threads", "writes_config", "z_index", "set_suspend",
    "hit_path", "hit_mismatch", "mouse_filter", "possibly_occluded",
    "get_physics_frames", "is_on_floor", "is_on_wall", "move_and_slide",
    "assembly_loaded", "assembly_name", "converted_node_path",
    "dotnet_available", "dotnet_version", "errors_truncated", "exit_code",
    "game_running", "instrumentation_node_count", "next_seq", "output_digest",
    "output_empty", "output_truncated", "project_file", "project_path",
    "restart_required", "runtime_error", "target_framework", "csproj_ref_exists",
    "engine_version", "is_dotnet_editor",
    "captured_frame", "saved_path", "when_satisfied_frame", "gda_capture_",

    "register_tool", "unregister_tool", "has_tool", "list_tools", "get_tool",
    "is_enabled", "set_enabled", "rescan", "register_autopilot_tools",
    "user_tools", "definition", "callable", "handle", "scanned", "registered",
    "failed", "errors", "directory", "enabled",

    "gda",

    "mcp_url", "port_source", "toolset_hash", "plugin_version", "cli_version",
    "protocol_version", "ipc_kind", "instance_id", "schema_version",
    "request_sent", "code_execute", "discard_unsaved",
    "confirmation_required", "converted_node_path", "scene_path",
    "scene_unsaved", "node_released", "applied_properties",
    "property_warnings", "inline_resources", "converted_node_paths",
    "failed_property", "when_satisfied_frame", "after_frames",
    "max_dimension", "args_file", "await_async", "target_path",
    "new_parent_path", "keep_world_position", "only_script_variables",
    "property_filter", "dry_run", "diagnostic_scope", "parse_error",
    "assembly_checked", "gda_runtime", "global_position", "enter_tree",
    "exit_tree", "physics_process", "unhandled_input", "gui_input",
    "static_init", "node_ref", "pivot_offset", "sub_resource",
    "add_render_canvas_item", "eval_cancelled", "eval_timeout",
    "exec_timeout", "main_thread_timeout", "queue_unavailable",
    "get_node_or_null", "rect_global_position", "rect_min_size",
    "rect_pivot_offset", "rect_position", "rect_rotation", "rect_scale",
    "rect_size",

    "dynamic", "required", "type", "description", "error", "file", "args",
    "name", "params", "tags", "category", "instance_ambiguous",
    "no_active_instance",

    "already_connected", "apply_physics_", "closed_tabs", "closed_tabs_warning",
    "create_physics_", "csharp_", "engine_managed", "execution_time_ms",
    "extension_api", "get_physics_", "main_scene", "not_connected",
    "registered_resource", "reloaded_scenes", "saved_scenes", "script_class",
    "set_physics_", "stale_references", "uid_preserved",
    "unsupported_binary_references", "update_file", "updated_files",
    "when_timeout", "would_delete", "wrapped_source",
    "channel_lost", "game_ready_timeout", "no_scene_open", "scene_ready",
    "script_error", "startup_in_progress", "timeout_cause", "viewport_degenerate",
    "next_action", "ready_diagnosis", "ready_lost", "scene_not_ready",
    "extension_not_loaded", "hello_attempts", "invalid_input_response",
    "missing_csharp_runtime", "csharp_script_not_compiled", "disk_persisted",
    "autosaved", "disk_changed", "autosave_setting", "runtime_mountable",
    "editor_runnable", "already_present", "script_class_not_found",
    "base_type_not_node", "abstract_script", "csharp_bridge"};

bool is_word_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_';
}

bool is_lower_alnum(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool is_lower_alnum_us(char c) { return is_lower_alnum(c) || c == '_'; }

void collect_snake_words(const std::string &segment, std::set<std::string> *out) {
  const size_t n = segment.size();
  size_t i = 0;
  while (i < n) {
    const bool word_start = (i == 0) || !is_word_char(segment[i - 1]);
    if (word_start && segment[i] >= 'a' && segment[i] <= 'z') {
      size_t j = i + 1;
      while (j < n && is_lower_alnum(segment[j])) {
        ++j;
      }
      if (j < n && segment[j] == '_') {
        size_t k = j + 1;
        while (k < n && is_lower_alnum_us(segment[k])) {
          ++k;
        }
        out->insert(segment.substr(i, k - i));
        i = k;
        continue;
      }
    }
    ++i;
  }
}

std::set<std::string> extract_backtick_snake_words(const std::string &text) {
  std::set<std::string> words;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t tick = text.find('`', pos);
    if (tick == std::string::npos) {
      break;
    }
    if (text.compare(tick, 3, "```") == 0) {
      const size_t close = text.find("```", tick + 3);
      if (close == std::string::npos) {
        break;
      }
      pos = close + 3;
      continue;
    }
    const size_t end = text.find('`', tick + 1);
    if (end == std::string::npos) {
      break;
    }
    collect_snake_words(text.substr(tick + 1, end - tick - 1), &words);
    pos = end + 1;
  }
  return words;
}

bool matches_skill_name(const std::string &name) {
  if (name.empty()) {
    return false;
  }
  size_t i = 0;
  if (!is_lower_alnum(name[i])) {
    return false;
  }
  while (i < name.size() && is_lower_alnum(name[i])) {
    ++i;
  }
  while (i < name.size()) {
    if (name[i] != '-') {
      return false;
    }
    ++i;
    if (i >= name.size() || !is_lower_alnum(name[i])) {
      return false;
    }
    while (i < name.size() && is_lower_alnum(name[i])) {
      ++i;
    }
  }
  return true;
}

bool starts_with(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

bool ends_with(const std::string &text, const char *suffix) {
  const std::string tail(suffix);
  return text.size() >= tail.size() &&
         text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

char ascii_lower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool contains_ci_token(const std::string &text, const char *token) {
  const std::string needle(token);
  const size_t n = needle.size();
  if (n == 0 || text.size() < n) {
    return false;
  }
  for (size_t i = 0; i + n <= text.size(); ++i) {
    bool same = true;
    for (size_t k = 0; k < n; ++k) {
      if (ascii_lower(text[i + k]) != ascii_lower(needle[k])) {
        same = false;
        break;
      }
    }
    if (!same) {
      continue;
    }
    const bool left_ok = i == 0 || !is_word_char(text[i - 1]);
    const bool right_ok = i + n >= text.size() || !is_word_char(text[i + n]);
    if (left_ok && right_ok) {
      return true;
    }
  }
  return false;
}

bool contains_token(const std::string &text, const char *token) {
  const std::string needle(token);
  if (needle.compare(0, 2, "--") != 0) {
    return text.find(needle) != std::string::npos;
  }
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t at = text.find(needle, pos);
    if (at == std::string::npos) {
      return false;
    }
    const size_t after = at + needle.size();
    if (after < text.size() &&
        (is_lower_alnum(text[after]) || text[after] == '-')) {
      pos = at + 1;
      continue;
    }
    return true;
  }
  return false;
}

bool is_skill_aux_path(const std::string &path) {
  if (starts_with(path, "references/")) {
    return true;
  }
  return starts_with(path, "scripts/") && ends_with(path, ".mjs");
}

class SkillCatalogFixture : public ::testing::Test {
protected:
  godot_autopilot::ToolCatalog catalog;

  SkillCatalogFixture() {
    godot_autopilot::ToolInfo info;
    info.name = "tool_call";
    info.category = "meta";
    info.description = "generic canonical tool call";
    info.input_schema = godot_autopilot::util::JsonValue::Parse(
        R"({"type":"object","properties":{"action":{"type":"string","enum":["status","errors","output","log","get","set","create","delete","list","play","stop","validate_scene","sample","sequence","object","node","memory","catalog","monitors","replace","trash","install","save","read","write","query","describe","call","batch"]},"path":{"type":"string"},"confirm":{"type":"boolean"}}})");
    catalog.add_tool(info);
    catalog.replace_aliases({{"call", "tool_call"}});
  }

  const godot_autopilot::ToolCatalog &registry_catalog() const {
    return catalog;
  }
};

TEST(SkillGenTest, AllSkillsPresent) {
  const std::vector<SkillSpec> skills = all_skills();
  ASSERT_EQ(skills.size(), kSkillCount);

  std::set<std::string> expected(kExpectedSkillNames,
                                 kExpectedSkillNames + kSkillCount);
  std::set<std::string> actual;
  for (const SkillSpec &spec : skills) {
    actual.insert(spec.name);
    EXPECT_TRUE(expected.count(spec.name) > 0)
        << "unexpected skill name: " << spec.name;
  }
  for (const char *name : kExpectedSkillNames) {
    EXPECT_TRUE(actual.count(name) > 0) << "missing skill: " << name;
  }
}

TEST(SkillGenTest, SkillNamesFollowSpecRules) {
  for (const SkillSpec &spec : all_skills()) {
    EXPECT_FALSE(spec.name.empty()) << "empty skill name";
    EXPECT_LE(spec.name.size(), size_t{64}) << "name too long: " << spec.name;
    EXPECT_TRUE(matches_skill_name(spec.name))
        << "name violates ^[a-z0-9]+(-[a-z0-9]+)*$: " << spec.name;
    EXPECT_EQ(skill_file_path(spec.name, "SKILL.md"),
              ".agents/skills/" + spec.name + "/SKILL.md")
        << spec.name;
  }
}

TEST(SkillGenTest, DescriptionsValid) {
  for (const SkillSpec &spec : all_skills()) {
    EXPECT_FALSE(spec.description.empty()) << "empty description: " << spec.name;
    EXPECT_LE(spec.description.size(), size_t{1024})
        << "description too long: " << spec.name;
  }
}

TEST(SkillGenTest, FilesLayout) {
  for (const SkillSpec &spec : all_skills()) {
    ASSERT_FALSE(spec.files.empty()) << "no files: " << spec.name;
    EXPECT_EQ(spec.files[0].relative_path, "SKILL.md")
        << "files[0] must be SKILL.md: " << spec.name;
    for (size_t i = 1; i < spec.files.size(); ++i) {
      EXPECT_TRUE(is_skill_aux_path(spec.files[i].relative_path))
          << spec.name << ": file path must start with 'references/' or 'scripts/': "
          << spec.files[i].relative_path;
    }
    for (const SkillFile &file : spec.files) {
      EXPECT_FALSE(file.body.empty())
          << spec.name << "/" << file.relative_path << ": empty body";
      EXPECT_EQ(file.body.find("PLACEHOLDER"), std::string::npos)
          << spec.name << "/" << file.relative_path
          << ": body still contains PLACEHOLDER";
      EXPECT_EQ(file.body.find(")gda_skill\""), std::string::npos)
          << spec.name << "/" << file.relative_path
          << ": body contains raw string terminator sequence";
    }
  }
}

TEST(SkillGenTest, RenderedFrontmatterWellFormed) {
  for (const SkillSpec &spec : all_skills()) {
    ASSERT_FALSE(spec.files.empty()) << spec.name;
    const std::string rendered = render_skill_md(spec);

    EXPECT_EQ(rendered.rfind("---\n", 0), 0u)
        << spec.name << ": output must start with '---\\n'";
    EXPECT_NE(rendered.find("\nname: " + spec.name + "\n"), std::string::npos)
        << spec.name << ": missing 'name: <name>' line";
    EXPECT_NE(rendered.find("author: godot-autopilot"), std::string::npos)
        << spec.name << ": missing author line";
    const std::string version_line =
        "version: \"" + std::string(GDA_VERSION) + "\"";
    EXPECT_NE(rendered.find(version_line), std::string::npos)
        << spec.name << ": missing " << version_line << " line";

    const std::string plain_desc =
        "description: " + spec.description + "\n";
    const std::string quoted_desc =
        "description: \"" + spec.description + "\"\n";
    if (spec.description.find(": ") != std::string::npos) {
      EXPECT_NE(rendered.find(quoted_desc), std::string::npos)
          << spec.name << ": description containing ': ' must be quoted";
    } else {
      EXPECT_TRUE(rendered.find(plain_desc) != std::string::npos ||
                  rendered.find(quoted_desc) != std::string::npos)
          << spec.name << ": description line missing or altered";
    }

    const size_t metadata = rendered.find("metadata:");
    ASSERT_NE(metadata, std::string::npos) << spec.name;
    const size_t fm_end = rendered.find("\n---", metadata);
    ASSERT_NE(fm_end, std::string::npos)
        << spec.name << ": frontmatter must end with a second '---'";
    EXPECT_EQ(rendered.compare(fm_end, 6, "\n---\n\n"), 0)
        << spec.name << ": frontmatter closing '---' malformed";
    EXPECT_EQ(rendered.compare(fm_end + 6, spec.files[0].body.size(),
                               spec.files[0].body),
              0)
        << spec.name << ": SKILL.md body mismatch after frontmatter";
  }
}

TEST_F(SkillCatalogFixture, ToolNamesExistInConstructedCatalog) {
  std::set<std::string> allowed;
  for (const godot_autopilot::ToolInfo &tool :
       registry_catalog().get_all_tools()) {
    allowed.insert(tool.name);
    const godot_autopilot::util::JsonValue *props = tool.input_schema.Find("properties");
    if (props != nullptr && props->IsObject()) {
      for (const auto &entry : *props) {
        allowed.insert(entry.first);
      }
    }
  }
  for (const std::string &alias : registry_catalog().alias_names()) {
    allowed.insert(alias);
  }
  for (const char *word : kManualWhitelist) {
    allowed.insert(word);
  }

  for (const SkillSpec &spec : all_skills()) {
    for (const SkillFile &file : spec.files) {
      if (!ends_with(file.relative_path, ".md")) {
        continue;
      }
      const std::set<std::string> words =
          extract_backtick_snake_words(file.body);
      for (const std::string &word : words) {
        EXPECT_TRUE(allowed.count(word) > 0)
            << "skill '" << spec.name << "' file '" << file.relative_path
            << "' references unknown backtick word: `" << word << "`";
      }
    }
  }
}

TEST(SkillGenTest, SkillBooksDescribeCliDiscovery) {
  const std::vector<SkillSpec> all = all_skills();
  const SkillSpec *entry_skill = nullptr;
  for (const SkillSpec &spec : all) {
    if (spec.name == "godot-autopilot") {
      entry_skill = &spec;
    }
  }
  ASSERT_NE(entry_skill, nullptr);
  ASSERT_FALSE(entry_skill->files.empty());
  const std::string &body = entry_skill->files[0].body;
  EXPECT_NE(body.find("gda tool list"), std::string::npos);
  EXPECT_NE(body.find("gda tool search"), std::string::npos);
  EXPECT_NE(body.find("gda tool describe"), std::string::npos);
  EXPECT_NE(body.find("gda tool call"), std::string::npos);
  EXPECT_NE(body.find("confirmation_required"), std::string::npos);
  EXPECT_NE(body.find("--confirm"), std::string::npos);
}

TEST(SkillGenTest, CliChannelContract) {
  const std::vector<SkillSpec> skills = all_skills();
  for (const SkillSpec &spec : skills) {
    ASSERT_FALSE(spec.files.empty()) << spec.name;
    const std::string &body = spec.files[0].body;
    EXPECT_NE(body.find("Godot Autopilot"), std::string::npos)
        << spec.name << ": SKILL.md must mention the Godot Autopilot entry";
    if (spec.name == "godot-autopilot") {
      EXPECT_TRUE(contains_ci_token(body, "gda"))
          << spec.name << ": SKILL.md must mention the gda CLI channel";
      EXPECT_NE(body.find("gda tool list"), std::string::npos);
      EXPECT_NE(body.find("gda tool call"), std::string::npos);
    }
    for (const SkillFile &file : spec.files) {
      if (!ends_with(file.relative_path, ".md")) {
        continue;
      }
      EXPECT_EQ(file.body.find("curl -"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("Invoke-WebRequest"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("bunx"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("9527"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("gda serve"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("gda api"), std::string::npos)
          << spec.name << "/" << file.relative_path;
      EXPECT_EQ(file.body.find("Allow write"), std::string::npos)
          << spec.name << "/" << file.relative_path;
    }
  }
}

TEST(SkillGenTest, SkillBooksCarryNoScriptBridge) {
  size_t total_files = 0;
  for (const SkillSpec &spec : all_skills()) {
    EXPECT_EQ(spec.description.find("gda.mjs"), std::string::npos)
        << spec.name << ": description must not mention the retired script bridge";
    for (const SkillFile &file : spec.files) {
      ++total_files;
      EXPECT_FALSE(ends_with(file.relative_path, ".mjs"))
          << spec.name << "/" << file.relative_path
          << ": skill books must not ship script files";
      EXPECT_EQ(file.body.find("gda.mjs"), std::string::npos)
          << spec.name << "/" << file.relative_path
          << ": skill books must not mention the retired script bridge";
    }
  }
  EXPECT_EQ(total_files, kSkillFileCount) << "embedded skill file count";
}

TEST(SkillGenTest, EverySkillHasSkillMd) {
  for (const SkillSpec &spec : all_skills()) {
    ASSERT_FALSE(spec.files.empty()) << "no files: " << spec.name;
    EXPECT_EQ(spec.files[0].relative_path, "SKILL.md")
        << "files[0] must be SKILL.md: " << spec.name;
    for (size_t i = 1; i < spec.files.size(); ++i) {
      EXPECT_TRUE(is_skill_aux_path(spec.files[i].relative_path))
          << spec.name << ": auxiliary file must be under references/: "
          << spec.files[i].relative_path;
    }
  }
}

const char *const kMcpTokens[] = {
    "MCP", "Model Context Protocol", "mcp_url", "mcpServers",
    "godot://skills", "search_tools", "get_tool_detail", "call_tool",
    "list_categories", "resources/list", "resources/read", "prompts/list",
    "prompts/get", "tasks/get", "tasks/cancel", "resultType", "taskId",
    "--direct", "--transport", "--no-fallback", "--input", "--async"};

const char *const kDriftTokens[] = {
    "252", "253", "325", "0.4.x", "0.6.x", "0.4.0", "0.6.0"};

const char *const kRecipeIds[] = {
    "bootstrap", "scene-script", "asset-pipeline", "playtest-debug",
    "safe-refactor", "error-recovery", "skills-sync", "long-task"};

std::set<std::string> extract_action_values(const std::string &text) {
  std::set<std::string> found;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t tick = text.find('`', pos);
    if (tick == std::string::npos) {
      break;
    }
    if (text.compare(tick, 3, "```") == 0) {
      const size_t close = text.find("```", tick + 3);
      if (close == std::string::npos) {
        break;
      }
      const bool is_json = text.compare(tick, 7, "```json") == 0;
      const std::string block = text.substr(tick + 3, close - tick - 3);
      if (is_json) {
        try {
          const godot_autopilot::util::JsonValue parsed =
              godot_autopilot::util::JsonValue::Parse(block);
          if (parsed.IsObject()) {
            const godot_autopilot::util::JsonValue *action = parsed.Find("action");
            if (action != nullptr && action->IsString()) {
              found.insert(action->GetString());
            }
          }
        } catch (...) {
        }
      }
      pos = close + 3;
      continue;
    }
    const size_t end = text.find('`', tick + 1);
    if (end == std::string::npos) {
      break;
    }
    const std::string span = text.substr(tick + 1, end - tick - 1);
    const size_t marker = span.find("action=");
    if (marker != std::string::npos) {
      std::string value = span.substr(marker + 7);
      const size_t stop = value.find_first_of(" \t,;\"'");
      if (stop != std::string::npos) {
        value = value.substr(0, stop);
      }
      if (!value.empty()) {
        found.insert(value);
      }
    }
    pos = end + 1;
  }
  return found;
}

std::set<std::string> schema_actions(const godot_autopilot::ToolCatalog &catalog) {
  std::set<std::string> result;
  const auto tool = catalog.get_tool("tool_call");
  if (!tool.has_value()) {
    return result;
  }
  const godot_autopilot::util::JsonValue *props = tool->input_schema.Find("properties");
  if (props == nullptr || !props->IsObject()) {
    return result;
  }
  const godot_autopilot::util::JsonValue *action = props->Find("action");
  if (action == nullptr) {
    return result;
  }
  const godot_autopilot::util::JsonValue *values = action->Find("enum");
  if (values == nullptr || !values->IsArray()) {
    return result;
  }
  for (size_t i = 0; i < values->Size(); ++i) {
    const godot_autopilot::util::JsonValue &item = (*values)[i];
    if (item.IsString()) {
      result.insert(item.GetString());
    }
  }
  return result;
}

TEST(SkillGenTest, SkillBooksContainNoMcpContent) {
  for (const SkillSpec &spec : all_skills()) {
    for (const SkillFile &file : spec.files) {
      if (!ends_with(file.relative_path, ".md")) {
        continue;
      }
      for (const char *token : kMcpTokens) {
        EXPECT_FALSE(contains_token(file.body, token))
            << spec.name << "/" << file.relative_path << " mentions " << token;
      }
    }
  }
}

TEST(SkillGenTest, SkillBooksContainNoDriftFacts) {
  for (const SkillSpec &spec : all_skills()) {
    for (const SkillFile &file : spec.files) {
      if (!ends_with(file.relative_path, ".md")) {
        continue;
      }
      for (const char *token : kDriftTokens) {
        EXPECT_EQ(file.body.find(token), std::string::npos)
            << spec.name << "/" << file.relative_path << " mentions " << token;
      }
    }
  }
}

TEST(SkillGenTest, EntryCardCliOnlyContract) {
  const char *const required[] = {
      "gda ping", "gda tool list", "gda tool search", "gda tool describe",
      "gda tool call", "--confirm", "confirmation_required", "gda doctor",
      "gda job"};
  const std::vector<SkillSpec> all = all_skills();
  const SkillSpec *entry = nullptr;
  for (const SkillSpec &spec : all) {
    if (spec.name == "godot-autopilot") {
      entry = &spec;
    }
  }
  ASSERT_NE(entry, nullptr);
  ASSERT_FALSE(entry->files.empty());
  const std::string &body = entry->files[0].body;
  for (const char *token : required) {
    EXPECT_NE(body.find(token), std::string::npos)
        << "entry card is missing CLI-only element: " << token;
  }
  size_t routing_rows = 0;
  size_t line_start = 0;
  while (line_start < body.size()) {
    const size_t line_end = body.find('\n', line_start);
    const std::string line = body.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
    if (line.find("| ") != std::string::npos &&
        line.find("references/") != std::string::npos) {
      ++routing_rows;
    }
    if (line_end == std::string::npos) {
      break;
    }
    line_start = line_end + 1;
  }
  EXPECT_GE(routing_rows, size_t{3}) << "entry card must route domains to references";
}

TEST(SkillGenTest, RecipesHaveEightSections) {
  for (const char *id : kRecipeIds) {
    bool found = false;
    for (const SkillSpec &spec : all_skills()) {
      for (const SkillFile &file : spec.files) {
        const std::string expected = std::string("references/recipes/") + id + ".md";
        if (file.relative_path != expected) {
          continue;
        }
        found = true;
        const std::string heading = std::string("# ") + id;
        EXPECT_EQ(file.body.compare(0, heading.size(), heading), 0)
            << expected << " first heading must be " << heading;
        for (int section = 1; section <= 8; ++section) {
          const std::string marker = "## " + std::to_string(section) + ". ";
          EXPECT_NE(file.body.find(marker), std::string::npos)
              << expected << " is missing section " << section;
        }
      }
    }
    EXPECT_TRUE(found) << "missing recipe " << id;
  }
}

TEST_F(SkillCatalogFixture, BacktickActionsVerifiedAgainstSchema) {
  const std::set<std::string> allowed = schema_actions(catalog);
  ASSERT_FALSE(allowed.empty());
  for (const SkillSpec &spec : all_skills()) {
    for (const SkillFile &file : spec.files) {
      if (!ends_with(file.relative_path, ".md")) {
        continue;
      }
      for (const std::string &action : extract_action_values(file.body)) {
        EXPECT_TRUE(allowed.count(action) > 0)
            << "action `" << action << "` in " << spec.name << "/"
            << file.relative_path << " is absent from the constructed schema";
      }
    }
  }
}

} // namespace
