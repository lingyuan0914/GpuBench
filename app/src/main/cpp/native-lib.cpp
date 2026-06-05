#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

#include "gl/GLEngine.h"
#include "vulkan/VulkanEngine.h"
#include "Benchmark.h"
#include "MathUtils.h"

#define LOG_TAG "GpuBench"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 全局引擎实例
static std::unique_ptr<GpuBench::GLEngine> g_glEngine;
static std::unique_ptr<GpuBench::VulkanEngine> g_vkEngine;
static ANativeWindow* g_window = nullptr;

// 基准测试状态
static std::atomic<bool> g_running{false};
static std::atomic<float> g_currentFps{0.0f};
static std::atomic<float> g_currentFrameTime{0.0f};
static std::atomic<uint64_t> g_triangleCount{0};
static std::thread g_benchmarkThread;

extern "C" {

// ============= 初始化 =============

JNIEXPORT jboolean JNICALL
Java_com_gpubench_MainActivity_nativeInitGL(JNIEnv* env, jobject thiz, jobject surface) {
    if (g_window) {
        ANativeWindow_release(g_window);
    }
    g_window = ANativeWindow_fromSurface(env, surface);

    g_glEngine = std::make_unique<GpuBench::GLEngine>();

    int width = ANativeWindow_getWidth(g_window);
    int height = ANativeWindow_getHeight(g_window);

    if (!g_glEngine->initialize(g_window, width, height)) {
        LOGE("Failed to initialize GL engine");
        g_glEngine.reset();
        return JNI_FALSE;
    }

    // 启动渲染线程
    g_running.store(true);
    g_benchmarkThread = std::thread([]() {
        LOGI("Starting GL Triangle Benchmark...");

        // 顶点着色器 - 实例化渲染
        const std::string vertexShader = R"(#version 320 es
            precision highp float;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;

            uniform mat4 uProjection;
            uniform mat4 uView;
            uniform float uTime;

            // 实例数据
            layout(location = 2) in vec3 aInstancePos;
            layout(location = 3) in vec4 aInstanceColor;
            layout(location = 4) in float aInstanceScale;

            out vec3 vNormal;
            out vec3 vWorldPos;
            out vec4 vColor;

            void main() {
                // 动画旋转
                float angle = uTime * 0.5 + aInstancePos.x * 0.1;
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

        // 片段着色器 - PBR 风格
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
                float specular = pow(NdotH, 64.0);
                float ambient = 0.15;

                vec3 color = vColor.rgb * (ambient + NdotL * 0.7) + vec3(specular * 0.3);
                fragColor = vec4(color, vColor.a);
            }
        )";

        GLuint program = GpuBench::GLShaderUtils::createProgram(vertexShader, fragmentShader);
        if (!program) {
            LOGE("Failed to create triangle shader program");
            return;
        }

        // 生成球体几何数据
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        GpuBench::generateSphere(32, 32, vertices, indices);

        // 创建 VAO/VBO/EBO
        GLuint VAO, VBO, EBO, instanceVBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glGenBuffers(1, &instanceVBO);

        glBindVertexArray(VAO);

        // 顶点数据
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        // 顶点属性
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // 实例数据 - 10000 个球体
        const int INSTANCE_COUNT = 10000;
        struct InstanceData {
            GpuBench::Vec3 position;
            GpuBench::Vec4 color;
            float scale;
        };
        std::vector<InstanceData> instances(INSTANCE_COUNT);
        srand(42);
        for (int i = 0; i < INSTANCE_COUNT; i++) {
            float x = (rand() % 200 - 100) / 10.0f;
            float y = (rand() % 200 - 100) / 10.0f;
            float z = (rand() % 200 - 100) / 10.0f;
            instances[i].position = {x, y, z};
            float r = (rand() % 100) / 100.0f;
            float g = (rand() % 100) / 100.0f;
            float b = (rand() % 100) / 100.0f;
            instances[i].color = {r, g, b, 1.0f};
            instances[i].scale = 0.05f + (rand() % 100) / 500.0f;
        }

        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);

        // 实例属性
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);

        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)sizeof(GpuBench::Vec3));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(GpuBench::Vec3) + sizeof(GpuBench::Vec4)));
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);

        // 矩阵
        GpuBench::Mat4 projection = GpuBench::Mat4::perspective(1.0472f,
            (float)g_glEngine->getWidth() / g_glEngine->getHeight(), 0.1f, 200.0f);
        GpuBench::Mat4 view = GpuBench::Mat4::lookAt({0, 0, 50}, {0, 0, 0}, {0, 1, 0});

        // FPS 计算
        GpuBench::FrameTimer timer;
        float time = 0.0f;
        uint64_t totalTriangles = indices.size() / 3 * INSTANCE_COUNT;
        g_triangleCount.store(totalTriangles);

        LOGI("Starting render loop with %d instances, %llu triangles per frame",
             INSTANCE_COUNT, (unsigned long long)totalTriangles);

        // 主渲染循环
        while (g_running.load()) {
            timer.beginFrame();

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProjection"), 1, GL_FALSE, projection.m);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, view.m);
            glUniform1f(glGetUniformLocation(program, "uTime"), time);
            glUniform3f(glGetUniformLocation(program, "uLightPos"), 20.0f, 20.0f, 20.0f);
            glUniform3f(glGetUniformLocation(program, "uCameraPos"), 0.0f, 0.0f, 50.0f);

            glBindVertexArray(VAO);
            glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr, INSTANCE_COUNT);

            eglSwapBuffers(g_glEngine->getDisplay(), g_glEngine->getSurface());

            timer.endFrame();
            time += 0.016f;

            // 更新 FPS
            g_currentFps.store(timer.getCurrentFps());
            g_currentFrameTime.store(timer.getCurrentMs());
        }

        // 清理
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &instanceVBO);
        glDeleteProgram(program);

        LOGI("GL Triangle Benchmark stopped");
    });

    LOGI("GL engine initialized successfully");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_gpubench_MainActivity_nativeInitVulkan(JNIEnv* env, jobject thiz, jobject surface) {
    if (g_window) {
        ANativeWindow_release(g_window);
    }
    g_window = ANativeWindow_fromSurface(env, surface);

    g_vkEngine = std::make_unique<GpuBench::VulkanEngine>();

    int width = ANativeWindow_getWidth(g_window);
    int height = ANativeWindow_getHeight(g_window);

    if (!g_vkEngine->initialize(g_window, width, height)) {
        LOGE("Failed to initialize Vulkan engine");
        g_vkEngine.reset();
        return JNI_FALSE;
    }

    // TODO: 实现 Vulkan 渲染循环
    LOGI("Vulkan engine initialized successfully");
    return JNI_TRUE;
}

// ============= 清理 =============

JNIEXPORT void JNICALL
Java_com_gpubench_MainActivity_nativeShutdown(JNIEnv* env, jobject thiz) {
    // 停止渲染线程
    g_running.store(false);
    if (g_benchmarkThread.joinable()) {
        g_benchmarkThread.join();
    }

    // 清理引擎
    if (g_glEngine) {
        g_glEngine->shutdown();
        g_glEngine.reset();
    }

    if (g_vkEngine) {
        g_vkEngine->shutdown();
        g_vkEngine.reset();
    }

    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }

    LOGI("Native shutdown complete");
}

// ============= 状态查询 =============

JNIEXPORT jboolean JNICALL
Java_com_gpubench_MainActivity_nativeIsRunning(JNIEnv* env, jobject thiz) {
    return g_running.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloat JNICALL
Java_com_gpubench_MainActivity_nativeGetCurrentFps(JNIEnv* env, jobject thiz) {
    return g_currentFps.load();
}

JNIEXPORT jfloat JNICALL
Java_com_gpubench_MainActivity_nativeGetCurrentFrameTime(JNIEnv* env, jobject thiz) {
    return g_currentFrameTime.load();
}

JNIEXPORT jlong JNICALL
Java_com_gpubench_MainActivity_nativeGetTriangleCount(JNIEnv* env, jobject thiz) {
    return g_triangleCount.load();
}

JNIEXPORT jstring JNICALL
Java_com_gpubench_MainActivity_nativeGetDeviceInfo(JNIEnv* env, jobject thiz) {
    // 获取 GPU 信息
    std::string info;

    if (g_glEngine) {
        info = "OpenGL ES 3.2";
    } else if (g_vkEngine) {
        info = "Vulkan 1.3";
    } else {
        info = "未初始化";
    }

    return env->NewStringUTF(info.c_str());
}

} // extern "C"
