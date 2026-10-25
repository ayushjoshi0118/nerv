#include "events/event_detector.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace nerv {

EventDetector::EventDetector(EventDetectorConfig config)
    : m_config(config) {}

void EventDetector::reset_state() {
    m_cpu_high_state = false;
    m_memory_high_state = false;
    m_disk_high_state = false;
    m_net_high_state = false;
    m_process_cpu_high_state.clear();
    m_cooldowns.clear();
    m_process_cooldowns.clear();
    m_known_processes.clear();
    m_first_run = true;
}

bool EventDetector::is_on_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) const {
    auto it = m_cooldowns.find(static_cast<int>(type));
    if (it == m_cooldowns.end()) return false;
    double elapsed = std::chrono::duration<double>(now - it->second).count();
    return elapsed < m_config.cooldown_seconds;
}

void EventDetector::set_cooldown(EventType type, const std::chrono::steady_clock::time_point& now) {
    m_cooldowns[static_cast<int>(type)] = now;
}

bool EventDetector::is_process_on_cooldown(const ProcessIdentity& id, const std::chrono::steady_clock::time_point& now) const {
    auto it = m_process_cooldowns.find(id);
    if (it == m_process_cooldowns.end()) return false;
    double elapsed = std::chrono::duration<double>(now - it->second).count();
    return elapsed < m_config.process_cooldown_seconds;
}

void EventDetector::set_process_cooldown(const ProcessIdentity& id, const std::chrono::steady_clock::time_point& now) {
    m_process_cooldowns[id] = now;
}

std::vector<SystemEvent> EventDetector::update(const TelemetrySnapshot& snapshot) {
    std::vector<SystemEvent> triggered;
    if (!snapshot.is_valid) return triggered;

    auto now_steady = std::chrono::steady_clock::now();
    auto now_sys = std::chrono::system_clock::now();

    // =========================================================================
    // 1. System CPU Threshold with Rising-Edge Hysteresis
    // =========================================================================
    if (snapshot.system.is_cpu_available) {
        float cpu = snapshot.system.total_cpu_percent;
        float reset_cpu_threshold = std::max(0.0f, m_config.cpu_threshold_percent - m_config.cpu_hysteresis_percent);

        if (cpu >= m_config.cpu_threshold_percent) {
            if (!m_cpu_high_state) {
                // Rising edge transition!
                if (!is_on_cooldown(EventType::CpuSpike, now_steady)) {
                    m_cpu_high_state = true;
                    set_cooldown(EventType::CpuSpike, now_steady);

                    SystemEvent ev;
                    ev.id = m_next_event_id++;
                    ev.type = EventType::CpuSpike;
                    ev.severity = (cpu >= 90.0f) ? EventSeverity::Critical : EventSeverity::Warning;
                    ev.title = (cpu >= 90.0f) ? "Critical CPU Spike" : "High CPU Utilization Spike";

                    std::stringstream ss;
                    ss << "System CPU crossed threshold: " << std::fixed << std::setprecision(1)
                       << cpu << "% (Threshold: " << m_config.cpu_threshold_percent << "%)";
                    ev.description = ss.str();

                    std::stringstream bss;
                    bss << "CPU: " << std::fixed << std::setprecision(1) << cpu << "%";
                    ev.badge_text = bss.str();

                    ev.measured_value = cpu;
                    ev.threshold_value = m_config.cpu_threshold_percent;
                    ev.unit = "%";
                    ev.world_position = {0.0f, 0.0f}; // System hub origin
                    ev.timestamp = now_sys;

                    triggered.push_back(ev);
                }
            }
            // If already m_cpu_high_state, prevent repeated effects during sustained high utilization
        } else if (cpu < reset_cpu_threshold) {
            // Drop below hysteresis bound re-arms rising-edge detector
            m_cpu_high_state = false;
        }
    }

    // =========================================================================
    // 2. System RAM Threshold with Rising-Edge Hysteresis
    // =========================================================================
    if (snapshot.system.is_ram_available) {
        float ram = snapshot.system.ram_usage_percent;
        float reset_ram_threshold = std::max(0.0f, m_config.memory_threshold_percent - m_config.memory_hysteresis_percent);

        if (ram >= m_config.memory_threshold_percent) {
            if (!m_memory_high_state) {
                if (!is_on_cooldown(EventType::MemoryHigh, now_steady)) {
                    m_memory_high_state = true;
                    set_cooldown(EventType::MemoryHigh, now_steady);

                    SystemEvent ev;
                    ev.id = m_next_event_id++;
                    ev.type = EventType::MemoryHigh;
                    ev.severity = (ram >= 92.0f) ? EventSeverity::Critical : EventSeverity::Warning;
                    ev.title = (ram >= 92.0f) ? "Critical Memory Exhaustion" : "Memory Pressure Alert";

                    double used_gb = static_cast<double>(snapshot.system.used_ram_bytes) / (1024.0 * 1024.0 * 1024.0);
                    double total_gb = static_cast<double>(snapshot.system.total_ram_bytes) / (1024.0 * 1024.0 * 1024.0);

                    std::stringstream ss;
                    ss << "Physical RAM usage high: " << std::fixed << std::setprecision(1) << ram << "% ("
                       << used_gb << " GB / " << total_gb << " GB)";
                    ev.description = ss.str();

                    std::stringstream bss;
                    bss << "RAM: " << std::fixed << std::setprecision(1) << ram << "%";
                    ev.badge_text = bss.str();

                    ev.measured_value = ram;
                    ev.threshold_value = m_config.memory_threshold_percent;
                    ev.unit = "%";
                    ev.world_position = {0.0f, 0.0f};
                    ev.timestamp = now_sys;

                    triggered.push_back(ev);
                }
            }
        } else if (ram < reset_ram_threshold) {
            m_memory_high_state = false;
        }
    }

    // =========================================================================
    // 3. Disk Activity Changes with Rising-Edge Hysteresis
    // =========================================================================
    float disk_mb = static_cast<float>(snapshot.system.total_disk_bytes_sec()) / (1024.0f * 1024.0f);
    float reset_disk_threshold = std::max(0.0f, m_config.disk_spike_mb_sec - m_config.disk_hysteresis_mb_sec);

    if (disk_mb >= m_config.disk_spike_mb_sec) {
        if (!m_disk_high_state) {
            if (!is_on_cooldown(EventType::DiskSpike, now_steady)) {
                m_disk_high_state = true;
                set_cooldown(EventType::DiskSpike, now_steady);

                SystemEvent ev;
                ev.id = m_next_event_id++;
                ev.type = EventType::DiskSpike;
                ev.severity = (disk_mb >= 100.0f) ? EventSeverity::Critical :
                              (disk_mb >= 50.0f)  ? EventSeverity::Warning : EventSeverity::Info;
                ev.title = "Heavy Disk I/O Activity";

                std::stringstream ss;
                ss << "Disk throughput surge: " << std::fixed << std::setprecision(1)
                   << disk_mb << " MB/s (Read: "
                   << (snapshot.system.disk_read_bytes_sec / (1024.0 * 1024.0)) << " MB/s, Write: "
                   << (snapshot.system.disk_write_bytes_sec / (1024.0 * 1024.0)) << " MB/s)";
                ev.description = ss.str();

                std::stringstream bss;
                bss << "DISK: " << std::fixed << std::setprecision(1) << disk_mb << " MB/s";
                ev.badge_text = bss.str();

                ev.measured_value = disk_mb;
                ev.threshold_value = m_config.disk_spike_mb_sec;
                ev.unit = "MB/s";
                ev.world_position = {0.0f, 0.0f};
                ev.timestamp = now_sys;

                triggered.push_back(ev);
            }
        }
    } else if (disk_mb < reset_disk_threshold) {
        m_disk_high_state = false;
    }

    // =========================================================================
    // 4. Network Throughput Changes with Rising-Edge Hysteresis
    // =========================================================================
    float net_mb = static_cast<float>(snapshot.system.total_network_bytes_sec()) / (1024.0f * 1024.0f);
    float reset_net_threshold = std::max(0.0f, m_config.net_spike_mb_sec - m_config.net_hysteresis_mb_sec);

    if (net_mb >= m_config.net_spike_mb_sec) {
        if (!m_net_high_state) {
            if (!is_on_cooldown(EventType::NetworkSpike, now_steady)) {
                m_net_high_state = true;
                set_cooldown(EventType::NetworkSpike, now_steady);

                SystemEvent ev;
                ev.id = m_next_event_id++;
                ev.type = EventType::NetworkSpike;
                ev.severity = (net_mb >= 40.0f) ? EventSeverity::Critical :
                              (net_mb >= 20.0f) ? EventSeverity::Warning : EventSeverity::Info;
                ev.title = "Network Bandwidth Surge";

                std::stringstream ss;
                ss << "Network traffic burst: " << std::fixed << std::setprecision(1)
                   << net_mb << " MB/s (RX: "
                   << (snapshot.system.network_rx_bytes_sec / (1024.0 * 1024.0)) << " MB/s, TX: "
                   << (snapshot.system.network_tx_bytes_sec / (1024.0 * 1024.0)) << " MB/s)";
                ev.description = ss.str();

                std::stringstream bss;
                bss << "NET: " << std::fixed << std::setprecision(1) << net_mb << " MB/s";
                ev.badge_text = bss.str();

                ev.measured_value = net_mb;
                ev.threshold_value = m_config.net_spike_mb_sec;
                ev.unit = "MB/s";
                ev.world_position = {0.0f, 0.0f};
                ev.timestamp = now_sys;

                triggered.push_back(ev);
            }
        }
    } else if (net_mb < reset_net_threshold) {
        m_net_high_state = false;
    }

    // =========================================================================
    // 5. Per-Process CPU Spikes with Hysteresis
    // =========================================================================
    if (m_config.enable_process_cpu_spikes) {
        float proc_thresh = m_config.process_cpu_spike_percent;
        float reset_proc_thresh = std::max(0.0f, proc_thresh - m_config.process_cpu_hysteresis_percent);

        for (const auto& proc : snapshot.processes) {
            if (!proc.is_accessible) continue;

            bool was_high = m_process_cpu_high_state[proc.identity];

            if (proc.cpu_percent >= proc_thresh) {
                if (!was_high) {
                    if (!is_process_on_cooldown(proc.identity, now_steady)) {
                        m_process_cpu_high_state[proc.identity] = true;
                        set_process_cooldown(proc.identity, now_steady);

                        SystemEvent ev;
                        ev.id = m_next_event_id++;
                        ev.type = EventType::ProcessCpuSpike;
                        ev.severity = (proc.cpu_percent >= 80.0f) ? EventSeverity::Critical : EventSeverity::Warning;
                        ev.title = "Process CPU Spike";

                        std::stringstream ss;
                        ss << proc.name << " (PID " << proc.identity.pid << ") CPU spiked to "
                           << std::fixed << std::setprecision(1) << proc.cpu_percent << "%";
                        ev.description = ss.str();

                        std::stringstream bss;
                        bss << proc.name << ": " << std::fixed << std::setprecision(1) << proc.cpu_percent << "%";
                        ev.badge_text = bss.str();

                        ev.measured_value = proc.cpu_percent;
                        ev.threshold_value = proc_thresh;
                        ev.unit = "%";
                        ev.related_process = proc.identity;
                        ev.timestamp = now_sys;

                        triggered.push_back(ev);
                    }
                }
            } else if (proc.cpu_percent < reset_proc_thresh) {
                m_process_cpu_high_state[proc.identity] = false;
            }
        }
    }

    // =========================================================================
    // 6. Process Lifecycle (Spawn & Exit)
    // =========================================================================
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

                    std::stringstream bss;
                    bss << "+ " << name << " [" << id.pid << "]";
                    ev.badge_text = bss.str();

                    ev.measured_value = static_cast<float>(id.pid);
                    ev.threshold_value = 0.0f;
                    ev.unit = "PID";
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

                    std::stringstream bss;
                    bss << "- " << name << " [" << id.pid << "]";
                    ev.badge_text = bss.str();

                    ev.measured_value = static_cast<float>(id.pid);
                    ev.threshold_value = 0.0f;
                    ev.unit = "PID";
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

    // Append to bounded event log (event detection is independent of visual animation)
    for (const auto& ev : triggered) {
        m_event_log.push_front(ev);
        while (m_event_log.size() > m_config.max_stored_events) {
            m_event_log.pop_back();
        }
    }

    return triggered;
}

} // namespace nerv
