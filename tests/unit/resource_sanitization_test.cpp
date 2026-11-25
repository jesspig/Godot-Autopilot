#include "resources/resource_sanitization.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using godot_autopilot::resources::node_property_is_exported;
using godot_autopilot::resources::redact_resource_value;
using godot_autopilot::resources::resource_value_response;
using godot_autopilot::sanitize_policy::is_sensitive_key;
using godot_autopilot::sanitize_policy::kRedactedValue;
using godot_autopilot::sanitize_policy::metadata_marks_sensitive;

godot_autopilot::util::JsonValue parse_json(const std::string &json) {
  return godot_autopilot::util::JsonValue::Parse(json);
}

TEST(ResourceSanitizationTest, SensitiveKeyPolicyCoversCommonCredentialNames) {
  const char *const sensitive[] = {
      "token", "password", "PassWord", "secret", "credential", "credentials",
      "api_key", "apiKey", "APIKey", "access_token", "accessToken",
      "clientSecret", "password_hash", "authorization", "cookie",
      "private_key", "script_content", "data", "base64"};
  for (const char *key : sensitive) {
    EXPECT_TRUE(is_sensitive_key(key)) << key;
  }
}

TEST(ResourceSanitizationTest, SensitiveKeyPolicyAvoidsUnrelatedSubstrings) {
  const char *const ordinary[] = {
      "name", "display_name", "monkey", "keyboard", "tokenizer",
      "secretary", "theme_color"};
  for (const char *key : ordinary) {
    EXPECT_FALSE(is_sensitive_key(key)) << key;
  }
}

TEST(ResourceSanitizationTest, SensitiveEditorSettingIsExplicitlyRedacted) {
  const std::string secret = "do-not-leak-this-value";
  const auto redacted = redact_resource_value(
      "plugin/api_token", parse_json("\"" + secret + "\""));
  ASSERT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value, godot_autopilot::util::JsonValue(kRedactedValue));

  const godot_autopilot::util::JsonValue response = resource_value_response(redacted);
  ASSERT_TRUE(response.IsObject());
  ASSERT_TRUE(response.Contains("redacted"));
  EXPECT_TRUE(response["redacted"].GetBool());
  EXPECT_EQ(response["value"], godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_EQ(response.Dump().find(secret), std::string::npos);
}

TEST(ResourceSanitizationTest, ResourceRedactionDoesNotDependOnTraceSetting) {
  const bool previous = godot_autopilot::sanitize_policy::enabled();
  godot_autopilot::sanitize_policy::set_enabled(false);
  const auto redacted = redact_resource_value(
      "plugin/password", parse_json(R"("still-hidden")"));
  godot_autopilot::sanitize_policy::set_enabled(previous);
  EXPECT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value, godot_autopilot::util::JsonValue(kRedactedValue));
}

TEST(ResourceSanitizationTest, OrdinaryEditorSettingRemainsReadable) {
  const godot_autopilot::util::JsonValue value = parse_json(R"({"base_color":"#202020"})");
  const auto redacted = redact_resource_value("interface/theme", value);
  EXPECT_FALSE(redacted.redacted);
  EXPECT_EQ(resource_value_response(redacted), value);
}

TEST(ResourceSanitizationTest, NestedSensitiveSettingFieldsAreRedacted) {
  const godot_autopilot::util::JsonValue value = parse_json(
      R"({"endpoint":"https://example.invalid","credential":{"user":"alice","password":"hidden"},"enabled":true})");
  const auto redacted = redact_resource_value("plugin/options", value);
  ASSERT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value["endpoint"], godot_autopilot::util::JsonValue("https://example.invalid"));
  EXPECT_EQ(redacted.value["credential"], godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_TRUE(redacted.value["enabled"].GetBool());
  EXPECT_EQ(redacted.value.Dump().find("hidden"), std::string::npos);
  EXPECT_TRUE(resource_value_response(redacted)["redacted"].GetBool());
}

TEST(ResourceSanitizationTest, SensitiveExportedNodePropertyIsRedacted) {
  const auto redacted = redact_resource_value(
      "client_secret", parse_json(R"("node-secret")"));
  ASSERT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value, godot_autopilot::util::JsonValue(kRedactedValue));
}

TEST(ResourceSanitizationTest, OrdinaryExportedNodePropertyRemainsReadable) {
  const auto redacted =
      redact_resource_value("display_name", parse_json(R"("Hero")"));
  EXPECT_FALSE(redacted.redacted);
  EXPECT_EQ(redacted.value, godot_autopilot::util::JsonValue("Hero"));
}

TEST(ResourceSanitizationTest, OnlyStoragePropertiesAreExported) {
  EXPECT_TRUE(node_property_is_exported(
      godot::PROPERTY_USAGE_STORAGE | godot::PROPERTY_USAGE_EDITOR));
  EXPECT_FALSE(node_property_is_exported(godot::PROPERTY_USAGE_EDITOR));
  EXPECT_FALSE(node_property_is_exported(
      godot::PROPERTY_USAGE_STORAGE | godot::PROPERTY_USAGE_INTERNAL));
  EXPECT_FALSE(node_property_is_exported(
      godot::PROPERTY_USAGE_STORAGE | godot::PROPERTY_USAGE_GROUP));
  EXPECT_FALSE(node_property_is_exported(
      godot::PROPERTY_USAGE_STORAGE | godot::PROPERTY_USAGE_CATEGORY));
  EXPECT_FALSE(node_property_is_exported(
      godot::PROPERTY_USAGE_STORAGE | godot::PROPERTY_USAGE_SUBGROUP));
}

TEST(ResourceSanitizationTest, SensitiveMetadataRedactsSafeNamedProperty) {
  const auto flag = redact_resource_value(
      "plugin_option", parse_json(R"("metadata-secret")"),
      {{"sensitive", "true"}});
  ASSERT_TRUE(flag.redacted);
  EXPECT_EQ(flag.value, godot_autopilot::util::JsonValue(kRedactedValue));

  const auto hint = redact_resource_value(
      "plugin_option", parse_json(R"("hint-secret")"),
      {{"hint_string", "Password"}});
  ASSERT_TRUE(hint.redacted);
  EXPECT_EQ(hint.value, godot_autopilot::util::JsonValue(kRedactedValue));
}

TEST(ResourceSanitizationTest, FalseSensitiveMetadataDoesNotRedact) {
  const auto redacted = redact_resource_value(
      "plugin_option", parse_json(R"("ordinary")"),
      {{"sensitive", "false"}});
  EXPECT_FALSE(redacted.redacted);
  EXPECT_EQ(redacted.value, godot_autopilot::util::JsonValue("ordinary"));
}

TEST(ResourceSanitizationTest, MetadataPolicyRequiresTruthyValues) {
  EXPECT_TRUE(metadata_marks_sensitive("sensitive", "true"));
  EXPECT_TRUE(metadata_marks_sensitive("is_secret", "1"));
  EXPECT_TRUE(metadata_marks_sensitive("hint_string", "Enter API password"));
  EXPECT_TRUE(metadata_marks_sensitive("hint_string", "Access token"));
  EXPECT_FALSE(metadata_marks_sensitive("sensitive", "false"));
  EXPECT_FALSE(metadata_marks_sensitive("sensitive", "0"));
  EXPECT_FALSE(metadata_marks_sensitive("description", "secret"));
}

TEST(ResourceSanitizationTest, ExplicitWhitelistCanReadRawSensitiveValue) {
  const godot_autopilot::util::JsonValue value = parse_json(R"("whitelisted-secret")");
  const auto redacted = redact_resource_value(
      "plugin/api_token", value, {}, {"plugin/API-token"});
  EXPECT_FALSE(redacted.redacted);
  EXPECT_EQ(redacted.value, value);
}

TEST(ResourceSanitizationTest, WhitelistDoesNotExposeOtherSensitiveFields) {
  const godot_autopilot::util::JsonValue value = parse_json(
      R"({"api_token":"allowed","password":"hidden","name":"kept"})");
  const auto redacted = redact_resource_value(
      "plugin/options", value, {}, {"api_token"});
  ASSERT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value["api_token"], godot_autopilot::util::JsonValue("allowed"));
  EXPECT_EQ(redacted.value["password"], godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_EQ(redacted.value["name"], godot_autopilot::util::JsonValue("kept"));
}

TEST(ResourceSanitizationTest, SensitiveValuesInsideArraysAreRedacted) {
  const godot_autopilot::util::JsonValue value = parse_json(
      R"([{"name":"first","token":"hidden"},{"name":"second"}])");
  const auto redacted = redact_resource_value("plugin/items", value);
  ASSERT_TRUE(redacted.redacted);
  ASSERT_TRUE(redacted.value.IsArray());
  EXPECT_EQ(redacted.value[0]["token"], godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_EQ(redacted.value[1]["name"], godot_autopilot::util::JsonValue("second"));
}

} // namespace
