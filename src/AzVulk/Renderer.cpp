#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <unordered_map>
#include <iostream>

namespace AzVulk {
    Renderer::Renderer (const Device& device, SwapChain& swapChain, Buffer& buffer,
                        DescriptorManager& descriptorManager,
                        Az3D::ResourceManager& resourceManager)
        : vulkanDevice(device), swapChain(swapChain), buffer(buffer),
          descriptorManager(descriptorManager),
          resourceManager(resourceManager) {
        
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        createOITRenderTargets();
        createOITRenderPass();
        // createOITFramebuffer(); // TODO: Integrate with depth manager
    }

    Renderer::~Renderer() {
        VkDevice device = vulkanDevice.device;

        cleanupOIT();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
    }

    void Renderer::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = vulkanDevice.queueFamilyIndices;

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(vulkanDevice.device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

    void Renderer::createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(vulkanDevice.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void Renderer::createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vulkanDevice.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    // Begin frame: handle synchronization, image acquisition, and render pass setup
    uint32_t Renderer::beginFrame(RasterPipeline& pipeline, Az3D::Camera& camera) {
        vkWaitForFences(vulkanDevice.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice.device, swapChain.swapChain, 
                                                UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            framebufferResized = true;
            return UINT32_MAX; // Return invalid index to signal error
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(vulkanDevice.device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // Start recording command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pipeline.renderPass;
        renderPassInfo.framebuffer = swapChain.framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain.extent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Set dynamic viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChain.extent.width);
        viewport.height = static_cast<float>(swapChain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain.extent;
        vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        // Update global UBO
        GlobalUBO ubo{};
        ubo.proj = camera.projectionMatrix;
        ubo.view = camera.viewMatrix;

        void* data;
        vkMapMemory(vulkanDevice.device, buffer.uniformBuffersMemory[currentFrame], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(vulkanDevice.device, buffer.uniformBuffersMemory[currentFrame]);

        return imageIndex;
    }

    // Draw scene with specified pipeline - uses pre-computed mesh mapping from ModelGroup
    void Renderer::drawScene(RasterPipeline& pipeline, const Az3D::ModelGroup& modelGroup) {

        // Use the pre-computed mesh mapping directly
        const auto& meshToInstanceIndices = modelGroup.meshMapping.toInstanceIndices;
        const auto& meshToUpdateIndices = modelGroup.meshMapping.toUpdateIndices;
        const auto& meshToPrevInstanceCount = modelGroup.meshMapping.toPrevInstanceCount;
        const auto& modelResources = modelGroup.modelResources;
        const auto& modelInstances = modelGroup.modelInstances;

        // Render all meshes with their instances
        const auto& meshBuffers = buffer.meshBuffers;

        // Build material to meshes mapping for efficient rendering
        std::unordered_map<size_t, std::vector<size_t>> materialToMeshes;

        for (const auto& [meshIndex, instanceIndices] : meshToInstanceIndices) {
            if (instanceIndices.empty()) continue;

            // Update or create the instance buffer for this mesh
            if (meshIndex < meshBuffers.size()) {

                size_t prevInstanceCount = (meshToPrevInstanceCount.count(meshIndex) > 0) 
                    ? meshToPrevInstanceCount.at(meshIndex) : 0;

                if (instanceIndices.size() != prevInstanceCount) {
                    // Buffer size changed - need to recreate
                    vkDeviceWaitIdle(vulkanDevice.device);
                    buffer.createMeshInstanceBuffer(meshIndex, instanceIndices, modelInstances);

                    // Update previous instance count in ModelGroup's mesh mapping
                    const_cast<Az3D::ModelGroup&>(modelGroup).meshMapping.toPrevInstanceCount[meshIndex] = instanceIndices.size();
                } else {
                    // Only update changes
                    auto updateIt = meshToUpdateIndices.find(meshIndex);
                    if (updateIt != meshToUpdateIndices.end() && !updateIt->second.empty()) {
                        // Use the pre-built buffer position mapping
                        const auto& bufferPosMap = modelGroup.meshMapping.toInstanceToBufferPos.at(meshIndex);
                        buffer.updateMeshInstanceBufferSelective(meshIndex, updateIt->second, instanceIndices, modelInstances, bufferPosMap);
                    }
                }

            }

            if (!instanceIndices.empty()) {
                // Get material from the first instance's resource
                const auto& resource = modelResources[modelInstances[instanceIndices[0]].modelResourceIndex];
                materialToMeshes[resource.materialIndex].push_back(meshIndex);
            }
        }

        // Bind pipeline once
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);
        
        // Render by material groups
        for (const auto& [materialIndex, meshIndices] : materialToMeshes) {
            // Bind descriptor set for this material
            VkDescriptorSet descriptorSet = descriptorManager.getDescriptorSet(currentFrame, materialIndex);
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            for (size_t meshIndex : meshIndices) {
                // Get instance count directly from the pre-computed mapping
                size_t instanceCount = 0;
                if (meshToInstanceIndices.find(meshIndex) != meshToInstanceIndices.end()) {
                    instanceCount = meshToInstanceIndices.at(meshIndex).size();
                }
                
                if (meshIndex < meshBuffers.size() && instanceCount > 0) {
                    const auto& meshBuffer = meshBuffers[meshIndex];

                    // Bind vertex buffer
                    VkBuffer vertexBuffers[] = {meshBuffer.vertexBuffer};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);

                    // Bind instance buffer
                    VkBuffer instanceBuffers[] = {meshBuffer.instanceBuffer};
                    VkDeviceSize instanceOffsets[] = {0};
                    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 1, 1, instanceBuffers, instanceOffsets);

                    // Bind index buffer
                    vkCmdBindIndexBuffer(commandBuffers[currentFrame], meshBuffer.indexBuffer, 0, meshBuffer.indexType);

                    // Draw all instances
                    vkCmdDrawIndexed(commandBuffers[currentFrame], meshBuffer.indexCount, static_cast<uint32_t>(instanceCount), 0, 0, 0);
                }
            }
        }
        
        // Clear the update queue after rendering - all changes have been applied
        const_cast<Az3D::ModelGroup&>(modelGroup).clearUpdateQueue();
    }

    // End frame: finalize command buffer, submit, and present
    void Renderer::endFrame(uint32_t imageIndex) {
        if (imageIndex == UINT32_MAX) {
            // Invalid image index, skip presentation
            return;
        }

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        // Submit and present
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(vulkanDevice.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain.swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        VkResult result = vkQueuePresentKHR(vulkanDevice.presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = true;
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // OIT (Order-Independent Transparency) Implementation
    void Renderer::createOITRenderTargets() {
        VkExtent2D extent = swapChain.extent;
        
        // Create accumulation buffer (RGBA16F)
        VkImageCreateInfo accumImageInfo{};
        accumImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        accumImageInfo.imageType = VK_IMAGE_TYPE_2D;
        accumImageInfo.extent.width = extent.width;
        accumImageInfo.extent.height = extent.height;
        accumImageInfo.extent.depth = 1;
        accumImageInfo.mipLevels = 1;
        accumImageInfo.arrayLayers = 1;
        accumImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        accumImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        accumImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        accumImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        accumImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        accumImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(vulkanDevice.device, &accumImageInfo, nullptr, &oitAccumImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT accumulation image!");
        }

        VkMemoryRequirements accumMemRequirements;
        vkGetImageMemoryRequirements(vulkanDevice.device, oitAccumImage, &accumMemRequirements);

        VkMemoryAllocateInfo accumAllocInfo{};
        accumAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        accumAllocInfo.allocationSize = accumMemRequirements.size;
        accumAllocInfo.memoryTypeIndex = vulkanDevice.findMemoryType(accumMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(vulkanDevice.device, &accumAllocInfo, nullptr, &oitAccumImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate OIT accumulation image memory!");
        }

        vkBindImageMemory(vulkanDevice.device, oitAccumImage, oitAccumImageMemory, 0);

        // Create accumulation image view
        VkImageViewCreateInfo accumViewInfo{};
        accumViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        accumViewInfo.image = oitAccumImage;
        accumViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        accumViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        accumViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        accumViewInfo.subresourceRange.baseMipLevel = 0;
        accumViewInfo.subresourceRange.levelCount = 1;
        accumViewInfo.subresourceRange.baseArrayLayer = 0;
        accumViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanDevice.device, &accumViewInfo, nullptr, &oitAccumImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT accumulation image view!");
        }

        // Create reveal buffer (R8)
        VkImageCreateInfo revealImageInfo{};
        revealImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        revealImageInfo.imageType = VK_IMAGE_TYPE_2D;
        revealImageInfo.extent.width = extent.width;
        revealImageInfo.extent.height = extent.height;
        revealImageInfo.extent.depth = 1;
        revealImageInfo.mipLevels = 1;
        revealImageInfo.arrayLayers = 1;
        revealImageInfo.format = VK_FORMAT_R8_UNORM;
        revealImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        revealImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        revealImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        revealImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        revealImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(vulkanDevice.device, &revealImageInfo, nullptr, &oitRevealImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT reveal image!");
        }

        VkMemoryRequirements revealMemRequirements;
        vkGetImageMemoryRequirements(vulkanDevice.device, oitRevealImage, &revealMemRequirements);

        VkMemoryAllocateInfo revealAllocInfo{};
        revealAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        revealAllocInfo.allocationSize = revealMemRequirements.size;
        revealAllocInfo.memoryTypeIndex = vulkanDevice.findMemoryType(revealMemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(vulkanDevice.device, &revealAllocInfo, nullptr, &oitRevealImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate OIT reveal image memory!");
        }

        vkBindImageMemory(vulkanDevice.device, oitRevealImage, oitRevealImageMemory, 0);

        // Create reveal image view
        VkImageViewCreateInfo revealViewInfo{};
        revealViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        revealViewInfo.image = oitRevealImage;
        revealViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        revealViewInfo.format = VK_FORMAT_R8_UNORM;
        revealViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        revealViewInfo.subresourceRange.baseMipLevel = 0;
        revealViewInfo.subresourceRange.levelCount = 1;
        revealViewInfo.subresourceRange.baseArrayLayer = 0;
        revealViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(vulkanDevice.device, &revealViewInfo, nullptr, &oitRevealImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT reveal image view!");
        }
    }

    void Renderer::createOITRenderPass() {
        // Accumulation attachment (RGBA16F)
        VkAttachmentDescription accumAttachment{};
        accumAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        accumAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        accumAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        accumAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        accumAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        accumAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        accumAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        accumAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Reveal attachment (R8)
        VkAttachmentDescription revealAttachment{};
        revealAttachment.format = VK_FORMAT_R8_UNORM;
        revealAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        revealAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        revealAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        revealAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        revealAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        revealAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        revealAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment (reuse existing depth buffer)
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load existing depth
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Attachment references
        VkAttachmentReference accumRef{};
        accumRef.attachment = 0;
        accumRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference revealRef{};
        revealRef.attachment = 1;
        revealRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 2;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentReference, 2> colorAttachments = {accumRef, revealRef};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        subpass.pColorAttachments = colorAttachments.data();
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = {accumAttachment, revealAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(vulkanDevice.device, &renderPassInfo, nullptr, &oitRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT render pass!");
        }
    }

    void Renderer::createOITFramebuffer(VkImageView depthImageView) {
        // TODO: Complete framebuffer creation when depth manager integration is ready
        
        std::array<VkImageView, 3> attachments = {
            oitAccumImageView,
            oitRevealImageView,
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = oitRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChain.extent.width;
        framebufferInfo.height = swapChain.extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanDevice.device, &framebufferInfo, nullptr, &oitFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create OIT framebuffer!");
        }
    }

    void Renderer::cleanupOIT() {
        VkDevice device = vulkanDevice.device;

        if (oitFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, oitFramebuffer, nullptr);
            oitFramebuffer = VK_NULL_HANDLE;
        }

        if (oitRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, oitRenderPass, nullptr);
            oitRenderPass = VK_NULL_HANDLE;
        }

        if (oitAccumImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, oitAccumImageView, nullptr);
            oitAccumImageView = VK_NULL_HANDLE;
        }

        if (oitRevealImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, oitRevealImageView, nullptr);
            oitRevealImageView = VK_NULL_HANDLE;
        }

        if (oitAccumImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, oitAccumImage, nullptr);
            oitAccumImage = VK_NULL_HANDLE;
        }

        if (oitRevealImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, oitRevealImage, nullptr);
            oitRevealImage = VK_NULL_HANDLE;
        }

        if (oitAccumImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, oitAccumImageMemory, nullptr);
            oitAccumImageMemory = VK_NULL_HANDLE;
        }

        if (oitRevealImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, oitRevealImageMemory, nullptr);
            oitRevealImageMemory = VK_NULL_HANDLE;
        }
    }

}
