#pragma once

#include <AzVulk/Device.h>
#include <AzVulk/Model.h>


namespace AzVulk {

class Pipeline {
public:
    Pipeline(Device& device, const char* vsPath, const char* fsPath);
    ~Pipeline();

    // Should explicitly call cleanup in case device is destroyed before pipeline
    void cleanup();


    // Prevent copy - Allow move
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = default;
    Pipeline& operator=(Pipeline&&) = default;

    void createGraphicsPipeline(const char* vertShaderPath = "Shaders/hello.vert.spv",
                                const char* fragShaderPath = "Shaders/hello.frag.spv");

    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    Device& device;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
};

} // namespace AzVulk