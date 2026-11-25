#ifndef GODOT_AUTOPILOT_MONITOR_ANALYSIS_HPP
#define GODOT_AUTOPILOT_MONITOR_ANALYSIS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace game_monitor_analysis {

struct MonitorDef {
  int index = 0;
  std::string name;
  std::string type;
  std::string category;
};

struct FrameSample {
  uint64_t number = 0;
  double delta_ms = 0.0;
  double elapsed_ms = 0.0;
  std::vector<double> builtin;
  std::vector<double> custom;
};

struct SeriesStat {
  std::string name;
  std::string type;
  uint64_t samples = 0;
  double min = 0.0;
  double max = 0.0;
  double avg = 0.0;
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
  double first = 0.0;
  double last = 0.0;
  double delta = 0.0;
};

struct Report {
  std::string session_id;
  uint64_t frames = 0;
  double duration_ms = 0.0;
  double frame_avg_ms = 0.0;
  double frame_p95_ms = 0.0;
  double frame_p99_ms = 0.0;
  double frame_max_ms = 0.0;
  uint64_t slow_frames = 0;
  std::vector<SeriesStat> series;
};

bool parse_header(const std::string &line, std::string *session_id,
                  std::vector<MonitorDef> *builtin,
                  std::vector<MonitorDef> *custom);

bool parse_frame(const std::string &line, FrameSample *out);

bool parse_custom_change(const std::string &line,
                         std::vector<MonitorDef> *custom);

Report analyze(const std::vector<FrameSample> &frames,
               const std::vector<MonitorDef> &builtin,
               const std::vector<MonitorDef> &custom,
               double slow_threshold_ms);

std::string report_to_json(const Report &report);

std::string session_list_to_json(const std::vector<std::string> &ids);

} // namespace game_monitor_analysis
} // namespace godot_autopilot

#endif
