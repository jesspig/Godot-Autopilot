#include "core/control/control_service.hpp"

namespace gda {
namespace control {

ControlService::ControlService()
    : invocation_(InvocationDeps{nullptr, nullptr, nullptr, nullptr, nullptr,
                                 &confirmation_, &catalog_, &jobs_, true,
                                 60000}) {}

ConfirmationService &ControlService::confirmation() { return confirmation_; }

const ConfirmationService &ControlService::confirmation() const {
  return confirmation_;
}

CatalogService &ControlService::catalog() { return catalog_; }

const CatalogService &ControlService::catalog() const { return catalog_; }

JobService &ControlService::jobs() { return jobs_; }

const JobService &ControlService::jobs() const { return jobs_; }

EventService &ControlService::events() { return events_; }

const EventService &ControlService::events() const { return events_; }

TraceService &ControlService::trace() { return trace_; }

const TraceService &ControlService::trace() const { return trace_; }

InvocationService &ControlService::invocation() { return invocation_; }

const InvocationService &ControlService::invocation() const {
  return invocation_;
}

} // namespace control
} // namespace gda
