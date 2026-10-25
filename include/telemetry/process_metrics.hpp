#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <functional>

namespace nerv {

struct ProcessIdentity {
    uint32_t pid{0};
    uint64_t start_time_ns{0}; // Start time in nanoseconds since epoch

    bool operator==(const ProcessIdentity& other) const {
        return pid == other.pid && start_time_ns == other.start_time_ns;
    }

    bool operator!=(const ProcessIdentity& other) const {
        return !(*this == other);
    }

    bool operator<(const ProcessIdentity& other) const {
        if (pid != other.pid) return pid < other.pid;
        return start_time_ns < other.start_time_ns;
    }
};

enum class ProcessState : uint8_t {
    Running,
    Terminated,
    Inaccessible
};

enum class ProcessCategory : uint8_t {
    SystemKernel,
    SystemService,
    UserInteractive,
    Development,
    BackgroundWorker
};

struct ProcessMetrics {
    ProcessIdentity identity;
    uint32_t parent_pid{0};
    std::string name;
    std::string exe_path;
    
    // Normalized CPU usage percentage (0.0 to 100.0%) across all cores
    float cpu_percent{0.0f};

    // Memory metrics
    uint64_t resident_memory_bytes{0}; // Working Set
    uint64_t virtual_memory_bytes{0};  // Commit size / Pagefile usage

    // I/O throughput
    uint64_t disk_read_bytes_sec{0};
    uint64_t disk_write_bytes_sec{0};

    uint32_t thread_count{0};
    ProcessState state{ProcessState::Running};
    ProcessCategory category{ProcessCategory::BackgroundWorker};

    // Metric availability flags
    bool is_accessible{true}; // False if OpenProcess failed due to permissions
    bool has_io_data{false};

    std::chrono::system_clock::time_point sample_timestamp;

    // Helper functions
    uint64_t total_disk_io_bytes_sec() const {
        return disk_read_bytes_sec + disk_write_bytes_sec;
    }

    static const char* state_to_string(ProcessState s) {
        switch (s) {
            case ProcessState::Running: return "Running";
            case ProcessState::Terminated: return "Terminated";
            case ProcessState::Inaccessible: return "Inaccessible";
        }
        return "Unknown";
    }

    static const char* category_to_string(ProcessCategory c) {
        switch (c) {
            case ProcessCategory::SystemKernel: return "Kernel / Core";
            case ProcessCategory::SystemService: return "System Service";
            case ProcessCategory::UserInteractive: return "Interactive App";
            case ProcessCategory::Development: return "Developer Tool";
            case ProcessCategory::BackgroundWorker: return "Background";
        }
        return "General";
    }
};

} // namespace nerv

namespace std {
template <>
struct hash<nerv::ProcessIdentity> {
    std::size_t operator()(const nerv::ProcessIdentity& id) const noexcept {
        return std::hash<uint32_t>()(id.pid) ^ (std::hash<uint64_t>()(id.start_time_ns) << 1);
    }
};
} // namespace std
