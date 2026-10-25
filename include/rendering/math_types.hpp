#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace nerv {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float in_x, float in_y) : x(in_x), y(in_y) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return (s != 0.0f) ? Vec2{x / s, y / s} : Vec2{0.0f, 0.0f}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

    float length_sq() const { return x * x + y * y; }
    float length() const { return std::sqrt(length_sq()); }

    Vec2 normalized() const {
        float len = length();
        return (len > 0.00001f) ? (*this / len) : Vec2{0.0f, 0.0f};
    }

    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float distance(const Vec2& o) const { return (*this - o).length(); }
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() = default;
    constexpr Vec3(float in_x, float in_y, float in_z) : x(in_x), y(in_y), z(in_z) {}
};

struct Vec4 {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    constexpr Vec4() = default;
    constexpr Vec4(float in_r, float in_g, float in_b, float in_a = 1.0f)
        : r(in_r), g(in_g), b(in_b), a(in_a) {}

    static constexpr Vec4 from_hex(unsigned int hex, float alpha = 1.0f) {
        float in_r = ((hex >> 16) & 0xFF) / 255.0f;
        float in_g = ((hex >> 8) & 0xFF) / 255.0f;
        float in_b = (hex & 0xFF) / 255.0f;
        return {in_r, in_g, in_b, alpha};
    }
};

namespace colors {
    // Official NERV Visual Identity Palette
    inline constexpr Vec4 BackgroundBlack = Vec4::from_hex(0x05070D);
    inline constexpr Vec4 DeepNavy        = Vec4::from_hex(0x0B1220);
    inline constexpr Vec4 ElectricCyan    = Vec4::from_hex(0x5DEBFF);
    inline constexpr Vec4 NeuralViolet    = Vec4::from_hex(0x9A7BFF);
    inline constexpr Vec4 ActiveGreen     = Vec4::from_hex(0x63F5B0);
    inline constexpr Vec4 WarningAmber    = Vec4::from_hex(0xFFBF69);
    inline constexpr Vec4 CriticalRed     = Vec4::from_hex(0xFF657A);
    inline constexpr Vec4 TextWhite       = Vec4::from_hex(0xEDF3FF);
}

struct Mat4 {
    std::array<float, 16> m{};

    static Mat4 identity() {
        Mat4 res;
        res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
        return res;
    }

    static Mat4 orthographic(float left, float right, float bottom, float top, float near_val = -1.0f, float far_val = 1.0f) {
        Mat4 res;
        float rl = right - left;
        float tb = top - bottom;
        float fn = far_val - near_val;

        res.m[0] = 2.0f / rl;
        res.m[5] = 2.0f / tb;
        res.m[10] = -2.0f / fn;
        res.m[12] = -(right + left) / rl;
        res.m[13] = -(top + bottom) / tb;
        res.m[14] = -(far_val + near_val) / fn;
        res.m[15] = 1.0f;
        return res;
    }

    const float* data() const { return m.data(); }
};

} // namespace nerv
