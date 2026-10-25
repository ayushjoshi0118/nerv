#include "history/history_store.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>

namespace nerv {

void HistoryStore::record_snapshot(const TelemetrySnapshot& snapshot) {
    if (!snapshot.is_valid) return;

    // Record system-wide metrics
    m_sys_cpu.push(snapshot.system.total_cpu_percent);
    m_sys_ram_pct.push(snapshot.system.ram_usage_percent);

    float read_mb = static_cast<float>(snapshot.system.disk_read_bytes_sec) / (1024.0f * 1024.0f);
    float write_mb = static_cast<float>(snapshot.system.disk_write_bytes_sec) / (1024.0f * 1024.0f);
    m_disk_read_mb.push(read_mb);
    m_disk_write_mb.push(write_mb);

    float rx_mb = static_cast<float>(snapshot.system.network_rx_bytes_sec) / (1024.0f * 1024.0f);
    float tx_mb = static_cast<float>(snapshot.system.network_tx_bytes_sec) / (1024.0f * 1024.0f);
    m_net_rx_mb.push(rx_mb);
    m_net_tx_mb.push(tx_mb);

    // Record per-process metrics for active processes
    for (const auto& proc : snapshot.processes) {
        if (!proc.is_accessible) continue;

        float ram_mb = static_cast<float>(proc.resident_memory_bytes) / (1024.0f * 1024.0f);
        m_proc_cpu[proc.identity].push(proc.cpu_percent);
        m_proc_ram_mb[proc.identity].push(ram_mb);
    }

    // Prune stale processes every 60 snapshots
    if (++m_prune_counter >= 60) {
        m_prune_counter = 0;
        std::unordered_map<ProcessIdentity, bool> active_map;
        for (const auto& proc : snapshot.processes) {
            active_map[proc.identity] = true;
        }

        for (auto it = m_proc_cpu.begin(); it != m_proc_cpu.end();) {
            if (active_map.find(it->first) == active_map.end()) {
                it = m_proc_cpu.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_proc_ram_mb.begin(); it != m_proc_ram_mb.end();) {
            if (active_map.find(it->first) == active_map.end()) {
                it = m_proc_ram_mb.erase(it);
            } else {
                ++it;
            }
        }
    }
}

const RingBuffer<float, HistoryStore::ProcessHistoryCapacity>*
HistoryStore::get_process_cpu_history(const ProcessIdentity& id) const {
    auto it = m_proc_cpu.find(id);
    return (it != m_proc_cpu.end()) ? &it->second : nullptr;
}

const RingBuffer<float, HistoryStore::ProcessHistoryCapacity>*
HistoryStore::get_process_ram_history(const ProcessIdentity& id) const {
    auto it = m_proc_ram_mb.find(id);
    return (it != m_proc_ram_mb.end()) ? &it->second : nullptr;
}

void HistoryStore::clear() {
    m_sys_cpu.clear();
    m_sys_ram_pct.clear();
    m_disk_read_mb.clear();
    m_disk_write_mb.clear();
    m_net_rx_mb.clear();
    m_net_tx_mb.clear();
    m_proc_cpu.clear();
    m_proc_ram_mb.clear();
}

bool HistoryStore::export_to_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "SampleIndex,CpuPercent,RamPercent,DiskReadMBs,DiskWriteMBs,NetRxMBs,NetTxMBs\n";
    size_t count = m_sys_cpu.size();
    for (size_t i = 0; i < count; ++i) {
        file << i << ","
             << std::fixed << std::setprecision(2)
             << m_sys_cpu[i] << ","
             << m_sys_ram_pct[i] << ","
             << m_disk_read_mb[i] << ","
             << m_disk_write_mb[i] << ","
             << m_net_rx_mb[i] << ","
             << m_net_tx_mb[i] << "\n";
    }
    return true;
}

bool HistoryStore::export_to_json(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    nlohmann::json j;
    j["application"] = "NERV";
    j["sample_count"] = m_sys_cpu.size();
    j["cpu_history"] = m_sys_cpu.to_vector();
    j["ram_pct_history"] = m_sys_ram_pct.to_vector();
    j["disk_read_mb_history"] = m_disk_read_mb.to_vector();
    j["disk_write_mb_history"] = m_disk_write_mb.to_vector();
    j["net_rx_mb_history"] = m_net_rx_mb.to_vector();
    j["net_tx_mb_history"] = m_net_tx_mb.to_vector();

    file << j.dump(4);
    return true;
}

} // namespace nerv
