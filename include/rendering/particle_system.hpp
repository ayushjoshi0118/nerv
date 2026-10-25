#pragma once

#include "rendering/math_types.hpp"
#include <vector>

namespace nerv {

struct AmbientParticle {
    Vec2 position;
    Vec2 velocity;
    float size{2.0f};
    float alpha{0.5f};
    float pulse_phase{0.0f};
};

class ParticleSystem {
public:
    explicit ParticleSystem(size_t count = 180);

    void update(float delta_time, const Vec2& center_pos);
    const std::vector<AmbientParticle>& get_particles() const { return m_particles; }

    void set_count(size_t count);
    size_t get_count() const { return m_particles.size(); }

private:
    void init_particle(AmbientParticle& p, const Vec2& center);

    std::vector<AmbientParticle> m_particles;
};

} // namespace nerv
