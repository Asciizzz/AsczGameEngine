// PipelineRaster.hpp
#pragma once
#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

struct RasterCfg {
    // external
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    bool hasMSAA = false;
    void setMSAA(VkSampleCountFlagBits samples) {
        msaaSamples = samples;
        hasMSAA = (samples != VK_SAMPLE_COUNT_1_BIT);
    }

    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    // legacy vertex input config
    // enum class InputType {
    //     None   = 0,
    //     Static = 1,
    //     Rigged = 2,

    //     // The rest are debug-only
    //     Single = 3
    // } vertexInputType = InputType::Static;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<std::vector<VkVertexInputAttributeDescription>> attributes;


    // defaults
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkBool32        depthTestEnable     = VK_TRUE;
    VkBool32        depthWriteEnable    = VK_TRUE;
    VkBool32        blendEnable         = VK_FALSE;
    VkFrontFace     frontFace           = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode   polygonMode         = VK_POLYGON_MODE_FILL;
    VkBool32        depthBiasEnable     = VK_FALSE;
    VkBool32        sampleShadingEnable = VK_FALSE;
    float           minSampleShading    = 1.0f;
    VkCompareOp     depthCompareOp      = VK_COMPARE_OP_LESS;

    // blend factors (as per your code)
    VkBlendFactor   srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp       colorBlendOp        = VK_BLEND_OP_ADD;
    VkBlendFactor   srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp       alphaBlendOp        = VK_BLEND_OP_ADD;

    // shader paths
    std::string vertPath;
    std::string fragPath;
};

class PipelineRaster : public PipelineBase {
public:
    PipelineRaster(VkDevice lDevice, RasterCfg cfg)
        : PipelineBase(lDevice), cfg(std::move(cfg)) {}

    void setRenderPass(VkRenderPass rp) { cfg.renderPass = rp; }
    void setMsaa(VkSampleCountFlagBits s) { cfg.msaaSamples = s; }
    void setDescLayouts(const std::vector<VkDescriptorSetLayout>& layouts) { cfg.setLayouts = layouts; }
    void setPushConstantRanges(const std::vector<VkPushConstantRange>& ranges) { cfg.pushConstantRanges = ranges; }

    void create() override;
    void recreate() override { cleanup(); create(); }
    void bindCmd(VkCommandBuffer cmd) const override {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    void bindSets(VkCommandBuffer cmd, VkDescriptorSet* sets, uint32_t count) const override {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, count, sets, 0, nullptr);
    }

    RasterCfg cfg;
};

} // namespace AzVulk
