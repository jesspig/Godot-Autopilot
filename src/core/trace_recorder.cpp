#include "trace_recorder.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <utility>

namespace godot_autopilot {

namespace {

std::atomic<uint64_t> g_id_counter{1};

const char *kStripKeys[] = {"data", "base64", "script_content"};

bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string strip_key_values(const std::string &text) {
  std::string out = text;
  for (const char *key : kStripKeys) {
    std::string token = "\"";
    token += key;
    token += "\"";
    std::size_t pos = 0;
    while ((pos = out.find(token, pos)) != std::string::npos) {
      std::size_t cur = pos + token.size();
      while (cur < out.size() && is_blank(out[cur])) {
        ++cur;
      }
      if (cur >= out.size() || out[cur] != ':') {
        pos += token.size();
        continue;
      }
      ++cur;
      while (cur < out.size() && is_blank(out[cur])) {
        ++cur;
      }
      if (cur >= out.size() || out[cur] != '"') {
        pos += token.size();
        continue;
      }
      ++cur;
      std::size_t begin = cur;
      bool closed = false;
      while (cur < out.size()) {
        if (out[cur] == '\\') {
          cur += 2;
          continue;
        }
        if (out[cur] == '"') {
          closed = true;
          break;
        }
        ++cur;
      }
      if (!closed) {
        break;
      }
      char buf[64];
      std::snprintf(buf, sizeof(buf), "<stripped len=%llu>",
                    static_cast<unsigned long long>(cur - begin));
      std::string replacement(buf);
      out.replace(begin, cur - begin, replacement);
      pos = begin + replacement.size();
    }
  }
  return out;
}

void append_escaped(std::string &out, const std::string &value) {
  for (unsigned char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
        break;
    }
  }
}

void append_string_field(std::string &out, const char *name, const std::string &value, bool first) {
  if (!first) {
    out += ',';
  }
  out += '"';
  out += name;
  out += "\":\"";
  append_escaped(out, value);
  out += '"';
}

void append_int_field(std::string &out, const char *name, int64_t value, bool first) {
  if (!first) {
    out += ',';
  }
  out += '"';
  out += name;
  out += "\":";
  out += std::to_string(value);
}

void append_bool_field(std::string &out, const char *name, bool value, bool first) {
  if (!first) {
    out += ',';
  }
  out += '"';
  out += name;
  out += value ? "\":true" : "\":false";
}

} // namespace

TraceRecorder &TraceRecorder::instance() {
  static TraceRecorder recorder;
  return recorder;
}

uint64_t TraceRecorder::record(TraceEvent event) {
  std::lock_guard<std::mutex> lock(mutex_);
  event.seq = next_seq_++;
  uint64_t seq = event.seq;
  events_.push_back(std::move(event));
  while (events_.size() > kCapacity) {
    events_.pop_front();
  }
  return seq;
}

std::vector<TraceEvent> TraceRecorder::query_recent(std::size_t limit) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TraceEvent> out;
  if (limit == 0 || events_.empty()) {
    return out;
  }
  std::size_t count = limit < events_.size() ? limit : events_.size();
  out.reserve(count);
  auto it = events_.end() - static_cast<std::deque<TraceEvent>::difference_type>(count);
  for (; it != events_.end(); ++it) {
    out.push_back(*it);
  }
  return out;
}

std::vector<TraceEvent> TraceRecorder::query_by_trace(const std::string &trace_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TraceEvent> out;
  for (const TraceEvent &event : events_) {
    if (event.trace_id == trace_id) {
      out.push_back(event);
    }
  }
  return out;
}

std::vector<TraceEvent> TraceRecorder::query_since(uint64_t since_seq,
                                                   uint64_t *next_seq) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<TraceEvent> out;
  for (const TraceEvent &event : events_) {
    if (event.seq >= since_seq) {
      out.push_back(event);
    }
  }
  if (next_seq) {
    *next_seq = next_seq_;
  }
  return out;
}

uint64_t TraceRecorder::next_seq() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return next_seq_;
}

void TraceRecorder::clear_for_test() {
  std::lock_guard<std::mutex> lock(mutex_);
  events_.clear();
  next_seq_ = 1;
}

std::size_t TraceRecorder::size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return events_.size();
}

int64_t TraceRecorder::wall_now_ms() {
  auto now = std::chrono::system_clock::now();
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

std::string TraceRecorder::new_trace_id() {
  uint64_t n = g_id_counter.fetch_add(1);
  std::string id = "tr_";
  id += std::to_string(wall_now_ms());
  id += "_";
  id += std::to_string(n);
  return id;
}

std::string TraceRecorder::new_span_id() {
  uint64_t n = g_id_counter.fetch_add(1);
  std::string id = "sp_";
  id += std::to_string(wall_now_ms());
  id += "_";
  id += std::to_string(n);
  return id;
}

SanitizeResult TraceRecorder::sanitize_args(const std::string &args_dump, bool desensitize) {
  SanitizeResult result;
  std::size_t max_chars = desensitize ? kDesensitizedMaxChars : kRawMaxChars;
  std::string text = desensitize ? strip_key_values(args_dump) : args_dump;
  if (text.size() > max_chars) {
    text.resize(max_chars);
    result.truncated = true;
  } else {
    result.truncated = false;
  }
  result.text = text;
  return result;
}

std::string TraceRecorder::fnv1a_hex(const std::string &text) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : text) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
  return std::string(buf);
}

std::string TraceRecorder::decode_base64(const std::string &input) {
  auto value_of = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') {
      return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
      return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
      return c - '0' + 52;
    }
    if (c == '+') {
      return 62;
    }
    if (c == '/') {
      return 63;
    }
    return -1;
  };
  std::string out;
  out.reserve(input.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (char c : input) {
    if (c == '=') {
      break;
    }
    const int value = value_of(c);
    if (value < 0) {
      continue;
    }
    acc = ((acc << 6) | static_cast<uint32_t>(value)) & 0xFFFu;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((acc >> bits) & 0xFFu);
    }
  }
  return out;
}

std::string TraceRecorder::to_json_line(const TraceEvent &event) {
  std::string out = "{";
  append_int_field(out, "seq", static_cast<int64_t>(event.seq), true);
  append_string_field(out, "trace_id", event.trace_id, false);
  append_string_field(out, "span_id", event.span_id, false);
  append_string_field(out, "parent_span", event.parent_span, false);
  append_string_field(out, "session_id", event.session_id, false);
  append_string_field(out, "tool", event.tool, false);
  append_string_field(out, "category", event.category, false);
  append_int_field(out, "flags", static_cast<int64_t>(event.flags), false);
  append_int_field(out, "side_effect", static_cast<int64_t>(event.side_effect), false);
  append_int_field(out, "depth", static_cast<int64_t>(event.depth), false);
  append_string_field(out, "thread", event.thread, false);
  append_int_field(out, "queue_wait_ms", event.queue_wait_ms, false);
  append_int_field(out, "duration_ms", event.duration_ms, false);
  append_int_field(out, "wall_start_ms", event.wall_start_ms, false);
  append_int_field(out, "wall_end_ms", event.wall_end_ms, false);
  append_string_field(out, "auth", event.auth, false);
  append_bool_field(out, "ok", event.ok, false);
  append_string_field(out, "error_code", event.error_code, false);
  append_string_field(out, "args_digest", event.args_digest, false);
  append_bool_field(out, "args_truncated", event.args_truncated, false);
  append_int_field(out, "result_size", event.result_size, false);
  append_string_field(out, "image_ref", event.image_ref, false);
  append_int_field(out, "image_bytes", event.image_bytes, false);
  append_string_field(out, "image_hash", event.image_hash, false);
  append_int_field(out, "image_width", static_cast<int64_t>(event.image_width), false);
  append_int_field(out, "image_height", static_cast<int64_t>(event.image_height), false);
  out += '}';
  return out;
}

} // namespace godot_autopilot
