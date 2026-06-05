#include "VulkanDevice.h"
#include <set>
#include <cstring>

namespace GpuBench {

VulkanDevice::~VulkanDevice() {
    shutdown();
}

bool VulkanDevice::initialize(ANativeWindow* window) {
    window_ = window;

    if (!createInstance()) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    if (!createSurface()) {
        LOGE("Failed to create Vulkan surface");
        return false;
    }

    if (!pickPhysicalDevice()) {
        LOGE("Failed to find suitable GPU");
        return false;
    }

    if (!createLogicalDevice()) {
        LOGE("Failed to create logical device");
        return false;
    }

    if (!createCommandPool()) {
        LOGE("Failed to create command pool");
        return false;
    }

    detectCapabilities();
    LOGI("Vulkan device initialized: %s", capabilities_.deviceName.c_str());
    return true;
}

void VulkanDevice::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

bool VulkanDevice::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GpuBench";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "GpuBench Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // 需要的扩展
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    // 可选扩展
    std::vector<const char*> optionalExtensions = {
        VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
    };

    // 验证层
    std::vector<const char*> layers;
#ifdef NDEBUG
    // Release 模式不启用验证层
#else
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = layers.size();
    createInfo.ppEnabledLayerNames = layers.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateInstance failed: %d", result);
        return false;
    }

    return true;
}

bool VulkanDevice::createSurface() {
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = window_;

    VkResult result = vkCreateAndroidSurfaceKHR(instance_, &createInfo, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateAndroidSurfaceKHR failed: %d", result);
        return false;
    }

    return true;
}

bool VulkanDevice::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOGE("No Vulkan-capable GPU found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // 选择最佳设备
    int bestScore = -1;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;

        // 检查是否支持所需队列族
        QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.isComplete()) continue;

        // 检查扩展支持
        if (!checkDeviceExtensionSupport(device)) continue;

        // 检查交换链支持
        SwapChainSupportDetails swapChainSupport;
        uint32_t formatCount, presentModeCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        if (formatCount == 0 || presentModeCount == 0) continue;

        if (score > bestScore) {
            bestScore = score;
            physicalDevice_ = device;
            indices_ = indices;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOGE("No suitable GPU found");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    LOGI("Selected GPU: %s (API: %d.%d.%d)",
         props.deviceName,
         VK_VERSION_MAJOR(props.apiVersion),
         VK_VERSION_MINOR(props.apiVersion),
         VK_VERSION_PATCH(props.apiVersion));

    return true;
}

bool VulkanDevice::createLogicalDevice() {
    std::set<uint32_t> uniqueQueueFamilies = {
        indices_.graphicsFamily.value(),
        indices_.presentFamily.value()
    };

    // 如果有独立的计算队列族
    if (indices_.computeFamily.has_value()) {
        uniqueQueueFamilies.insert(indices_.computeFamily.value());
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // 设备特性 - 启用所有现代特性
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    deviceFeatures.tessellationShader = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.shaderStorageImageReadWithoutFormat = VK_TRUE;
    deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    deviceFeatures.fullDrawIndexUint32 = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.textureCompressionBC = VK_TRUE;
    deviceFeatures.occlusionQueryPrecise = VK_TRUE;
    deviceFeatures.pipelineStatisticsQuery = VK_TRUE;
    deviceFeatures.independentBlend = VK_TRUE;
    deviceFeatures.dualSrcBlend = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.wideLines = VK_TRUE;
    deviceFeatures.largePoints = VK_TRUE;
    deviceFeatures.alphaToOne = VK_TRUE;
    deviceFeatures.multiViewport = VK_TRUE;

    // Vulkan 1.1 特性
    VkPhysicalDeviceVulkan11Features features11{};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.storageBuffer16BitAccess = VK_TRUE;
    features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    features11.storagePushConstant16 = VK_TRUE;
    features11.multiview = VK_TRUE;
    features11.variablePointers = VK_TRUE;
    features11.variablePointersStorageBuffer = VK_TRUE;

    // Vulkan 1.2 特性
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.storageBuffer8BitAccess = VK_TRUE;
    features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    features12.storagePushConstant8 = VK_TRUE;
    features12.shaderBufferInt64Atomics = VK_TRUE;
    features12.shaderSharedInt64Atomics = VK_TRUE;
    features12.shaderFloat16 = VK_TRUE;
    features12.shaderInt8 = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.samplerFilterMinmax = VK_TRUE;
    features12.scalarBlockLayout = VK_TRUE;
    features12.imagelessFramebuffer = VK_TRUE;
    features12.uniformBufferStandardLayout = VK_TRUE;
    features12.separateDepthStencilLayouts = VK_TRUE;
    features12.hostQueryReset = VK_TRUE;
    features12.timelineSemaphore = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.vulkanMemoryModel = VK_TRUE;
    features12.vulkanMemoryModelDeviceScope = VK_TRUE;
    features12.subgroupBroadcastDynamicId = VK_TRUE;

    // Vulkan 1.3 特性
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.robustImageAccess = VK_TRUE;
    features13.inlineUniformBlock = VK_TRUE;
    features13.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_TRUE;
    features13.pipelineCreationCacheControl = VK_TRUE;
    features13.privateData = VK_TRUE;
    features13.shaderDemoteToHelperInvocation = VK_TRUE;
    features13.shaderTerminateInvocation = VK_TRUE;
    features13.subgroupSizeControl = VK_TRUE;
    features13.computeFullSubgroups = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.textureCompressionASTC_HDR = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;
    features13.maintenance4 = VK_TRUE;

    // 链式结构
    features11.pNext = &features12;
    features12.pNext = &features13;

    // 设备扩展
    std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features11;
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateDevice failed: %d", result);
        return false;
    }

    // 获取队列
    vkGetDeviceQueue(device_, indices_.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices_.presentFamily.value(), 0, &presentQueue_);

    if (indices_.computeFamily.has_value()) {
        vkGetDeviceQueue(device_, indices_.computeFamily.value(), 0, &computeQueue_);
    } else {
        computeQueue_ = graphicsQueue_;
    }

    return true;
}

bool VulkanDevice::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices_.graphicsFamily.value();

    VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateCommandPool failed: %d", result);
        return false;
    }

    return true;
}

void VulkanDevice::detectCapabilities() {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;

    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    vkGetPhysicalDeviceFeatures(physicalDevice_, &features);

    capabilities_.deviceName = props.deviceName;
    capabilities_.deviceType = props.deviceType;
    capabilities_.driverVersion = props.driverVersion;

    capabilities_.computeShader = true; // Vulkan 默认支持
    capabilities_.tessellation = features.tessellationShader;
    capabilities_.geometryShader = features.geometryShader;
    capabilities_.multiDrawIndirect = features.multiDrawIndirect;
    capabilities_.drawIndirectFirstInstance = features.drawIndirectFirstInstance;
    capabilities_.shaderStorageBuffer = true; // Vulkan 默认支持
    capabilities_.shaderStorageImageReadWithoutFormat = features.shaderStorageImageReadWithoutFormat;
    capabilities_.shaderStorageImageWriteWithoutFormat = features.shaderStorageImageWriteWithoutFormat;
    capabilities_.subgroupSize = 0; // 需要 Vulkan 1.1
    capabilities_.subgroupOperations = false; // 需要 Vulkan 1.1
    capabilities_.maxPushConstantsSize = props.limits.maxPushConstantsSize;
    capabilities_.maxTextureSize = props.limits.maxImageDimension2D;

    // 计算着色器限制
    for (int i = 0; i < 3; i++) {
        capabilities_.maxComputeWorkGroupCount[i] = props.limits.maxComputeWorkGroupCount[i];
        capabilities_.maxComputeWorkGroupSize[i] = props.limits.maxComputeWorkGroupSize[i];
    }
    capabilities_.maxComputeWorkGroupInvocations = props.limits.maxComputeWorkGroupInvocations;

    LOGI("Vulkan Capabilities:");
    LOGI("  Device: %s", capabilities_.deviceName.c_str());
    LOGI("  Type: %d", capabilities_.deviceType);
    LOGI("  Subgroup Size: %d", capabilities_.subgroupSize);
    LOGI("  Max Texture Size: %d", capabilities_.maxTextureSize);
    LOGI("  Max Push Constants: %d", capabilities_.maxPushConstantsSize);
    LOGI("  Compute Work Group Size: %d x %d x %d",
         capabilities_.maxComputeWorkGroupSize[0],
         capabilities_.maxComputeWorkGroupSize[1],
         capabilities_.maxComputeWorkGroupSize[2]);
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // 图形队列
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // 呈现队列
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        // 计算队列 (优先选择独立计算队列)
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.computeFamily.has_value() ||
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
            }
        }

        if (indices.isComplete()) break;
        i++;
    }

    return indices;
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport;
        uint32_t formatCount, presentModeCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
        swapChainAdequate = formatCount > 0 && presentModeCount > 0;
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails VulkanDevice::querySwapChainSupport() const {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("Failed to find suitable memory type");
    return 0;
}

VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                            VkImageTiling tiling,
                                            VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    LOGE("Failed to find supported format");
    return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanDevice::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkCommandBuffer VulkanDevice::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

VkBuffer VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    VkResult result = vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateBuffer failed: %d", result);
        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    // 启用缓冲区设备地址
    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &flagsInfo;

    result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        LOGE("vkAllocateMemory failed: %d", result);
        vkDestroyBuffer(device_, buffer, nullptr);
        return VK_NULL_HANDLE;
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
    return buffer;
}

void VulkanDevice::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) const {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory, nullptr);
    }
}

VkImage VulkanDevice::createImage(uint32_t width, uint32_t height, VkFormat format,
                                   VkImageTiling tiling, VkImageUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VkResult result = vkCreateImage(device_, &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateImage failed: %d", result);
        return VK_NULL_HANDLE;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    result = vkAllocateMemory(device_, &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        LOGE("vkAllocateMemory failed: %d", result);
        vkDestroyImage(device_, image, nullptr);
        return VK_NULL_HANDLE;
    }

    vkBindImageMemory(device_, image, memory, 0);
    return image;
}

void VulkanDevice::destroyImage(VkImage image, VkDeviceMemory memory) const {
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory, nullptr);
    }
}

VkImageView VulkanDevice::createImageView(VkImage image, VkFormat format,
                                            VkImageAspectFlags aspectFlags) const {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateImageView failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return imageView;
}

void VulkanDevice::destroyImageView(VkImageView imageView) const {
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
}

} // namespace GpuBench
