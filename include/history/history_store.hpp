#pragma once

#include "history/ring_buffer.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include <unordered_map>
#include <string>

namespace nerv {

struct TimeSample {
    double timestamp_sec{0.0};
    float value{0.0f};
};

class HistoryStore {
public:
    static constexpr size_t SystemHistoryCapacity = 120;
    static constexpr size_t ProcessHistoryCapacity = 60;

    HistoryStore() = default;

    void record_snapshot(const TelemetrySnapshot& snapshot);

    // System history getters
    const RingBuffer<float, SystemHistoryCapacity>& get_cpu_history() const { return m_sys_cpu; }
    const RingBuffer<float, SystemHistoryCapacity>& get_ram_pct_history() const { return m_sys_ram_pct; }
    const RingBuffer<float, SystemHistoryCapacity>& get_disk_read_history() const { return m_disk_read_mb; }
    const RingBuffer<float, SystemHistoryCapacity>& get_disk_write_history() const { return m_disk_write_mb; }
    const RingBuffer<float, SystemHistoryCapacity>& get_net_rx_history() const { return m_net_rx_mb; }
    const RingBuffer<float, SystemHistoryCapacity>& get_net_tx_history() const { return m_net_tx_mb; }

    // Process history getters
    const RingBuffer<float, ProcessHistoryCapacity>* get_process_cpu_history(const ProcessIdentity& id) const;
    const RingBuffer<float, ProcessHistoryCapacity>* get_process_ram_history(const ProcessIdentity& id) const;

    // Export capabilities
    bool export_to_csv(const std::string& filepath) const;
    bool export_to_json(const std::string& filepath) const;

    void clear();

private:
    RingBuffer<float, SystemHistoryCapacity> m_sys_cpu;
    RingBuffer<float, SystemHistoryCapacity> m_sys_ram_pct;
    RingBuffer<float, SystemHistoryCapacity> m_disk_read_mb;
    RingBuffer<float, SystemHistoryCapacity> m_disk_write_mb;
    RingBuffer<float, SystemHistoryCapacity> m_net_rx_mb;
    RingBuffer<float, SystemHistoryCapacity> m_net_tx_mb;

    std::unordered_map<ProcessIdentity, RingBuffer<float, ProcessHistoryCapacity>> m_proc_cpu;
    std::unordered_map<ProcessIdentity, RingBuffer<float, ProcessHistoryCapacity>> m_proc_ram_mb;

    // Prune stale process history periodically
    uint32_t m_prune_counter{0};
};

} // namespace nerv
