#include "VulkanEngine.h"

namespace GpuBench {

VulkanEngine::VulkanEngine() = default;

VulkanEngine::~VulkanEngine() {
    shutdown();
}

bool VulkanEngine::initialize(void* surface, int width, int height) {
    if (initialized_) return true;

    window_ = static_cast<ANativeWindow*>(surface);
    width_ = width;
    height_ = height;

    // 初始化设备
    device_ = std::make_unique<VulkanDevice>();
    if (!device_->initialize(window_)) {
        LOGE("Failed to initialize Vulkan device");
        return false;
    }

    // 创建交换链
    if (!createSwapchain()) {
        LOGE("Failed to create swapchain");
        return false;
    }

    // 创建深度资源
    if (!createDepthResources()) {
        LOGE("Failed to create depth resources");
        return false;
    }

    // 创建渲染通道
    if (!createRenderPass()) {
        LOGE("Failed to create render pass");
        return false;
    }

    // 创建帧缓冲
    if (!createFramebuffers()) {
        LOGE("Failed to create framebuffers");
        return false;
    }

    // 创建同步对象
    if (!createSyncObjects()) {
        LOGE("Failed to create sync objects");
        return false;
    }

    initialized_ = true;
    LOGI("Vulkan engine initialized: %dx%d", width_, height_);
    return true;
}

void VulkanEngine::shutdown() {
    if (!initialized_) return;

    vkDeviceWaitIdle(device_->getDevice());

    cleanupSwapchain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device_->getDevice(), imageAvailableSemaphores_[i], nullptr);
        vkDestroySemaphore(device_->getDevice(), renderFinishedSemaphores_[i], nullptr);
        vkDestroyFence(device_->getDevice(), inFlightFences_[i], nullptr);
    }

    device_.reset();
    initialized_ = false;
    LOGI("Vulkan engine shut down");
}

bool VulkanEngine::createSwapchain() {
    SwapChainSupportDetails swapChainSupport = device_->querySwapChainSupport();

    // 选择格式
    VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for (const auto& format : swapChainSupport.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    // 选择呈现模式 - 优先 Mailbox (三重缓冲)
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
                                         static_cast<uint32_t>(width_)));
        extent.height = std::max(swapChainSupport.capabilities.minImageExtent.height,
                                 std::min(swapChainSupport.capabilities.maxImageExtent.height,
                                          static_cast<uint32_t>(height_)));
    }

    // 图像数量 - 三重缓冲
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    // 创建交换链
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device_->getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // 队列族共享
    QueueFamilyIndices indices = device_->getQueueFamilyIndices();
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

    VkResult result = vkCreateSwapchainKHR(device_->getDevice(), &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed: %d", result);
        return false;
    }

    // 获取交换链图像
    vkGetSwapchainImagesKHR(device_->getDevice(), swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_->getDevice(), swapchain_, &imageCount, swapchainImages_.data());

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;

    // 创建图像视图
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); i++) {
        swapchainImageViews_[i] = device_->createImageView(
            swapchainImages_[i], swapchainFormat_, VK_IMAGE_ASPECT_COLOR_BIT);
        if (swapchainImageViews_[i] == VK_NULL_HANDLE) {
            return false;
        }
    }

    LOGI("Swapchain created: %dx%d, %d images", extent.width, extent.height, imageCount);
    return true;
}

bool VulkanEngine::createDepthResources() {
    VkFormat depthFormat = device_->findDepthFormat();

    depthImage_ = device_->createImage(
        swapchainExtent_.width, swapchainExtent_.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImageMemory_);

    if (depthImage_ == VK_NULL_HANDLE) return false;

    depthImageView_ = device_->createImageView(depthImage_, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    return depthImageView_ != VK_NULL_HANDLE;
}

bool VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = device_->findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device_->getDevice(), &renderPassInfo, nullptr, &renderPass_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateRenderPass failed: %d", result);
        return false;
    }

    return true;
}

bool VulkanEngine::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());

    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapchainImageViews_[i],
            depthImageView_
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = attachments.size();
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(device_->getDevice(), &framebufferInfo,
                                               nullptr, &framebuffers_[i]);
        if (result != VK_SUCCESS) {
            LOGE("vkCreateFramebuffer failed: %d", result);
            return false;
        }
    }

    return true;
}

bool VulkanEngine::createSyncObjects() {
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device_->getDevice(), &semaphoreInfo, nullptr,
                              &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_->getDevice(), &semaphoreInfo, nullptr,
                              &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_->getDevice(), &fenceInfo, nullptr,
                          &inFlightFences_[i]) != VK_SUCCESS) {
            LOGE("Failed to create synchronization objects");
            return false;
        }
    }

    return true;
}

void VulkanEngine::cleanupSwapchain() {
    for (auto framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_->getDevice(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(device_->getDevice(), renderPass_, nullptr);

    device_->destroyImageView(depthImageView_);
    device_->destroyImage(depthImage_, depthImageMemory_);

    for (auto imageView : swapchainImageViews_) {
        device_->destroyImageView(imageView);
    }

    vkDestroySwapchainKHR(device_->getDevice(), swapchain_, nullptr);
}

VkShaderModule VulkanEngine::createShaderModule(const uint32_t* code, size_t size) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device_->getDevice(), &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateShaderModule failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

std::vector<std::string> VulkanEngine::getTestNames() const {
    return {
        "Triangle Rendering",
        "Complex Shaders",
        "Compute Shaders",
        "Texture Sampling"
    };
}

TestResult VulkanEngine::run(const std::string& testName, int frameCount) {
    if (testName == "Triangle Rendering") return runTriangleTest(frameCount);
    if (testName == "Complex Shaders") return runShaderTest(frameCount);
    if (testName == "Compute Shaders") return runComputeTest(frameCount);
    if (testName == "Texture Sampling") return runTextureTest(frameCount);

    LOGE("Unknown test: %s", testName.c_str());
    return {};
}

void VulkanEngine::transitionImageLayout(VkImage image, VkFormat format,
                                           VkImageLayout oldLayout, VkImageLayout newLayout) const {
    VkCommandBuffer commandBuffer = device_->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage, destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    device_->endSingleTimeCommands(commandBuffer);
}

void VulkanEngine::copyBufferToImage(VkBuffer buffer, VkImage image,
                                       uint32_t width, uint32_t height) const {
    VkCommandBuffer commandBuffer = device_->beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    device_->endSingleTimeCommands(commandBuffer);
}

} // namespace GpuBench
