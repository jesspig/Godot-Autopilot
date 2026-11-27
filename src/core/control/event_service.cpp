#include "core/control/event_service.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

EventService::EventService(std::size_t soft_limit, std::size_t hard_limit)
    : soft_limit_(soft_limit), hard_limit_(hard_limit) {}

bool EventService::matches(const Subscription &subscription,
                           const EventRecord &event) const {
  if (!subscription.active ||
      subscription.instance_id != event.instance_id) {
    return false;
  }
  return subscription.event_type.empty() ||
         subscription.event_type == event.event_type;
}

std::size_t EventService::pending_count(
    const Subscription &subscription) const {
  std::size_t count = 0;
  for (const EventRecord &event : events_) {
    if (event.seq > subscription.cursor && matches(subscription, event)) {
      ++count;
    }
  }
  return count;
}

EventRecord EventService::publish(
    const std::string &instance_id, const std::string &event_type,
    const std::string &payload, const std::string &traceparent,
    const std::optional<std::string> &task_id,
    const std::optional<std::string> &request_id) {
  EventRecord event;
  event.seq = next_seq_++;
  event.instance_id = instance_id;
  event.event_type = event_type;
  event.payload = payload;
  event.traceparent = traceparent;
  event.task_id = task_id;
  event.request_id = request_id;
  events_.push_back(event);
  const std::map<std::string, std::uint64_t>::iterator found =
      highest_.find(instance_id);
  if (found == highest_.end() || found->second < event.seq) {
    highest_[instance_id] = event.seq;
  }
  return event;
}

std::string EventService::subscribe(const std::string &instance_id,
                                    const std::string &event_type,
                                    std::uint64_t cursor) {
  if (!cursor_valid(instance_id, cursor)) {
    return std::string();
  }
  ++subscription_counter_;
  const std::string id = "sub-" + std::to_string(subscription_counter_);
  Subscription subscription;
  subscription.id = id;
  subscription.instance_id = instance_id;
  subscription.event_type = event_type;
  subscription.cursor = cursor;
  subscriptions_[id] = subscription;
  return id;
}

bool EventService::unsubscribe(const std::string &id) {
  const std::map<std::string, Subscription>::iterator found =
      subscriptions_.find(id);
  if (found == subscriptions_.end()) {
    return false;
  }
  subscriptions_.erase(found);
  return true;
}

std::vector<EventRecord> EventService::poll(const std::string &id,
                                            std::size_t limit,
                                            gcp::Backpressure *pressure) {
  std::vector<EventRecord> out;
  const std::map<std::string, Subscription>::iterator found =
      subscriptions_.find(id);
  if (found == subscriptions_.end()) {
    if (pressure != nullptr) {
      *pressure = gcp::Backpressure::none;
    }
    return out;
  }
  Subscription &subscription = found->second;
  const gcp::Backpressure level = backpressure(id);
  if (pressure != nullptr) {
    *pressure = level;
  }
  if (level == gcp::Backpressure::hard) {
    return out;
  }
  for (const EventRecord &event : events_) {
    if (limit > 0 && out.size() >= limit) {
      break;
    }
    if (event.seq > subscription.cursor && matches(subscription, event)) {
      out.push_back(event);
    }
  }
  if (!out.empty()) {
    subscription.cursor = out.back().seq;
    subscription.delivered += out.size();
  }
  return out;
}

bool EventService::cursor_valid(const std::string &instance_id,
                                std::uint64_t cursor) const {
  if (instance_id.empty() || cursor > highest_seq(instance_id)) {
    return false;
  }
  const std::map<std::string, std::uint64_t>::const_iterator floor =
      floors_.find(instance_id);
  return floor == floors_.end() || cursor >= floor->second;
}

gcp::Backpressure EventService::backpressure(const std::string &id) const {
  const std::map<std::string, Subscription>::const_iterator found =
      subscriptions_.find(id);
  if (found == subscriptions_.end()) {
    return gcp::Backpressure::none;
  }
  return gcp::classify_backpressure(pending_count(found->second), soft_limit_,
                                    hard_limit_);
}

std::size_t EventService::pending_for(const std::string &id) const {
  const std::map<std::string, Subscription>::const_iterator found =
      subscriptions_.find(id);
  return found == subscriptions_.end() ? 0 : pending_count(found->second);
}

std::uint64_t EventService::next_seq() const { return next_seq_; }

std::uint64_t EventService::highest_seq(const std::string &instance_id) const {
  const std::map<std::string, std::uint64_t>::const_iterator found =
      highest_.find(instance_id);
  return found == highest_.end() ? 0 : found->second;
}

void EventService::set_floor(const std::string &instance_id,
                             std::uint64_t floor) {
  floors_[instance_id] = floor;
}

std::vector<EventRecord> EventService::since(const std::string &instance_id,
                                             std::uint64_t cursor,
                                             std::size_t limit) const {
  std::vector<EventRecord> out;
  if (!cursor_valid(instance_id, cursor)) {
    return out;
  }
  for (const EventRecord &event : events_) {
    if (limit > 0 && out.size() >= limit) {
      break;
    }
    if (event.instance_id == instance_id && event.seq > cursor) {
      out.push_back(event);
    }
  }
  return out;
}

std::size_t EventService::size() const { return events_.size(); }

void EventService::clear() {
  events_.clear();
  subscriptions_.clear();
  highest_.clear();
  floors_.clear();
  next_seq_ = 1;
  subscription_counter_ = 0;
}

} // namespace control
} // namespace gda
