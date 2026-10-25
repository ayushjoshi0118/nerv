#pragma once

#include "telemetry/i_system_monitor.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace nerv {

class TelemetryCollector {
public:
    explicit TelemetryCollector(std::unique_ptr<ISystemMonitor> monitor);
    ~TelemetryCollector();

    void start();
    void stop();

    // Configuration
    void set_sampling_interval_ms(uint32_t interval_ms);
    uint32_t get_sampling_interval_ms() const { return m_interval_ms.load(); }

    void set_paused(bool paused) { m_is_paused.store(paused); }
    bool is_paused() const { return m_is_paused.load(); }

    // Fast non-blocking snapshot retrieval for the graphics render thread
    TelemetrySnapshotPtr get_latest_snapshot() const;

    const ISystemMonitor* get_monitor() const { return m_monitor.get(); }

private:
    void collection_worker();

    std::unique_ptr<ISystemMonitor> m_monitor;
    std::thread m_worker_thread;
    std::atomic<bool> m_is_running{false};
    std::atomic<bool> m_is_paused{false};
    std::atomic<uint32_t> m_interval_ms{1000};

    mutable std::mutex m_snapshot_mutex;
    std::condition_variable m_cv;
    TelemetrySnapshotPtr m_latest_snapshot;
    uint64_t m_sequence{0};
};

} // namespace nerv
