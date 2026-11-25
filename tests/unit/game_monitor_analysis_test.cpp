#include "core/observability/monitor_analysis.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using godot_autopilot::game_monitor_analysis::FrameSample;
using godot_autopilot::game_monitor_analysis::MonitorDef;
using godot_autopilot::game_monitor_analysis::analyze;
using godot_autopilot::game_monitor_analysis::parse_custom_change;
using godot_autopilot::game_monitor_analysis::parse_frame;
using godot_autopilot::game_monitor_analysis::parse_header;
using godot_autopilot::game_monitor_analysis::report_to_json;

TEST(GameMonitorAnalysisTest, ParseHeaderReadsSessionAndDefinitions) {
  const std::string line =
      "{\"type\":\"session\",\"schema\":1,\"id\":\"s-1\","
      "\"builtin_monitors\":[{\"i\":0,\"n\":\"time/fps\",\"t\":\"time\"}],"
      "\"custom_monitors\":[{\"i\":0,\"n\":\"Game/Enemies\",\"t\":0,"
      "\"cat\":\"Game\"}]}";
  std::string id;
  std::vector<MonitorDef> builtin;
  std::vector<MonitorDef> custom;
  ASSERT_TRUE(parse_header(line, &id, &builtin, &custom));
  EXPECT_EQ(id, "s-1");
  ASSERT_EQ(builtin.size(), 1u);
  EXPECT_EQ(builtin[0].name, "time/fps");
  EXPECT_EQ(builtin[0].type, "time");
  ASSERT_EQ(custom.size(), 1u);
  EXPECT_EQ(custom[0].name, "Game/Enemies");
  EXPECT_EQ(custom[0].category, "Game");
}

TEST(GameMonitorAnalysisTest, ParseFrameKeepsNullCustomValue) {
  const std::string line =
      "{\"type\":\"frame\",\"n\":5,\"f\":9,\"pf\":3,\"d\":16.5,"
      "\"t\":100.0,\"w\":1000,\"v\":[60,0.016],\"c\":[12,null]}";
  FrameSample frame;
  ASSERT_TRUE(parse_frame(line, &frame));
  EXPECT_EQ(frame.number, 5u);
  EXPECT_DOUBLE_EQ(frame.delta_ms, 16.5);
  ASSERT_EQ(frame.builtin.size(), 2u);
  EXPECT_DOUBLE_EQ(frame.builtin[1], 0.016);
  ASSERT_EQ(frame.custom.size(), 2u);
  EXPECT_DOUBLE_EQ(frame.custom[0], 12.0);
  EXPECT_TRUE(std::isnan(frame.custom[1]));
}

TEST(GameMonitorAnalysisTest, ParseCustomChangeReplacesDefinitions) {
  const std::string line =
      "{\"type\":\"custom_change\",\"n\":7,\"custom_monitors\":["
      "{\"i\":0,\"n\":\"Game/DPS\",\"t\":\"time\",\"cat\":\"Game\"}]}";
  std::vector<MonitorDef> custom;
  ASSERT_TRUE(parse_custom_change(line, &custom));
  ASSERT_EQ(custom.size(), 1u);
  EXPECT_EQ(custom[0].name, "Game/DPS");
  EXPECT_EQ(custom[0].type, "time");
}

TEST(GameMonitorAnalysisTest, AnalyzeComputesFrameAndSeriesStatistics) {
  std::vector<MonitorDef> builtin = {{0, "time/fps", "time", ""}};
  std::vector<MonitorDef> custom = {{0, "Game/Enemies", "quantity", "Game"}};
  std::vector<FrameSample> frames;
  const char *lines[] = {
      "{\"type\":\"frame\",\"n\":1,\"d\":10,\"t\":0,\"v\":[50],\"c\":[1]}",
      "{\"type\":\"frame\",\"n\":2,\"d\":20,\"t\":10,\"v\":[60],\"c\":[3]}",
      "{\"type\":\"frame\",\"n\":3,\"d\":30,\"t\":30,\"v\":[70],\"c\":[5]}"};
  for (const char *line : lines) {
    FrameSample frame;
    ASSERT_TRUE(parse_frame(line, &frame));
    frames.push_back(std::move(frame));
  }
  const auto report = analyze(frames, builtin, custom, 25.0);
  EXPECT_EQ(report.frames, 3u);
  EXPECT_DOUBLE_EQ(report.duration_ms, 30.0);
  EXPECT_DOUBLE_EQ(report.frame_avg_ms, 20.0);
  EXPECT_DOUBLE_EQ(report.frame_max_ms, 30.0);
  EXPECT_EQ(report.slow_frames, 1u);
  ASSERT_EQ(report.series.size(), 2u);
  EXPECT_EQ(report.series[0].name, "time/fps");
  EXPECT_DOUBLE_EQ(report.series[0].avg, 60.0);
  EXPECT_DOUBLE_EQ(report.series[1].delta, 4.0);
  const std::string json = report_to_json(report);
  EXPECT_NE(json.find("\"session_id\""), std::string::npos);
  EXPECT_NE(json.find("\"Game/Enemies\""), std::string::npos);
}

TEST(GameMonitorAnalysisTest, RejectsNonSessionAndNonFrameLines) {
  std::string id;
  std::vector<MonitorDef> builtin;
  EXPECT_FALSE(parse_header("{\"type\":\"frame\"}", &id, &builtin, nullptr));
  FrameSample frame;
  EXPECT_FALSE(parse_frame("{\"type\":\"session\"}", &frame));
}
