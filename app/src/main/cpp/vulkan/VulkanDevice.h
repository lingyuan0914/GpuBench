#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <string>
#include <optional>
#include <android/native_window.h>

#include "ShaderUtils.h"

namespace GpuBench {

// 队列族索引
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// 交换链支持详情
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Vulkan 设备管理
class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    bool initialize(ANativeWindow* window);
    void shutdown();

    // 获取器
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkInstance getInstance() const { return instance_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkQueue getPresentQueue() const { return presentQueue_; }
    VkQueue getComputeQueue() const { return computeQueue_; }
    uint32_t getGraphicsQueueFamily() const { return indices_.graphicsFamily.value(); }
    uint32_t getComputeQueueFamily() const { return indices_.computeFamily.value_or(indices_.graphicsFamily.value()); }
    VkCommandPool getCommandPool() const { return commandPool_; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return indices_; }

    // 工具函数
    SwapChainSupportDetails querySwapChainSupport() const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features) const;
    VkFormat findDepthFormat() const;

    // 命令缓冲区
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

    // 创建工具
    VkBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const;
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) const;

    VkImage createImage(uint32_t width, uint32_t height, VkFormat format,
                        VkImageTiling tiling, VkImageUsageFlags usage,
                        VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const;
    void destroyImage(VkImage image, VkDeviceMemory memory) const;

    VkImageView createImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspectFlags) const;
    void destroyImageView(VkImageView imageView) const;

    // 物理设备特性
    struct DeviceCapabilities {
        bool computeShader = false;
        bool tessellation = false;
        bool geometryShader = false;
        bool multiDrawIndirect = false;
        bool drawIndirectFirstInstance = false;
        bool shaderStorageBuffer = false;
        bool shaderStorageImageReadWithoutFormat = false;
        bool shaderStorageImageWriteWithoutFormat = false;
        bool atomicInt64 = false;
        bool variablePointers = false;
        bool subgroupOperations = false;
        bool descriptorIndexing = false;
        bool bufferDeviceAddress = false;
        bool rayTracing = false;
        bool meshShading = false;
        uint32_t maxComputeWorkGroupCount[3] = {0};
        uint32_t maxComputeWorkGroupSize[3] = {0};
        uint32_t maxComputeWorkGroupInvocations = 0;
        uint32_t maxTextureSize = 0;
        uint32_t maxPushConstantsSize = 0;
        uint32_t subgroupSize = 0;
        std::string deviceName;
        std::string driverVersion;
        VkPhysicalDeviceType deviceType;
    } capabilities_;

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    ANativeWindow* window_ = nullptr;

    QueueFamilyIndices indices_;

    bool createInstance();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createCommandPool();
    void detectCapabilities();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
};

} // namespace GpuBench
