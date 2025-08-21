// ComputePipeline.hpp
#pragma once
#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

struct ComputePipelineConfig {
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::string compPath;
};

class ComputePipeline : public BasePipeline {
public:
    ComputePipeline(VkDevice device, ComputePipelineConfig cfg)
        : BasePipeline(device), cfg(std::move(cfg)) {}

    void create() override;
    void recreate() override { cleanup(); create(); }
    void bind(VkCommandBuffer cmd) const override {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }

private:
    ComputePipelineConfig cfg;
};

} // namespace AzVulk
