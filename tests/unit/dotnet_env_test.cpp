#include "util/dotnet_env.hpp"

#include <gtest/gtest.h>

namespace {

using godot_autopilot::dotnet_env::DotnetProbe;
using godot_autopilot::dotnet_env::feature_name_is_dotnet;
using godot_autopilot::dotnet_env::is_dotnet_environment;

DotnetProbe probe(bool mono_feature, bool dotnet_feature,
                  bool csharp_script_class, int engine_major,
                  int engine_minor) {
  DotnetProbe p;
  p.mono_feature = mono_feature;
  p.dotnet_feature = dotnet_feature;
  p.csharp_script_class = csharp_script_class;
  p.engine_major = engine_major;
  p.engine_minor = engine_minor;
  return p;
}

TEST(DotnetEnv, FeatureNameSwitchesAtGodot44) {
  EXPECT_FALSE(feature_name_is_dotnet(4, 3));
  EXPECT_TRUE(feature_name_is_dotnet(4, 4));
  EXPECT_TRUE(feature_name_is_dotnet(4, 7));
  EXPECT_TRUE(feature_name_is_dotnet(5, 0));
  EXPECT_FALSE(feature_name_is_dotnet(3, 5));
  EXPECT_FALSE(feature_name_is_dotnet(0, 0));
}

TEST(DotnetEnv, CSharpScriptClassMeansDotnetBuild) {
  EXPECT_TRUE(is_dotnet_environment(probe(false, false, true, 4, 7)));
  EXPECT_TRUE(is_dotnet_environment(probe(false, false, true, 0, 0)));
}

TEST(DotnetEnv, ModernEngineUsesDotnetFeatureName) {
  EXPECT_TRUE(is_dotnet_environment(probe(false, true, false, 4, 7)));
  EXPECT_FALSE(is_dotnet_environment(probe(true, false, false, 4, 7)));
  EXPECT_TRUE(is_dotnet_environment(probe(true, true, false, 4, 7)));
}

TEST(DotnetEnv, LegacyEngineUsesMonoFeatureName) {
  EXPECT_TRUE(is_dotnet_environment(probe(true, false, false, 4, 3)));
  EXPECT_FALSE(is_dotnet_environment(probe(false, true, false, 4, 3)));
}

TEST(DotnetEnv, UnknownVersionAcceptsEitherFeatureName) {
  EXPECT_TRUE(is_dotnet_environment(probe(true, false, false, 0, 0)));
  EXPECT_TRUE(is_dotnet_environment(probe(false, true, false, 0, 0)));
  EXPECT_FALSE(is_dotnet_environment(probe(false, false, false, 0, 0)));
}

TEST(DotnetEnv, PlainEditorIsNotDotnetEnvironment) {
  EXPECT_FALSE(is_dotnet_environment(probe(false, false, false, 4, 7)));
  EXPECT_FALSE(is_dotnet_environment(probe(false, false, false, 4, 3)));
}

} // namespace
