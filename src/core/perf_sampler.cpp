#include "perf_sampler.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/performance.hpp>

#include "command_queue.hpp"
#include "log_persist.hpp"
#include "monitor.hpp"

namespace godot_autopilot {
namespace perf_sampler {

namespace {

const CommandQueue *g_queue = nullptr;
FrameAccumulator g_frame;
double g_elapsed_ms = 0.0;
monitor::Counters g_last;

int64_t env_int(const char *name, int64_t fallback) {
  if (const char *value = std::getenv(name)) {
    char *end = nullptr;
    const long long parsed = std::strtoll(value, &end, 10);
    if (end && *end == '\0' && parsed > 0) {
      return static_cast<int64_t>(parsed);
    }
  }
  return fallback;
}

std::string fixed2(double value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", value);
  return std::string(buf);
}

} // namespace

void set_queue(const CommandQueue *queue) { g_queue = queue; }

void reset() {
  g_frame = FrameAccumulator{};
  g_elapsed_ms = 0.0;
  g_last = monitor::Counters{};
}

int64_t interval_ms() {
  static const int64_t value =
      env_int("GODOT_AUTOPILOT_PERF_INTERVAL_MS", 5000);
  return value;
}

int64_t request_timeout_ms() {
  static const int64_t value =
      env_int("GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS", 60000);
  return value;
}

void tick(double delta) {
  accumulate_frame(g_frame, delta * 1000.0);
  g_elapsed_ms += delta * 1000.0;
  if (g_elapsed_ms < static_cast<double>(interval_ms())) {
    return;
  }

  const double seconds = g_elapsed_ms / 1000.0;
  const monitor::Counters now = monitor::counters();
  RateWindow window;
  window.requests = now.requests - g_last.requests;
  window.responses = now.responses - g_last.responses;
  window.errors = now.errors - g_last.errors;
  window.inflight = now.inflight;
  window.latency_ms_sum = now.latency_ms_sum - g_last.latency_ms_sum;
  window.latency_samples = now.latency_samples - g_last.latency_samples;
  g_last = now;
  const Rates rates = compute_rates(window, seconds);

  double fps = 0.0;
  int64_t objects = 0;
  double time_process = 0.0;
  int64_t nodes = 0;
  if (auto *engine = godot::Engine::get_singleton()) {
    fps = engine->get_frames_per_second();
  }
  if (auto *performance = godot::Performance::get_singleton()) {
    time_process = performance->get_monitor(godot::Performance::TIME_PROCESS);
    objects = static_cast<int64_t>(
        performance->get_monitor(godot::Performance::OBJECT_COUNT));
    nodes = static_cast<int64_t>(
        performance->get_monitor(godot::Performance::OBJECT_NODE_COUNT));
  }

  int64_t memory = 0;
  if (auto *os = godot::OS::get_singleton()) {
    memory = static_cast<int64_t>(os->get_static_memory_usage());
  }

  size_t pending = 0;
  uint64_t rejected = 0;
  uint64_t queue_lock_ms = 0;
  if (g_queue) {
    const CommandQueue::Stats stats = g_queue->stats();
    pending = stats.pending;
    rejected = stats.rejected_full + stats.rejected_closed;
    queue_lock_ms = stats.lock_wait_ns / 1000000ULL;
  }

  const std::string attrs = monitor::build_attrs({
      {"window_ms", fixed2(g_elapsed_ms)},
      {"frame_avg_ms", fixed2(average_frame_ms(g_frame))},
      {"frame_max_ms", fixed2(g_frame.max_ms)},
      {"frames", std::to_string(g_frame.frames)},
      {"fps", fixed2(fps)},
      {"cpu_frame_time_ms", fixed2(time_process * 1000.0)},
      {"memory_static_bytes", std::to_string(memory)},
      {"object_count", std::to_string(objects)},
      {"node_count", std::to_string(nodes)},
      {"queue_depth", std::to_string(pending)},
      {"queue_rejected", std::to_string(rejected)},
      {"queue_lock_wait_ms", std::to_string(queue_lock_ms)},
      {"requests", std::to_string(window.requests)},
      {"responses", std::to_string(window.responses)},
      {"errors", std::to_string(window.errors)},
      {"inflight", std::to_string(window.inflight)},
      {"throughput_rps", fixed2(rates.requests_per_second)},
      {"error_rate", fixed2(rates.error_rate)},
      {"avg_latency_ms", fixed2(rates.average_latency_ms)},
  });
  monitor::perf("interval", attrs);

  monitor::expire_requests(milliseconds_to_nanoseconds(request_timeout_ms()));
  monitor::persist_health("snapshot", LogPersist::instance().health_summary());

  g_frame = FrameAccumulator{};
  g_elapsed_ms = 0.0;
}

} // namespace perf_sampler
} // namespace godot_autopilot
