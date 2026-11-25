#pragma once

#include "core/control/catalog_service.hpp"
#include "core/control/confirmation_service.hpp"
#include "core/control/event_service.hpp"
#include "core/control/invocation_service.hpp"
#include "core/control/job_service.hpp"
#include "core/control/trace_service.hpp"

namespace gda {
namespace control {

class ControlService {
public:
  ControlService();

  ConfirmationService &confirmation();
  const ConfirmationService &confirmation() const;
  CatalogService &catalog();
  const CatalogService &catalog() const;
  JobService &jobs();
  const JobService &jobs() const;
  EventService &events();
  const EventService &events() const;
  TraceService &trace();
  const TraceService &trace() const;
  InvocationService &invocation();
  const InvocationService &invocation() const;

private:
  ConfirmationService confirmation_;
  CatalogService catalog_;
  JobService jobs_;
  EventService events_;
  TraceService trace_;
  InvocationService invocation_;
};

} // namespace control
} // namespace gda
