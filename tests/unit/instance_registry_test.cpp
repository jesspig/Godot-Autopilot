#include "core/instance_registry.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using godot_autopilot::InstanceRecord;
using godot_autopilot::InstanceRegistry;

namespace {

void set_registry_env(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_registry_env(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

InstanceRecord sample_record() {
  InstanceRecord record;
  record.instance_id = "instance-0001";
  record.port = 9527;
  record.pid = 4242;
  record.project_path = "C:/projects/Demo";
  record.api_token = "token-abc";
  record.endpoints.mcp.host = "127.0.0.1";
  record.endpoints.mcp.port = 9527;
  record.endpoints.mcp.path = "/mcp";
  record.endpoints.mcp.protocol = "mcp";
  record.endpoints.mcp.auth = "none";
  record.endpoints.api.host = "127.0.0.1";
  record.endpoints.api.port = 9527;
  record.endpoints.api.path = "/api";
  record.endpoints.api.protocol = "http";
  record.endpoints.api.auth = "bearer";
  record.gda_home = "C:/gda/home/.godot-autopilot";
  record.started_at = 1700000000;
  record.version = "0.1.0";
  return record;
}

} // namespace

TEST(InstanceRegistryTest, RecordRoundTripsThroughJson) {
  const InstanceRecord original = sample_record();
  const mcp::JsonValue doc = godot_autopilot::record_to_json(original);

  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json(doc, parsed));
  EXPECT_EQ(parsed.instance_id, original.instance_id);
  EXPECT_EQ(parsed.port, original.port);
  EXPECT_EQ(parsed.pid, original.pid);
  EXPECT_EQ(parsed.project_path, original.project_path);
  EXPECT_EQ(parsed.api_token, original.api_token);
  EXPECT_EQ(parsed.gda_home, original.gda_home);
  EXPECT_EQ(parsed.started_at, original.started_at);
  EXPECT_EQ(parsed.version, original.version);
  EXPECT_EQ(parsed.endpoints.mcp.host, original.endpoints.mcp.host);
  EXPECT_EQ(parsed.endpoints.mcp.port, original.endpoints.mcp.port);
  EXPECT_EQ(parsed.endpoints.mcp.path, original.endpoints.mcp.path);
  EXPECT_EQ(parsed.endpoints.mcp.protocol, original.endpoints.mcp.protocol);
  EXPECT_EQ(parsed.endpoints.mcp.auth, original.endpoints.mcp.auth);
  EXPECT_EQ(parsed.endpoints.api.host, original.endpoints.api.host);
  EXPECT_EQ(parsed.endpoints.api.port, original.endpoints.api.port);
  EXPECT_EQ(parsed.endpoints.api.path, original.endpoints.api.path);
  EXPECT_EQ(parsed.endpoints.api.protocol, original.endpoints.api.protocol);
  EXPECT_EQ(parsed.endpoints.api.auth, original.endpoints.api.auth);
}

TEST(InstanceRegistryTest, RecordRoundTripsThroughJsonText) {
  const InstanceRecord original = sample_record();
  const std::string text = godot_autopilot::record_to_json(original).Dump(2);

  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json_text(text, parsed));
  EXPECT_EQ(parsed.instance_id, original.instance_id);
  EXPECT_EQ(parsed.endpoints.api.path, original.endpoints.api.path);
}

TEST(InstanceRegistryTest, MalformedJsonTextFailsWithoutThrowing) {
  InstanceRecord parsed;
  EXPECT_FALSE(godot_autopilot::record_from_json_text("{not-json", parsed));
  EXPECT_FALSE(godot_autopilot::record_from_json_text("", parsed));
}

TEST(InstanceRegistryTest, MissingFieldFailsValidation) {
  mcp::JsonValue::Object partial;
  partial["instance_id"] = "instance-0001";
  InstanceRecord parsed;
  EXPECT_FALSE(
      godot_autopilot::record_from_json(mcp::JsonValue(std::move(partial)),
                                        parsed));
}

TEST(InstanceRegistryTest, NonObjectDocumentFailsValidation) {
  InstanceRecord parsed;
  EXPECT_FALSE(godot_autopilot::record_from_json(
      mcp::JsonValue(mcp::JsonValue::Array()), parsed));
}

TEST(InstanceRegistryTest, WriteListRemoveRoundTrip) {
  const std::filesystem::path base =
      std::filesystem::temp_directory_path() / "gda_instance_registry_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  set_registry_env("GODOT_AUTOPILOT_HOME", base.u8string().c_str());

  InstanceRecord record = sample_record();
  record.instance_id = "instance-round-trip";
  ASSERT_TRUE(InstanceRegistry::write(record));

  const std::vector<InstanceRecord> listed = InstanceRegistry::list_all();
  ASSERT_EQ(listed.size(), 1u);
  EXPECT_EQ(listed[0].instance_id, record.instance_id);
  EXPECT_EQ(listed[0].endpoints.api.path, record.endpoints.api.path);

  EXPECT_TRUE(InstanceRegistry::remove(record.instance_id));
  EXPECT_TRUE(InstanceRegistry::list_all().empty());

  unset_registry_env("GODOT_AUTOPILOT_HOME");
  std::filesystem::remove_all(base, ec);
}
