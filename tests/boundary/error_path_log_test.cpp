#include "core/log_system.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::ErrorPathEvent;
using godot_autopilot::LogCategory;
using godot_autopilot::LogEntry;
using godot_autopilot::LogLevel;
using godot_autopilot::LogSystem;
using godot_autopilot::classify_error_path_exception;
using godot_autopilot::format_error_path_detail;
using godot_autopilot::log_error_path;

namespace {

ErrorPathEvent make_event() {
    ErrorPathEvent event;
    event.request_id = "REQPATH_7";
    event.method = "tools/call";
    event.error_code = -32603;
    event.connection_id = "conn-abc";
    event.exception_type = "non_std_exception";
    return event;
}

}

TEST(ErrorPathLogTest, FormatCarriesInterfaceFields) {
    const std::string detail = format_error_path_detail(make_event());
    EXPECT_NE(detail.find("request_id=\"REQPATH_7\""), std::string::npos);
    EXPECT_NE(detail.find("method=\"tools/call\""), std::string::npos);
    EXPECT_NE(detail.find("error_code=-32603"), std::string::npos);
    EXPECT_NE(detail.find("connection_id=\"conn-abc\""), std::string::npos);
    EXPECT_NE(detail.find("exception_type=\"non_std_exception\""),
              std::string::npos);
}

TEST(ErrorPathLogTest, FormatEscapesControlCharacters) {
    ErrorPathEvent event = make_event();
    event.method = "tools/call\nforged";
    const std::string detail = format_error_path_detail(event);
    EXPECT_EQ(detail.find('\n'), std::string::npos);
    EXPECT_NE(detail.find("tools/call forged"), std::string::npos);
}

TEST(ErrorPathLogTest, ClassifyExceptionType) {
    EXPECT_EQ(classify_error_path_exception("internal error: non-std exception"),
              "non_std_exception");
    EXPECT_EQ(classify_error_path_exception("internal error: codec failed"),
              "std_exception");
    EXPECT_EQ(classify_error_path_exception("handler error: bad state"),
              "std_exception");
    EXPECT_TRUE(classify_error_path_exception("unknown tool: missing").empty());
}

TEST(ErrorPathLogTest, LogErrorPathWritesTransportErrorEntry) {
    log_error_path(make_event());
    LogSystem::Query query;
    query.filter_text = "REQPATH_7";
    const std::vector<LogEntry> entries = LogSystem::instance().query(query);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].level, LogLevel::Error);
    EXPECT_EQ(entries[0].category, LogCategory::Transport);
    EXPECT_EQ(entries[0].message, "MCP error path");
    EXPECT_NE(entries[0].detail.find("method=\"tools/call\""),
              std::string::npos);
    EXPECT_NE(entries[0].detail.find("error_code=-32603"), std::string::npos);
    EXPECT_NE(entries[0].detail.find("connection_id=\"conn-abc\""),
              std::string::npos);
}
