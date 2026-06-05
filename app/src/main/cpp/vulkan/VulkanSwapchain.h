#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <android/native_window.h>

#include "VulkanDevice.h"

namespace GpuBench {

// Vulkan 交换链管理
class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() = default;

    bool initialize(const VulkanDevice& device, ANativeWindow* window, int width, int height);
    void shutdown(const VulkanDevice& device);

    // 获取器
    VkSwapchainKHR getSwapchain() const { return swapchain_; }
    VkFormat getFormat() const { return format_; }
    VkExtent2D getExtent() const { return extent_; }
    const std::vector<VkImage>& getImages() const { return images_; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews_; }
    uint32_t getImageCount() const { return images_.size(); }

    // 获取下一个图像
    VkResult acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const;

    // 呈现图像
    VkResult present(VkQueue presentQueue, VkSemaphore waitSemaphore, uint32_t imageIndex) const;

private:
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat format_;
    VkExtent2D extent_;
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;

    bool createSwapchain(const VulkanDevice& device, ANativeWindow* window, int width, int height);
    bool createImageViews(const VulkanDevice& device);
    void cleanup(const VulkanDevice& device);
};

} // namespace GpuBench
