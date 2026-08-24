#ifndef GODOT_AUTOPILOT_ERROR_WATERMARK_HPP
#define GODOT_AUTOPILOT_ERROR_WATERMARK_HPP

#include <cstdint>
#include <mutex>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace error_watermark {

struct WatermarkState {
  std::mutex mtx;
  int64_t pending = 0;
};

inline WatermarkState &watermark_state() {
  static WatermarkState state;
  return state;
}

inline void record_error(int64_t count = 1) {
  WatermarkState &state = watermark_state();
  std::lock_guard<std::mutex> lock(state.mtx);
  state.pending += count;
}

inline int64_t consume_new_errors() {
  WatermarkState &state = watermark_state();
  std::lock_guard<std::mutex> lock(state.mtx);
  int64_t total = state.pending;
  state.pending = 0;
  return total;
}

inline int64_t count_response_errors(const mcp::JsonValue &response) {
  if (!response.IsObject())
    return 0;
  int64_t count = 0;
  if (auto *err = response.Find("error"); err != nullptr && err->IsString()) {
    ++count;
  }
  if (auto *results = response.Find("results");
      results != nullptr && results->IsArray()) {
    for (const auto &item : results->GetArray()) {
      if (!item.IsObject())
        continue;
      if (auto *err = item.Find("error"); err != nullptr && err->IsString()) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace error_watermark
} // namespace godot_autopilot

#endif
