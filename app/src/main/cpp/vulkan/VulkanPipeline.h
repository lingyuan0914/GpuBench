#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "VulkanDevice.h"

namespace GpuBench {

// Vulkan 管道创建工具
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline() = default;

    // 图形管道创建器
    struct GraphicsPipelineCreateInfo {
        VkShaderModule vertexShader = VK_NULL_HANDLE;
        VkShaderModule fragmentShader = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        // 顶点输入
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttributes;

        // 图元拓扑
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 视口和裁剪
        VkExtent2D extent;
        VkViewport viewport{};
        VkRect2D scissor{};

        // 光栅化
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float lineWidth = 1.0f;

        // 多重采样
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

        // 深度测试
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

        // 混合
        bool blendEnable = false;
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;

        // 动态状态
        std::vector<VkDynamicState> dynamicStates;
    };

    VkPipeline createGraphicsPipeline(const VulkanDevice& device,
                                       const GraphicsPipelineCreateInfo& createInfo) const;

    // 计算管道创建器
    VkPipeline createComputePipeline(const VulkanDevice& device,
                                      VkShaderModule computeShader,
                                      VkPipelineLayout pipelineLayout) const;

    // 管道布局创建器
    VkPipelineLayout createPipelineLayout(const VulkanDevice& device,
                                           const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                           const std::vector<VkPushConstantRange>& pushConstants = {}) const;

    // 描述符集布局创建器
    VkDescriptorSetLayout createDescriptorSetLayout(
        const VulkanDevice& device,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings) const;

    // 描述符池创建器
    VkDescriptorPool createDescriptorPool(
        const VulkanDevice& device,
        const std::vector<VkDescriptorPoolSize>& poolSizes,
        uint32_t maxSets) const;

    // 着色器模块创建器
    VkShaderModule createShaderModule(const VulkanDevice& device,
                                       const uint32_t* code, size_t size) const;
};

} // namespace GpuBench
