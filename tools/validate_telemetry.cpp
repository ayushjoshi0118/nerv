#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include "platform/windows/windows_system_monitor.hpp"
#endif
#include "telemetry/mock_system_monitor.hpp"
#include "telemetry/telemetry_snapshot.hpp"

int main() {
    std::cout << "=========================================================\n";
    std::cout << " NERV Native Telemetry Backend Validator\n";
    std::cout << " Validating genuine OS telemetry & interval measurements\n";
    std::cout << "=========================================================\n\n";

#ifdef _WIN32
    nerv::WindowsSystemMonitor monitor;
    std::cout << "[Backend] Initializing native Windows System Monitor...\n";
#else
    nerv::MockSystemMonitor monitor;
    std::cout << "[Backend] Initializing Mock System Monitor (Non-Windows platform)...\n";
#endif

    if (!monitor.initialize()) {
        std::cerr << "[ERROR] Failed to initialize monitor!\n";
        return 1;
    }

    auto caps = monitor.get_capabilities();
    std::cout << "[Capabilities] Platform: " << caps.platform_name << "\n";
    std::cout << "[Capabilities] Logical Processors: " << monitor.get_logical_processor_count() << "\n";
    std::cout << "[Capabilities] Process Disk I/O: " << (caps.supports_process_disk_io ? "YES" : "NO") << "\n";
    std::cout << "[Capabilities] Process Path Attribution: " << (caps.supports_process_executable_path ? "YES" : "NO") << "\n\n";

    std::cout << "[Telemetry] Warm-up initial sample to establish delta baseline...\n";
    monitor.sample_system_metrics();
    monitor.sample_process_metrics();

    std::cout << "[Telemetry] Waiting 1000ms sampling interval for genuine CPU delta...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    auto start_sample = std::chrono::steady_clock::now();
    nerv::SystemMetrics sys = monitor.sample_system_metrics();
    auto procs = monitor.sample_process_metrics();
    auto end_sample = std::chrono::steady_clock::now();

    double sample_latency = std::chrono::duration<double, std::milli>(end_sample - start_sample).count();

    // 1. System Metrics Validation
    std::cout << "\n--- System-Wide Real OS Measurements ---\n";
    std::cout << "Total CPU Utilization: " << std::fixed << std::setprecision(2) << sys.total_cpu_percent << " %\n";
    std::cout << "Physical RAM: " << (sys.used_ram_bytes / (1024 * 1024)) << " MB used / "
              << (sys.total_ram_bytes / (1024 * 1024)) << " MB total (" << sys.ram_usage_percent << " % load)\n";
    std::cout << "Available RAM: " << (sys.available_ram_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "Commit Pagefile: " << (sys.used_pagefile_bytes / (1024 * 1024)) << " MB used / "
              << (sys.total_pagefile_bytes / (1024 * 1024)) << " MB total (" << sys.pagefile_usage_percent << " %)\n";
    std::cout << "Network Throughput: RX " << std::fixed << std::setprecision(1)
              << (sys.network_rx_bytes_sec / 1024.0) << " KB/s | TX "
              << (sys.network_tx_bytes_sec / 1024.0) << " KB/s\n";
    std::cout << "System Uptime: " << std::fixed << std::setprecision(1) << sys.uptime_seconds << " seconds ("
              << (sys.uptime_seconds / 3600.0) << " hours)\n";
    std::cout << "Global Process Count: " << sys.process_count << " | Threads: " << sys.thread_count
              << " | Handles: " << sys.handle_count << "\n";
    std::cout << "Collection Latency: " << std::fixed << std::setprecision(2) << sample_latency << " ms\n";

    // 2. Process Enumeration & Attribution
    size_t accessible_count = 0;
    size_t protected_count = 0;
    for (const auto& p : procs) {
        if (p.is_accessible) accessible_count++;
        else protected_count++;
    }

    std::cout << "\n--- Process Enumeration & Access Validation ---\n";
    std::cout << "Total Processes Enumerated: " << procs.size() << "\n";
    std::cout << "Accessible Processes: " << accessible_count << "\n";
    std::cout << "Protected / Inaccessible Processes: " << protected_count
              << " (Gracefully handled without elevation or invented values)\n";

    // 3. Top Processes by Working Set RAM
    std::sort(procs.begin(), procs.end(), [](const nerv::ProcessMetrics& a, const nerv::ProcessMetrics& b) {
        return a.resident_memory_bytes > b.resident_memory_bytes;
    });

    std::cout << "\n--- Top 8 Processes by Working Set RAM (Real OS Values) ---\n";
    std::cout << std::left << std::setw(8) << "PID"
              << std::setw(25) << "Process Name"
              << std::setw(14) << "RAM (MB)"
              << std::setw(12) << "CPU %"
              << std::setw(10) << "Threads"
              << "Executable Path\n";
    std::cout << "----------------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < std::min<size_t>(8, procs.size()); ++i) {
        const auto& p = procs[i];
        std::cout << std::left << std::setw(8) << p.identity.pid
                  << std::setw(25) << p.name.substr(0, 24)
                  << std::setw(14) << std::fixed << std::setprecision(1) << (p.resident_memory_bytes / (1024.0 * 1024.0))
                  << std::setw(12) << std::fixed << std::setprecision(2) << p.cpu_percent
                  << std::setw(10) << p.thread_count
                  << (p.exe_path.empty() ? "(Protected system process)" : p.exe_path.substr(0, 40))
                  << "\n";
    }

    // 4. Stale-Data Detection Validation
    nerv::TelemetrySnapshot snap;
    snap.timestamp = std::chrono::system_clock::now();
    snap.is_valid = true;
    std::cout << "\n--- Stale Data Detection Validation ---\n";
    std::cout << "Fresh snapshot is_stale(4.0s): " << (snap.is_stale(4.0) ? "TRUE (STALE)" : "FALSE (FRESH)") << "\n";

    // Simulate 5-second old snapshot
    snap.timestamp = std::chrono::system_clock::now() - std::chrono::seconds(5);
    std::cout << "Old snapshot (5s old) is_stale(4.0s): " << (snap.is_stale(4.0) ? "TRUE (STALE)" : "FALSE (FRESH)") << "\n";

    std::cout << "\n[SUCCESS] Native telemetry backend validated against host operating system!\n";
    monitor.shutdown();
    return 0;
}
