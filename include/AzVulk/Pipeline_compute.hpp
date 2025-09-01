#pragma once

#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

struct ComputePipelineConfig {
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::string compPath;
};

class PipelineCompute : public PipelineBase {
public:
    PipelineCompute(VkDevice lDevice, ComputePipelineConfig cfg)
        : PipelineBase(lDevice), cfg(std::move(cfg)) {}

    void create() override;
    void recreate() override { cleanup(); create(); }
    void bindCmd(VkCommandBuffer cmd) const override {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    void bindSets(VkCommandBuffer cmd, VkDescriptorSet* sets, uint32_t count) const override {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, count, sets, 0, nullptr);
    }

    ComputePipelineConfig cfg;
};

} // namespace AzVulk
