#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

struct EventRecord {
  std::uint64_t seq = 0;
  std::string instance_id;
  std::string event_type;
  std::string payload;
  std::string traceparent;
  std::optional<std::string> task_id;
  std::optional<std::string> request_id;
};

struct Subscription {
  std::string id;
  std::string instance_id;
  std::string event_type;
  std::uint64_t cursor = 0;
  std::size_t delivered = 0;
  bool active = true;
};

class EventService {
public:
  explicit EventService(std::size_t soft_limit = 100,
                        std::size_t hard_limit = 1000);

  EventRecord publish(const std::string &instance_id,
                      const std::string &event_type,
                      const std::string &payload,
                      const std::string &traceparent = std::string(),
                      const std::optional<std::string> &task_id = std::nullopt,
                      const std::optional<std::string> &request_id =
                          std::nullopt);

  std::string subscribe(const std::string &instance_id,
                        const std::string &event_type, std::uint64_t cursor);
  bool unsubscribe(const std::string &id);

  std::vector<EventRecord> poll(const std::string &id, std::size_t limit,
                                gcp::Backpressure *pressure = nullptr);

  bool cursor_valid(const std::string &instance_id,
                    std::uint64_t cursor) const;
  gcp::Backpressure backpressure(const std::string &id) const;
  std::size_t pending_for(const std::string &id) const;

  std::uint64_t next_seq() const;
  std::uint64_t highest_seq(const std::string &instance_id) const;
  void set_floor(const std::string &instance_id, std::uint64_t floor);

  std::vector<EventRecord> since(const std::string &instance_id,
                                 std::uint64_t cursor,
                                 std::size_t limit) const;

  std::size_t size() const;
  void clear();

private:
  bool matches(const Subscription &subscription,
               const EventRecord &event) const;
  std::size_t pending_count(const Subscription &subscription) const;

  std::size_t soft_limit_ = 100;
  std::size_t hard_limit_ = 1000;
  std::uint64_t next_seq_ = 1;
  std::size_t subscription_counter_ = 0;
  std::map<std::string, Subscription> subscriptions_;
  std::map<std::string, std::uint64_t> highest_;
  std::map<std::string, std::uint64_t> floors_;
  std::vector<EventRecord> events_;
};

} // namespace control
} // namespace gda
