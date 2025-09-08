// PipelineRaster.hpp
#pragma once
#include "AzVulk/Pipeline_base.hpp"

namespace AzVulk {

// Enhanced enums for fluent API
enum class VertexInput {
    None,
    Static,
    StaticInstanced, 
    Rigged,
    Single
};

enum class CullMode {
    None = VK_CULL_MODE_NONE,
    Front = VK_CULL_MODE_FRONT_BIT,
    Back = VK_CULL_MODE_BACK_BIT,
    FrontAndBack = VK_CULL_MODE_FRONT_AND_BACK
};

enum class BlendMode {
    None,           // No blending
    Alpha,          // Standard alpha blending
    Additive,       // Additive blending
    Multiply        // Multiplicative blending
};

struct RasterCfg {
    // External dependencies - set by pipeline system
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    bool hasMSAA = false;
    
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;

    // Vertex input configuration
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<std::vector<VkVertexInputAttributeDescription>> attributes;

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
    std::string vertPath;
    std::string fragPath;

    // FLUENT API METHODS
    
    // Shader configuration
    RasterCfg& withShaders(const std::string& vertexPath, const std::string& fragmentPath) {
        vertPath = vertexPath;
        fragPath = fragmentPath;
        return *this;
    }

    // Vertex input configuration
    RasterCfg& withVertexInput(VertexInput inputType);
    
    // Direct vertex input configuration from named inputs
    RasterCfg& withVertexInput(const std::vector<VkVertexInputBindingDescription>& inputBindings,
                               const std::vector<std::vector<VkVertexInputAttributeDescription>>& inputAttributes) {
        bindings = inputBindings;
        attributes = inputAttributes;
        return *this;
    }

    // Depth testing
    RasterCfg& withDepthTest(bool enable, VkCompareOp compareOp = VK_COMPARE_OP_LESS) {
        depthTestEnable = enable ? VK_TRUE : VK_FALSE;
        depthCompareOp = compareOp;
        return *this;
    }

    RasterCfg& withDepthWrite(bool enable) {
        depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
        return *this;
    }

    // Culling
    RasterCfg& withCulling(CullMode mode) {
        cullMode = static_cast<VkCullModeFlags>(mode);
        return *this;
    }

    // Blending
    RasterCfg& withBlending(BlendMode mode);

    // Polygon mode
    RasterCfg& withPolygonMode(VkPolygonMode mode) {
        polygonMode = mode;
        return *this;
    }

    // Push constants
    RasterCfg& withPushConstants(VkShaderStageFlags stages, uint32_t offset, uint32_t size) {
        pushConstantRanges.push_back({stages, offset, size});
        return *this;
    }

    // Descriptor set layouts (internal - used by pipeline system)
    RasterCfg& withDescriptorLayouts(const std::vector<VkDescriptorSetLayout>& layouts) {
        setLayouts = layouts;
        return *this;
    }

    // MSAA support
    void setMSAA(VkSampleCountFlagBits samples) {
        msaaSamples = samples;
        hasMSAA = (samples != VK_SAMPLE_COUNT_1_BIT);
    }
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
