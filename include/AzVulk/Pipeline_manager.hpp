#pragma once

#include "AzVulk/Pipeline_graphic.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace AzVulk {


// Structure for named vertex input configurations
struct NamedVertexInput {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<std::vector<VkVertexInputAttributeDescription>> attributes;
};

// JSON asset structure for pipeline definition
struct PipelineAsset {
    std::string name;
    std::string inheritsFrom = "";  // Optional prototype inheritance
    
    // Shader configuration
    std::string vertexShader = "";
    std::string fragmentShader = "";
    
    // Vertex input
    std::string vertexInput = "StaticInstanced";
    
    // Depth testing
    bool depthTest = true;
    bool depthWrite = true;
    std::string depthCompare = "Less";
    
    // Culling
    std::string cullMode = "Back";
    
    // Blending
    std::string blendMode = "None";
    
    // Polygon mode
    std::string polygonMode = "Fill";
    
    // Push constants
    struct PushConstant {
        std::vector<std::string> stages = {"Fragment"};
        uint32_t offset = 0;
        uint32_t size = 0;
    };
    std::vector<PushConstant> pushConstants;
    
    // Additional properties
    bool depthBias = false;
    bool sampleShading = false;
    float minSampleShading = 1.0f;
    
    // Descriptor layout configuration
    std::vector<std::string> descriptorLayouts;  // e.g., ["global", "material", "texture"]
};

// Pipeline manager class for handling all pipeline configurations
class PipelineManager {
public:
    PipelineManager() = default;

    ~PipelineManager() { clear(); }
    void clear();
    
    // Remove copy/move constructors to prevent issues
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    // Load all pipeline configurations from JSON files
    bool loadPipelinesFromJson(const std::string& jsonFilePath);
    
    // Get a pipeline configuration by name
    RasterCfg getPipelineConfig(const std::string& name) const;
    
    // Check if a pipeline exists
    bool hasPipeline(const std::string& name) const;
    
    // Get all available pipeline names
    std::vector<std::string> getAllPipelineNames() const;
    
    // Initialize all pipelines with common Vulkan objects
    void initializePipelines(
        VkDevice device,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaa,
        const std::unordered_map<std::string, VkDescriptorSetLayout>& namedLayouts,
        const std::unordered_map<std::string, NamedVertexInput>& namedVertexInputs
    );
    
    // Get a pipeline instance by name
    PipelineRaster* getPipeline(const std::string& name) const;
    
    // Check if a pipeline instance exists
    bool hasPipelineInstance(const std::string& name) const;
    
    // Recreate all pipelines (for window resize, etc.)
    void recreateAllPipelines(VkRenderPass newRenderPass);

private:
    // Storage for configurations
    std::unordered_map<std::string, RasterCfg> pipelineConfigs;
    std::unordered_map<std::string, RasterCfg> prototypes;
    std::unordered_map<std::string, PipelineAsset> pipelineAssets;  // Store the original asset data
    
    // Storage for actual pipeline instances
    std::unordered_map<std::string, std::unique_ptr<PipelineRaster>> pipelineInstances;
    
    // JSON parsing helpers
    RasterCfg parseRasterConfig(const PipelineAsset& asset) const;
    CullMode parseCullMode(const std::string& str) const;
    BlendMode parseBlendMode(const std::string& str) const;
    VkCompareOp parseCompareOp(const std::string& str) const;
    VkPolygonMode parsePolygonMode(const std::string& str) const;
    VkShaderStageFlags parseShaderStages(const std::vector<std::string>& stages) const;
};

// Utility macros for easier access - now require a manager instance
#define PIPELINE_GET(manager, name) (manager)->getPipelineConfig(name)
#define PIPELINE_INSTANCE(manager, name) (manager)->getPipeline(name)
#define PIPELINE_INIT(manager, device, renderPass, msaa, namedLayouts, namedVertexInputs) \
    (manager)->initializePipelines(device, renderPass, msaa, namedLayouts, namedVertexInputs)

} // namespace AzVulk
