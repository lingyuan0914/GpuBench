#pragma once

#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <vector>
#include <string>
#include <memory>

#include "Benchmark.h"
#include "VulkanDevice.h"
#include "Timer.h"
#include "MathUtils.h"

namespace GpuBench {

// Vulkan 测试引擎
class VulkanEngine : public Benchmark {
public:
    VulkanEngine();
    ~VulkanEngine() override;

    bool initialize(void* surface, int width, int height) override;
    void shutdown() override;

    TestResult run(const std::string& testName, int frameCount) override;
    std::vector<std::string> getTestNames() const override;
    std::string getApiName() const override { return "Vulkan 1.3"; }

private:
    // 设备和交换链
    std::unique_ptr<VulkanDevice> device_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainFormat_;
    VkExtent2D swapchainExtent_;

    // 深度缓冲
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    // 渲染通道和帧缓冲
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // 同步对象
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;
    const int MAX_FRAMES_IN_FLIGHT = 2;

    ANativeWindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;

    // 初始化函数
    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createSyncObjects();
    void cleanupSwapchain();

    // 测试实现
    TestResult runTriangleTest(int frameCount);
    TestResult runShaderTest(int frameCount);
    TestResult runComputeTest(int frameCount);
    TestResult runTextureTest(int frameCount);

    // 工具函数
    VkShaderModule createShaderModule(const uint32_t* code, size_t size) const;
    VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule module,
                                                       VkShaderStageFlagBits stage) const;

    // 纹理工具
    void transitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
};

} // namespace GpuBench
