#pragma once

#include <AzVulk/Device.h>

namespace AzVulk {

struct PipelineCfgInfo {
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;

	PipelineCfgInfo(uint32_t width, uint32_t height);
};

class Pipeline {
public:
    Pipeline(AzVulk::Device& dev, const std::string& vertfile, const std::string& fragfile, const PipelineCfgInfo& cfgInfo);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    // static PipelineCfgInfo defaultPipelineCfgInfo(uint32_t width, uint32_t height);

private:
    static std::vector<char> readFile(const std::string& path);
    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

    void createGraphicsPipeline(const std::string& vertfile, const std::string& fragfile, PipelineCfgInfo cfgInfo);


    AzVulk::Device& device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
};

} // namespace AzVulk
