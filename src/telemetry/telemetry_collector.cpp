#include "telemetry/telemetry_collector.hpp"
#include <chrono>
#include <algorithm>

namespace nerv {

TelemetryCollector::TelemetryCollector(std::unique_ptr<ISystemMonitor> monitor)
    : m_monitor(std::move(monitor)) {
    m_latest_snapshot = std::make_shared<TelemetrySnapshot>();
}

TelemetryCollector::~TelemetryCollector() {
    stop();
}

void TelemetryCollector::start() {
    if (m_is_running.load()) return;
    if (m_monitor) {
        m_monitor->initialize();
    }
    m_is_running.store(true);
    m_worker_thread = std::thread(&TelemetryCollector::collection_worker, this);
}

void TelemetryCollector::stop() {
    if (!m_is_running.load()) return;
    m_is_running.store(false);
    m_cv.notify_all();

    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }

    if (m_monitor) {
        m_monitor->shutdown();
    }
}

void TelemetryCollector::set_sampling_interval_ms(uint32_t interval_ms) {
    // Keep within reasonable boundaries: 200ms to 10000ms
    uint32_t clamped = std::clamp(interval_ms, 200u, 10000u);
    m_interval_ms.store(clamped);
    m_cv.notify_all();
}

TelemetrySnapshotPtr TelemetryCollector::get_latest_snapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshot_mutex);
    return m_latest_snapshot;
}

void TelemetryCollector::collection_worker() {
    while (m_is_running.load()) {
        if (!m_is_paused.load() && m_monitor) {
            auto start_time = std::chrono::steady_clock::now();

            auto new_snapshot = std::make_shared<TelemetrySnapshot>();
            new_snapshot->system = m_monitor->sample_system_metrics();
            new_snapshot->processes = m_monitor->sample_process_metrics();
            new_snapshot->timestamp = std::chrono::system_clock::now();
            new_snapshot->sequence_number = ++m_sequence;
            new_snapshot->is_valid = true;

            auto end_time = std::chrono::steady_clock::now();
            new_snapshot->collection_duration_ms =
                std::chrono::duration<double, std::milli>(end_time - start_time).count();

            {
                std::lock_guard<std::mutex> lock(m_snapshot_mutex);
                m_latest_snapshot = new_snapshot;
            }
        }

        std::unique_lock<std::mutex> lock(m_snapshot_mutex);
        m_cv.wait_for(lock, std::chrono::milliseconds(m_interval_ms.load()), [this]() {
            return !m_is_running.load();
        });
    }
}

} // namespace nerv
