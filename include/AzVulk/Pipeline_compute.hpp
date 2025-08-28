#pragma once

#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

struct ComputePipelineConfig {
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::string compPath;
};

class ComputePipeline : public BasePipeline {
public:
    ComputePipeline(VkDevice lDevice, ComputePipelineConfig cfg)
        : BasePipeline(lDevice), cfg(std::move(cfg)) {}

    void create() override;
    void recreate() override { cleanup(); create(); }
    void bind(VkCommandBuffer cmd) const override {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

    ComputePipelineConfig cfg;
};

} // namespace AzVulk
