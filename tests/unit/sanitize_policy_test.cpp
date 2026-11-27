#include "core/observability/sanitize_policy.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::sanitize_policy::is_sensitive_key;
using godot_autopilot::sanitize_policy::kRedactedValue;
using godot_autopilot::sanitize_policy::metadata_marks_sensitive;
using godot_autopilot::sanitize_policy::redact_sensitive_values;
using godot_autopilot::sanitize_policy::sensitive_key_is_allowed;

namespace {

godot_autopilot::util::JsonValue parse_json(const std::string &json) {
  return godot_autopilot::util::JsonValue::Parse(json);
}

} // namespace

TEST(SanitizePolicyTest, SensitiveKeyPolicyCoversCommonCredentialNames) {
  const char *const sensitive[] = {
      "token", "password", "PassWord", "secret", "credential", "credentials",
      "api_key", "apiKey", "APIKey", "access_token", "accessToken",
      "clientSecret", "password_hash", "authorization", "cookie",
      "private_key", "script_content", "data", "base64"};
  for (const char *key : sensitive) {
    EXPECT_TRUE(is_sensitive_key(key)) << key;
  }
}

TEST(SanitizePolicyTest, SensitiveKeyPolicyAvoidsUnrelatedSubstrings) {
  const char *const ordinary[] = {
      "name", "display_name", "monkey", "keyboard", "tokenizer",
      "secretary", "theme_color"};
  for (const char *key : ordinary) {
    EXPECT_FALSE(is_sensitive_key(key)) << key;
  }
}

TEST(SanitizePolicyTest, MetadataPolicyRequiresTruthyValues) {
  EXPECT_TRUE(metadata_marks_sensitive("sensitive", "true"));
  EXPECT_TRUE(metadata_marks_sensitive("is_secret", "1"));
  EXPECT_TRUE(metadata_marks_sensitive("hint_string", "Enter API password"));
  EXPECT_TRUE(metadata_marks_sensitive("hint_string", "Access token"));
  EXPECT_FALSE(metadata_marks_sensitive("sensitive", "false"));
  EXPECT_FALSE(metadata_marks_sensitive("sensitive", "0"));
  EXPECT_FALSE(metadata_marks_sensitive("description", "secret"));
}

TEST(SanitizePolicyTest, RedactsSensitiveValuesInObjectsAndArrays) {
  const godot_autopilot::util::JsonValue value = parse_json(
      R"({"name":"kept","api_token":"hidden","items":[{"token":"secret-value"},{"name":"second"}]})");
  const auto redacted = redact_sensitive_values(value);
  EXPECT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value["name"], godot_autopilot::util::JsonValue("kept"));
  EXPECT_EQ(redacted.value["api_token"],
            godot_autopilot::util::JsonValue(kRedactedValue));
  ASSERT_TRUE(redacted.value["items"].IsArray());
  EXPECT_EQ(redacted.value["items"][0]["token"],
            godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_EQ(redacted.value["items"][1]["name"],
            godot_autopilot::util::JsonValue("second"));
  EXPECT_EQ(redacted.value.Dump().find("hidden"), std::string::npos);
  EXPECT_EQ(redacted.value.Dump().find("secret-value"), std::string::npos);
}

TEST(SanitizePolicyTest, ExplicitWhitelistKeepsOnlyNamedSensitiveField) {
  const godot_autopilot::util::JsonValue value = parse_json(
      R"({"api_token":"allowed","password":"hidden","name":"kept"})");
  const auto redacted = redact_sensitive_values(value, {"api_token"});
  EXPECT_TRUE(redacted.redacted);
  EXPECT_EQ(redacted.value["api_token"],
            godot_autopilot::util::JsonValue("allowed"));
  EXPECT_EQ(redacted.value["password"],
            godot_autopilot::util::JsonValue(kRedactedValue));
  EXPECT_EQ(redacted.value["name"], godot_autopilot::util::JsonValue("kept"));
  EXPECT_TRUE(sensitive_key_is_allowed("API-Token", {"api_token"}));
}

TEST(SanitizePolicyTest, PlainValuesPassThroughUnchanged) {
  const godot_autopilot::util::JsonValue value = parse_json(R"("plain")");
  const auto redacted = redact_sensitive_values(value);
  EXPECT_FALSE(redacted.redacted);
  EXPECT_EQ(redacted.value, value);
}
