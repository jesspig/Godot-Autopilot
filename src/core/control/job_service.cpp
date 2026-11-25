#include "core/control/job_service.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace {

std::string job_flatten(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == '\t' || c == '\n' || c == '\r' || c == '|') {
      out.push_back(' ');
      continue;
    }
    out.push_back(c);
  }
  return out;
}

std::vector<std::string> job_split_fields(const std::string &line) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= line.size()) {
    const std::size_t end = line.find('\t', start);
    if (end == std::string::npos) {
      out.push_back(line.substr(start));
      break;
    }
    out.push_back(line.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

} // namespace

JobService::JobService(std::int64_t default_ttl_ms)
    : default_ttl_ms_(default_ttl_ms) {}

std::string JobService::make_task_id() {
  ++task_counter_;
  return "task-" + std::to_string(task_counter_);
}

TaskRecord *JobService::mutable_find(const TaskKey &key) {
  const std::map<std::string, TaskRecord>::iterator found =
      tasks_.find(key.task_id);
  if (found == tasks_.end()) {
    return nullptr;
  }
  if (found->second.key.instance_id != key.instance_id ||
      found->second.key.generation != key.generation) {
    return nullptr;
  }
  return &found->second;
}

const TaskRecord *JobService::const_find(const TaskKey &key) const {
  const std::map<std::string, TaskRecord>::const_iterator found =
      tasks_.find(key.task_id);
  if (found == tasks_.end()) {
    return nullptr;
  }
  if (found->second.key.instance_id != key.instance_id ||
      found->second.key.generation != key.generation) {
    return nullptr;
  }
  return &found->second;
}

TaskRecord JobService::create(const std::string &instance_id,
                              std::int64_t generation,
                              const std::string &task_id, std::int64_t ttl_ms,
                              std::int64_t now_ms) {
  TaskRecord record;
  record.key.instance_id = instance_id;
  record.key.generation = generation;
  record.key.task_id = task_id.empty() ? make_task_id() : task_id;
  record.state = gcp::TaskState::pending;
  record.cancel_stage = gcp::CancelStage::requested;
  record.created_at_ms = now_ms;
  record.updated_at_ms = now_ms;
  record.ttl_ms = ttl_ms > 0 ? ttl_ms : default_ttl_ms_;
  tasks_[record.key.task_id] = record;
  return record;
}

bool JobService::exists(const TaskKey &key) const {
  return const_find(key) != nullptr;
}

std::optional<TaskRecord> JobService::get(const TaskKey &key) const {
  const TaskRecord *found = const_find(key);
  if (found == nullptr) {
    return std::nullopt;
  }
  return *found;
}

bool JobService::advance(const TaskKey &key, gcp::TaskState to,
                         std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || !gcp::can_transition(record->state, to)) {
    return false;
  }
  record->state = to;
  record->updated_at_ms = now_ms;
  return true;
}

bool JobService::set_progress(const TaskKey &key, int progress,
                              const std::string &message,
                              std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || gcp::is_terminal_task(record->state)) {
    return false;
  }
  record->progress = std::max(0, std::min(100, progress));
  record->progress_message = message;
  record->updated_at_ms = now_ms;
  return true;
}

bool JobService::set_result(const TaskKey &key, const std::string &result,
                            std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || gcp::is_terminal_task(record->state)) {
    return false;
  }
  if (record->state == gcp::TaskState::pending ||
      record->state == gcp::TaskState::blocked) {
    record->state = gcp::TaskState::running;
  }
  if (!gcp::can_transition(record->state, gcp::TaskState::succeeded)) {
    return false;
  }
  record->state = gcp::TaskState::succeeded;
  record->result = result;
  record->progress = 100;
  record->updated_at_ms = now_ms;
  return true;
}

bool JobService::fail(const TaskKey &key, gcp::ErrorCode code,
                      const std::string &message, std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || !gcp::can_transition(record->state,
                                                gcp::TaskState::failed)) {
    return false;
  }
  record->state = gcp::TaskState::failed;
  record->error = code;
  record->error_message = message;
  record->updated_at_ms = now_ms;
  return true;
}

bool JobService::request_cancel(const TaskKey &key, std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || gcp::is_terminal_task(record->state)) {
    return false;
  }
  if (record->state == gcp::TaskState::cancelling) {
    return true;
  }
  if (!gcp::can_transition(record->state, gcp::TaskState::cancelling)) {
    return false;
  }
  record->state = gcp::TaskState::cancelling;
  record->cancel_stage = gcp::CancelStage::requested;
  record->updated_at_ms = now_ms;
  return true;
}

bool JobService::advance_cancel(const TaskKey &key, gcp::CancelStage stage,
                                std::int64_t now_ms) {
  TaskRecord *record = mutable_find(key);
  if (record == nullptr || record->state != gcp::TaskState::cancelling) {
    return false;
  }
  if (!gcp::can_advance(record->cancel_stage, stage)) {
    return false;
  }
  record->cancel_stage = stage;
  record->updated_at_ms = now_ms;
  if (stage == gcp::CancelStage::completed) {
    record->state = gcp::TaskState::cancelled;
  }
  return true;
}

std::size_t JobService::expire(std::int64_t now_ms) {
  std::size_t removed = 0;
  for (std::map<std::string, TaskRecord>::iterator it = tasks_.begin();
       it != tasks_.end();) {
    const TaskRecord &record = it->second;
    if (record.ttl_ms > 0 && gcp::is_terminal_task(record.state) &&
        record.updated_at_ms + record.ttl_ms <= now_ms) {
      it = tasks_.erase(it);
      ++removed;
      continue;
    }
    ++it;
  }
  return removed;
}

std::vector<TaskRecord> JobService::list(const std::string &instance_id,
                                         std::int64_t generation) const {
  std::vector<TaskRecord> out;
  for (const std::pair<const std::string, TaskRecord> &item : tasks_) {
    if (item.second.key.instance_id != instance_id ||
        item.second.key.generation != generation) {
      continue;
    }
    out.push_back(item.second);
  }
  return out;
}

void JobService::set_generation(const std::string &instance_id,
                                std::int64_t generation) {
  generations_[instance_id] = generation;
}

std::int64_t JobService::generation(const std::string &instance_id) const {
  const std::map<std::string, std::int64_t>::const_iterator found =
      generations_.find(instance_id);
  return found == generations_.end() ? 0 : found->second;
}

std::size_t JobService::size() const { return tasks_.size(); }

std::string JobService::snapshot() const {
  std::string out;
  for (const std::pair<const std::string, TaskRecord> &item : tasks_) {
    const TaskRecord &record = item.second;
    std::ostringstream line;
    line << job_flatten(record.key.instance_id) << '\t' << record.key.generation
         << '\t' << job_flatten(record.key.task_id) << '\t'
         << static_cast<int>(record.state) << '\t' << record.progress << '\t'
         << job_flatten(record.progress_message) << '\t' << job_flatten(record.result)
         << '\t' << static_cast<int>(record.error) << '\t'
         << job_flatten(record.error_message) << '\t'
         << static_cast<int>(record.cancel_stage) << '\t'
         << record.created_at_ms << '\t' << record.updated_at_ms << '\t'
         << record.ttl_ms;
    out += line.str();
    out.push_back('\n');
  }
  return out;
}

bool JobService::restore(const std::string &text) {
  std::map<std::string, TaskRecord> restored;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = text.find('\n', start);
    const std::string line =
        end == std::string::npos ? text.substr(start)
                                 : text.substr(start, end - start);
    const std::vector<std::string> fields = job_split_fields(line);
    if (fields.size() != 13u) {
      return false;
    }
    TaskRecord record;
    record.key.instance_id = fields[0];
    record.key.generation = std::stoll(fields[1]);
    record.key.task_id = fields[2];
    record.state = static_cast<gcp::TaskState>(std::stoi(fields[3]));
    record.progress = std::stoi(fields[4]);
    record.progress_message = fields[5];
    record.result = fields[6];
    record.error = static_cast<gcp::ErrorCode>(std::stoi(fields[7]));
    record.error_message = fields[8];
    record.cancel_stage = static_cast<gcp::CancelStage>(std::stoi(fields[9]));
    record.created_at_ms = std::stoll(fields[10]);
    record.updated_at_ms = std::stoll(fields[11]);
    record.ttl_ms = std::stoll(fields[12]);
    restored[record.key.task_id] = record;
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  tasks_ = restored;
  return true;
}

void JobService::clear() {
  tasks_.clear();
  generations_.clear();
}

} // namespace control
} // namespace gda
