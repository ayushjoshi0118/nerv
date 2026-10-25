#pragma once

#include "telemetry/process_metrics.hpp"
#include "rendering/math_types.hpp"
#include <string>
#include <chrono>

namespace nerv {

enum class EventSeverity : uint8_t {
    Info,
    Warning,
    Critical
};

enum class EventType : uint8_t {
    CpuSpike,
    ProcessCpuSpike,
    MemoryHigh,
    DiskSpike,
    NetworkSpike,
    ProcessSpawned,
    ProcessTerminated
};

struct SystemEvent {
    uint64_t id{0};
    EventType type{EventType::CpuSpike};
    EventSeverity severity{EventSeverity::Info};
    std::string title;
    std::string description;
    std::string badge_text; // Short formatted text for in-world shockwave badge
    float measured_value{0.0f};
    float threshold_value{0.0f};
    std::string unit;

    ProcessIdentity related_process;
    Vec2 world_position{0.0f, 0.0f}; // Position in graph world coordinates for shockwaves
    std::chrono::system_clock::time_point timestamp;

    static const char* event_type_to_string(EventType t) {
        switch (t) {
            case EventType::CpuSpike: return "System CPU Spike";
            case EventType::ProcessCpuSpike: return "Process CPU Spike";
            case EventType::MemoryHigh: return "Memory Pressure";
            case EventType::DiskSpike: return "Disk Activity Surge";
            case EventType::NetworkSpike: return "Network Bandwidth Surge";
            case EventType::ProcessSpawned: return "Process Spawned";
            case EventType::ProcessTerminated: return "Process Terminated";
        }
        return "Unknown Event";
    }

    static const char* severity_to_string(EventSeverity s) {
        switch (s) {
            case EventSeverity::Info: return "INFO";
            case EventSeverity::Warning: return "WARNING";
            case EventSeverity::Critical: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    static Vec4 severity_to_color(EventSeverity s) {
        switch (s) {
            case EventSeverity::Info: return colors::ElectricCyan;
            case EventSeverity::Warning: return colors::WarningAmber;
            case EventSeverity::Critical: return colors::CriticalRed;
        }
        return colors::TextWhite;
    }
};

} // namespace nerv
