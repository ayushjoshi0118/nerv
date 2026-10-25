#pragma once

#include "events/system_event.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>

namespace nerv {

struct EventDetectorConfig {
    float cpu_threshold_percent{75.0f};
    float memory_threshold_percent{85.0f};
    float disk_spike_mb_sec{40.0f};
    float net_spike_mb_sec{15.0f};
    double cooldown_seconds{4.0};
    bool enable_spawn_events{true};
    bool enable_exit_events{true};
    size_t max_stored_events{100};
};

class EventDetector {
public:
    explicit EventDetector(EventDetectorConfig config = {});

    // Evaluates snapshot and returns any newly triggered events
    std::vector<SystemEvent> update(const TelemetrySnapshot& snapshot);

    const std::deque<SystemEvent>& get_event_log() const { return m_event_log; }
    void clear_events() { m_event_log.clear(); }

    EventDetectorConfig& get_config() { return m_config; }
    const EventDetectorConfig& get_config() const { return m_config; }

private:
    bool is_on_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) const;
    void set_cooldown(EventType type, const std::chrono::steady_clock::time_point& now);

    EventDetectorConfig m_config;
    uint64_t m_next_event_id{1};
    std::deque<SystemEvent> m_event_log;

    // Cooldown tracking per event type
    std::unordered_map<int, std::chrono::steady_clock::time_point> m_cooldowns;

    // Track previously known processes to detect spawns and exits
    std::unordered_map<ProcessIdentity, std::string> m_known_processes;
    bool m_first_run{true};
};

} // namespace nerv
