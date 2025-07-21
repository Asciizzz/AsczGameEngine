#pragma once

#include <AzVulk/Model.hpp>


namespace AzVulk {

class Pipeline {
public:
    Pipeline(Device& device, const char* vsPath, const char* fsPath, VkRenderPass renderPass);
    void cleanup();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;


    void createGraphicsPipeline(const char* vertShaderPath = "Shaders/hello.vert.spv",
                                const char* fragShaderPath = "Shaders/hello.frag.spv",
                                VkRenderPass renderPass = VK_NULL_HANDLE);

    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    Device& device;

    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
};

} // namespace AzVulk