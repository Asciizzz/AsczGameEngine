#include "AzVulk/Pipeline.h"

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cassert>

namespace AzVulk {

Pipeline::Pipeline(AzVulk::Device& dev, const std::string& vertfile, const std::string& fragfile, const PipelineCfgInfo& cfgInfo)
: device{dev} {
    createGraphicsPipeline(vertfile, fragfile, cfgInfo);
}

Pipeline::~Pipeline() {
    vkDestroyShaderModule(device.device(), vertShaderModule, nullptr);
    vkDestroyShaderModule(device.device(), fragShaderModule, nullptr);
    vkDestroyPipeline(device.device(), graphicsPipeline, nullptr);
}

std::vector<char> Pipeline::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + std::string(path));
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void Pipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) 
        throw std::runtime_error("Failed to create shader module");
}

void Pipeline::createGraphicsPipeline(const std::string& vertfile, const std::string& fragfile, PipelineCfgInfo cfgInfo) {
    assert(cfgInfo.pipelineLayout != VK_NULL_HANDLE && "Pipeline layout must be set before creating the pipeline");
    assert(cfgInfo.renderPass != VK_NULL_HANDLE && "Render pass must be set before creating the pipeline");
    auto vertShaderCode = readFile(vertfile);
    auto fragShaderCode = readFile(fragfile);

    createShaderModule(vertShaderCode, &vertShaderModule);
    createShaderModule(fragShaderCode, &fragShaderModule);

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";
    shaderStages[0].pNext = nullptr;
    shaderStages[0].flags = 0;
    shaderStages[0].pSpecializationInfo = nullptr;

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";
    shaderStages[1].pNext = nullptr;
    shaderStages[1].flags = 0;
    shaderStages[1].pSpecializationInfo = nullptr;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = 0; // No vertex attributes for now
    vertexInputInfo.vertexBindingDescriptionCount = 0; // No vertex bindings for now
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // No vertex attributes for now
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // No vertex bindings for now

    VkPipelineViewportStateCreateInfo viewportInfo{};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = &cfgInfo.viewport;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = &cfgInfo.scissor;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &cfgInfo.inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &cfgInfo.rasterizationInfo;
    pipelineInfo.pMultisampleState = &cfgInfo.multisampleInfo;
    pipelineInfo.pColorBlendState = &cfgInfo.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &cfgInfo.depthStencilInfo;
    pipelineInfo.pDynamicState = nullptr; // No dynamic states for now
    pipelineInfo.pNext = nullptr; // No additional structures for now
    pipelineInfo.flags = 0; // No special flags for now

    pipelineInfo.layout = cfgInfo.pipelineLayout;
    pipelineInfo.renderPass = cfgInfo.renderPass;
    pipelineInfo.subpass = cfgInfo.subpass;

    pipelineInfo.basePipelineIndex = -1; // No base pipeline for now
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // No base pipeline for now

    if (vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }
}



PipelineCfgInfo::PipelineCfgInfo(uint32_t width, uint32_t height) {
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 3 vertices per triangle, as opposed to strip which connects the last vertex of the previous triangle to the first vertex of the next triangle
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // Squish the screen
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Cut the screen
    scissor.offset = {0, 0};
    scissor.extent = {width, height};

    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.lineWidth = 1.0f;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationInfo.depthBiasEnable = VK_FALSE;
    rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional
    rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
    rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.minSampleShading = 1.0f;           // Optional
    multisampleInfo.pSampleMask = nullptr;             // Optional
    multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
    multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
    colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
    colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
    colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // depthStencilInfo.depthTestEnable = VK_TRUE;
    // depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthTestEnable = VK_FALSE;
    depthStencilInfo.depthWriteEnable = VK_FALSE;

    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds = 0.0f;  // Optional
    depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.front = {};  // Optional
    depthStencilInfo.back = {};   // Optional
}

} // namespace AzVulk
