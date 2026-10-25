#include "rendering/shader_manager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace nerv {

ShaderProgram::~ShaderProgram() {
    if (m_program_id != 0) {
        glDeleteProgram(m_program_id);
        m_program_id = 0;
    }
}

GLuint ShaderProgram::compile_shader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src_ptr = source.c_str();
    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        std::cerr << "[Shader Error] Compilation failed:\n" << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderProgram::compile_and_link(const std::string& vertex_src, const std::string& fragment_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (vert == 0) return false;

    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (frag == 0) {
        glDeleteShader(vert);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
        std::cerr << "[Shader Error] Program linking failed:\n" << info_log << std::endl;
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(program);
        return false;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    if (m_program_id != 0) {
        glDeleteProgram(m_program_id);
    }
    m_program_id = program;
    m_uniform_cache.clear();
    return true;
}

void ShaderProgram::bind() const {
    if (m_program_id != 0) {
        glUseProgram(m_program_id);
    }
}

void ShaderProgram::unbind() const {
    glUseProgram(0);
}

GLint ShaderProgram::get_uniform_location(const std::string& name) const {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(m_program_id, name.c_str());
    m_uniform_cache[name] = loc;
    return loc;
}

void ShaderProgram::set_mat4(const std::string& name, const Mat4& mat) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, mat.data());
    }
}

void ShaderProgram::set_vec4(const std::string& name, const Vec4& v) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform4f(loc, v.r, v.g, v.b, v.a);
    }
}

void ShaderProgram::set_vec3(const std::string& name, const Vec3& v) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform3f(loc, v.x, v.y, v.z);
    }
}

void ShaderProgram::set_vec2(const std::string& name, const Vec2& v) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform2f(loc, v.x, v.y);
    }
}

void ShaderProgram::set_float(const std::string& name, float val) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1f(loc, val);
    }
}

void ShaderProgram::set_int(const std::string& name, int val) const {
    GLint loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1i(loc, val);
    }
}

std::string ShaderManager::read_file_to_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool ShaderManager::load_program(const std::string& name, const std::string& vert_path,
                                const std::string& frag_path, const std::string& fallback_vert,
                                const std::string& fallback_frag) {
    std::string vert_src = read_file_to_string(vert_path);
    if (vert_src.empty()) vert_src = fallback_vert;

    std::string frag_src = read_file_to_string(frag_path);
    if (frag_src.empty()) frag_src = fallback_frag;

    if (vert_src.empty() || frag_src.empty()) {
        std::cerr << "[ShaderManager] No valid shader source for " << name << std::endl;
        return false;
    }

    ShaderProgram prog;
    if (!prog.compile_and_link(vert_src, frag_src)) {
        return false;
    }

    m_programs[name] = std::move(prog);
    return true;
}

const ShaderProgram* ShaderManager::get_program(const std::string& name) const {
    auto it = m_programs.find(name);
    return (it != m_programs.end()) ? &it->second : nullptr;
}

} // namespace nerv
