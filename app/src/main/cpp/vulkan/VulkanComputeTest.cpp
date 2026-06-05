#include "VulkanEngine.h"

namespace GpuBench {

TestResult VulkanEngine::runComputeTest(int frameCount) {
    LOGI("Running Vulkan Compute Test...");

    // 注意：实际的 Vulkan 计算着色器需要使用 glslangValidator 预编译为 SPIR-V
    // 这里提供框架代码，实际使用时需要添加编译后的着色器

    TestResult result;
    result.name = "Compute Shaders";
    result.api = getApiName();
    result.fps = 0.0f;
    result.frameTimeMs = 0.0f;
    result.triangles = 0;
    result.drawCalls = 0;

    LOGW("Vulkan compute test requires SPIR-V shaders - compile with glslangValidator");
    return result;
}

} // namespace GpuBench
