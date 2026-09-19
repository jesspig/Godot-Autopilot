#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>
#include <mcp/client/McpClient.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/command_queue.hpp"
#include "tools/register_all.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include "util/skill_gen.hpp"
#include <version.hpp>

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace godot_autopilot::skill_gen;

constexpr size_t kSkillCount = 8;
const char *const kExpectedSkillNames[kSkillCount] = {
    "godot-autopilot",
    "godot-autopilot-scene-system",
    "godot-autopilot-resources",
    "godot-autopilot-scripting",
    "godot-autopilot-runtime",
    "godot-autopilot-servers",
    "godot-autopilot-content",
    "godot-autopilot-csharp"};

const char *const kManualWhitelist[] = {
    "new_errors_since_last_call", "retry_after_ms", "memory", "user", "res",
    "godot", "at_frame", "stop_on_error", "rollback_on_error", "parent_path",
    "max_depth", "include_properties", "source_code", "function_name",
    "timeout_ms", "auto_owner", "SceneRoot", "target", "retryable", "isError",
    "true", "false",
    "godot_autopilot", "editor_readiness", "resource_ops", "record_error",
    "writes_file", "is_error",
    "already_playing", "physics_stalled", "node_count", "scan_truncated",
    "just_pressed", "global_rect",
    "check_equal", "make_current", "close_scene", "get_tile_data",
    "target_position", "sprite_frames", "custom_minimum_size", "motion_mode",
    "cone_twist", "generic_6dof", "axis_lock", "collision_exception",
    "game_", "create_",
    "add_render_canvas_item_", "api_type", "call_method",
    "can_instantiate", "cast_motion", "cast_to", "check_almost_equal",
    "collide_shape", "collider_id", "create_render_", "dependency_count",
    "elapsed_ms", "from_archive", "gda_test", "get_node", "get_property",
    "get_rest_info", "get_world_3d", "hint_string", "ignored_params",
    "instance_id", "instantiate_failed", "intersect_point", "intersect_ray",
    "intersect_shape", "is_playing", "is_valid", "just_released",
    "keep_state", "last_activity_ms", "load_failed", "missing_dependencies",
    "missing_dependency", "mouse_button", "mouse_motion", "parent_class",
    "physics_frame", "recent_engine_errors", "running_over_budget",
    "set_property", "total_lines", "unresolved_uids",
    "action_press", "action_release", "alternative_tile", "anchor_",
    "anchors_preset", "call_deferred", "can_process", "create_tile",
    "create_timer", "current_physics_frame", "dest_files", "dest_md5",
    "exact_match", "ext_resource", "find_track", "from_uid_path",
    "full_rect", "game_runtime", "gd_resource", "gd_scene",
    "generator_parameters", "get_bus_index", "group_file", "grow_",
    "import_sidecar_warning", "import_sidecar_warnings", "importer_defaults",
    "importer_name", "importer_version", "instance_placeholder",
    "is_action_just_pressed", "is_action_just_pressed_by_event",
    "is_action_pressed", "is_editor_hint", "is_input_action_",
    "is_instance_valid", "key_label", "layout_mode", "live_", "load_steps",
    "map_get_iteration_id", "mark_unsaved", "max_errors_per_second",
    "max_polyphony", "max_queued_messages", "max_warnings_per_second",
    "modifies_window", "node_paths", "offset_", "owner_uid_path",
    "parent_id_path", "parse_input_event", "physical_keycode", "press_input_",
    "pressed_event_id", "process_events", "process_frame", "process_owner",
    "queue_free", "release_input_", "runtime_node_select_", "scanning_changes",
    "scripting_enabled", "send_error", "send_message", "set_active",
    "set_anchors_and_offsets_preset", "set_anchors_preset", "set_cell",
    "set_custom_mouse_cursor", "shows_alert", "side_effect", "source_file",
    "source_md5", "sub_resource", "tile_map_data", "tile_set", "to_uid_path",
    "track_insert_key", "tree_exited", "uid_cache", "unique_id",
    "use_multiple_threads", "writes_config", "z_index", "set_suspend"};

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

class SkillRegistryFixture : public ::testing::Test {
  std::unique_ptr<mcp::McpServer> server;
  std::unique_ptr<mcp::McpClient> client;
  std::thread server_thread;
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server, queue, catalog, index, 9527);
    server_thread = std::thread([this] { server->Run(); });

    mcp::ClientOptions cops;
    cops.client_info = mcp::Implementation{"GsdTestClient", "1.0.0"};
    cops.connect_mode = mcp::ConnectMode::Auto;
    client = mcp::McpClient::Create(pair.client, cops);
  }

  void TearDown() override {
    if (client)
      client->Close();
    if (server)
      server->Close();
    if (server_thread.joinable())
      server_thread.join();
  }

protected:
  const godot_autopilot::ToolCatalog &registry_catalog() const {
    return catalog;
  }
};

TEST(SkillGenTest, AllEightSkillsPresent) {
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
      EXPECT_TRUE(starts_with(spec.files[i].relative_path, "references/"))
          << spec.name << ": reference path must start with 'references/': "
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

TEST_F(SkillRegistryFixture, ToolNamesExistInRegistry) {
  std::set<std::string> allowed;
  for (const godot_autopilot::ToolInfo &tool :
       registry_catalog().get_all_tools()) {
    allowed.insert(tool.name);
    const mcp::JsonValue *props = tool.input_schema.Find("properties");
    if (props != nullptr && props->IsObject()) {
      for (const auto &entry : *props) {
        allowed.insert(entry.first);
      }
    }
  }
  for (const char *word : kManualWhitelist) {
    allowed.insert(word);
  }

  for (const SkillSpec &spec : all_skills()) {
    for (const SkillFile &file : spec.files) {
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

TEST(SkillGenTest, EverySkillDeclaresReferences) {
  for (const SkillSpec &spec : all_skills()) {
    ASSERT_GE(spec.files.size(), size_t{2})
        << spec.name << " must declare at least one references/ file";
    EXPECT_EQ(spec.files[0].relative_path, "SKILL.md")
        << "files[0] must be SKILL.md: " << spec.name;
    for (size_t i = 1; i < spec.files.size(); ++i) {
      EXPECT_TRUE(starts_with(spec.files[i].relative_path, "references/"))
          << spec.name << ": reference path must start with 'references/': "
          << spec.files[i].relative_path;
    }
  }
}

} // namespace
