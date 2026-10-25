#pragma once

#include "telemetry/i_system_monitor.hpp"
#include <vector>
#include <random>

namespace nerv {

class MockSystemMonitor : public ISystemMonitor {
public:
    MockSystemMonitor(uint32_t process_count = 35);
    ~MockSystemMonitor() override = default;

    bool initialize() override;
    void shutdown() override;

    SystemMetrics sample_system_metrics() override;
    std::vector<ProcessMetrics> sample_process_metrics() override;

    BackendCapabilities get_capabilities() const override;
    uint32_t get_logical_processor_count() const override { return 16; }

    // Mock control helpers for testing
    void set_cpu_spike(float cpu_pct) { m_forced_cpu = cpu_pct; }
    void trigger_process_spawn(uint32_t pid, const std::string& name);
    void trigger_process_exit(uint32_t pid);

private:
    uint32_t m_initial_count;
    float m_forced_cpu{-1.0f};
    uint64_t m_tick{0};
    std::vector<ProcessMetrics> m_mock_processes;
    std::mt19937 m_rng{1337};
};

} // namespace nerv
