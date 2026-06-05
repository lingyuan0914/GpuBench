#include "VulkanPipeline.h"
#include <array>

namespace GpuBench {

VkPipeline VulkanPipeline::createGraphicsPipeline(
    const VulkanDevice& device,
    const GraphicsPipelineCreateInfo& createInfo) const {

    // 着色器阶段
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = createInfo.vertexShader;
    vertShaderStageInfo.pName = "main";
    shaderStages.push_back(vertShaderStageInfo);

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = createInfo.fragmentShader;
    fragShaderStageInfo.pName = "main";
    shaderStages.push_back(fragShaderStageInfo);

    // 顶点输入
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = createInfo.vertexBindings.size();
    vertexInputInfo.pVertexBindingDescriptions = createInfo.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = createInfo.vertexAttributes.size();
    vertexInputInfo.pVertexAttributeDescriptions = createInfo.vertexAttributes.data();

    // 输入装配
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = createInfo.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 视口和裁剪
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(createInfo.extent.width);
    viewport.height = static_cast<float>(createInfo.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = createInfo.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 光栅化
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = createInfo.polygonMode;
    rasterizer.lineWidth = createInfo.lineWidth;
    rasterizer.cullMode = createInfo.cullMode;
    rasterizer.frontFace = createInfo.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 多重采样
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = createInfo.samples;

    // 深度模板
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = createInfo.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = createInfo.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = createInfo.depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 颜色混合
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                           VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT |
                                           VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = createInfo.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = createInfo.srcColorBlendFactor;
    colorBlendAttachment.dstColorBlendFactor = createInfo.dstColorBlendFactor;
    colorBlendAttachment.colorBlendOp = createInfo.colorBlendOp;
    colorBlendAttachment.srcAlphaBlendFactor = createInfo.srcAlphaBlendFactor;
    colorBlendAttachment.dstAlphaBlendFactor = createInfo.dstAlphaBlendFactor;
    colorBlendAttachment.alphaBlendOp = createInfo.alphaBlendOp;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 动态状态
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = createInfo.dynamicStates.size();
    dynamicState.pDynamicStates = createInfo.dynamicStates.data();

    // 创建图形管道
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = createInfo.pipelineLayout;
    pipelineInfo.renderPass = createInfo.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(device.getDevice(), VK_NULL_HANDLE, 1,
                                                 &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateGraphicsPipelines failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

VkPipeline VulkanPipeline::createComputePipeline(
    const VulkanDevice& device,
    VkShaderModule computeShader,
    VkPipelineLayout pipelineLayout) const {

    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShader;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeShaderStageInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline pipeline;
    VkResult result = vkCreateComputePipelines(device.getDevice(), VK_NULL_HANDLE, 1,
                                                &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateComputePipelines failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return pipeline;
}

VkPipelineLayout VulkanPipeline::createPipelineLayout(
    const VulkanDevice& device,
    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
    const std::vector<VkPushConstantRange>& pushConstants) const {

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
    pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

    VkPipelineLayout pipelineLayout;
    VkResult result = vkCreatePipelineLayout(device.getDevice(), &pipelineLayoutInfo,
                                              nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        LOGE("vkCreatePipelineLayout failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return pipelineLayout;
}

VkDescriptorSetLayout VulkanPipeline::createDescriptorSetLayout(
    const VulkanDevice& device,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) const {

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    VkResult result = vkCreateDescriptorSetLayout(device.getDevice(), &layoutInfo,
                                                    nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateDescriptorSetLayout failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return descriptorSetLayout;
}

VkDescriptorPool VulkanPipeline::createDescriptorPool(
    const VulkanDevice& device,
    const std::vector<VkDescriptorPoolSize>& poolSizes,
    uint32_t maxSets) const {

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    VkDescriptorPool descriptorPool;
    VkResult result = vkCreateDescriptorPool(device.getDevice(), &poolInfo,
                                               nullptr, &descriptorPool);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateDescriptorPool failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return descriptorPool;
}

VkShaderModule VulkanPipeline::createShaderModule(const VulkanDevice& device,
                                                    const uint32_t* code, size_t size) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device.getDevice(), &createInfo,
                                            nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateShaderModule failed: %d", result);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

} // namespace GpuBench
