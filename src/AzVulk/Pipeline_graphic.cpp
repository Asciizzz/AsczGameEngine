// PipelineRaster.cpp
#include "AzVulk/Pipeline_graphic.hpp"
#include "Az3D/Az3D.hpp"

#include <stdexcept>

using namespace AzVulk;

RasterCfg& RasterCfg::withBlending(BlendMode mode) {
    switch (mode) {
    case BlendMode::None:
        blendEnable = VK_FALSE;
        break;

    case BlendMode::Alpha:
        blendEnable = VK_TRUE;
        srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendOp = VK_BLEND_OP_ADD;
        srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        alphaBlendOp = VK_BLEND_OP_ADD;
        break;

    case BlendMode::Additive:
        blendEnable = VK_TRUE;
        srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendOp = VK_BLEND_OP_ADD;
        srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        alphaBlendOp = VK_BLEND_OP_ADD;
        break;

    case BlendMode::Multiply:
        blendEnable = VK_TRUE;
        srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendOp = VK_BLEND_OP_ADD;
        srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        alphaBlendOp = VK_BLEND_OP_ADD;
        break;
    }

    return *this;
}

void PipelineRaster::create() {
    // 1. Shader modules - check for empty paths
    if (cfg.vertPath.empty() || cfg.fragPath.empty()) {
        throw std::runtime_error("Pipeline has empty shader paths! Vertex: '" + cfg.vertPath + "', Fragment: '" + cfg.fragPath + "'");
    }
    
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

    // 2. Vertex input
    VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    std::vector<VkVertexInputBindingDescription> bindings = cfg.bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    for (const auto& attrList : cfg.attributes) {
        attrs.insert(attrs.end(), attrList.begin(), attrList.end());
    }

    if (bindings.empty() || attrs.empty()) {
        // No vertex input
        vin.vertexBindingDescriptionCount   = 0;
        vin.pVertexBindingDescriptions      = nullptr;
        vin.vertexAttributeDescriptionCount = 0;
        vin.pVertexAttributeDescriptions    = nullptr;
    } else {
        vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
        vin.pVertexBindingDescriptions      = bindings.data();
        vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vin.pVertexAttributeDescriptions    = attrs.data();
    }

    /* Deprecated
    auto vstaticLayout = Az3D::TinyVertexStatic::getLayout();
    auto vriggedLayout = Az3D::TinyVertexRig::getLayout();
    auto instanceBind  = Az3D::StaticInstance::getBindingDescription();
    auto instanceAttrs = Az3D::StaticInstance::getAttributeDescriptions();

    switch (cfg.vertexInputType) {
    case RasterCfg::InputType::None:
    default:
        break;

    case RasterCfg::InputType::Static:
        {
            auto staticBind = vstaticLayout.getBindingDescription();
            auto staticAttrs = vstaticLayout.getAttributeDescriptions();
            
            bindings = { staticBind, instanceBind };
            attrs.insert(attrs.end(), staticAttrs.begin(), staticAttrs.end());
            attrs.insert(attrs.end(), instanceAttrs.begin(), instanceAttrs.end());

            vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
            vin.pVertexBindingDescriptions      = bindings.data();
            vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
            vin.pVertexAttributeDescriptions    = attrs.data();
        }
        break;

    case RasterCfg::InputType::Rigged:
        {
            auto riggedBind = vriggedLayout.getBindingDescription();
            auto riggedAttrs = vriggedLayout.getAttributeDescriptions();
            
            // For the time being, we will work without instancing
            bindings = { riggedBind };
            attrs.insert(attrs.end(), riggedAttrs.begin(), riggedAttrs.end());

            vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
            vin.pVertexBindingDescriptions      = bindings.data();
            vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
            vin.pVertexAttributeDescriptions    = attrs.data();
        }
        break;

    // Debug input types

        case RasterCfg::InputType::Single:
        {
            auto singleBind = vstaticLayout.getBindingDescription();
            auto singleAttrs = vstaticLayout.getAttributeDescriptions();

            // For the time being, we will work without instancing
            bindings = { singleBind };
            attrs.insert(attrs.end(), singleAttrs.begin(), singleAttrs.end());

            vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
            vin.pVertexBindingDescriptions      = bindings.data();
            vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
            vin.pVertexAttributeDescriptions    = attrs.data();
        }
        break;
    }
    */

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

    // 3. Layout
    VkPipelineLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    lci.setLayoutCount = static_cast<uint32_t>(cfg.setLayouts.size());
    lci.pSetLayouts    = cfg.setLayouts.data();
    lci.pushConstantRangeCount = static_cast<uint32_t>(cfg.pushConstantRanges.size());
    lci.pPushConstantRanges    = cfg.pushConstantRanges.data();
    if (vkCreatePipelineLayout(lDevice, &lci, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout (graphics)");

    // 4. Graphics pipeline
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

    if (vkCreateGraphicsPipelines(lDevice, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline");

    vkDestroyShaderModule(lDevice, frag, nullptr);
    vkDestroyShaderModule(lDevice, vert, nullptr);
}