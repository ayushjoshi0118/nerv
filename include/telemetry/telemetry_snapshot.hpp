#pragma once

#include "telemetry/system_metrics.hpp"
#include "telemetry/process_metrics.hpp"
#include <vector>
#include <memory>
#include <chrono>

namespace nerv {

struct TelemetrySnapshot {
    SystemMetrics system;
    std::vector<ProcessMetrics> processes;
    std::chrono::system_clock::time_point timestamp;
    double collection_duration_ms{0.0};
    uint64_t sequence_number{0};
    bool is_valid{false};

    double age_seconds() const {
        if (!is_valid) return 999999.0;
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration<double>(now - timestamp).count();
    }

    bool is_stale(double max_age_sec = 4.0) const {
        return age_seconds() > max_age_sec;
    }
};

using TelemetrySnapshotPtr = std::shared_ptr<const TelemetrySnapshot>;

} // namespace nerv
