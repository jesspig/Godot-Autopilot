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

struct TaskKey {
  std::string instance_id;
  std::int64_t generation = 0;
  std::string task_id;
};

struct TaskRecord {
  TaskKey key;
  gcp::TaskState state = gcp::TaskState::pending;
  int progress = 0;
  std::string progress_message;
  std::string result;
  gcp::ErrorCode error = gcp::ErrorCode::none;
  std::string error_message;
  gcp::CancelStage cancel_stage = gcp::CancelStage::requested;
  std::int64_t created_at_ms = 0;
  std::int64_t updated_at_ms = 0;
  std::int64_t ttl_ms = 0;
};

class JobService {
public:
  explicit JobService(std::int64_t default_ttl_ms = 300000);

  std::string make_task_id();

  TaskRecord create(const std::string &instance_id, std::int64_t generation,
                    const std::string &task_id = std::string(),
                    std::int64_t ttl_ms = 0, std::int64_t now_ms = 0);

  bool exists(const TaskKey &key) const;
  std::optional<TaskRecord> get(const TaskKey &key) const;

  bool advance(const TaskKey &key, gcp::TaskState to, std::int64_t now_ms);
  bool set_progress(const TaskKey &key, int progress,
                    const std::string &message, std::int64_t now_ms);
  bool set_result(const TaskKey &key, const std::string &result,
                  std::int64_t now_ms);
  bool fail(const TaskKey &key, gcp::ErrorCode code,
            const std::string &message, std::int64_t now_ms);
  bool request_cancel(const TaskKey &key, std::int64_t now_ms);
  bool advance_cancel(const TaskKey &key, gcp::CancelStage stage,
                      std::int64_t now_ms);

  std::size_t expire(std::int64_t now_ms);
  std::vector<TaskRecord> list(const std::string &instance_id,
                               std::int64_t generation) const;

  void set_generation(const std::string &instance_id, std::int64_t generation);
  std::int64_t generation(const std::string &instance_id) const;

  std::size_t size() const;
  std::string snapshot() const;
  bool restore(const std::string &text);
  void clear();

private:
  TaskRecord *mutable_find(const TaskKey &key);
  const TaskRecord *const_find(const TaskKey &key) const;

  std::int64_t default_ttl_ms_ = 300000;
  std::size_t task_counter_ = 0;
  std::map<std::string, TaskRecord> tasks_;
  std::map<std::string, std::int64_t> generations_;
};

} // namespace control
} // namespace gda
