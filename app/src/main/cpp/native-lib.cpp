#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <memory>

#include "gl/GLEngine.h"
#include "vulkan/VulkanEngine.h"
#include "Benchmark.h"

#define LOG_TAG "GpuBench"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 全局引擎实例
static std::unique_ptr<GpuBench::GLEngine> g_glEngine;
static std::unique_ptr<GpuBench::VulkanEngine> g_vkEngine;
static ANativeWindow* g_window = nullptr;

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

    return JNI_TRUE;
}

// ============= 运行测试 =============

JNIEXPORT jstring JNICALL
Java_com_gpubench_MainActivity_nativeRunGLTest(JNIEnv* env, jobject thiz,
                                                jstring testName, jint frameCount) {
    if (!g_glEngine) {
        LOGE("GL engine not initialized");
        return env->NewStringUTF("{}");
    }

    const char* name = env->GetStringUTFChars(testName, nullptr);
    GpuBench::TestResult result = g_glEngine->run(name, frameCount);
    env->ReleaseStringUTFChars(testName, name);

    // 构建 JSON 结果
    char json[512];
    snprintf(json, sizeof(json),
             "{\"name\":\"%s\",\"api\":\"%s\",\"fps\":%.2f,\"frameTime\":%.3f,"
             "\"triangles\":%llu,\"drawCalls\":%llu}",
             result.name.c_str(), result.api.c_str(),
             result.fps, result.frameTimeMs,
             (unsigned long long)result.triangles,
             (unsigned long long)result.drawCalls);

    return env->NewStringUTF(json);
}

JNIEXPORT jstring JNICALL
Java_com_gpubench_MainActivity_nativeRunVulkanTest(JNIEnv* env, jobject thiz,
                                                     jstring testName, jint frameCount) {
    if (!g_vkEngine) {
        LOGE("Vulkan engine not initialized");
        return env->NewStringUTF("{}");
    }

    const char* name = env->GetStringUTFChars(testName, nullptr);
    GpuBench::TestResult result = g_vkEngine->run(name, frameCount);
    env->ReleaseStringUTFChars(testName, name);

    // 构建 JSON 结果
    char json[512];
    snprintf(json, sizeof(json),
             "{\"name\":\"%s\",\"api\":\"%s\",\"fps\":%.2f,\"frameTime\":%.3f,"
             "\"triangles\":%llu,\"drawCalls\":%llu}",
             result.name.c_str(), result.api.c_str(),
             result.fps, result.frameTimeMs,
             (unsigned long long)result.triangles,
             (unsigned long long)result.drawCalls);

    return env->NewStringUTF(json);
}

// ============= 获取测试列表 =============

JNIEXPORT jobjectArray JNICALL
Java_com_gpubench_MainActivity_nativeGetGLTestNames(JNIEnv* env, jobject thiz) {
    if (!g_glEngine) return nullptr;

    auto names = g_glEngine->getTestNames();
    jobjectArray result = env->NewObjectArray(names.size(), env->FindClass("java/lang/String"), nullptr);

    for (size_t i = 0; i < names.size(); i++) {
        env->SetObjectArrayElement(result, i, env->NewStringUTF(names[i].c_str()));
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_gpubench_MainActivity_nativeGetVulkanTestNames(JNIEnv* env, jobject thiz) {
    if (!g_vkEngine) return nullptr;

    auto names = g_vkEngine->getTestNames();
    jobjectArray result = env->NewObjectArray(names.size(), env->FindClass("java/lang/String"), nullptr);

    for (size_t i = 0; i < names.size(); i++) {
        env->SetObjectArrayElement(result, i, env->NewStringUTF(names[i].c_str()));
    }

    return result;
}

// ============= 清理 =============

JNIEXPORT void JNICALL
Java_com_gpubench_MainActivity_nativeShutdown(JNIEnv* env, jobject thiz) {
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
}

// ============= 获取设备信息 =============

JNIEXPORT jstring JNICALL
Java_com_gpubench_MainActivity_nativeGetDeviceInfo(JNIEnv* env, jobject thiz) {
    char info[1024];

    // 获取 Vulkan 设备信息
    if (g_vkEngine) {
        snprintf(info, sizeof(info),
                 "{\"api\":\"Vulkan 1.3\",\"device\":\"%s\"}",
                 g_vkEngine->getApiName().c_str());
    } else if (g_glEngine) {
        snprintf(info, sizeof(info),
                 "{\"api\":\"OpenGL ES 3.2\",\"device\":\"%s\"}",
                 g_glEngine->getApiName().c_str());
    } else {
        snprintf(info, sizeof(info), "{\"api\":\"Unknown\",\"device\":\"Unknown\"}");
    }

    return env->NewStringUTF(info);
}

} // extern "C"
