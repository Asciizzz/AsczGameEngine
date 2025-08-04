#include "AzVulk/TransparencyPipeline.hpp"
#include "AzVulk/ShaderManager.hpp"
#include "Az3D/RenderSystem.hpp"
#include "Az3D/Mesh.hpp"
#include <stdexcept>
#include <array>

namespace AzVulk {
    TransparencyPipeline::TransparencyPipeline(VkDevice device, VkExtent2D swapChainExtent, 
                                             VkRenderPass oitRenderPass, VkDescriptorSetLayout descriptorSetLayout)
        : device(device), swapChainExtent(swapChainExtent) {
        createTransparencyPipeline(oitRenderPass, descriptorSetLayout);
    }

    TransparencyPipeline::~TransparencyPipeline() {
        cleanup();
    }

    void TransparencyPipeline::recreate(VkExtent2D newExtent, VkRenderPass oitRenderPass) {
        swapChainExtent = newExtent;
        cleanup();
        // Note: We'd need to pass descriptorSetLayout again, but for now this is a placeholder
        // createTransparencyPipeline(oitRenderPass, descriptorSetLayout);
    }

    void TransparencyPipeline::createTransparencyPipeline(VkRenderPass oitRenderPass, VkDescriptorSetLayout descriptorSetLayout) {
        // Load transparency shaders
        auto vertShaderCode = ShaderManager::readFile("Shaders/Rasterize/transparent.vert.spv");
        auto fragShaderCode = ShaderManager::readFile("Shaders/Rasterize/transparent.frag.spv");

        // Create a temporary shader manager to create modules
        ShaderManager tempShaderManager(device);
        VkShaderModule vertShaderModule = tempShaderManager.createShaderModule("Shaders/Rasterize/transparent.vert.spv");
        VkShaderModule fragShaderModule = tempShaderManager.createShaderModule("Shaders/Rasterize/transparent.frag.spv");

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

        // Vertex input (same as regular pipeline)
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        // Get vertex binding and attributes (need to include the correct header)
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
        attributeDescriptions.reserve(vertexAttributeDescriptions.size() + instanceAttributeDescriptions.size());
        
        // Add vertex attributes
        for (const auto& attr : vertexAttributeDescriptions) {
            attributeDescriptions.push_back(attr);
        }
        
        // Add instance attributes
        for (const auto& attr : instanceAttributeDescriptions) {
            attributeDescriptions.push_back(attr);
        }

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport and scissor
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE; // No culling for transparency
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth testing (test but don't write)
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;  // Test against existing depth
        depthStencil.depthWriteEnable = VK_FALSE; // Don't write depth (crucial for OIT)
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // Color blending for OIT
        // Accumulation buffer (attachment 0): GL_ONE, GL_ONE (additive)
        VkPipelineColorBlendAttachmentState accumBlendAttachment{};
        accumBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        accumBlendAttachment.blendEnable = VK_TRUE;
        accumBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        accumBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        accumBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        accumBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        accumBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        accumBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // Reveal buffer (attachment 1): GL_ZERO, GL_ONE_MINUS_SRC_ALPHA (multiplicative)
        VkPipelineColorBlendAttachmentState revealBlendAttachment{};
        revealBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
        revealBlendAttachment.blendEnable = VK_TRUE;
        revealBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        revealBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        revealBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        revealBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        revealBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        revealBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        std::array<VkPipelineColorBlendAttachmentState, 2> colorBlendAttachments = {
            accumBlendAttachment, revealBlendAttachment
        };

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();

        // Dynamic states
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create transparency pipeline layout!");
        }

        // Create graphics pipeline
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
        pipelineInfo.renderPass = oitRenderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparencyPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create transparency graphics pipeline!");
        }

        // Clean up shader modules
        tempShaderManager.destroyShaderModule(fragShaderModule);
        tempShaderManager.destroyShaderModule(vertShaderModule);
    }

    void TransparencyPipeline::cleanup() {
        if (transparencyPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, transparencyPipeline, nullptr);
            transparencyPipeline = VK_NULL_HANDLE;
        }

        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
    }
}
