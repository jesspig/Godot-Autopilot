#ifndef GODOT_AUTOPILOT_ERROR_WATERMARK_HPP
#define GODOT_AUTOPILOT_ERROR_WATERMARK_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace error_watermark {

struct WatermarkState {
  std::mutex mtx;
  std::unordered_map<std::string, int64_t> by_request;
  int64_t unattributed = 0;
};

inline WatermarkState &watermark_state() {
  static WatermarkState state;
  return state;
}

inline void record_error(int64_t count = 1,
                         const std::string &request_id = std::string()) {
  if (count <= 0) {
    return;
  }
  WatermarkState &state = watermark_state();
  std::lock_guard<std::mutex> lock(state.mtx);
  if (request_id.empty()) {
    state.unattributed += count;
  } else {
    state.by_request[request_id] += count;
  }
}

inline int64_t consume_new_errors(
    const std::string &request_id = std::string()) {
  WatermarkState &state = watermark_state();
  std::lock_guard<std::mutex> lock(state.mtx);
  int64_t total = state.unattributed;
  state.unattributed = 0;
  if (!request_id.empty()) {
    auto it = state.by_request.find(request_id);
    if (it != state.by_request.end()) {
      total += it->second;
      state.by_request.erase(it);
    }
  }
  return total;
}

inline bool object_has_error(const mcp::JsonValue &object) {
  if (!object.IsObject()) {
    return false;
  }
  if (object.Find("error") != nullptr) {
    return true;
  }
  if (const mcp::JsonValue *runtime = object.Find("runtime_error");
      runtime != nullptr && runtime->IsBool() && runtime->GetBool()) {
    return true;
  }
  if (const mcp::JsonValue *structured = object.Find("structured_error");
      structured != nullptr && structured->IsObject()) {
    if (const mcp::JsonValue *code = structured->Find("code");
        code != nullptr && code->IsString() && !code->GetString().empty()) {
      return true;
    }
  }
  return false;
}

inline int64_t count_response_errors(const mcp::JsonValue &response) {
  if (!response.IsObject()) {
    return 0;
  }
  int64_t count = object_has_error(response) ? 1 : 0;
  if (const mcp::JsonValue *results = response.Find("results");
      results != nullptr && results->IsArray()) {
    for (const auto &item : results->GetArray()) {
      if (object_has_error(item)) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace error_watermark
} // namespace godot_autopilot

#endif
