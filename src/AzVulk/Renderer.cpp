#include "AzVulk/Renderer.hpp"
#include "AzVulk/DepthManager.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
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
    }

    Renderer::~Renderer() {
        VkDevice device = vulkanDevice.device;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
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
    swapchainImageCount = swapChain.images.size();
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateFence(vulkanDevice.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create fence for a frame!");
            }
        }
    }

    // Begin frame: handle synchronization, image acquisition, and render pass setup
    uint32_t Renderer::beginFrame(RasterPipeline& pipeline, Az3D::Camera& camera) {
        vkWaitForFences(vulkanDevice.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            vulkanDevice.device,
            swapChain.swapChain,
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame], // per-frame semaphore for acquire
            VK_NULL_HANDLE,
            &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            framebufferResized = true;
            return UINT32_MAX;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // If a previous frame is using this image, wait for it to finish
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(vulkanDevice.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        vkResetFences(vulkanDevice.device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pipeline.renderPass;
        renderPassInfo.framebuffer = swapChain.framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain.extent;

        glm::vec3 sunDir = glm::normalize(glm::vec3(-1.0f, -1.0f, 1.0f));
        glm::vec3 skyHorizon = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 skyZenith = glm::vec3(0.1f, 0.2f, 0.9f);
        glm::vec3 viewDir = camera.forward;
        float sky_t = glm::clamp(viewDir.y * 2.2f, 0.0f, 1.0f);
        float skyGradT = glm::pow(sky_t, 0.35f);
        glm::vec3 skyColor = glm::mix(skyHorizon, skyZenith, skyGradT);

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

        GlobalUBO ubo{};
        ubo.proj = camera.projectionMatrix;
        ubo.view = camera.viewMatrix;
        ubo.cameraPos = glm::vec4(camera.pos, glm::radians(camera.fov));
        ubo.cameraForward = glm::vec4(camera.forward, camera.aspectRatio);
        ubo.cameraRight = glm::vec4(camera.right, 0.0f);
        ubo.cameraUp = glm::vec4(camera.up, 0.0f);
        ubo.nearFar = glm::vec4(camera.nearPlane, camera.farPlane, 0.0f, 0.0f);

        memcpy(buffer.uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

        return imageIndex;
    }

    // Draw scene with specified pipeline - uses pre-computed mesh mapping from ModelGroup
    void Renderer::drawScene(RasterPipeline& pipeline, Az3D::ModelGroup& modelGroup) {

        // Use the pre-computed mesh mapping directly
        const auto& meshMapping = modelGroup.meshMapping;
        const auto& modelResources = modelGroup.modelResources;
        const auto& modelInstances = modelGroup.modelInstances;

        // Render all meshes with their instances
        const auto& meshBuffers = buffer.meshBuffers;

        // Build material to meshes mapping for efficient rendering
        std::unordered_map<size_t, std::vector<size_t>> materialToMeshes;

        for (const auto& [meshIndex, meshData] : meshMapping) {
            const auto& instanceIndices = meshData.instanceIndices;
            if (instanceIndices.empty()) continue;

            // Update or create the instance buffer for this mesh
            if (meshIndex < meshBuffers.size()) {

                size_t prevInstanceCount = meshData.prevInstanceCount;

                if (instanceIndices.size() != prevInstanceCount) {
                    // Buffer size changed - need to recreate
                    vkDeviceWaitIdle(vulkanDevice.device);
                    buffer.createMeshInstanceBuffer(meshIndex, const_cast<Az3D::ModelGroup&>(modelGroup).meshMapping[meshIndex], modelInstances);
                } else {
                    // Only update changes
                    if (!meshData.updateIndices.empty()) {
                        buffer.updateMeshInstanceBufferSelective(meshIndex, const_cast<Az3D::ModelGroup&>(modelGroup).meshMapping[meshIndex], modelInstances);
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

            // Bind both global (set 0) and material (set 1) descriptor sets
            VkDescriptorSet globalSet = descriptorManager.globalDynamicDescriptor.getSet(currentFrame);
            VkDescriptorSet materialSet = descriptorManager.materialDynamicDescriptor.getMapSet(materialIndex, currentFrame);
            std::array<VkDescriptorSet, 2> sets = {globalSet, materialSet};
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

            for (size_t meshIndex : meshIndices) {
                // Get instance count directly from the pre-computed mapping
                size_t instanceCount = 0;
                if (meshMapping.find(meshIndex) != meshMapping.end()) {
                    instanceCount = meshMapping.at(meshIndex).instanceIndices.size();
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
        modelGroup.clearUpdateQueue();
    }

    // Sky rendering using dedicated sky pipeline
    void Renderer::drawSky(RasterPipeline& skyPipeline) {
        // Bind sky pipeline
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline.graphicsPipeline);

        // Bind only the global descriptor set (set 0) for sky
        VkDescriptorSet globalSet = descriptorManager.globalDynamicDescriptor.getSet(currentFrame);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    skyPipeline.pipelineLayout, 0, 1, &globalSet, 0, nullptr);

        // Bind dummy vertex buffer for both vertex and instance bindings (Vulkan requires this even if not used)
        VkBuffer dummyBuffers[] = { buffer.dummyVertexBuffer, buffer.dummyVertexBuffer };
        VkDeviceSize dummyOffsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 2, dummyBuffers, dummyOffsets);

        // Draw fullscreen triangle (3 vertices, no input)
        vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);
    }


    // End frame: finalize command buffer, submit, and present
    void Renderer::endFrame(uint32_t imageIndex) {
        if (imageIndex == UINT32_MAX) return;

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

}
