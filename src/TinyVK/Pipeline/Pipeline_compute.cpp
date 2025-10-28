// PipelineCompute.cpp
#include "tinyVK/Pipeline/Pipeline_compute.hpp"
#include <stdexcept>

using namespace tinyVK;

void PipelineCompute::create() {
    auto code = PipelineCore::readFile(cfg.compPath);
    VkShaderModule mod = core.createModule(code);

    VkPipelineLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    lci.setLayoutCount = static_cast<uint32_t>(cfg.setLayouts.size());
    lci.pSetLayouts    = cfg.setLayouts.data();
    lci.pushConstantRangeCount = static_cast<uint32_t>(cfg.pushConstantRanges.size());
    lci.pPushConstantRanges    = cfg.pushConstantRanges.data();
    
    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(core.getDevice(), &lci, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout (compute)");
    core.setLayout(layout);

    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName  = "main";

    VkComputePipelineCreateInfo ci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    ci.stage  = stage;
    ci.layout = layout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(core.getDevice(), VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create compute pipeline");
    core.setPipeline(pipeline);

    vkDestroyShaderModule(core.getDevice(), mod, nullptr);
}
