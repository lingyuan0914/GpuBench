#include "VulkanSwapchain.h"
#include <algorithm>

namespace GpuBench {

bool VulkanSwapchain::initialize(const VulkanDevice& device, ANativeWindow* window,
                                  int width, int height) {
    if (!createSwapchain(device, window, width, height)) {
        return false;
    }

    if (!createImageViews(device)) {
        return false;
    }

    return true;
}

void VulkanSwapchain::shutdown(const VulkanDevice& device) {
    cleanup(device);
}

bool VulkanSwapchain::createSwapchain(const VulkanDevice& device, ANativeWindow* window,
                                        int width, int height) {
    SwapChainSupportDetails swapChainSupport = device.querySwapChainSupport();

    // 选择格式
    VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for (const auto& format : swapChainSupport.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // 选择呈现模式
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : swapChainSupport.presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    // 选择交换范围
    VkExtent2D extent;
    if (swapChainSupport.capabilities.currentExtent.width != 0xFFFFFFFF) {
        extent = swapChainSupport.capabilities.currentExtent;
    } else {
        extent.width = std::max(swapChainSupport.capabilities.minImageExtent.width,
                                std::min(swapChainSupport.capabilities.maxImageExtent.width,
                                         static_cast<uint32_t>(width)));
        extent.height = std::max(swapChainSupport.capabilities.minImageExtent.height,
                                 std::min(swapChainSupport.capabilities.maxImageExtent.height,
                                          static_cast<uint32_t>(height)));
    }

    // 图像数量
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // 创建交换链
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // 队列族共享
    QueueFamilyIndices indices = device.getQueueFamilyIndices();
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device.getDevice(), &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed: %d", result);
        return false;
    }

    // 获取交换链图像
    vkGetSwapchainImagesKHR(device.getDevice(), swapchain_, &imageCount, nullptr);
    images_.resize(imageCount);
    vkGetSwapchainImagesKHR(device.getDevice(), swapchain_, &imageCount, images_.data());

    format_ = surfaceFormat.format;
    extent_ = extent;

    LOGI("Swapchain created: %dx%d, %d images, format: %d",
         extent.width, extent.height, imageCount, format_);
    return true;
}

bool VulkanSwapchain::createImageViews(const VulkanDevice& device) {
    imageViews_.resize(images_.size());

    for (size_t i = 0; i < images_.size(); i++) {
        imageViews_[i] = device.createImageView(images_[i], format_, VK_IMAGE_ASPECT_COLOR_BIT);
        if (imageViews_[i] == VK_NULL_HANDLE) {
            return false;
        }
    }

    return true;
}

void VulkanSwapchain::cleanup(const VulkanDevice& device) {
    for (auto imageView : imageViews_) {
        device.destroyImageView(imageView);
    }
    imageViews_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device.getDevice(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

VkResult VulkanSwapchain::acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const {
    return vkAcquireNextImageKHR(VK_NULL_HANDLE, swapchain_, UINT64_MAX,
                                  semaphore, VK_NULL_HANDLE, &imageIndex);
}

VkResult VulkanSwapchain::present(VkQueue presentQueue, VkSemaphore waitSemaphore,
                                    uint32_t imageIndex) const {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;

    VkSwapchainKHR swapchains[] = {swapchain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

} // namespace GpuBench
