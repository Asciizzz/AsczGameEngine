#pragma once

#include "AzVulk/Pipeline_core.hpp"

namespace AzVulk {

struct ComputePipelineConfig {
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::string compPath;
};

class PipelineCompute {
public:
    PipelineCompute(VkDevice lDevice, ComputePipelineConfig cfg)
        : core(lDevice), cfg(std::move(cfg)) {}

    void create();
    void recreate() { core.cleanup(); create(); }
    void cleanup() { core.cleanup(); }
    
    void bindCmd(VkCommandBuffer cmd) const {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, core.getPipeline());
    }

    void bindSets(VkCommandBuffer cmd, VkDescriptorSet* sets, uint32_t count) const {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, core.getLayout(), 0, count, sets, 0, nullptr);
    }

    // Push constants methods
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const {
        core.pushConstants(cmd, stageFlags, offset, size, pValues);
    }

    template<typename T>
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, const T* data) const {
        core.pushConstants(cmd, stageFlags, offset, data);
    }

    template<typename T>
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, const T& data) const {
        core.pushConstants(cmd, stageFlags, offset, data);
    }

    ComputePipelineConfig cfg;

private:
    PipelineCore core;
};

} // namespace AzVulk
