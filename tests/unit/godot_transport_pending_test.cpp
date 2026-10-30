#include "core/godot_transport.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using godot_autopilot::mcp_wire::make_pending_key;
using godot_autopilot::mcp_wire::request_id_key;

TEST(GodotTransportPendingTest, MakePendingKeySeparatesConnectionAndId) {
  const std::string key_a = make_pending_key("conn-1", "i:7");
  const std::string key_b = make_pending_key("conn-2", "i:7");

  EXPECT_NE(key_a, key_b);
  EXPECT_NE(key_a.find("conn-1"), std::string::npos);
  EXPECT_NE(key_a.find("i:7"), std::string::npos);
}

TEST(GodotTransportPendingTest, MakePendingKeyStableForSameInputs) {
  EXPECT_EQ(make_pending_key("conn-1", "i:7"), make_pending_key("conn-1", "i:7"));
  EXPECT_EQ(make_pending_key("conn-1", "s:abc"),
            make_pending_key("conn-1", "s:abc"));
  EXPECT_NE(make_pending_key("conn-1", "i:7"), make_pending_key("conn-1", "i:8"));
}

TEST(GodotTransportPendingTest, RequestIdKeyDistinguishesIntAndString) {
  EXPECT_EQ(request_id_key(mcp::RequestId{int64_t{7}}), "i:7");
  EXPECT_EQ(request_id_key(mcp::RequestId{std::string("i:7")}), "s:i:7");
  EXPECT_NE(request_id_key(mcp::RequestId{int64_t{7}}),
            request_id_key(mcp::RequestId{std::string("7")}));
}
