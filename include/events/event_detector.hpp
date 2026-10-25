#pragma once

#include "events/system_event.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>

namespace nerv {

struct EventDetectorConfig {
    // Thresholds and Hysteresis
    float cpu_threshold_percent{75.0f};
    float cpu_hysteresis_percent{5.0f}; // Re-arms when CPU drops below (threshold - hysteresis)

    float process_cpu_spike_percent{50.0f};
    float process_cpu_hysteresis_percent{10.0f}; // Re-arms when process CPU drops below (threshold - hysteresis)

    float memory_threshold_percent{85.0f};
    float memory_hysteresis_percent{4.0f}; // Re-arms when RAM drops below (threshold - hysteresis)

    float disk_spike_mb_sec{35.0f};
    float disk_hysteresis_mb_sec{10.0f}; // Re-arms when Disk drops below (threshold - hysteresis)

    float net_spike_mb_sec{15.0f};
    float net_hysteresis_mb_sec{4.0f}; // Re-arms when Net drops below (threshold - hysteresis)

    // Cooldown timings
    double cooldown_seconds{4.0};
    double process_cooldown_seconds{8.0};

    // Feature toggles
    bool enable_spawn_events{true};
    bool enable_exit_events{true};
    bool enable_process_cpu_spikes{true};
    bool enable_visual_effects{true}; // Allows disabling animation independently of event detection
    size_t max_stored_events{150};
};

class EventDetector {
public:
    explicit EventDetector(EventDetectorConfig config = {});

    // Evaluates snapshot and returns any newly triggered events
    std::vector<SystemEvent> update(const TelemetrySnapshot& snapshot);

    const std::deque<SystemEvent>& get_event_log() const { return m_event_log; }
    void clear_events() { m_event_log.clear(); }
    void reset_state();

    EventDetectorConfig& get_config() { return m_config; }
    const EventDetectorConfig& get_config() const { return m_config; }

    // State inspection for tests and diagnostics
    bool is_cpu_in_high_state() const { return m_cpu_high_state; }
    bool is_memory_in_high_state() const { return m_memory_high_state; }
    bool is_disk_in_high_state() const { return m_disk_high_state; }
    bool is_net_in_high_state() const { return m_net_high_state; }

private:
    bool is_on_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) const;
    void set_cooldown(EventType type, const std::chrono::steady_clock::time_point& now);

    bool is_process_on_cooldown(const ProcessIdentity& id, const std::chrono::steady_clock::time_point& now) const;
    void set_process_cooldown(const ProcessIdentity& id, const std::chrono::steady_clock::time_point& now);

    EventDetectorConfig m_config;
    uint64_t m_next_event_id{1};
    std::deque<SystemEvent> m_event_log;

    // Rising-edge hysteresis state trackers (prevents repeated effects during sustained high utilization)
    bool m_cpu_high_state{false};
    bool m_memory_high_state{false};
    bool m_disk_high_state{false};
    bool m_net_high_state{false};
    std::unordered_map<ProcessIdentity, bool> m_process_cpu_high_state;

    // Cooldown tracking
    std::unordered_map<int, std::chrono::steady_clock::time_point> m_cooldowns;
    std::unordered_map<ProcessIdentity, std::chrono::steady_clock::time_point> m_process_cooldowns;

    // Track previously known processes to detect spawns and exits
    std::unordered_map<ProcessIdentity, std::string> m_known_processes;
    bool m_first_run{true};
};

} // namespace nerv
