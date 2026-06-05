#pragma once

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <string>
#include <vector>
#include <chrono>
#include <android/log.h>
#include <EGL/egl.h>
#include "MathUtils.h"
#include "ShaderUtils.h"
#include "Timer.h"

#define LOG_TAG "SceneTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace GpuBench {

struct SceneResult {
    std::string sceneName;
    int triangleCount;  // 三角形数量 (万)
    float avgFps;
    float avgFrameTime;
    float score;        // 场景分数
};

class SceneTest {
public:
    // 运行单个场景测试
    static SceneResult runScene(EGLDisplay display, EGLSurface surface,
                                 int width, int height, int sceneLevel) {
        SceneResult result;
        result.sceneName = "Scene " + std::to_string(sceneLevel);

        // 根据场景级别设置参数
        int instanceCount;
        int sphereDetail;
        switch (sceneLevel) {
            case 1: // 100万面
                instanceCount = 3000;
                sphereDetail = 24;
                result.triangleCount = 100;
                break;
            case 2: // 300万面
                instanceCount = 8000;
                sphereDetail = 24;
                result.triangleCount = 300;
                break;
            case 3: // 500万面
                instanceCount = 15000;
                sphereDetail = 24;
                result.triangleCount = 500;
                break;
            default:
                instanceCount = 3000;
                sphereDetail = 24;
                result.triangleCount = 100;
        }

        // 顶点着色器
        const std::string vertexShader = R"(#version 320 es
            precision highp float;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;

            uniform mat4 uProjection;
            uniform mat4 uView;
            uniform float uTime;

            layout(location = 2) in vec3 aInstancePos;
            layout(location = 3) in vec4 aInstanceColor;
            layout(location = 4) in float aInstanceScale;

            out vec3 vNormal;
            out vec3 vWorldPos;
            out vec4 vColor;

            void main() {
                float angle = uTime * 0.3 + aInstancePos.x * 0.05 + aInstancePos.y * 0.05;
                float s = sin(angle);
                float c = cos(angle);
                vec3 pos = aPosition;
                pos = vec3(pos.x * c - pos.z * s, pos.y, pos.x * s + pos.z * c);

                vec3 worldPos = pos * aInstanceScale + aInstancePos;
                gl_Position = uProjection * uView * vec4(worldPos, 1.0);
                vNormal = aNormal;
                vWorldPos = worldPos;
                vColor = aInstanceColor;
            }
        )";

        // 片段着色器
        const std::string fragmentShader = R"(#version 320 es
            precision highp float;

            in vec3 vNormal;
            in vec3 vWorldPos;
            in vec4 vColor;

            layout(location = 0) out vec4 fragColor;

            uniform vec3 uLightPos;
            uniform vec3 uCameraPos;

            void main() {
                vec3 N = normalize(vNormal);
                vec3 L = normalize(uLightPos - vWorldPos);
                vec3 V = normalize(uCameraPos - vWorldPos);
                vec3 H = normalize(L + V);

                float NdotL = max(dot(N, L), 0.0);
                float NdotH = max(dot(N, H), 0.0);
                float specular = pow(NdotH, 32.0);
                float ambient = 0.2;

                vec3 color = vColor.rgb * (ambient + NdotL * 0.6) + vec3(specular * 0.2);
                fragColor = vec4(color, 1.0);
            }
        )";

        GLuint program = GLShaderUtils::createProgram(vertexShader, fragmentShader);
        if (!program) {
            result.avgFps = 0;
            result.avgFrameTime = 0;
            result.score = 0;
            return result;
        }

        // 生成球体
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        generateSphere(sphereDetail, sphereDetail, vertices, indices);

        // 创建 VAO/VBO/EBO
        GLuint VAO, VBO, EBO, instanceVBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glGenBuffers(1, &instanceVBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // 实例数据
        struct InstanceData {
            Vec3 position;
            Vec4 color;
            float scale;
        };
        std::vector<InstanceData> instances(instanceCount);
        srand(sceneLevel * 1000);
        float spread = 10.0f + sceneLevel * 5.0f;
        for (int i = 0; i < instanceCount; i++) {
            float x = (rand() % 200 - 100) / 100.0f * spread;
            float y = (rand() % 200 - 100) / 100.0f * spread;
            float z = (rand() % 200 - 100) / 100.0f * spread;
            instances[i].position = {x, y, z};
            float r = (rand() % 100) / 100.0f;
            float g = (rand() % 100) / 100.0f;
            float b = (rand() % 100) / 100.0f;
            instances[i].color = {r, g, b, 1.0f};
            instances[i].scale = 0.03f + (rand() % 100) / 1000.0f;
        }

        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);

        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)sizeof(Vec3));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(Vec3) + sizeof(Vec4)));
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);

        // 矩阵
        Mat4 projection = Mat4::perspective(1.0472f, (float)width / height, 0.1f, 200.0f);
        Mat4 view = Mat4::lookAt({0, 0, (float)(40 + sceneLevel * 10)}, {0, 0, 0}, {0, 1, 0});

        // 渲染测试
        FrameTimer timer;
        float time = 0.0f;
        const int FRAME_COUNT = 300;

        // 预热
        for (int i = 0; i < 30; i++) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection.m);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view.m);
            glUniform1f(glGetUniformLocation(program, "uTime"), time);
            glUniform3f(glGetUniformLocation(program, "uLightPos"), 20.0f, 20.0f, 20.0f);
            glUniform3f(glGetUniformLocation(program, "uCameraPos"), 0.0f, 0.0f, 40.0f);

            glBindVertexArray(VAO);
            glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, instanceCount);

            eglSwapBuffers(display, surface);
            time += 0.016f;
        }

        // 正式测试
        for (int i = 0; i < FRAME_COUNT; i++) {
            timer.beginFrame();

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection.m);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view.m);
            glUniform1f(glGetUniformLocation(program, "uTime"), time);
            glUniform3f(glGetUniformLocation(program, "uLightPos"), 20.0f, 20.0f, 20.0f);
            glUniform3f(glGetUniformLocation(program, "uCameraPos"), 0.0f, 0.0f, 40.0f);

            glBindVertexArray(VAO);
            glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, instanceCount);

            eglSwapBuffers(display, surface);

            timer.endFrame();
            time += 0.016f;
        }

        // 计算结果
        result.avgFps = timer.getAverageFps();
        result.avgFrameTime = timer.getAverageMs();

        // 计算分数: FPS * 三角形权重
        float triangleWeight = result.triangleCount / 100.0f; // 100万面为基准
        result.score = result.avgFps * triangleWeight;

        // 清理
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &instanceVBO);
        glDeleteProgram(program);

        LOGI("Scene %d: %.0f FPS, %.1f ms, Score: %.0f",
             sceneLevel, result.avgFps, result.avgFrameTime, result.score);

        return result;
    }

    // 运行完整基准测试 (所有场景)
    static std::vector<SceneResult> runBenchmark(EGLDisplay display, EGLSurface surface,
                                                   int width, int height) {
        std::vector<SceneResult> results;

        for (int i = 1; i <= 3; i++) {
            LOGI("Running Scene %d...", i);
            SceneResult result = runScene(display, surface, width, height, i);
            results.push_back(result);

            // 场景间短暂休息
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        return results;
    }

    // 计算总分
    static float calculateTotalScore(const std::vector<SceneResult>& results) {
        if (results.empty()) return 0;

        float totalScore = 0;
        for (const auto& result : results) {
            totalScore += result.score;
        }
        return totalScore / results.size();
    }
};

} // namespace GpuBench
