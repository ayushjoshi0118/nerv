#pragma once

#ifdef _WIN32

#include "telemetry/i_system_monitor.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <unordered_map>
#include <chrono>

namespace nerv {

struct ProcessSampleHistory {
    uint64_t last_proc_time_100ns{0};
    uint64_t last_disk_read_bytes{0};
    uint64_t last_disk_write_bytes{0};
};

class WindowsSystemMonitor : public ISystemMonitor {
public:
    WindowsSystemMonitor();
    ~WindowsSystemMonitor() override;

    bool initialize() override;
    void shutdown() override;

    SystemMetrics sample_system_metrics() override;
    std::vector<ProcessMetrics> sample_process_metrics() override;

    BackendCapabilities get_capabilities() const override;
    uint32_t get_logical_processor_count() const override { return m_processor_count; }

private:
    uint32_t m_processor_count{1};

    // System CPU tracking
    uint64_t m_prev_sys_idle_time{0};
    uint64_t m_prev_sys_kernel_time{0};
    uint64_t m_prev_sys_user_time{0};
    std::chrono::steady_clock::time_point m_prev_sample_time;

    // Network tracking
    uint64_t m_prev_net_rx_octets{0};
    uint64_t m_prev_net_tx_octets{0};
    std::chrono::steady_clock::time_point m_prev_net_sample_time;

    // Per-process tracking for CPU delta calculations
    std::unordered_map<ProcessIdentity, ProcessSampleHistory> m_process_history;

    // Helper methods
    static uint64_t filetime_to_uint64(const FILETIME& ft);
    static uint64_t filetime_to_nanos(const FILETIME& ft);
    static std::string wide_to_utf8(const std::wstring& wstr);
    static ProcessCategory categorize_process(const std::string& name, const std::string& path);
};

} // namespace nerv

#endif // _WIN32
