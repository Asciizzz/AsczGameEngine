#pragma once

#include <AzVulk/Device.h>
#include <AzVulk/SwapChain.h>
#include <AzVulk/Model.h>


namespace AzVulk {

class Pipeline {
public:
    Pipeline(Device& device, SwapChain& swapChain, const char* vsPath, const char* fsPath);
    ~Pipeline(); void cleanup();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;


    void createGraphicsPipeline(const char* vertShaderPath = "Shaders/hello.vert.spv",
                                const char* fragShaderPath = "Shaders/hello.frag.spv");

    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    Device& device;
    SwapChain& swapChain;
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
};

} // namespace AzVulk