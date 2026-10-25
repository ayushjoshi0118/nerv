#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <string>

namespace nerv {

struct SystemMetrics {
    // Overall CPU utilization (0.0 to 100.0%)
    float total_cpu_percent{0.0f};
    
    // Per-core CPU utilization (0.0 to 100.0% each)
    std::vector<float> per_core_cpu_percent;

    // Physical Memory (RAM)
    uint64_t total_ram_bytes{0};
    uint64_t available_ram_bytes{0};
    uint64_t used_ram_bytes{0};
    float ram_usage_percent{0.0f};

    // Commit / Pagefile Memory
    uint64_t total_pagefile_bytes{0};
    uint64_t available_pagefile_bytes{0};
    uint64_t used_pagefile_bytes{0};
    float pagefile_usage_percent{0.0f};

    // System-wide Disk Throughput (Bytes per second)
    uint64_t disk_read_bytes_sec{0};
    uint64_t disk_write_bytes_sec{0};

    // System-wide Network Throughput (Bytes per second)
    uint64_t network_rx_bytes_sec{0};
    uint64_t network_tx_bytes_sec{0};

    // Global counts
    uint32_t process_count{0};
    uint32_t thread_count{0};
    uint32_t handle_count{0};

    // System uptime in seconds
    double uptime_seconds{0.0};

    // Measurement metadata
    std::chrono::system_clock::time_point sample_timestamp;
    double collection_duration_ms{0.0};

    // Metric availability status
    bool is_cpu_available{true};
    bool is_ram_available{true};
    bool is_disk_available{true};
    bool is_network_available{true};
    bool is_per_core_available{false};

    uint64_t total_disk_bytes_sec() const {
        return disk_read_bytes_sec + disk_write_bytes_sec;
    }

    uint64_t total_network_bytes_sec() const {
        return network_rx_bytes_sec + network_tx_bytes_sec;
    }
};

struct BackendCapabilities {
    bool supports_per_core_cpu{false};
    bool supports_system_disk_io{true};
    bool supports_system_network_io{true};
    bool supports_process_disk_io{true};
    bool supports_process_parent_pid{true};
    bool supports_process_thread_count{true};
    bool supports_process_start_time{true};
    bool supports_process_executable_path{true};
    std::string platform_name{"Windows"};
};

} // namespace nerv
