#include "tools/text_ops.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using godot_autopilot::text_ops::CSharpClassInfo;
using godot_autopilot::text_ops::parse_csharp_classes;
using godot_autopilot::text_ops::script_load_diagnostics;

CSharpClassInfo single_class(const std::string &source) {
  const std::vector<CSharpClassInfo> classes = parse_csharp_classes(source);
  EXPECT_EQ(classes.size(), 1u) << source;
  return classes.empty() ? CSharpClassInfo() : classes.front();
}

TEST(TextOpsCSharpParsing, SpaceBeforeBaseColonIsParsed) {
  const CSharpClassInfo info = single_class(
      "using Godot;\npublic partial class TestTypes : Resource\n{\n}\n");
  EXPECT_TRUE(info.found);
  EXPECT_EQ(info.name, "TestTypes");
  EXPECT_TRUE(info.is_partial);
  EXPECT_EQ(info.base, "Resource");
}

TEST(TextOpsCSharpParsing, TightBaseColonIsParsed) {
  const CSharpClassInfo info =
      single_class("public partial class TestTypes: Resource {}");
  EXPECT_EQ(info.name, "TestTypes");
  EXPECT_EQ(info.base, "Resource");
}

TEST(TextOpsCSharpParsing, SpaceBeforeGenericListIsSkipped) {
  const CSharpClassInfo info =
      single_class("public class Box <T> : Resource {}");
  EXPECT_EQ(info.name, "Box");
  EXPECT_EQ(info.base, "Resource");
}

TEST(TextOpsCSharpParsing, GenericListThenBaseIsParsed) {
  const CSharpClassInfo info =
      single_class("public partial class Box<T> : Resource {}");
  EXPECT_EQ(info.name, "Box");
  EXPECT_EQ(info.base, "Resource");
}

TEST(TextOpsCSharpParsing, TabAndNewlineBeforeBaseColonIsSkipped) {
  const CSharpClassInfo info = single_class("class Foo\t:\nBar {}");
  EXPECT_EQ(info.name, "Foo");
  EXPECT_EQ(info.base, "Bar");
}

TEST(TextOpsCSharpParsing, MissingBaseLeavesBaseEmpty) {
  const CSharpClassInfo info = single_class("public partial class Foo { }");
  EXPECT_EQ(info.name, "Foo");
  EXPECT_TRUE(info.base.empty());
}

TEST(TextOpsCSharpParsing, QualifiedBaseKeepsTailSegment) {
  const CSharpClassInfo info =
      single_class("public partial class Foo : Godot.Resource {}");
  EXPECT_EQ(info.base, "Resource");
}

TEST(TextOpsCSharpParsing, StringAndCommentNoiseIsIgnored) {
  const std::vector<CSharpClassInfo> classes = parse_csharp_classes(
      "// class Ghost : Resource\n"
      "public partial class Real : Node\n"
      "{\n"
      "  string s = \"class Fake : Resource\";\n"
      "}\n");
  ASSERT_EQ(classes.size(), 1u);
  EXPECT_EQ(classes[0].name, "Real");
  EXPECT_EQ(classes[0].base, "Node");
}

TEST(TextOpsCSharpParsing, MultipleClassesAreAllReturned) {
  const std::vector<CSharpClassInfo> classes =
      parse_csharp_classes("class A : Node {}\nclass B : Resource {}\n");
  ASSERT_EQ(classes.size(), 2u);
  EXPECT_EQ(classes[0].name, "A");
  EXPECT_EQ(classes[0].base, "Node");
  EXPECT_EQ(classes[1].name, "B");
  EXPECT_EQ(classes[1].base, "Resource");
}

TEST(TextOpsCSharpParsing, PartialDetectionNeedsModifierBeforeClass) {
  const CSharpClassInfo info = single_class("class Foo : Node {}");
  EXPECT_FALSE(info.is_partial);
}

TEST(TextOpsCSharpDiagnostics, LoadedButNotInstantiableKeepsOkTrue) {
  const mcp::JsonValue diag = script_load_diagnostics(true, false);
  const mcp::JsonValue *ok = diag.Find("ok");
  ASSERT_NE(ok, nullptr);
  ASSERT_TRUE(ok->IsBool());
  EXPECT_TRUE(ok->GetBool());
  const mcp::JsonValue *instantiable = diag.Find("instantiable");
  ASSERT_NE(instantiable, nullptr);
  ASSERT_TRUE(instantiable->IsBool());
  EXPECT_FALSE(instantiable->GetBool());
}

TEST(TextOpsCSharpDiagnostics, LoadedAndInstantiableReportsBothTrue) {
  const mcp::JsonValue diag = script_load_diagnostics(true, true);
  const mcp::JsonValue *ok = diag.Find("ok");
  ASSERT_NE(ok, nullptr);
  ASSERT_TRUE(ok->IsBool());
  EXPECT_TRUE(ok->GetBool());
  const mcp::JsonValue *instantiable = diag.Find("instantiable");
  ASSERT_NE(instantiable, nullptr);
  ASSERT_TRUE(instantiable->IsBool());
  EXPECT_TRUE(instantiable->GetBool());
}

TEST(TextOpsCSharpDiagnostics, LoadFailureOmitsInstantiable) {
  const mcp::JsonValue diag = script_load_diagnostics(false, false);
  const mcp::JsonValue *ok = diag.Find("ok");
  ASSERT_NE(ok, nullptr);
  ASSERT_TRUE(ok->IsBool());
  EXPECT_FALSE(ok->GetBool());
  EXPECT_EQ(diag.Find("instantiable"), nullptr);
}

} // namespace
