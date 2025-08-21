// GraphicsPipeline.cpp
#include "AzVulk/Pipeline_graphic.hpp"
#include "Az3D/Az3D.hpp"

#include <array>
#include <stdexcept>

using namespace AzVulk;

void GraphicsPipeline::create() {
    // 1) Shader modules
    auto vertCode = readFile(cfg.vertPath);
    auto fragCode = readFile(cfg.fragPath);
    VkShaderModule vert = createModule(vertCode);
    VkShaderModule frag = createModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName  = "main";

    // 2) Vertex input (from your code) :contentReference[oaicite:0]{index=0}
    auto vBind  = Az3D::Vertex::getBindingDescription();
    auto vAttrs = Az3D::Vertex::getAttributeDescriptions();
    auto iBind  = Az3D::Model::getBindingDescription();
    auto iAttrs = Az3D::Model::getAttributeDescriptions();

    std::array<VkVertexInputBindingDescription, 2> bindings{ vBind, iBind };
    std::vector<VkVertexInputAttributeDescription> attrs;
    attrs.insert(attrs.end(), vAttrs.begin(), vAttrs.end());
    attrs.insert(attrs.end(), iAttrs.begin(), iAttrs.end());

    VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
    vin.pVertexBindingDescriptions      = bindings.data();
    vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vin.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = cfg.polygonMode;
    rs.cullMode    = cfg.cullMode;
    rs.frontFace   = cfg.frontFace;
    rs.depthBiasEnable = cfg.depthBiasEnable;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = cfg.msaaSamples;
    ms.sampleShadingEnable  = cfg.sampleShadingEnable;
    ms.minSampleShading     = cfg.minSampleShading;

    VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable  = cfg.depthTestEnable;
    ds.depthWriteEnable = cfg.depthWriteEnable;
    ds.depthCompareOp   = cfg.depthCompareOp;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable           = cfg.blendEnable;
    cba.srcColorBlendFactor   = cfg.srcColorBlendFactor;
    cba.dstColorBlendFactor   = cfg.dstColorBlendFactor;
    cba.colorBlendOp          = cfg.colorBlendOp;
    cba.srcAlphaBlendFactor   = cfg.srcAlphaBlendFactor;
    cba.dstAlphaBlendFactor   = cfg.dstAlphaBlendFactor;
    cba.alphaBlendOp          = cfg.alphaBlendOp;

    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    const std::vector<VkDynamicState> dynStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyn.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dyn.pDynamicStates    = dynStates.data();

    // 3) Layout (from your code) :contentReference[oaicite:1]{index=1}
    VkPipelineLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    lci.setLayoutCount = static_cast<uint32_t>(cfg.setLayouts.size());
    lci.pSetLayouts    = cfg.setLayouts.data();
    if (vkCreatePipelineLayout(device, &lci, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout (graphics)");

    // 4) Graphics pipeline (from your code) :contentReference[oaicite:2]{index=2}
    VkGraphicsPipelineCreateInfo pci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pci.stageCount = 2;
    pci.pStages    = stages;
    pci.pVertexInputState   = &vin;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout     = layout;
    pci.renderPass = cfg.renderPass;
    pci.subpass    = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline");

    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);
}
