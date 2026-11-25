#include "core/storage/instance_registry.hpp"
#include "support/test_temp.hpp"

#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif

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
  record.port_source = "first_random";
  return record;
}

} // namespace

TEST(InstanceRegistryTest, RecordRoundTripsThroughJson) {
  const InstanceRecord original = sample_record();
  const godot_autopilot::util::JsonValue doc = godot_autopilot::record_to_json(original);

  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json(doc, parsed));
  EXPECT_EQ(parsed.instance_id, original.instance_id);
  EXPECT_EQ(parsed.port, original.port);
  EXPECT_EQ(parsed.pid, original.pid);
  EXPECT_EQ(parsed.project_path, original.project_path);
  EXPECT_EQ(parsed.gda_home, original.gda_home);
  EXPECT_EQ(parsed.started_at, original.started_at);
  EXPECT_EQ(parsed.version, original.version);
  EXPECT_EQ(parsed.port_source, original.port_source);
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
  godot_autopilot::util::JsonValue::Object partial;
  partial["instance_id"] = "instance-0001";
  InstanceRecord parsed;
  EXPECT_FALSE(
      godot_autopilot::record_from_json(godot_autopilot::util::JsonValue(std::move(partial)),
                                        parsed));
}

TEST(InstanceRegistryTest, NonObjectDocumentFailsValidation) {
  InstanceRecord parsed;
  EXPECT_FALSE(godot_autopilot::record_from_json(
      godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::Array()), parsed));
}

TEST(InstanceRegistryTest, RecordWithIpcEndpointRoundTrips) {
  InstanceRecord original = sample_record();
  original.endpoints.ipc.kind = "unix";
  original.endpoints.ipc.path = "C:/gda/run/sample-1234abcd.sock";
  original.endpoints.ipc.token = "ipc-token";
  original.endpoints.ipc.protocol = "mcp-jsonrpc";
  original.endpoints.ipc.framing = "length-prefixed-json";
  original.endpoints.ipc.auth = "token";
  original.endpoints.ipc.version = 1;
  original.endpoints.ipc.present = true;
  const godot_autopilot::util::JsonValue doc = godot_autopilot::record_to_json(original);
  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json(doc, parsed));
  EXPECT_TRUE(parsed.endpoints.ipc.present);
  EXPECT_EQ(parsed.endpoints.ipc.kind, "unix");
  EXPECT_EQ(parsed.endpoints.ipc.path, "C:/gda/run/sample-1234abcd.sock");
  EXPECT_EQ(parsed.endpoints.ipc.token, "ipc-token");
  EXPECT_EQ(parsed.endpoints.ipc.protocol, "mcp-jsonrpc");
  EXPECT_EQ(parsed.endpoints.ipc.framing, "length-prefixed-json");
  EXPECT_EQ(parsed.endpoints.ipc.auth, "token");
  EXPECT_EQ(parsed.endpoints.ipc.version, 1);
}

TEST(InstanceRegistryTest, TcpIpcEndpointRoundTrips) {
  InstanceRecord original = sample_record();
  original.endpoints.ipc.kind = "tcp";
  original.endpoints.ipc.host = "127.0.0.1";
  original.endpoints.ipc.port = 21437;
  original.endpoints.ipc.token = "tcp-token";
  original.endpoints.ipc.present = true;
  const godot_autopilot::util::JsonValue doc = godot_autopilot::record_to_json(original);
  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json(doc, parsed));
  EXPECT_TRUE(parsed.endpoints.ipc.present);
  EXPECT_EQ(parsed.endpoints.ipc.kind, "tcp");
  EXPECT_EQ(parsed.endpoints.ipc.host, "127.0.0.1");
  EXPECT_EQ(parsed.endpoints.ipc.port, 21437);
  EXPECT_EQ(parsed.endpoints.ipc.token, "tcp-token");
}

TEST(InstanceRegistryTest, MissingIpcEndpointIsBackwardCompatible) {
  const InstanceRecord original = sample_record();
  const godot_autopilot::util::JsonValue doc = godot_autopilot::record_to_json(original);
  InstanceRecord parsed;
  ASSERT_TRUE(godot_autopilot::record_from_json(doc, parsed));
  EXPECT_FALSE(parsed.endpoints.ipc.present);
}

TEST(InstanceRegistryTest, InvalidIpcEndpointFailsValidation) {
  const InstanceRecord original = sample_record();
  godot_autopilot::util::JsonValue doc = godot_autopilot::record_to_json(original);
  godot_autopilot::util::JsonValue::Object bad;
  bad["path"] = 5;
  doc["endpoints"]["ipc"] = godot_autopilot::util::JsonValue(std::move(bad));
  InstanceRecord parsed;
  EXPECT_FALSE(godot_autopilot::record_from_json(doc, parsed));
}

TEST(InstanceRegistryTest, WriteListRemoveRoundTrip) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_instance_registry_test";
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

#ifndef _WIN32
TEST(InstanceRegistryTest, WrittenRegistryFileIsOwnerOnly) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_instance_registry_perm_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  set_registry_env("GODOT_AUTOPILOT_HOME", base.u8string().c_str());
  InstanceRecord record = sample_record();
  record.instance_id = "instance-perm-test";
  ASSERT_TRUE(InstanceRegistry::write(record));

  std::filesystem::path instance_file;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(base, ec)) {
    if (entry.is_regular_file() && entry.path().filename() ==
                                       std::filesystem::path(
                                           "instance-perm-test.json")) {
      instance_file = entry.path();
      break;
    }
  }
  ASSERT_FALSE(instance_file.empty())
      << "registry instance file was not written under GDA home";
  struct stat info {};
  ASSERT_EQ(::stat(instance_file.c_str(), &info), 0);
  EXPECT_EQ(info.st_mode & 0777, 0600);

  unset_registry_env("GODOT_AUTOPILOT_HOME");
  std::filesystem::remove_all(base, ec);
}
#endif

TEST(InstanceRegistryTest, AtomicWriteLeavesNoTempFiles) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_instance_registry_atomic_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  set_registry_env("GODOT_AUTOPILOT_HOME", base.u8string().c_str());
  InstanceRecord record = sample_record();
  record.instance_id = "instance-atomic";
  ASSERT_TRUE(InstanceRegistry::write(record));

  const std::filesystem::path index =
      base / ".godot-autopilot" / "instances.json";
  EXPECT_TRUE(std::filesystem::exists(index));

  int temp_count = 0;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(base, ec)) {
    if (entry.is_regular_file(ec) && entry.path().extension() == ".tmp") {
      ++temp_count;
    }
  }
  EXPECT_EQ(temp_count, 0);

  unset_registry_env("GODOT_AUTOPILOT_HOME");
  std::filesystem::remove_all(base, ec);
}

TEST(InstanceRegistryTest, IndexAlwaysReflectsCompleteRecords) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_instance_registry_index_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  set_registry_env("GODOT_AUTOPILOT_HOME", base.u8string().c_str());
  InstanceRecord first = sample_record();
  first.instance_id = "index-first";
  ASSERT_TRUE(InstanceRegistry::write(first));
  InstanceRecord second = sample_record();
  second.instance_id = "index-second";
  ASSERT_TRUE(InstanceRegistry::write(second));

  const std::vector<InstanceRecord> listed = InstanceRegistry::list_all();
  ASSERT_EQ(listed.size(), 2u);

  const std::filesystem::path index =
      base / ".godot-autopilot" / "instances.json";
  std::ifstream in(index, std::ios::binary);
  ASSERT_TRUE(in.good());
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  godot_autopilot::util::JsonValue index_doc;
  ASSERT_NO_THROW(index_doc = godot_autopilot::util::JsonValue::Parse(text));
  const godot_autopilot::util::JsonValue *instances = index_doc.Find("instances");
  ASSERT_NE(instances, nullptr);
  ASSERT_TRUE(instances->IsArray());
  EXPECT_EQ(instances->GetArray().size(), 2u);

  unset_registry_env("GODOT_AUTOPILOT_HOME");
  std::filesystem::remove_all(base, ec);
}

TEST(InstanceRegistryTest, RemoveUpdatesIndexAtomically) {
  const std::filesystem::path base =
      gda_test::test_temp_root() / "gda_instance_registry_remove_test";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  ASSERT_TRUE(std::filesystem::create_directories(base, ec));
  ASSERT_FALSE(ec);

  set_registry_env("GODOT_AUTOPILOT_HOME", base.u8string().c_str());
  InstanceRecord record = sample_record();
  record.instance_id = "remove-me";
  ASSERT_TRUE(InstanceRegistry::write(record));
  ASSERT_TRUE(InstanceRegistry::remove("remove-me"));

  const std::filesystem::path index =
      base / ".godot-autopilot" / "instances.json";
  std::ifstream in(index, std::ios::binary);
  ASSERT_TRUE(in.good());
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  godot_autopilot::util::JsonValue index_doc = godot_autopilot::util::JsonValue::Parse(text);
  const godot_autopilot::util::JsonValue *instances = index_doc.Find("instances");
  ASSERT_NE(instances, nullptr);
  ASSERT_TRUE(instances->IsArray());
  EXPECT_TRUE(instances->GetArray().empty());
  EXPECT_FALSE(std::filesystem::exists(index.string() + ".tmp"));

  unset_registry_env("GODOT_AUTOPILOT_HOME");
  std::filesystem::remove_all(base, ec);
}
