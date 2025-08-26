// GraphicsPipeline.hpp
#pragma once
#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

struct RasterPipelineConfig {
    // external
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    std::vector<VkDescriptorSetLayout> setLayouts;

    // special
    bool hasVertexInput = true;

    // defaults
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkBool32 depthTestEnable = VK_TRUE;
    VkBool32 depthWriteEnable = VK_TRUE;
    VkBool32 blendEnable = VK_FALSE;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkBool32 depthBiasEnable = VK_FALSE;
    VkBool32 sampleShadingEnable = VK_FALSE;
    float     minSampleShading = 1.0f;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    // blend factors (as per your code)
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp     colorBlendOp        = VK_BLEND_OP_ADD;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp     alphaBlendOp        = VK_BLEND_OP_ADD;

    // shader paths
    std::string vertPath;
    std::string fragPath;
};

class GraphicsPipeline : public BasePipeline {
public:
    GraphicsPipeline(VkDevice device, RasterPipelineConfig cfg)
        : BasePipeline(device), cfg(std::move(cfg)) {}

    void setRenderPass(VkRenderPass rp) { cfg.renderPass = rp; }
    void setMsaa(VkSampleCountFlagBits s) { cfg.msaaSamples = s; }
    void setDescLayouts(const std::vector<VkDescriptorSetLayout>& layouts) { cfg.setLayouts = layouts; }

    void create() override;
    void recreate() override { cleanup(); create(); }
    void bind(VkCommandBuffer cmd) const override {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    RasterPipelineConfig cfg;
};

} // namespace AzVulk
