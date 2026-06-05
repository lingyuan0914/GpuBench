#pragma once

#include <string>
#include <vector>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <android/log.h>

#define LOG_TAG "GpuBench"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace GpuBench {

// OpenGL 着色器编译
class GLShaderUtils {
public:
    static GLuint compileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            LOGE("Shader compilation failed: %s", infoLog);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint createProgram(const std::string& vertexSource,
                                const std::string& fragmentSource) {
        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        if (!vertexShader || !fragmentShader) {
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
            LOGE("Program linking failed: %s", infoLog);
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return program;
    }

    static GLuint createComputeProgram(const std::string& computeSource) {
        GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);
        if (!computeShader) return 0;

        GLuint program = glCreateProgram();
        glAttachShader(program, computeShader);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
            LOGE("Compute program linking failed: %s", infoLog);
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(computeShader);
        return program;
    }
};

} // namespace GpuBench
