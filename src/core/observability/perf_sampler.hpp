#ifndef GODOT_AUTOPILOT_PERF_SAMPLER_HPP
#define GODOT_AUTOPILOT_PERF_SAMPLER_HPP

#include <cstdint>

namespace godot_autopilot {

class CommandQueue;

namespace perf_sampler {

struct FrameAccumulator {
  uint64_t frames = 0;
  double total_ms = 0.0;
  double max_ms = 0.0;
};

inline void accumulate_frame(FrameAccumulator &acc, double delta_ms) {
  if (delta_ms < 0.0) {
    delta_ms = 0.0;
  }
  ++acc.frames;
  acc.total_ms += delta_ms;
  if (delta_ms > acc.max_ms) {
    acc.max_ms = delta_ms;
  }
}

inline double average_frame_ms(const FrameAccumulator &acc) {
  if (acc.frames == 0) {
    return 0.0;
  }
  return acc.total_ms / static_cast<double>(acc.frames);
}

struct RateWindow {
  uint64_t requests = 0;
  uint64_t responses = 0;
  uint64_t errors = 0;
  int64_t inflight = 0;
  int64_t latency_ms_sum = 0;
  uint64_t latency_samples = 0;
};

struct Rates {
  double requests_per_second = 0.0;
  double errors_per_second = 0.0;
  double error_rate = 0.0;
  double average_latency_ms = 0.0;
};

inline Rates compute_rates(const RateWindow &window, double seconds) {
  Rates out;
  if (seconds <= 0.0) {
    return out;
  }
  out.requests_per_second = static_cast<double>(window.requests) / seconds;
  out.errors_per_second = static_cast<double>(window.errors) / seconds;
  if (window.responses > 0) {
    out.error_rate = static_cast<double>(window.errors) /
                     static_cast<double>(window.responses);
  }
  if (window.latency_samples > 0) {
    out.average_latency_ms = static_cast<double>(window.latency_ms_sum) /
                             static_cast<double>(window.latency_samples);
  }
  return out;
}

inline int64_t milliseconds_to_nanoseconds(int64_t ms) { return ms * 1000000; }

void set_queue(const CommandQueue *queue);
void reset();
void tick(double delta);
int64_t interval_ms();
int64_t request_timeout_ms();

} // namespace perf_sampler
} // namespace godot_autopilot

#endif
