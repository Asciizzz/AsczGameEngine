// PLineRaster.hpp
#pragma once
#include "tinyVk/Pipeline/PLineCore.hpp"

namespace tinyVk {

struct RasterCfg {
    // External dependencies - set by pipeline system
    VkRenderPass renderPass = VK_NULL_HANDLE;
    
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    // Vertex input configuration
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // Pipeline state - with sensible defaults
    VkCullModeFlags cullMode            = VK_CULL_MODE_BACK_BIT;
    VkBool32        depthTestEnable     = VK_TRUE;
    VkBool32        depthWriteEnable    = VK_TRUE;
    VkBool32        blendEnable         = VK_FALSE;
    VkFrontFace     frontFace           = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode   polygonMode         = VK_POLYGON_MODE_FILL;
    VkBool32        depthBiasEnable     = VK_FALSE;
    VkBool32        sampleShadingEnable = VK_FALSE;
    float           minSampleShading    = 1.0f;
    VkCompareOp     depthCompareOp      = VK_COMPARE_OP_LESS;

    // Blend factors
    VkBlendFactor   srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp       colorBlendOp        = VK_BLEND_OP_ADD;
    VkBlendFactor   srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor   dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp       alphaBlendOp        = VK_BLEND_OP_ADD;

    // Shader paths
    std::string vrtxPath;
    std::string fragPath;
};

class PLineRaster {
public:
    PLineRaster(VkDevice device, RasterCfg cfg)
        : core(device), cfg(std::move(cfg)) {}

    void withRenderPass(VkRenderPass rp) { cfg.renderPass = rp; }
    void setDescLayouts(const std::vector<VkDescriptorSetLayout>& layouts) { cfg.setLayouts = layouts; }
    void setPushConstantRanges(const std::vector<VkPushConstantRange>& ranges) { cfg.pushConstantRanges = ranges; }

    void create();
    void recreate() { core.cleanup(); create(); }
    void cleanup() { core.cleanup(); }
    
    void bindCmd(VkCommandBuffer cmd) const {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, core.getPipeline());
    }

    void bindSets(VkCommandBuffer cmd, uint32_t firstSet, const VkDescriptorSet* sets, uint32_t count,
                    const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0) const {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                core.getLayout(), firstSet, count, sets,
                                dynamicOffsetCount, dynamicOffsets);
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

    RasterCfg cfg;

private:
    PLineCore core;
};

} // namespace tinyVk
