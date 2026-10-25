#include "events/event_detector.hpp"
#include <sstream>
#include <iomanip>

namespace nerv {

EventDetector::EventDetector(EventDetectorConfig config)
    : m_config(config) {}

bool EventDetector::is_on_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) const {
    auto it = m_cooldowns.find(static_cast<int>(type));
    if (it == m_cooldowns.end()) return false;
    double elapsed = std::chrono::duration<double>(now - it->second).count();
    return elapsed < m_config.cooldown_seconds;
}

void EventDetector::set_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) {
    m_cooldowns[static_cast<int>(type)] = now;
}

std::vector<SystemEvent> EventDetector::update(const TelemetrySnapshot& snapshot) {
    std::vector<SystemEvent> triggered;
    if (!snapshot.is_valid) return triggered;

    auto now_steady = std::chrono::steady_clock::now();
    auto now_sys = std::chrono::system_clock::now();

    // 1. Check System CPU Threshold
    if (snapshot.system.is_cpu_available &&
        snapshot.system.total_cpu_percent >= m_config.cpu_threshold_percent) {
        if (!is_on_cooldown(EventType::CpuSpike, now_steady)) {
            SystemEvent ev;
            ev.id = m_next_event_id++;
            ev.type = EventType::CpuSpike;
            ev.severity = (snapshot.system.total_cpu_percent > 90.0f) ? EventSeverity::Critical : EventSeverity::Warning;
            ev.title = "High CPU Utilization Spike";
            std::stringstream ss;
            ss << "System CPU crossed threshold: " << std::fixed << std::setprecision(1)
               << snapshot.system.total_cpu_percent << "% >= " << m_config.cpu_threshold_percent << "%";
            ev.description = ss.str();
            ev.measured_value = snapshot.system.total_cpu_percent;
            ev.threshold_value = m_config.cpu_threshold_percent;
            ev.unit = "%";
            ev.world_position = {0.0f, 0.0f}; // System hub origin
            ev.timestamp = now_sys;

            triggered.push_back(ev);
            set_cooldown(EventType::CpuSpike, now_steady);
        }
    }

    // 2. Check System RAM Threshold
    if (snapshot.system.is_ram_available &&
        snapshot.system.ram_usage_percent >= m_config.memory_threshold_percent) {
        if (!is_on_cooldown(EventType::MemoryHigh, now_steady)) {
            SystemEvent ev;
            ev.id = m_next_event_id++;
            ev.type = EventType::MemoryHigh;
            ev.severity = (snapshot.system.ram_usage_percent > 92.0f) ? EventSeverity::Critical : EventSeverity::Warning;
            ev.title = "Memory Pressure Alert";
            std::stringstream ss;
            ss << "Physical RAM usage high: " << std::fixed << std::setprecision(1)
               << snapshot.system.ram_usage_percent << "% >= " << m_config.memory_threshold_percent << "%";
            ev.description = ss.str();
            ev.measured_value = snapshot.system.ram_usage_percent;
            ev.threshold_value = m_config.memory_threshold_percent;
            ev.unit = "%";
            ev.world_position = {0.0f, 0.0f};
            ev.timestamp = now_sys;

            triggered.push_back(ev);
            set_cooldown(EventType::MemoryHigh, now_steady);
        }
    }

    // 3. Check Disk Spike
    float disk_mb = static_cast<float>(snapshot.system.total_disk_bytes_sec()) / (1024.0f * 1024.0f);
    if (disk_mb >= m_config.disk_spike_mb_sec) {
        if (!is_on_cooldown(EventType::DiskSpike, now_steady)) {
            SystemEvent ev;
            ev.id = m_next_event_id++;
            ev.type = EventType::DiskSpike;
            ev.severity = EventSeverity::Info;
            ev.title = "Heavy Disk I/O Activity";
            std::stringstream ss;
            ss << "Disk throughput: " << std::fixed << std::setprecision(1)
               << disk_mb << " MB/s >= " << m_config.disk_spike_mb_sec << " MB/s";
            ev.description = ss.str();
            ev.measured_value = disk_mb;
            ev.threshold_value = m_config.disk_spike_mb_sec;
            ev.unit = "MB/s";
            ev.world_position = {0.0f, 0.0f};
            ev.timestamp = now_sys;

            triggered.push_back(ev);
            set_cooldown(EventType::DiskSpike, now_steady);
        }
    }

    // 4. Check Network Spike
    float net_mb = static_cast<float>(snapshot.system.total_network_bytes_sec()) / (1024.0f * 1024.0f);
    if (net_mb >= m_config.net_spike_mb_sec) {
        if (!is_on_cooldown(EventType::NetworkSpike, now_steady)) {
            SystemEvent ev;
            ev.id = m_next_event_id++;
            ev.type = EventType::NetworkSpike;
            ev.severity = EventSeverity::Info;
            ev.title = "Network Bandwidth Surge";
            std::stringstream ss;
            ss << "Network traffic: " << std::fixed << std::setprecision(1)
               << net_mb << " MB/s >= " << m_config.net_spike_mb_sec << " MB/s";
            ev.description = ss.str();
            ev.measured_value = net_mb;
            ev.threshold_value = m_config.net_spike_mb_sec;
            ev.unit = "MB/s";
            ev.world_position = {0.0f, 0.0f};
            ev.timestamp = now_sys;

            triggered.push_back(ev);
            set_cooldown(EventType::NetworkSpike, now_steady);
        }
    }

    // 5. Process Lifecycle (Spawn & Exit)
    std::unordered_map<ProcessIdentity, std::string> current_processes;
    for (const auto& proc : snapshot.processes) {
        current_processes[proc.identity] = proc.name;
    }

    if (!m_first_run) {
        // Detect spawned processes
        if (m_config.enable_spawn_events) {
            for (const auto& [id, name] : current_processes) {
                if (m_known_processes.find(id) == m_known_processes.end()) {
                    SystemEvent ev;
                    ev.id = m_next_event_id++;
                    ev.type = EventType::ProcessSpawned;
                    ev.severity = EventSeverity::Info;
                    ev.title = "Process Spawned";
                    ev.description = name + " (PID " + std::to_string(id.pid) + ") launched";
                    ev.related_process = id;
                    ev.timestamp = now_sys;
                    triggered.push_back(ev);
                }
            }
        }

        // Detect terminated processes
        if (m_config.enable_exit_events) {
            for (const auto& [id, name] : m_known_processes) {
                if (current_processes.find(id) == current_processes.end()) {
                    SystemEvent ev;
                    ev.id = m_next_event_id++;
                    ev.type = EventType::ProcessTerminated;
                    ev.severity = EventSeverity::Info;
                    ev.title = "Process Terminated";
                    ev.description = name + " (PID " + std::to_string(id.pid) + ") exited";
                    ev.related_process = id;
                    ev.timestamp = now_sys;
                    triggered.push_back(ev);
                }
            }
        }
    } else {
        m_first_run = false;
    }

    m_known_processes = std::move(current_processes);

    // Append to bounded event log
    for (const auto& ev : triggered) {
        m_event_log.push_front(ev);
        while (m_event_log.size() > m_config.max_stored_events) {
            m_event_log.pop_back();
        }
    }

    return triggered;
}

} // namespace nerv
