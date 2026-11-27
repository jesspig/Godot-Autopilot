#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/control/catalog_service.hpp"
#include "core/control/confirmation_service.hpp"
#include "core/control/control_types.hpp"
#include "core/control/job_service.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

class Clock {
public:
  virtual ~Clock() = default;
  virtual std::int64_t now_ms() const = 0;
};

class FixedClock : public Clock {
public:
  explicit FixedClock(std::int64_t now_ms = 0);
  void set(std::int64_t now_ms);
  std::int64_t now_ms() const override;

private:
  std::int64_t now_ms_ = 0;
};

class ExportProbe {
public:
  virtual ~ExportProbe() = default;
  virtual bool exporting() const = 0;
};

class StaticExportProbe : public ExportProbe {
public:
  void set(bool exporting);
  bool exporting() const override;

private:
  bool exporting_ = false;
};

class RiskProbe {
public:
  virtual ~RiskProbe() = default;
  virtual bool high_risk(const std::string &tool) const = 0;
};

class StaticRiskProbe : public RiskProbe {
public:
  void add(const std::string &tool);
  bool high_risk(const std::string &tool) const override;

private:
  std::vector<std::string> tools_;
};

struct InvocationRequest {
  std::string request_id;
  std::string instance_id;
  std::int64_t generation = 0;
  std::string tool;
  gcp::Params params;
  std::string traceparent;
  std::int64_t deadline_ms = 30000;
  bool deferred = false;
};

struct InvocationResult {
  gcp::Status status = gcp::Status::ok;
  gcp::ErrorCode error = gcp::ErrorCode::none;
  std::string error_message;
  std::string payload;
  Artifact artifact;
  std::string task_id;
  gcp::Challenge confirmation;
};

class InvocationSink {
public:
  virtual ~InvocationSink() = default;
  virtual InvocationResult dispatch(const InvocationRequest &request) = 0;
};

class DeferredWaiter {
public:
  virtual ~DeferredWaiter() = default;
  virtual bool wait(const std::string &task_id, std::int64_t deadline_ms,
                    InvocationResult &out) = 0;
};

struct InvocationDeps {
  Clock *clock = nullptr;
  ExportProbe *export_probe = nullptr;
  RiskProbe *risk_probe = nullptr;
  InvocationSink *sink = nullptr;
  DeferredWaiter *waiter = nullptr;
  ConfirmationService *confirmation = nullptr;
  CatalogService *catalog = nullptr;
  JobService *jobs = nullptr;
  bool desensitize = true;
  std::int64_t confirmation_ttl_ms = 60000;
};

class InvocationService {
public:
  explicit InvocationService(InvocationDeps deps = InvocationDeps());

  InvocationResult invoke(const InvocationRequest &request);
  InvocationResult confirm(const std::string &request_id,
                           const InvocationRequest &original,
                           const gcp::Params &params);
  InvocationResult cancel(const std::string &instance_id,
                          std::int64_t generation, const std::string &task_id);

  void set_clock(Clock *clock);
  void set_export_probe(ExportProbe *probe);
  void set_risk_probe(RiskProbe *probe);
  void set_sink(InvocationSink *sink);
  void set_waiter(DeferredWaiter *waiter);
  void set_desensitize(bool enabled);

  std::int64_t record_error(const std::string &request_id);
  std::int64_t consume_new_errors(const std::string &request_id);
  void clear_errors();

private:
  std::int64_t now_ms() const;
  std::string resolve_tool(const std::string &tool) const;
  InvocationResult reject(const std::string &request_id, gcp::ErrorCode code,
                          const std::string &message);
  InvocationResult dispatch_normalized(const InvocationRequest &request);
  void finalize(InvocationResult &result, const std::string &request_id);

  InvocationDeps deps_;
  std::map<std::string, std::int64_t> errors_;
};

} // namespace control
} // namespace gda
