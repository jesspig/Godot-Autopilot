#include "resources/skill_resources.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <set>
#include <string>
#include <vector>

using godot_autopilot::skill_gen::SkillFile;
using godot_autopilot::skill_gen::SkillSpec;
using godot_autopilot::skill_resources::ContentResult;
using godot_autopilot::skill_resources::ParsedUri;
using godot_autopilot::skill_resources::UriKind;

namespace {

const std::vector<SkillSpec> &skills() {
  static const std::vector<SkillSpec> cached =
      godot_autopilot::skill_gen::all_skills();
  return cached;
}

const char *const kExpectedSkillNames[] = {
    "godot-autopilot",
    "godot-autopilot-scene-system",
    "godot-autopilot-resources",
    "godot-autopilot-scripting",
    "godot-autopilot-runtime",
    "godot-autopilot-servers",
    "godot-autopilot-content",
    "godot-autopilot-csharp"};

} // namespace

TEST(SkillResourcesTest, ParsesCatalogUri) {
  const ParsedUri lower = godot_autopilot::skill_resources::parse_uri(
      "godot://skills");
  EXPECT_EQ(lower.kind, UriKind::catalog);
  EXPECT_TRUE(lower.name.empty());
  EXPECT_TRUE(lower.file.empty());

  const ParsedUri upper = godot_autopilot::skill_resources::parse_uri(
      "GODOT://SKILLS");
  EXPECT_EQ(upper.kind, UriKind::catalog);
}

TEST(SkillResourcesTest, ParsesSkillUri) {
  const ParsedUri uri = godot_autopilot::skill_resources::parse_uri(
      "godot://skills/godot-autopilot");
  EXPECT_EQ(uri.kind, UriKind::skill);
  EXPECT_EQ(uri.name, "godot-autopilot");
  EXPECT_TRUE(uri.file.empty());
}

TEST(SkillResourcesTest, ParsesFileUri) {
  const ParsedUri single = godot_autopilot::skill_resources::parse_uri(
      "godot://skills/godot-autopilot/references/http-fallback.md");
  EXPECT_EQ(single.kind, UriKind::file);
  EXPECT_EQ(single.name, "godot-autopilot");
  EXPECT_EQ(single.file, "references/http-fallback.md");

  const ParsedUri nested = godot_autopilot::skill_resources::parse_uri(
      "godot://skills/demo/a/b/c.md");
  EXPECT_EQ(nested.kind, UriKind::file);
  EXPECT_EQ(nested.name, "demo");
  EXPECT_EQ(nested.file, "a/b/c.md");
}

TEST(SkillResourcesTest, RejectsInvalidUris) {
  const char *const invalid[] = {
      "",
      "godot:",
      "godot://",
      "godot://skill",
      "godot://skillsx/foo",
      "godot://scene/tree",
      "godot://skills/",
      "godot://skills//",
      "godot://skills//foo",
      "godot://skills/foo/",
      "godot://skills/foo//bar",
      "godot://skills/foo/bar/",
      "godot://skills?x=1",
      "xgodot://skills"};
  for (const char *uri : invalid) {
    EXPECT_EQ(godot_autopilot::skill_resources::parse_uri(uri).kind,
              UriKind::invalid)
        << "uri: " << uri;
  }
}

TEST(SkillResourcesTest, BuildsCatalogFromEmbeddedSkills) {
  const std::vector<SkillSpec> &all = skills();
  ASSERT_EQ(all.size(), 8u);

  const std::vector<godot_autopilot::skill_resources::CatalogEntry> catalog =
      godot_autopilot::skill_resources::build_catalog(all);
  ASSERT_EQ(catalog.size(), all.size());

  std::set<std::string> names;
  for (std::size_t i = 0; i < catalog.size(); ++i) {
    EXPECT_EQ(catalog[i].name, all[i].name);
    EXPECT_FALSE(catalog[i].name.empty());
    EXPECT_FALSE(catalog[i].description.empty());
    EXPECT_EQ(catalog[i].files.size(), all[i].files.size());
    ASSERT_FALSE(catalog[i].files.empty()) << catalog[i].name;
    EXPECT_EQ(catalog[i].files.front(), "SKILL.md") << catalog[i].name;
    for (const std::string &file : catalog[i].files) {
      EXPECT_FALSE(file.empty()) << catalog[i].name;
    }
    names.insert(catalog[i].name);
  }

  EXPECT_EQ(names.size(), 8u);
  for (const char *name : kExpectedSkillNames) {
    EXPECT_TRUE(names.count(name) == 1) << "missing skill: " << name;
  }
}

TEST(SkillResourcesTest, CatalogJsonCarriesCountAndEntries) {
  const std::string json = godot_autopilot::skill_resources::catalog_to_json(
      godot_autopilot::skill_resources::build_catalog(skills()));
  const mcp::JsonValue root = mcp::JsonValue::Parse(json);
  ASSERT_TRUE(root.IsObject());

  const mcp::JsonValue *count = root.Find("count");
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->GetInt(), static_cast<int64_t>(skills().size()));

  const mcp::JsonValue *entries = root.Find("skills");
  ASSERT_NE(entries, nullptr);
  ASSERT_TRUE(entries->IsArray());
  ASSERT_EQ(entries->Size(), skills().size());

  for (std::size_t i = 0; i < entries->Size(); ++i) {
    const mcp::JsonValue &item = (*entries)[i];
    ASSERT_TRUE(item.IsObject());
    const mcp::JsonValue *name = item.Find("name");
    const mcp::JsonValue *description = item.Find("description");
    const mcp::JsonValue *files = item.Find("files");
    ASSERT_NE(name, nullptr);
    ASSERT_NE(description, nullptr);
    ASSERT_NE(files, nullptr);
    EXPECT_TRUE(name->IsString());
    EXPECT_EQ(name->GetString(), skills()[i].name);
    EXPECT_TRUE(description->IsString());
    ASSERT_TRUE(files->IsArray());
    EXPECT_EQ(files->Size(), skills()[i].files.size());
    ASSERT_GT(files->Size(), 1u);
    EXPECT_EQ((*files)[0].GetString(), "SKILL.md");
  }
}

TEST(SkillResourcesTest, ResolvesKnownAndUnknownTargets) {
  const std::vector<SkillSpec> &all = skills();

  const godot_autopilot::skill_resources::ResolvedUri catalog =
      godot_autopilot::skill_resources::resolve_uri(all, "godot://skills");
  EXPECT_EQ(catalog.parsed.kind, UriKind::catalog);
  EXPECT_EQ(catalog.skill, nullptr);
  EXPECT_EQ(catalog.file, nullptr);

  const godot_autopilot::skill_resources::ResolvedUri skill =
      godot_autopilot::skill_resources::resolve_uri(
          all, "godot://skills/godot-autopilot");
  ASSERT_NE(skill.skill, nullptr);
  EXPECT_EQ(skill.skill->name, "godot-autopilot");
  EXPECT_EQ(skill.file, nullptr);

  const godot_autopilot::skill_resources::ResolvedUri reference =
      godot_autopilot::skill_resources::resolve_uri(
          all, "godot://skills/godot-autopilot/references/http-fallback.md");
  ASSERT_NE(reference.skill, nullptr);
  ASSERT_NE(reference.file, nullptr);
  EXPECT_EQ(reference.file->relative_path, "references/http-fallback.md");

  const godot_autopilot::skill_resources::ResolvedUri unknown_skill =
      godot_autopilot::skill_resources::resolve_uri(all,
                                                    "godot://skills/nope");
  EXPECT_EQ(unknown_skill.parsed.kind, UriKind::skill);
  EXPECT_EQ(unknown_skill.skill, nullptr);

  const godot_autopilot::skill_resources::ResolvedUri unknown_file =
      godot_autopilot::skill_resources::resolve_uri(
          all, "godot://skills/godot-autopilot/references/nope.md");
  ASSERT_NE(unknown_file.skill, nullptr);
  EXPECT_EQ(unknown_file.file, nullptr);
}

TEST(SkillResourcesTest, ResolvesCatalogContent) {
  const ContentResult content = godot_autopilot::skill_resources::resolve_content(
      skills(), "godot://skills");
  EXPECT_TRUE(content.error.empty());
  EXPECT_EQ(content.kind, UriKind::catalog);
  EXPECT_EQ(content.mime_type, "application/json");

  const mcp::JsonValue root = mcp::JsonValue::Parse(content.text);
  const mcp::JsonValue *count = root.Find("count");
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->GetInt(), static_cast<int64_t>(skills().size()));
}

TEST(SkillResourcesTest, ResolvesSkillMarkdownContent) {
  const std::vector<SkillSpec> &all = skills();
  const SkillSpec *spec =
      godot_autopilot::skill_resources::find_skill(all, "godot-autopilot");
  ASSERT_NE(spec, nullptr);

  const ContentResult by_name =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/godot-autopilot");
  EXPECT_TRUE(by_name.error.empty());
  EXPECT_EQ(by_name.kind, UriKind::skill);
  EXPECT_EQ(by_name.mime_type, "text/markdown");
  EXPECT_EQ(by_name.text, godot_autopilot::skill_gen::render_skill_md(*spec));
  EXPECT_EQ(by_name.text.rfind("---\n", 0), 0u);
  EXPECT_NE(by_name.text.find("\nname: godot-autopilot\n"), std::string::npos);
  EXPECT_NE(by_name.text.find("author: godot-autopilot"), std::string::npos);

  const ContentResult by_file =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/godot-autopilot/SKILL.md");
  EXPECT_TRUE(by_file.error.empty());
  EXPECT_EQ(by_file.kind, UriKind::file);
  EXPECT_EQ(by_file.text, by_name.text);
}

TEST(SkillResourcesTest, ResolvesReferenceContentVerbatim) {
  const std::vector<SkillSpec> &all = skills();
  const SkillSpec *spec =
      godot_autopilot::skill_resources::find_skill(all, "godot-autopilot");
  ASSERT_NE(spec, nullptr);
  const SkillFile *file = godot_autopilot::skill_resources::find_file(
      *spec, "references/http-fallback.md");
  ASSERT_NE(file, nullptr);

  const ContentResult content =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/godot-autopilot/references/http-fallback.md");
  EXPECT_TRUE(content.error.empty());
  EXPECT_EQ(content.kind, UriKind::file);
  EXPECT_EQ(content.mime_type, "text/markdown");
  EXPECT_FALSE(content.text.empty());
  EXPECT_EQ(content.text, file->body);
  EXPECT_EQ(content.text.find("author: godot-autopilot"), std::string::npos);
}

TEST(SkillResourcesTest, AcceptsCaseInsensitiveSchemeForContent) {
  const ContentResult upper =
      godot_autopilot::skill_resources::resolve_content(
          skills(), "GODOT://SKILLS/godot-autopilot");
  EXPECT_TRUE(upper.error.empty());
  EXPECT_EQ(upper.kind, UriKind::skill);
  EXPECT_NE(upper.text.find("\nname: godot-autopilot\n"), std::string::npos);
}

TEST(SkillResourcesTest, ReportsStructuredErrors) {
  const std::vector<SkillSpec> &all = skills();

  const ContentResult foreign =
      godot_autopilot::skill_resources::resolve_content(all,
                                                        "godot://scene/tree");
  EXPECT_FALSE(foreign.error.empty());
  EXPECT_TRUE(foreign.text.empty());
  EXPECT_NE(foreign.error.find("godot://scene/tree"), std::string::npos);

  const ContentResult unknown_skill =
      godot_autopilot::skill_resources::resolve_content(all,
                                                        "godot://skills/nope");
  EXPECT_FALSE(unknown_skill.error.empty());
  EXPECT_TRUE(unknown_skill.text.empty());
  EXPECT_NE(unknown_skill.error.find("nope"), std::string::npos);

  const ContentResult unknown_file =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/godot-autopilot/references/nope.md");
  EXPECT_FALSE(unknown_file.error.empty());
  EXPECT_TRUE(unknown_file.text.empty());
  EXPECT_NE(unknown_file.error.find("references/nope.md"), std::string::npos);

  const ContentResult trailing_slash =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/godot-autopilot/");
  EXPECT_FALSE(trailing_slash.error.empty());
  EXPECT_TRUE(trailing_slash.text.empty());

  const ContentResult mixed_case_name =
      godot_autopilot::skill_resources::resolve_content(
          all, "godot://skills/Godot-Autopilot");
  EXPECT_FALSE(mixed_case_name.error.empty());
  EXPECT_NE(mixed_case_name.error.find("Godot-Autopilot"), std::string::npos);
}
