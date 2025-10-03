// PipelineRaster.hpp
#pragma once
#include "TinyVK/Pipeline/Pipeline_core.hpp"

namespace TinyVK {

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


};

class PipelineRaster {
public:
    PipelineRaster(VkDevice device, RasterCfg cfg)
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

    void bindSets(VkCommandBuffer cmd, VkDescriptorSet* sets, uint32_t count) const {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, core.getLayout(), 0, count, sets, 0, nullptr);
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
    PipelineCore core;
};

} // namespace TinyVK
