#include "core/http_stream_registry.hpp"

#include <utility>
#include <vector>

namespace godot_autopilot {
namespace http_stream {

namespace {

std::mutex &registry_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::string, std::shared_ptr<Sink>> &registry() {
  static std::unordered_map<std::string, std::shared_ptr<Sink>> entries;
  return entries;
}

} // namespace

void Sink::push(const std::string &frame) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) {
    return;
  }
  frames_.push_back(frame);
}

std::vector<std::string> Sink::drain() {
  std::vector<std::string> drained;
  std::lock_guard<std::mutex> lock(mutex_);
  drained.reserve(frames_.size());
  while (!frames_.empty()) {
    drained.push_back(std::move(frames_.front()));
    frames_.pop_front();
  }
  return drained;
}

void Sink::close() {
  std::lock_guard<std::mutex> lock(mutex_);
  closed_ = true;
  frames_.clear();
}

bool Sink::closed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return closed_;
}

std::shared_ptr<Sink> acquire(const std::string &connection_id) {
  if (connection_id.empty()) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(registry_mutex());
  auto &entries = registry();
  auto found = entries.find(connection_id);
  if (found != entries.end()) {
    return found->second;
  }
  auto sink = std::make_shared<Sink>();
  entries.emplace(connection_id, sink);
  return sink;
}

void release(const std::string &connection_id) {
  if (connection_id.empty()) {
    return;
  }
  std::shared_ptr<Sink> sink;
  {
    std::lock_guard<std::mutex> lock(registry_mutex());
    auto &entries = registry();
    auto found = entries.find(connection_id);
    if (found == entries.end()) {
      return;
    }
    sink = found->second;
    entries.erase(found);
  }
  if (sink) {
    sink->close();
  }
}

bool send_to(const std::string &connection_id, const std::string &frame) {
  if (connection_id.empty()) {
    return false;
  }
  std::shared_ptr<Sink> sink;
  {
    std::lock_guard<std::mutex> lock(registry_mutex());
    const auto found = registry().find(connection_id);
    if (found == registry().end()) {
      return false;
    }
    sink = found->second;
  }
  if (!sink) {
    return false;
  }
  sink->push(frame);
  return true;
}

size_t broadcast(const std::string &frame) {
  std::vector<std::shared_ptr<Sink>> sinks;
  {
    std::lock_guard<std::mutex> lock(registry_mutex());
    sinks.reserve(registry().size());
    for (const auto &entry : registry()) {
      sinks.push_back(entry.second);
    }
  }
  size_t delivered = 0;
  for (const std::shared_ptr<Sink> &sink : sinks) {
    if (!sink) {
      continue;
    }
    sink->push(frame);
    ++delivered;
  }
  return delivered;
}

size_t active_count() {
  std::lock_guard<std::mutex> lock(registry_mutex());
  return registry().size();
}

} // namespace http_stream
} // namespace godot_autopilot
