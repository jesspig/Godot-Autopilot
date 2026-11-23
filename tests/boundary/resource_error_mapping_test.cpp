#include "resources/resource_error_mapping.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::resource_error_code;

TEST(ResourceErrorMappingTest, MapsNotFoundToInvalidParams) {
  EXPECT_EQ(resource_error_code("resource_not_found"),
            mcp::McpErrorCode::InvalidParams);
}

TEST(ResourceErrorMappingTest, ReadFailedFallsBackToInternalError) {
  EXPECT_EQ(resource_error_code("resource_read_failed"),
            mcp::McpErrorCode::InternalError);
}

TEST(ResourceErrorMappingTest, TimeoutCodeIsNotMappedByResourceLayer) {
  EXPECT_EQ(resource_error_code("main_thread_timeout"),
            mcp::McpErrorCode::InternalError);
}

TEST(ResourceErrorMappingTest, FallsBackToInternalError) {
  EXPECT_EQ(resource_error_code("some_other_code"),
            mcp::McpErrorCode::InternalError);
  EXPECT_EQ(resource_error_code(""), mcp::McpErrorCode::InternalError);
}
