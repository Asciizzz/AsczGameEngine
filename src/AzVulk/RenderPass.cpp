// RenderPass.cpp
#include "AzVulk/RenderPass.hpp"
#include <stdexcept>
#include <vector>

namespace AzVulk {

    RenderPassConfig RenderPassConfig::createForwardRenderingConfig(VkFormat swapChainFormat, VkSampleCountFlagBits msaaSamples) {
        RenderPassConfig config;
        config.colorFormat = swapChainFormat;
        config.colorSamples = msaaSamples;
        config.hasDepth = true;
        config.depthSamples = msaaSamples;

        if (msaaSamples != VK_SAMPLE_COUNT_1_BIT) {
            config.hasResolve = true;
            config.resolveFormat = swapChainFormat;
        }
        return config;
    }

    RenderPassConfig RenderPassConfig::createDeferredGBufferConfig(VkSampleCountFlagBits msaaSamples) {
        RenderPassConfig config;
        config.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        config.colorSamples = msaaSamples;
        config.hasDepth = true;
        config.depthSamples = msaaSamples;
        config.hasResolve = false;
        return config;
    }

    RenderPassConfig RenderPassConfig::createShadowMapConfig() {
        RenderPassConfig config;
        config.colorFormat = VK_FORMAT_UNDEFINED;
        config.hasDepth = true;
        config.depthFormat = VK_FORMAT_D32_SFLOAT;
        config.depthSamples = VK_SAMPLE_COUNT_1_BIT;
        config.hasResolve = false;
        return config;
    }

    RenderPass::RenderPass(VkDevice device, VkPhysicalDevice physicalDevice, const RenderPassConfig& config)
        : device(device), physicalDevice(physicalDevice), config(config) {
        createRenderPass();
    }

    RenderPass::~RenderPass() {
        cleanup();
    }

    void RenderPass::recreate(const RenderPassConfig& newConfig) {
        cleanup();
        this->config = newConfig;
        createRenderPass();
    }

    void RenderPass::createRenderPass() {
        std::vector<VkAttachmentDescription2> attachments;
        std::vector<VkAttachmentReference2> colorRefs;
        VkAttachmentReference2 depthRef{};
        VkAttachmentReference2 colorResolveRef{};
        VkAttachmentReference2 depthResolveRef{};
        bool useDepthResolve = false;

        uint32_t attachmentIndex = 0;

        // --- Color attachment
        if (config.colorFormat != VK_FORMAT_UNDEFINED) {
            VkAttachmentDescription2 color{};
            color.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            color.format = config.colorFormat;
            color.samples = config.colorSamples;
            color.loadOp = config.colorLoadOp;
            color.storeOp = config.colorStoreOp;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color.finalLayout = config.hasResolve ?
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachments.push_back(color);

            VkAttachmentReference2 ref{};
            ref.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            ref.attachment = attachmentIndex++;
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ref.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorRefs.push_back(ref);
        }

        // --- Depth attachment
        if (config.hasDepth) {
            VkAttachmentDescription2 depth{};
            depth.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            depth.format = config.depthFormat;
            depth.samples = config.depthSamples;
            depth.loadOp = config.depthLoadOp;
            depth.storeOp = config.depthStoreOp;
            depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depth);

            depthRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            depthRef.attachment = attachmentIndex++;
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (config.depthSamples != VK_SAMPLE_COUNT_1_BIT) {
                useDepthResolve = true;

                VkAttachmentDescription2 depthResolve{};
                depthResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                depthResolve.format = config.depthFormat;
                depthResolve.samples = VK_SAMPLE_COUNT_1_BIT;
                depthResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depthResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depthResolve.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                attachments.push_back(depthResolve);

                depthResolveRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                depthResolveRef.attachment = attachmentIndex++;
                depthResolveRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                depthResolveRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            }
        }

        // --- Color resolve (for MSAA color)
        bool hasColorResolve = false;
        if (config.hasResolve) {
            VkAttachmentDescription2 resolve{};
            resolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            resolve.format = config.resolveFormat;
            resolve.samples = VK_SAMPLE_COUNT_1_BIT;
            resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachments.push_back(resolve);

            colorResolveRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            colorResolveRef.attachment = attachmentIndex++;
            colorResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorResolveRef.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            hasColorResolve = true;
        }

        // --- Depth resolve struct

        VkResolveModeFlagBits resolveMode = chooseDepthResolveMode(physicalDevice);
        bool depthResolveSupported = (resolveMode != VK_RESOLVE_MODE_NONE);

        // --- Subpass
        VkSubpassDescription2 subpass{};
        subpass.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
        subpass.pResolveAttachments = hasColorResolve ? &colorResolveRef : nullptr;
        subpass.pDepthStencilAttachment = config.hasDepth ? &depthRef : nullptr;
        subpass.pNext = nullptr; // default

        // --- Depth resolve struct
        VkSubpassDescriptionDepthStencilResolve depthResolveDesc{};
        if (useDepthResolve && depthResolveSupported) {
            depthResolveDesc.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
            depthResolveDesc.depthResolveMode = resolveMode;
            depthResolveDesc.stencilResolveMode = VK_RESOLVE_MODE_NONE;
            depthResolveDesc.pDepthStencilResolveAttachment = &depthResolveRef;

            // Chain it into the subpass
            subpass.pNext = &depthResolveDesc;
        }

        // --- Dependency
        VkSubpassDependency2 dep{};
        dep.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dependencyFlags = 0;
        dep.viewOffset = 0;  // replaces src/dstQueueFamilyIndex

        // --- RenderPass create
        VkRenderPassCreateInfo2 info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dep;

        auto fpCreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR)
            vkGetDeviceProcAddr(device, "vkCreateRenderPass2KHR");
        if (!fpCreateRenderPass2KHR) {
            throw std::runtime_error("vkCreateRenderPass2KHR not available");
        }
        if (fpCreateRenderPass2KHR(device, &info, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void RenderPass::cleanup() {
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }
    }



    VkResolveModeFlagBits RenderPass::chooseDepthResolveMode(VkPhysicalDevice physicalDevice) {
        VkPhysicalDeviceDepthStencilResolveProperties dsResolveProps{};
        dsResolveProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

        VkPhysicalDeviceProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props2.pNext = &dsResolveProps;

        vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

        if (dsResolveProps.supportedDepthResolveModes & VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) {
            return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        }

        // fallback: no hardware resolve support
        return VK_RESOLVE_MODE_NONE;
    }

} // namespace AzVulk
