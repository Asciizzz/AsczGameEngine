// ComputePipeline.cpp
#include "AzVulk/Pipeline_compute.hpp"
#include <stdexcept>

using namespace AzVulk;

void ComputePipeline::create() {
    auto code = readFile(cfg.compPath);
    VkShaderModule mod = createModule(code);

    VkPipelineLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    lci.setLayoutCount = static_cast<uint32_t>(cfg.setLayouts.size());
    lci.pSetLayouts    = cfg.setLayouts.data();
    if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout (compute)");

    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    ci.stage  = stage;
    ci.layout = layout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create compute pipeline");

    vkDestroyShaderModule(device, mod, nullptr);
}
