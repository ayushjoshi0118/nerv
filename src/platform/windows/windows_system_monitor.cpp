#ifdef _WIN32

#include "platform/windows/windows_system_monitor.hpp"
#include <psapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <algorithm>
#include <iostream>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

namespace nerv {

WindowsSystemMonitor::WindowsSystemMonitor() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_processor_count = si.dwNumberOfProcessors > 0 ? si.dwNumberOfProcessors : 1;
}

WindowsSystemMonitor::~WindowsSystemMonitor() {
    shutdown();
}

uint64_t WindowsSystemMonitor::filetime_to_uint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

uint64_t WindowsSystemMonitor::filetime_to_nanos(const FILETIME& ft) {
    // 1 FILETIME tick = 100 nanoseconds
    return filetime_to_uint64(ft) * 100ULL;
}

std::string WindowsSystemMonitor::wide_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

ProcessCategory WindowsSystemMonitor::categorize_process(const std::string& name, const std::string& path) {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower_name == "system" || lower_name == "registry" || lower_name == "smss.exe" || lower_name == "csrss.exe") {
        return ProcessCategory::SystemKernel;
    }
    if (lower_name == "services.exe" || lower_name == "svchost.exe" || lower_name == "lsass.exe" ||
        lower_name == "wininit.exe" || lower_name == "dwm.exe" || lower_name == "spoolsv.exe") {
        return ProcessCategory::SystemService;
    }
    if (lower_name == "code.exe" || lower_name == "devenv.exe" || lower_name == "cl.exe" ||
        lower_name == "git.exe" || lower_name == "cmake.exe" || lower_name == "powershell.exe" ||
        lower_name == "cmd.exe" || lower_name == "wt.exe" || lower_name == "nerv.exe") {
        return ProcessCategory::Development;
    }
    if (lower_name == "explorer.exe" || lower_name == "chrome.exe" || lower_name == "msedge.exe" ||
        lower_name == "firefox.exe" || lower_name == "spotify.exe" || lower_name == "discord.exe" ||
        lower_name == "slack.exe" || lower_name == "notepad.exe") {
        return ProcessCategory::UserInteractive;
    }

    if (path.find("Windows\\System32") != std::string::npos || path.find("Windows\\SysWOW64") != std::string::npos) {
        return ProcessCategory::SystemService;
    }

    return ProcessCategory::BackgroundWorker;
}

bool WindowsSystemMonitor::initialize() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        m_prev_sys_idle_time = filetime_to_uint64(idleTime);
        m_prev_sys_kernel_time = filetime_to_uint64(kernelTime);
        m_prev_sys_user_time = filetime_to_uint64(userTime);
        m_prev_sample_time = std::chrono::steady_clock::now();
    }

    // Initialize network octets
    PMIB_IF_TABLE2 pIfTable = nullptr;
    if (GetIfTable2(&pIfTable) == NO_ERROR) {
        uint64_t total_rx = 0;
        uint64_t total_tx = 0;
        for (ULONG i = 0; i < pIfTable->NumEntries; ++i) {
            const auto& row = pIfTable->Table[i];
            if (row.OperStatus == IfOperStatusUp && row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
                total_rx += row.InOctets;
                total_tx += row.OutOctets;
            }
        }
        m_prev_net_rx_octets = total_rx;
        m_prev_net_tx_octets = total_tx;
        m_prev_net_sample_time = std::chrono::steady_clock::now();
        FreeMibTable(pIfTable);
    }

    return true;
}

void WindowsSystemMonitor::shutdown() {
    m_process_history.clear();
}

SystemMetrics WindowsSystemMonitor::sample_system_metrics() {
    auto start_time = std::chrono::steady_clock::now();
    SystemMetrics metrics;

    // 1. Total CPU Utilization via GetSystemTimes
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        uint64_t cur_idle = filetime_to_uint64(idleTime);
        uint64_t cur_kernel = filetime_to_uint64(kernelTime);
        uint64_t cur_user = filetime_to_uint64(userTime);

        uint64_t delta_idle = (cur_idle >= m_prev_sys_idle_time) ? (cur_idle - m_prev_sys_idle_time) : 0;
        uint64_t delta_kernel = (cur_kernel >= m_prev_sys_kernel_time) ? (cur_kernel - m_prev_sys_kernel_time) : 0;
        uint64_t delta_user = (cur_user >= m_prev_sys_user_time) ? (cur_user - m_prev_sys_user_time) : 0;

        uint64_t total_sys = delta_kernel + delta_user;
        if (total_sys > 0) {
            double busy = 1.0 - (static_cast<double>(delta_idle) / static_cast<double>(total_sys));
            metrics.total_cpu_percent = std::clamp(static_cast<float>(busy * 100.0), 0.0f, 100.0f);
        } else {
            metrics.total_cpu_percent = 0.0f;
        }

        m_prev_sys_idle_time = cur_idle;
        m_prev_sys_kernel_time = cur_kernel;
        m_prev_sys_user_time = cur_user;
        metrics.is_cpu_available = true;
    } else {
        metrics.is_cpu_available = false;
    }

    // 2. Physical and Virtual RAM via GlobalMemoryStatusEx
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        metrics.total_ram_bytes = mem.ullTotalPhys;
        metrics.available_ram_bytes = mem.ullAvailPhys;
        metrics.used_ram_bytes = mem.ullTotalPhys - mem.ullAvailPhys;
        metrics.ram_usage_percent = static_cast<float>(mem.dwMemoryLoad);

        metrics.total_pagefile_bytes = mem.ullTotalPageFile;
        metrics.available_pagefile_bytes = mem.ullAvailPageFile;
        metrics.used_pagefile_bytes = mem.ullTotalPageFile - mem.ullAvailPageFile;
        if (metrics.total_pagefile_bytes > 0) {
            metrics.pagefile_usage_percent = (static_cast<float>(metrics.used_pagefile_bytes) /
                                             static_cast<float>(metrics.total_pagefile_bytes)) * 100.0f;
        }
        metrics.is_ram_available = true;
    } else {
        metrics.is_ram_available = false;
    }

    // 3. Network Throughput via GetIfTable2
    PMIB_IF_TABLE2 pIfTable = nullptr;
    auto now_net = std::chrono::steady_clock::now();
    double net_delta_sec = std::chrono::duration<double>(now_net - m_prev_net_sample_time).count();
    if (net_delta_sec > 0.001 && GetIfTable2(&pIfTable) == NO_ERROR) {
        uint64_t cur_rx = 0;
        uint64_t cur_tx = 0;
        for (ULONG i = 0; i < pIfTable->NumEntries; ++i) {
            const auto& row = pIfTable->Table[i];
            if (row.OperStatus == IfOperStatusUp && row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
                cur_rx += row.InOctets;
                cur_tx += row.OutOctets;
            }
        }

        if (m_prev_net_rx_octets > 0 && cur_rx >= m_prev_net_rx_octets) {
            metrics.network_rx_bytes_sec = static_cast<uint64_t>((cur_rx - m_prev_net_rx_octets) / net_delta_sec);
        }
        if (m_prev_net_tx_octets > 0 && cur_tx >= m_prev_net_tx_octets) {
            metrics.network_tx_bytes_sec = static_cast<uint64_t>((cur_tx - m_prev_net_tx_octets) / net_delta_sec);
        }

        m_prev_net_rx_octets = cur_rx;
        m_prev_net_tx_octets = cur_tx;
        m_prev_net_sample_time = now_net;
        FreeMibTable(pIfTable);
        metrics.is_network_available = true;
    } else {
        metrics.is_network_available = false;
    }

    // 4. System Uptime & Counts
    metrics.uptime_seconds = static_cast<double>(GetTickCount64()) / 1000.0;
    metrics.sample_timestamp = std::chrono::system_clock::now();

    // 5. Total Disk throughput (aggregated from process sampling or performance info)
    PERFORMANCE_INFORMATION perfInfo{};
    perfInfo.cb = sizeof(perfInfo);
    if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
        metrics.process_count = perfInfo.ProcessCount;
        metrics.thread_count = perfInfo.ThreadCount;
        metrics.handle_count = perfInfo.HandleCount;
    }

    auto end_time = std::chrono::steady_clock::now();
    metrics.collection_duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return metrics;
}

std::vector<ProcessMetrics> WindowsSystemMonitor::sample_process_metrics() {
    std::vector<ProcessMetrics> results;
    auto now_clock = std::chrono::system_clock::now();
    auto now_steady = std::chrono::steady_clock::now();
    double delta_sec = std::chrono::duration<double>(now_steady - m_prev_sample_time).count();
    if (delta_sec <= 0.0001) delta_sec = 1.0;

    m_prev_sample_time = now_steady;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return results;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    std::unordered_map<ProcessIdentity, ProcessSampleHistory> current_seen;
    uint64_t total_disk_read = 0;
    uint64_t total_disk_write = 0;

    if (Process32FirstW(snap, &pe)) {
        do {
            ProcessMetrics proc;
            proc.identity.pid = pe.th32ProcessID;
            proc.parent_pid = pe.th32ParentProcessID;
            proc.name = wide_to_utf8(pe.szExeFile);
            proc.thread_count = pe.cntThreads;
            proc.sample_timestamp = now_clock;
            proc.state = ProcessState::Running;

            // PID 0 is System Idle Process
            if (pe.th32ProcessID == 0) {
                proc.name = "System Idle Process";
                proc.category = ProcessCategory::SystemKernel;
                proc.is_accessible = false;
                results.push_back(proc);
                continue;
            }

            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) {
                // Access denied (protected system service or higher integrity level)
                proc.is_accessible = false;
                proc.category = categorize_process(proc.name, "");
                results.push_back(proc);
                continue;
            }

            proc.is_accessible = true;

            // 1. Process Times (Creation time and CPU utilization)
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                proc.identity.start_time_ns = filetime_to_nanos(ftCreate);

                uint64_t proc_time_100ns = filetime_to_uint64(ftKernel) + filetime_to_uint64(ftUser);

                auto it = m_process_history.find(proc.identity);
                if (it != m_process_history.end() && proc_time_100ns >= it->second.last_proc_time_100ns) {
                    uint64_t delta_proc_100ns = proc_time_100ns - it->second.last_proc_time_100ns;
                    double delta_proc_sec = static_cast<double>(delta_proc_100ns) * 1e-7;

                    // Normalized CPU percentage = (process CPU time / (interval * num_processors)) * 100
                    double cpu = (delta_proc_sec / (delta_sec * m_processor_count)) * 100.0;
                    proc.cpu_percent = std::clamp(static_cast<float>(cpu), 0.0f, 100.0f);
                } else {
                    proc.cpu_percent = 0.0f;
                }

                current_seen[proc.identity].last_proc_time_100ns = proc_time_100ns;
            }

            // 2. Process Memory
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                proc.resident_memory_bytes = pmc.WorkingSetSize;
                proc.virtual_memory_bytes = pmc.PrivateUsage;
            }

            // 3. Process I/O Counters
            IO_COUNTERS io{};
            if (GetProcessIoCounters(hProc, &io)) {
                proc.has_io_data = true;
                auto it = m_process_history.find(proc.identity);
                if (it != m_process_history.end()) {
                    if (io.ReadTransferCount >= it->second.last_disk_read_bytes) {
                        proc.disk_read_bytes_sec = static_cast<uint64_t>(
                            (io.ReadTransferCount - it->second.last_disk_read_bytes) / delta_sec);
                    }
                    if (io.WriteTransferCount >= it->second.last_disk_write_bytes) {
                        proc.disk_write_bytes_sec = static_cast<uint64_t>(
                            (io.WriteTransferCount - it->second.last_disk_write_bytes) / delta_sec);
                    }
                }
                current_seen[proc.identity].last_disk_read_bytes = io.ReadTransferCount;
                current_seen[proc.identity].last_disk_write_bytes = io.WriteTransferCount;

                total_disk_read += proc.disk_read_bytes_sec;
                total_disk_write += proc.disk_write_bytes_sec;
            }

            // 4. Executable Path
            WCHAR path_buf[MAX_PATH * 2]{};
            DWORD path_len = MAX_PATH * 2;
            if (QueryFullProcessImageNameW(hProc, 0, path_buf, &path_len) && path_len > 0) {
                proc.exe_path = wide_to_utf8(path_buf);
            }

            proc.category = categorize_process(proc.name, proc.exe_path);
            CloseHandle(hProc);

            results.push_back(proc);
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);

    m_process_history = std::move(current_seen);
    return results;
}

BackendCapabilities WindowsSystemMonitor::get_capabilities() const {
    BackendCapabilities caps;
    caps.supports_per_core_cpu = false; // SystemTimes gives aggregate
    caps.supports_system_disk_io = true;
    caps.supports_system_network_io = true;
    caps.supports_process_disk_io = true;
    caps.supports_process_parent_pid = true;
    caps.supports_process_thread_count = true;
    caps.supports_process_start_time = true;
    caps.supports_process_executable_path = true;
    caps.platform_name = "Windows (Native Performance APIs)";
    return caps;
}

} // namespace nerv

#endif // _WIN32
