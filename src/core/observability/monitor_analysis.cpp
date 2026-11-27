#include "core/observability/monitor_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "util/json_value.hpp"

#include "util/input_guards.hpp"
#include "util/string_util.hpp"

namespace godot_autopilot {
namespace game_monitor_analysis {

namespace {

double number_field(const godot_autopilot::util::JsonValue &object, const char *key,
                    double fallback = 0.0) {
  const godot_autopilot::util::JsonValue *value = object.Find(key);
  if (value == nullptr) {
    return fallback;
  }
  if (value->IsInt()) {
    return static_cast<double>(value->GetInt());
  }
  if (value->IsDouble()) {
    return value->GetDouble();
  }
  return fallback;
}

std::string string_field(const godot_autopilot::util::JsonValue &object, const char *key) {
  const godot_autopilot::util::JsonValue *value = object.Find(key);
  if (value != nullptr && value->IsString()) {
    return value->GetString();
  }
  return std::string();
}

bool read_definition(const godot_autopilot::util::JsonValue &entry, MonitorDef *out) {
  if (!entry.IsObject()) {
    return false;
  }
  out->index = static_cast<int>(number_field(entry, "i"));
  out->name = string_field(entry, "n");
  out->type = string_field(entry, "t");
  out->category = string_field(entry, "cat");
  return !out->name.empty();
}

double percentile(const std::vector<double> &sorted, double fraction) {
  if (sorted.empty()) {
    return 0.0;
  }
  const double raw = std::ceil(fraction * static_cast<double>(sorted.size())) - 1.0;
  const std::size_t index = static_cast<std::size_t>(
      std::max(0.0, std::min(raw, static_cast<double>(sorted.size() - 1))));
  return sorted[index];
}

SeriesStat make_stat(const std::string &name, const std::string &type,
                     const std::vector<double> &source) {
  SeriesStat stat;
  stat.name = name;
  stat.type = type;
  std::vector<double> values;
  values.reserve(source.size());
  for (double value : source) {
    if (std::isfinite(value)) {
      values.push_back(value);
    }
  }
  stat.samples = static_cast<uint64_t>(values.size());
  if (values.empty()) {
    return stat;
  }
  const double sum = std::accumulate(values.begin(), values.end(), 0.0);
  stat.avg = sum / static_cast<double>(values.size());
  stat.min = *std::min_element(values.begin(), values.end());
  stat.max = *std::max_element(values.begin(), values.end());
  stat.first = values.front();
  stat.last = values.back();
  stat.delta = stat.last - stat.first;
  std::sort(values.begin(), values.end());
  stat.p50 = percentile(values, 0.50);
  stat.p95 = percentile(values, 0.95);
  stat.p99 = percentile(values, 0.99);
  return stat;
}

void append_number(std::string &out, double value) {
  if (!std::isfinite(value)) {
    out += "0";
    return;
  }
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "%.6f", value);
  out += buffer;
}

void append_quoted(std::string &out, const std::string &value) {
  out += '"';
  out += util::json_escape(value);
  out += '"';
}

} // namespace

bool parse_header(const std::string &line, std::string *session_id,
                  std::vector<MonitorDef> *builtin,
                  std::vector<MonitorDef> *custom) {
  godot_autopilot::util::JsonValue root;
  if (!util::try_parse_json(line, root) || !root.IsObject()) {
    return false;
  }
  const godot_autopilot::util::JsonValue *type = root.Find("type");
  if (type == nullptr || !type->IsString() || type->GetString() != "session") {
    return false;
  }
  if (session_id != nullptr) {
    *session_id = string_field(root, "id");
  }
  if (builtin != nullptr) {
    builtin->clear();
    const godot_autopilot::util::JsonValue *array = root.Find("builtin_monitors");
    if (array != nullptr && array->IsArray()) {
      for (const godot_autopilot::util::JsonValue &item : array->GetArray()) {
        MonitorDef definition;
        if (read_definition(item, &definition)) {
          builtin->push_back(std::move(definition));
        }
      }
    }
  }
  if (custom != nullptr) {
    custom->clear();
    const godot_autopilot::util::JsonValue *array = root.Find("custom_monitors");
    if (array != nullptr && array->IsArray()) {
      for (const godot_autopilot::util::JsonValue &item : array->GetArray()) {
        MonitorDef definition;
        if (read_definition(item, &definition)) {
          custom->push_back(std::move(definition));
        }
      }
    }
  }
  return true;
}

bool parse_frame(const std::string &line, FrameSample *out) {
  if (out == nullptr) {
    return false;
  }
  godot_autopilot::util::JsonValue root;
  if (!util::try_parse_json(line, root) || !root.IsObject()) {
    return false;
  }
  const godot_autopilot::util::JsonValue *type = root.Find("type");
  if (type == nullptr || !type->IsString() || type->GetString() != "frame") {
    return false;
  }
  out->number = static_cast<uint64_t>(number_field(root, "n"));
  out->delta_ms = number_field(root, "d");
  out->elapsed_ms = number_field(root, "t");
  out->builtin.clear();
  out->custom.clear();
  const char *arrays[] = {"v", "c"};
  std::vector<double> *targets[] = {&out->builtin, &out->custom};
  for (int i = 0; i < 2; ++i) {
    const godot_autopilot::util::JsonValue *array = root.Find(arrays[i]);
    if (array == nullptr || !array->IsArray()) {
      continue;
    }
    for (const godot_autopilot::util::JsonValue &item : array->GetArray()) {
      if (item.IsNull()) {
        targets[i]->push_back(std::numeric_limits<double>::quiet_NaN());
      } else if (item.IsInt()) {
        targets[i]->push_back(static_cast<double>(item.GetInt()));
      } else if (item.IsDouble()) {
        targets[i]->push_back(item.GetDouble());
      } else {
        targets[i]->push_back(std::numeric_limits<double>::quiet_NaN());
      }
    }
  }
  return true;
}

bool parse_custom_change(const std::string &line,
                         std::vector<MonitorDef> *custom) {
  if (custom == nullptr) {
    return false;
  }
  godot_autopilot::util::JsonValue root;
  if (!util::try_parse_json(line, root) || !root.IsObject()) {
    return false;
  }
  const godot_autopilot::util::JsonValue *type = root.Find("type");
  if (type == nullptr || !type->IsString() ||
      type->GetString() != "custom_change") {
    return false;
  }
  custom->clear();
  const godot_autopilot::util::JsonValue *array = root.Find("custom_monitors");
  if (array != nullptr && array->IsArray()) {
    for (const godot_autopilot::util::JsonValue &item : array->GetArray()) {
      MonitorDef definition;
      if (read_definition(item, &definition)) {
        custom->push_back(std::move(definition));
      }
    }
  }
  return true;
}

Report analyze(const std::vector<FrameSample> &frames,
               const std::vector<MonitorDef> &builtin,
               const std::vector<MonitorDef> &custom,
               double slow_threshold_ms) {
  Report report;
  report.frames = static_cast<uint64_t>(frames.size());
  if (!frames.empty()) {
    report.duration_ms =
        frames.back().elapsed_ms - frames.front().elapsed_ms;
    if (report.duration_ms < 0.0) {
      report.duration_ms = 0.0;
    }
  }
  std::vector<double> deltas;
  deltas.reserve(frames.size());
  for (const FrameSample &frame : frames) {
    if (std::isfinite(frame.delta_ms)) {
      deltas.push_back(frame.delta_ms);
    }
    if (frame.delta_ms > slow_threshold_ms) {
      ++report.slow_frames;
    }
  }
  const SeriesStat frame_stat = make_stat("frame/delta_ms", "time", deltas);
  report.frame_avg_ms = frame_stat.avg;
  report.frame_p95_ms = frame_stat.p95;
  report.frame_p99_ms = frame_stat.p99;
  report.frame_max_ms = frame_stat.max;

  for (std::size_t i = 0; i < builtin.size(); ++i) {
    std::vector<double> values;
    values.reserve(frames.size());
    for (const FrameSample &frame : frames) {
      values.push_back(i < frame.builtin.size() ? frame.builtin[i] : 0.0);
    }
    report.series.push_back(make_stat(builtin[i].name, builtin[i].type, values));
  }
  for (std::size_t i = 0; i < custom.size(); ++i) {
    std::vector<double> values;
    values.reserve(frames.size());
    for (const FrameSample &frame : frames) {
      values.push_back(i < frame.custom.size() ? frame.custom[i] : 0.0);
    }
    report.series.push_back(make_stat(custom[i].name, custom[i].type, values));
  }
  return report;
}

std::string report_to_json(const Report &report) {
  std::string out;
  out.reserve(4096);
  out += "{\"session_id\":";
  append_quoted(out, report.session_id);
  out += ",\"frames\":";
  out += std::to_string(report.frames);
  out += ",\"duration_ms\":";
  append_number(out, report.duration_ms);
  out += ",\"frame\":{\"avg_ms\":";
  append_number(out, report.frame_avg_ms);
  out += ",\"p95_ms\":";
  append_number(out, report.frame_p95_ms);
  out += ",\"p99_ms\":";
  append_number(out, report.frame_p99_ms);
  out += ",\"max_ms\":";
  append_number(out, report.frame_max_ms);
  out += ",\"slow_frames\":";
  out += std::to_string(report.slow_frames);
  out += "},\"series\":[";
  for (std::size_t i = 0; i < report.series.size(); ++i) {
    const SeriesStat &stat = report.series[i];
    if (i != 0) {
      out += ',';
    }
    out += "{\"name\":";
    append_quoted(out, stat.name);
    out += ",\"type\":";
    append_quoted(out, stat.type);
    out += ",\"samples\":";
    out += std::to_string(stat.samples);
    out += ",\"min\":";
    append_number(out, stat.min);
    out += ",\"max\":";
    append_number(out, stat.max);
    out += ",\"avg\":";
    append_number(out, stat.avg);
    out += ",\"p50\":";
    append_number(out, stat.p50);
    out += ",\"p95\":";
    append_number(out, stat.p95);
    out += ",\"p99\":";
    append_number(out, stat.p99);
    out += ",\"first\":";
    append_number(out, stat.first);
    out += ",\"last\":";
    append_number(out, stat.last);
    out += ",\"delta\":";
    append_number(out, stat.delta);
    out += '}';
  }
  out += "]}";
  return out;
}

std::string session_list_to_json(const std::vector<std::string> &ids) {
  std::string out = "[";
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    append_quoted(out, ids[i]);
  }
  out += ']';
  return out;
}

} // namespace game_monitor_analysis
} // namespace godot_autopilot
