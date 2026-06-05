#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>
#include <string>
#include <vector>

#include "Benchmark.h"
#include "ShaderUtils.h"
#include "Timer.h"
#include "MathUtils.h"

namespace GpuBench {

// OpenGL ES 测试引擎
class GLEngine : public Benchmark {
public:
    GLEngine();
    ~GLEngine() override;

    bool initialize(void* surface, int width, int height) override;
    void shutdown() override;

    TestResult run(const std::string& testName, int frameCount) override;
    std::vector<std::string> getTestNames() const override;
    std::string getApiName() const override { return "OpenGL ES 3.2"; }

private:
    // EGL 上下文
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    ANativeWindow* window_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;

    // 测试实现
    TestResult runTriangleTest(int frameCount);
    TestResult runShaderTest(int frameCount);
    TestResult runComputeTest(int frameCount);
    TestResult runTextureTest(int frameCount);

    // 辅助函数
    bool initEGL();
    void shutdownEGL();
    void checkGLError(const std::string& op);
    void printGLInfo();

    // OpenGL 特性检测
    struct GLCapabilities {
        bool computeShader = false;
        bool tessellation = false;
        bool geometryShader = false;
        bool instancedRendering = false;
        bool indirectDraw = false;
        bool multiDrawIndirect = false;
        bool shaderStorageBuffer = false;
        bool atomicCounters = false;
        bool imageLoadStore = false;
        bool transformFeedback = false;
        bool seamlessCubemap = false;
        bool textureBuffer = false;
        bool uniformBufferObject = false;
        int maxTextureSize = 0;
        int maxComputeWorkGroupCount[3] = {0};
        int maxComputeWorkGroupSize[3] = {0};
        int maxComputeWorkGroupInvocations = 0;
    } capabilities_;

    void detectCapabilities();
};

} // namespace GpuBench
