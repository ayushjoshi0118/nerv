#pragma once

#include "rendering/math_types.hpp"
#include "events/system_event.hpp"
#include <vector>
#include <string>

namespace nerv {

struct ActiveShockwave {
    Vec2 center{0.0f, 0.0f};
    float current_radius{0.0f};
    float max_radius{280.0f};
    float lifetime{1.4f};
    float elapsed{0.0f};
    Vec4 color{colors::ElectricCyan};

    // Actual measurement and event context
    uint64_t event_id{0};
    EventType event_type{EventType::CpuSpike};
    EventSeverity severity{EventSeverity::Info};
    std::string badge_text;      // e.g. "CPU: 88.4%" or "DISK: 45.2 MB/s"
    std::string secondary_text;  // e.g. "Threshold: 75.0%"
    float measured_value{0.0f};
    std::string unit;

    float get_progress() const {
        return (lifetime > 0.0f) ? std::min(1.0f, elapsed / lifetime) : 1.0f;
    }

    bool is_finished() const {
        return elapsed >= lifetime;
    }
};

class ShockwaveSystem {
public:
    ShockwaveSystem() = default;

    void trigger(const Vec2& center, const Vec4& color = colors::ElectricCyan,
                 float max_radius = 280.0f, float lifetime = 1.4f,
                 const std::string& badge_text = "", const std::string& secondary_text = "",
                 EventSeverity severity = EventSeverity::Info,
                 EventType event_type = EventType::CpuSpike,
                 float measured_value = 0.0f, const std::string& unit = "",
                 uint64_t event_id = 0);

    void update(float delta_time);
    void clear() { m_shockwaves.clear(); }

    const std::vector<ActiveShockwave>& get_shockwaves() const { return m_shockwaves; }

private:
    std::vector<ActiveShockwave> m_shockwaves;
};

} // namespace nerv
