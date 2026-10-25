#pragma once

#include <glad/glad.h>
#include "rendering/math_types.hpp"
#include <string>
#include <unordered_map>

namespace nerv {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool compile_and_link(const std::string& vertex_src, const std::string& fragment_src);
    void bind() const;
    void unbind() const;

    GLuint get_id() const { return m_program_id; }
    bool is_valid() const { return m_program_id != 0; }

    void set_mat4(const std::string& name, const Mat4& mat) const;
    void set_vec4(const std::string& name, const Vec4& v) const;
    void set_vec3(const std::string& name, const Vec3& v) const;
    void set_vec2(const std::string& name, const Vec2& v) const;
    void set_float(const std::string& name, float val) const;
    void set_int(const std::string& name, int val) const;

private:
    GLint get_uniform_location(const std::string& name) const;
    static GLuint compile_shader(GLenum type, const std::string& source);

    GLuint m_program_id{0};
    mutable std::unordered_map<std::string, GLint> m_uniform_cache;
};

class ShaderManager {
public:
    ShaderManager() = default;

    bool load_program(const std::string& name, const std::string& vert_path, const std::string& frag_path,
                      const std::string& fallback_vert = "", const std::string& fallback_frag = "");

    const ShaderProgram* get_program(const std::string& name) const;

private:
    static std::string read_file_to_string(const std::string& path);

    std::unordered_map<std::string, ShaderProgram> m_programs;
};

} // namespace nerv
