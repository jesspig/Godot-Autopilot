#ifndef GODOT_AUTOPILOT_HTTP_STREAM_REGISTRY_HPP
#define GODOT_AUTOPILOT_HTTP_STREAM_REGISTRY_HPP

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace http_stream {

class Sink {
public:
  void push(const std::string &frame);
  std::vector<std::string> drain();
  void close();
  bool closed() const;

private:
  mutable std::mutex mutex_;
  std::deque<std::string> frames_;
  bool closed_{false};
};

std::shared_ptr<Sink> acquire(const std::string &connection_id);
void release(const std::string &connection_id);
bool send_to(const std::string &connection_id, const std::string &frame);
size_t broadcast(const std::string &frame);
size_t active_count();

} // namespace http_stream
} // namespace godot_autopilot

#endif
