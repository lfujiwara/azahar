// Copyright 2014-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

#ifdef __EMSCRIPTEN__
namespace {
struct WebGLBinding {
    std::string name;
    int binding;
    bool is_block;
};
// Bindings stripped from each shader's source, applied to programs after link.
std::unordered_map<GLuint, std::vector<WebGLBinding>> webgl_bindings;

// GLSL ES 3.00 lacks layout(binding), uniform locations, and in/out locations between stages.
// Strip those qualifiers, remember the bindings, and emulate samplerBuffer with a 2D texture
// (1024 texels per row, see RasterizerOpenGL LUT upload).
std::string PatchForWebGL(std::string_view source, GLenum type,
                          std::vector<WebGLBinding>& bindings) {
    std::string out;
    out.reserve(source.size() + 256);
    std::size_t pos = 0;
    while (true) {
        const std::size_t at = source.find("layout", pos);
        if (at == source.npos) {
            out.append(source.substr(pos));
            break;
        }
        const bool word_start = at == 0 || !(std::isalnum(source[at - 1]) || source[at - 1] == '_');
        std::size_t open = at + 6;
        while (open < source.size() && std::isspace(source[open])) {
            open++;
        }
        if (!word_start || open >= source.size() || source[open] != '(') {
            out.append(source.substr(pos, at + 6 - pos));
            pos = at + 6;
            continue;
        }
        const std::size_t close = source.find(')', open);
        const std::string_view args = source.substr(open + 1, close - open - 1);
        const std::size_t decl_end = source.find_first_of(";{[", close);
        const std::string_view decl = source.substr(close + 1, decl_end - close - 1);
        std::size_t first = decl.find_first_not_of(" \t\n");
        const std::string_view first_word =
            decl.substr(first, decl.find_first_of(" \t\n", first) - first);
        std::size_t name_end = decl.find_last_not_of(" \t\n") + 1;
        std::size_t name_begin = name_end;
        while (name_begin > 0 && (std::isalnum(decl[name_begin - 1]) || decl[name_begin - 1] == '_')) {
            name_begin--;
        }
        const std::string name{decl.substr(name_begin, name_end - name_begin)};
        const bool drop_location = first_word == "uniform" ||
                                   (type == GL_VERTEX_SHADER && first_word == "out") ||
                                   (type == GL_FRAGMENT_SHADER && first_word == "in");

        std::string kept;
        std::size_t item_pos = 0;
        while (item_pos <= args.size()) {
            std::size_t comma = args.find(',', item_pos);
            if (comma == args.npos) {
                comma = args.size();
            }
            std::string item{args.substr(item_pos, comma - item_pos)};
            item_pos = comma + 1;
            std::erase_if(item, [](char c) { return std::isspace(c); });
            if (item.empty()) {
                continue;
            }
            if (item.starts_with("binding=")) {
                bindings.push_back({name, std::stoi(item.substr(8)), source[decl_end] == '{'});
                continue;
            }
            if (item.starts_with("location=") && drop_location) {
                continue;
            }
            if (item.starts_with("set=")) {
                continue;
            }
            kept += kept.empty() ? item : "," + item;
        }
        out.append(source.substr(pos, at - pos));
        if (!kept.empty()) {
            out += "layout(" + kept + ")";
        }
        pos = close + 1;
    }

    const auto replace_all = [&out](std::string_view from, std::string_view to) {
        for (std::size_t p = out.find(from); p != out.npos; p = out.find(from, p + to.size())) {
            out.replace(p, from.size(), to);
        }
    };
    replace_all("out gl_PerVertex {\n    vec4 gl_Position;\n};", "");
    replace_all("precision highp samplerBuffer;", "");
    replace_all("precision mediump samplerBuffer;", "");
    replace_all("precision highp uimage2D;", "");
    replace_all("precision mediump uimage2D;", "");
    if (out.find("samplerBuffer") != out.npos) {
        replace_all("uniform samplerBuffer", "uniform highp sampler2D");
        replace_all("texelFetch(texture_buffer_lut_", "texelFetchLut(texture_buffer_lut_");
        const std::size_t decl = out.find("uniform highp sampler2D texture_buffer_lut_rgba;");
        const std::size_t line_end = out.find('\n', decl);
        out.insert(line_end + 1,
                   "vec4 texelFetchLut(highp sampler2D s, int i) { return texelFetch(s, "
                   "ivec2(i & 1023, i >> 10), 0); }\n");
    }
    return out;
}
} // namespace
#endif

GLuint LoadShader(std::string_view source, GLenum type, const std::string& debug_name) {
    std::string preamble;
#ifdef __EMSCRIPTEN__
    std::vector<WebGLBinding> bindings;
    const std::string patched = PatchForWebGL(source, type, bindings);
    source = patched;
#endif
    if (GLES) {
        preamble = R"(#version 320 es

#if defined(GL_ANDROID_extension_pack_es31a)
#extension GL_ANDROID_extension_pack_es31a : enable
#endif // defined(GL_ANDROID_extension_pack_es31a)

#if defined(GL_EXT_clip_cull_distance)
#extension GL_EXT_clip_cull_distance : enable
#endif // defined(GL_EXT_clip_cull_distance)
)";
    } else {
        preamble = "#version 430 core\n";
    }
#ifdef __EMSCRIPTEN__
    preamble = type == GL_FRAGMENT_SHADER
                   ? "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                     "#define fma(a, b, c) ((a) * (b) + (c))\n"
                   : "#version 300 es\n#define fma(a, b, c) ((a) * (b) + (c))\n";
#endif

    std::string_view debug_type;
    switch (type) {
    case GL_VERTEX_SHADER:
        debug_type = "vertex";
        break;
    case GL_GEOMETRY_SHADER:
        debug_type = "geometry";
        break;
    case GL_FRAGMENT_SHADER:
        debug_type = "fragment";
        break;
    default:
        UNREACHABLE();
    }

    std::array<const GLchar*, 2> src_arr{preamble.data(), source.data()};
    std::array<GLint, 2> lengths{static_cast<GLint>(preamble.size()),
                                 static_cast<GLint>(source.size())};
    GLuint shader_id = glCreateShader(type);
    if (shader_id == 0) {
        GLenum err = glGetError();
        LOG_ERROR(Render_OpenGL, "glCreateShader failed for {} shader {} (err {})", debug_type,
                  debug_name, err);
        return shader_id;
    }

#ifdef __EMSCRIPTEN__
    webgl_bindings[shader_id] = std::move(bindings);
#endif
    glShaderSource(shader_id, static_cast<GLsizei>(src_arr.size()), src_arr.data(), lengths.data());
    LOG_DEBUG(Render_OpenGL, "Compiling {} shader {}...", debug_type, debug_name);
    glCompileShader(shader_id);

    GLint result = GL_FALSE;
    GLint info_log_length;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> shader_error(info_log_length);
        glGetShaderInfoLog(shader_id, info_log_length, nullptr, &shader_error[0]);
        if (result == GL_TRUE) {
            LOG_DEBUG(Render_OpenGL, "Compile message for {} shader {}:\n{}", debug_type,
                      debug_name, &shader_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error compiling {} shader {}:\n{}", debug_type, debug_name,
                      &shader_error[0]);
            LOG_ERROR(Render_OpenGL, "Shader source code:\n{}{}", src_arr[0], src_arr[1]);
        }
    } else if (result == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "Error compiling {} shader {}:\nNo log produced.", debug_type,
                  debug_name);
    }
    return shader_id;
}

GLuint LoadProgram(bool separable_program, std::span<const GLuint> shaders,
                   const std::string& debug_name) {
    // Link the program
    LOG_DEBUG(Render_OpenGL, "Linking program...");

    GLuint program_id = glCreateProgram();

    for (GLuint shader : shaders) {
        if (shader != 0) {
            glAttachShader(program_id, shader);
        }
    }

#ifndef __EMSCRIPTEN__
    if (separable_program) {
        glProgramParameteri(program_id, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }

    glProgramParameteri(program_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
#endif
    glLinkProgram(program_id);

    // Check the program
    GLint result = GL_FALSE;
    GLint info_log_length;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);

    if (info_log_length > 1) {
        std::vector<char> program_error(info_log_length);
        glGetProgramInfoLog(program_id, info_log_length, nullptr, &program_error[0]);
        if (result == GL_TRUE) {
            LOG_DEBUG(Render_OpenGL, "Link message for shader {}:\n{}", debug_name,
                      &program_error[0]);
        } else {
            LOG_ERROR(Render_OpenGL, "Error linking shader {}:\n{}", debug_name, &program_error[0]);
        }
    } else if (result == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "Error linking shader {}: No log produced.", debug_name);
    }

#ifdef __EMSCRIPTEN__
    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glUseProgram(program_id);
    for (GLuint shader : shaders) {
        for (const auto& b : webgl_bindings[shader]) {
            if (b.is_block) {
                const GLuint index = glGetUniformBlockIndex(program_id, b.name.c_str());
                if (index != GL_INVALID_INDEX) {
                    glUniformBlockBinding(program_id, index, b.binding);
                }
            } else if (const GLint loc = glGetUniformLocation(program_id, b.name.c_str());
                       loc >= 0) {
                glUniform1i(loc, b.binding);
            }
        }
    }
    glUseProgram(prev_program);
#endif

    for (GLuint shader : shaders) {
        if (shader != 0) {
            glDetachShader(program_id, shader);
        }
    }

    return program_id;
}

} // namespace OpenGL
