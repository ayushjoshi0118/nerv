#include "telemetry/mock_system_monitor.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>

namespace nerv {

MockSystemMonitor::MockSystemMonitor(uint32_t process_count)
    : m_initial_count(process_count) {}

bool MockSystemMonitor::initialize() {
    m_mock_processes.clear();
    m_tick = 0;

    // Fixed root processes
    ProcessMetrics kernel;
    kernel.identity = {4, 1000000000ULL};
    kernel.parent_pid = 0;
    kernel.name = "System";
    kernel.exe_path = "ntoskrnl.exe";
    kernel.cpu_percent = 2.5f;
    kernel.resident_memory_bytes = 256 * 1024 * 1024;
    kernel.virtual_memory_bytes = 512 * 1024 * 1024;
    kernel.thread_count = 280;
    kernel.state = ProcessState::Running;
    kernel.category = ProcessCategory::SystemKernel;
    kernel.is_accessible = true;
    m_mock_processes.push_back(kernel);

    const std::vector<std::pair<std::string, ProcessCategory>> templates = {
        {"svchost.exe", ProcessCategory::SystemService},
        {"explorer.exe", ProcessCategory::UserInteractive},
        {"chrome.exe", ProcessCategory::UserInteractive},
        {"code.exe", ProcessCategory::Development},
        {"discord.exe", ProcessCategory::UserInteractive},
        {"spotify.exe", ProcessCategory::UserInteractive},
        {"git.exe", ProcessCategory::Development},
        {"devenv.exe", ProcessCategory::Development},
        {"terminal.exe", ProcessCategory::Development},
        {"dwm.exe", ProcessCategory::SystemService},
        {"audiodg.exe", ProcessCategory::SystemService},
        {"taskhostw.exe", ProcessCategory::SystemService},
        {"SearchHost.exe", ProcessCategory::BackgroundWorker},
        {"RuntimeBroker.exe", ProcessCategory::SystemService},
        {"nerv.exe", ProcessCategory::Development}
    };

    uint32_t pid_counter = 1000;
    for (uint32_t i = 1; i < m_initial_count; ++i) {
        ProcessMetrics p;
        p.identity = {pid_counter++, 1000000000ULL + i * 1000000ULL};
        
        // Assign some hierarchy: attach to System or explorer or chrome
        if (i < 5) {
            p.parent_pid = 4; // child of System
        } else if (i < 15) {
            p.parent_pid = 1001; // child of svchost/explorer
        } else {
            p.parent_pid = 1002;
        }

        const auto& t = templates[i % templates.size()];
        p.name = t.first;
        p.category = t.second;
        p.exe_path = "C:\\Windows\\System32\\" + p.name;
        p.cpu_percent = (i % 7 == 0) ? (12.0f + (i % 5) * 3.5f) : ((i % 3) * 0.8f);
        p.resident_memory_bytes = (50ULL + (i * 23ULL) % 800) * 1024 * 1024;
        p.virtual_memory_bytes = p.resident_memory_bytes * 2;
        p.thread_count = 4 + (i % 24);
        p.state = ProcessState::Running;
        p.is_accessible = true;
        m_mock_processes.push_back(p);
    }

    return true;
}

void MockSystemMonitor::shutdown() {
    m_mock_processes.clear();
}

SystemMetrics MockSystemMonitor::sample_system_metrics() {
    SystemMetrics sys;
    m_tick++;

    float base_cpu = (m_forced_cpu >= 0.0f) ? m_forced_cpu : (20.0f + 15.0f * std::sin(m_tick * 0.2f));
    sys.total_cpu_percent = std::clamp(base_cpu, 0.0f, 100.0f);

    sys.per_core_cpu_percent.resize(16);
    for (size_t c = 0; c < 16; ++c) {
        float offset = std::sin(m_tick * 0.15f + c * 0.4f) * 10.0f;
        sys.per_core_cpu_percent[c] = std::clamp(sys.total_cpu_percent + offset, 0.0f, 100.0f);
    }

    sys.total_ram_bytes = 32ULL * 1024 * 1024 * 1024;
    sys.used_ram_bytes = 14ULL * 1024 * 1024 * 1024 + (uint64_t)(std::sin(m_tick * 0.05f) * 1024 * 1024 * 1024);
    sys.available_ram_bytes = sys.total_ram_bytes - sys.used_ram_bytes;
    sys.ram_usage_percent = (static_cast<float>(sys.used_ram_bytes) / sys.total_ram_bytes) * 100.0f;

    sys.total_pagefile_bytes = 40ULL * 1024 * 1024 * 1024;
    sys.used_pagefile_bytes = 18ULL * 1024 * 1024 * 1024;
    sys.available_pagefile_bytes = sys.total_pagefile_bytes - sys.used_pagefile_bytes;
    sys.pagefile_usage_percent = (static_cast<float>(sys.used_pagefile_bytes) / sys.total_pagefile_bytes) * 100.0f;

    sys.disk_read_bytes_sec = (uint64_t)(std::max(0.0f, std::sin(m_tick * 0.3f)) * 45 * 1024 * 1024);
    sys.disk_write_bytes_sec = (uint64_t)(std::max(0.0f, std::cos(m_tick * 0.25f)) * 20 * 1024 * 1024);

    sys.network_rx_bytes_sec = (uint64_t)(std::max(0.0f, std::sin(m_tick * 0.4f)) * 12 * 1024 * 1024);
    sys.network_tx_bytes_sec = (uint64_t)(std::max(0.0f, std::cos(m_tick * 0.4f)) * 3 * 1024 * 1024);

    sys.process_count = static_cast<uint32_t>(m_mock_processes.size());
    sys.thread_count = sys.process_count * 18;
    sys.handle_count = sys.process_count * 450;
    sys.uptime_seconds = 18450.0 + m_tick;
    sys.sample_timestamp = std::chrono::system_clock::now();
    sys.collection_duration_ms = 1.25;

    sys.is_cpu_available = true;
    sys.is_ram_available = true;
    sys.is_disk_available = true;
    sys.is_network_available = true;
    sys.is_per_core_available = true;

    return sys;
}

std::vector<ProcessMetrics> MockSystemMonitor::sample_process_metrics() {
    auto now = std::chrono::system_clock::now();
    for (size_t i = 1; i < m_mock_processes.size(); ++i) {
        auto& p = m_mock_processes[i];
        p.sample_timestamp = now;
        // Mild variation in CPU
        if (m_forced_cpu >= 0.0f && i == 3) {
            p.cpu_percent = m_forced_cpu;
        } else {
            float wave = std::sin(m_tick * 0.1f + i * 0.7f);
            p.cpu_percent = std::clamp(p.cpu_percent + wave * 0.5f, 0.0f, 95.0f);
        }
    }
    return m_mock_processes;
}

BackendCapabilities MockSystemMonitor::get_capabilities() const {
    BackendCapabilities caps;
    caps.supports_per_core_cpu = true;
    caps.supports_system_disk_io = true;
    caps.supports_system_network_io = true;
    caps.supports_process_disk_io = true;
    caps.supports_process_parent_pid = true;
    caps.supports_process_thread_count = true;
    caps.supports_process_start_time = true;
    caps.supports_process_executable_path = true;
    caps.platform_name = "Mock / Synthetic Platform";
    return caps;
}

void MockSystemMonitor::trigger_process_spawn(uint32_t pid, const std::string& name) {
    ProcessMetrics p;
    p.identity = {pid, static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())};
    p.parent_pid = 4;
    p.name = name;
    p.exe_path = "C:\\Windows\\System32\\" + name;
    p.cpu_percent = 5.0f;
    p.resident_memory_bytes = 120 * 1024 * 1024;
    p.virtual_memory_bytes = 240 * 1024 * 1024;
    p.thread_count = 12;
    p.state = ProcessState::Running;
    p.category = ProcessCategory::UserInteractive;
    p.is_accessible = true;
    m_mock_processes.push_back(p);
}

void MockSystemMonitor::trigger_process_exit(uint32_t pid) {
    auto it = std::find_if(m_mock_processes.begin(), m_mock_processes.end(),
                           [pid](const ProcessMetrics& p) { return p.identity.pid == pid; });
    if (it != m_mock_processes.end()) {
        m_mock_processes.erase(it);
    }
}

} // namespace nerv
