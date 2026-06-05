#pragma once

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <string>
#include <vector>
#include <chrono>
#include <android/log.h>

#define LOG_TAG "ComputeTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace GpuBench {

struct ComputeResult {
    float gflops;           // 浮点运算性能
    float memoryBandwidth;  // 内存带宽 (GB/s)
    float computeScore;     // 综合计算分数
    int iterations;         // 迭代次数
};

class ComputeTest {
public:
    static ComputeResult runGflopsTest() {
        ComputeResult result = {0, 0, 0, 0};

        // 计算着色器 - 浮点运算测试
        const std::string computeShader = R"(#version 320 es
            precision highp float;

            layout(local_size_x = 256) in;

            layout(std430, binding = 0) buffer InputBuffer {
                float input_data[];
            };

            layout(std430, binding = 1) buffer OutputBuffer {
                float output_data[];
            };

            uniform uint uIterations;

            void main() {
                uint id = gl_GlobalInvocationID.x;
                float val = input_data[id];

                // 密集浮点运算
                for (uint i = 0u; i < uIterations; i++) {
                    val = val * 1.0001 + 0.0001;
                    val = val / 1.0001;
                    val = sqrt(val * val + 1.0);
                    val = sin(val) * cos(val);
                    val = val * val + 0.5;
                }

                output_data[id] = val;
            }
        )";

        GLuint program = createComputeProgram(computeShader);
        if (!program) return result;

        const uint32_t ELEMENT_COUNT = 1024 * 1024; // 100万元素
        const uint32_t ITERATIONS = 100;

        // 创建缓冲区
        std::vector<float> inputData(ELEMENT_COUNT, 1.0f);
        std::vector<float> outputData(ELEMENT_COUNT);

        GLuint inputSsbo, outputSsbo;
        glGenBuffers(1, &inputSsbo);
        glGenBuffers(1, &outputSsbo);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inputSsbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, inputData.size() * sizeof(float), inputData.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outputSsbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, outputData.size() * sizeof(float), nullptr, GL_STATIC_READ);

        // 绑定缓冲区
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSsbo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputSsbo);

        // 设置 uniform
        glUseProgram(program);
        glUniform1ui(glGetUniformLocation(program, "uIterations"), ITERATIONS);

        // 预热
        glDispatchCompute(ELEMENT_COUNT / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glFinish();

        // 正式测试
        auto startTime = std::chrono::high_resolution_clock::now();

        const int TEST_RUNS = 10;
        for (int i = 0; i < TEST_RUNS; i++) {
            glDispatchCompute(ELEMENT_COUNT / 256, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
        glFinish();

        auto endTime = std::chrono::high_resolution_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        // 计算 GFLOPS
        // 每个元素每次迭代约 6 次浮点运算
        uint64_t totalOps = (uint64_t)ELEMENT_COUNT * ITERATIONS * TEST_RUNS * 6;
        float seconds = elapsedMs / 1000.0f;
        result.gflops = (totalOps / seconds) / 1e9f;

        // 计算内存带宽
        // 每次运行读写 2 * ELEMENT_COUNT * sizeof(float) 字节
        uint64_t totalBytes = (uint64_t)2 * ELEMENT_COUNT * sizeof(float) * TEST_RUNS;
        result.memoryBandwidth = (totalBytes / seconds) / 1e9f; // GB/s

        result.computeScore = result.gflops * 0.7f + result.memoryBandwidth * 0.3f;
        result.iterations = ITERATIONS;

        // 清理
        glDeleteBuffers(1, &inputSsbo);
        glDeleteBuffers(1, &outputSsbo);
        glDeleteProgram(program);

        LOGI("Compute Test: %.2f GFLOPS, %.2f GB/s", result.gflops, result.memoryBandwidth);

        return result;
    }

private:
    static GLuint createComputeProgram(const std::string& source) {
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            LOGE("Compute shader error: %s", log);
            glDeleteShader(shader);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char log[512];
            glGetProgramInfoLog(program, 512, nullptr, log);
            LOGE("Program link error: %s", log);
            glDeleteProgram(program);
            glDeleteShader(shader);
            return 0;
        }

        glDeleteShader(shader);
        return program;
    }
};

} // namespace GpuBench
