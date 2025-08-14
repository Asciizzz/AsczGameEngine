#include "AzVulk/Pipeline.hpp"
#include "AzVulk/ShaderManager.hpp"
#include "AzVulk/Buffer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <vector>
#include <array>

namespace AzVulk {
    // Static factory methods for common configurations
    RasterPipelineConfig RasterPipelineConfig::createOpaqueConfig(VkSampleCountFlagBits msaaSamples) {
        RasterPipelineConfig config;
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.depthTestEnable = VK_TRUE;
        config.depthWriteEnable = VK_TRUE;
        config.blendEnable = VK_FALSE;
        config.msaaSamples = msaaSamples;
        config.sampleShadingEnable = VK_TRUE;
        config.minSampleShading = 0.2f;
        return config;
    }

    RasterPipelineConfig RasterPipelineConfig::createTransparentConfig(VkSampleCountFlagBits msaaSamples) {
        RasterPipelineConfig config;
        config.cullMode = VK_CULL_MODE_BACK_BIT;
        config.depthTestEnable = VK_TRUE;
        config.depthWriteEnable = VK_FALSE; // Don't write to depth buffer for transparency
        config.blendEnable = VK_TRUE;
        config.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        config.msaaSamples = msaaSamples;
        config.sampleShadingEnable = VK_FALSE;
        return config;
    }

    RasterPipelineConfig RasterPipelineConfig::createSkyConfig(VkSampleCountFlagBits msaaSamples) {
        RasterPipelineConfig config;
        config.cullMode = VK_CULL_MODE_NONE;           // No culling for fullscreen quad
        config.depthTestEnable = VK_FALSE;             // Sky is always furthest
        config.depthWriteEnable = VK_FALSE;            // Don't write depth
        config.depthCompareOp = VK_COMPARE_OP_ALWAYS;  // Always pass depth test
        config.blendEnable = VK_FALSE;                 // No blending needed
        config.msaaSamples = msaaSamples;
        config.sampleShadingEnable = VK_FALSE;
        return config;
    }


    Pipeline::Pipeline( VkDevice device, VkRenderPass renderPass,
                                    std::vector<VkDescriptorSetLayout> descriptorSetLayouts,
                                    const char* vertexShaderPath, const char* fragmentShaderPath,
                                    const RasterPipelineConfig& config)
            :   device(device),
                renderPass(renderPass),
                descriptorSetLayouts(descriptorSetLayouts),
                vertexShaderPath(vertexShaderPath),
                fragmentShaderPath(fragmentShaderPath),
                config(config) {
            createGraphicsPipeline();
    }

    Pipeline::~Pipeline() {
        cleanup();
    }

    void Pipeline::recreate(VkRenderPass newRenderPass, const RasterPipelineConfig& newConfig) {
        renderPass = newRenderPass;
        config = newConfig;
        
        cleanup();
        createGraphicsPipeline();
    }

    void Pipeline::createGraphicsPipeline() {
        auto vertShaderCode = ShaderManager::readFile(vertexShaderPath);
        auto fragShaderCode = ShaderManager::readFile(fragmentShaderPath);

        VkShaderModuleCreateInfo vertCreateInfo{};
        vertCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertCreateInfo.codeSize = vertShaderCode.size();
        vertCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

        VkShaderModule vertShaderModule;
        if (vkCreateShaderModule(device, &vertCreateInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex shader module!");
        }

        VkShaderModuleCreateInfo fragCreateInfo{};
        fragCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragCreateInfo.codeSize = fragShaderCode.size();
        fragCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

        VkShaderModule fragShaderModule;
        if (vkCreateShaderModule(device, &fragCreateInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create fragment shader module!");
        }

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Get vertex binding and attributes
        auto vertexBindingDescription = Az3D::Vertex::getBindingDescription();
        auto vertexAttributeDescriptions = Az3D::Vertex::getAttributeDescriptions();

        // Get instance binding and attributes
        auto instanceBindingDescription = Az3D::ModelInstance::getBindingDescription();
        auto instanceAttributeDescriptions = Az3D::ModelInstance::getAttributeDescriptions();

        // Combine binding descriptions
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions = {
            vertexBindingDescription, instanceBindingDescription
        };

        // Combine attribute descriptions
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.insert(attributeDescriptions.end(), 
                                    vertexAttributeDescriptions.begin(), vertexAttributeDescriptions.end());
        attributeDescriptions.insert(attributeDescriptions.end(), 
                                    instanceAttributeDescriptions.begin(), instanceAttributeDescriptions.end());

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = config.polygonMode;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = config.cullMode;
        rasterizer.frontFace = config.frontFace;
        rasterizer.depthBiasEnable = config.depthBiasEnable;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = config.sampleShadingEnable;
        multisampling.rasterizationSamples = config.msaaSamples;
        multisampling.minSampleShading = config.minSampleShading;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =   VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = config.blendEnable;
        colorBlendAttachment.srcColorBlendFactor = config.srcColorBlendFactor;
        colorBlendAttachment.dstColorBlendFactor = config.dstColorBlendFactor;
        colorBlendAttachment.colorBlendOp = config.colorBlendOp;
        colorBlendAttachment.srcAlphaBlendFactor = config.srcAlphaBlendFactor;
        colorBlendAttachment.dstAlphaBlendFactor = config.dstAlphaBlendFactor;
        colorBlendAttachment.alphaBlendOp = config.alphaBlendOp;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = config.depthTestEnable;
        depthStencil.depthWriteEnable = config.depthWriteEnable;
        depthStencil.depthCompareOp = config.depthCompareOp;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();


        // Use two set layouts: set 0 (global), set 1 (material), set 2 (texture)
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void Pipeline::cleanup() {
        // Clean up main pipeline
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        
        // Note: descriptorSetLayout is now managed by DescriptorManager, don't destroy it here
        // Note: renderPass is managed externally, don't destroy it here
    }
}
