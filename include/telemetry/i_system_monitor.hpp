#pragma once

#include "telemetry/system_metrics.hpp"
#include "telemetry/process_metrics.hpp"
#include <vector>
#include <memory>

namespace nerv {

class ISystemMonitor {
public:
    virtual ~ISystemMonitor() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // Telemetry sampling (called periodically from background telemetry collector)
    virtual SystemMetrics sample_system_metrics() = 0;
    virtual std::vector<ProcessMetrics> sample_process_metrics() = 0;

    // System hardware / platform capabilities
    virtual BackendCapabilities get_capabilities() const = 0;
    virtual uint32_t get_logical_processor_count() const = 0;
};

} // namespace nerv
