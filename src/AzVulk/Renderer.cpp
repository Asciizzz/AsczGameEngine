#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <cstring>

using namespace Az3D;

namespace AzVulk {
        Renderer::Renderer (const Device& device, SwapChain& swapChain,
                            GlobalUBOManager& globalUBOManager,
                            ResourceManager& resourceManager,
                            DepthManager& depthManager) :
        vulkanDevice(device), swapChain(swapChain),
        globalUBOManager(globalUBOManager),
        resourceManager(resourceManager),
        depthManager(depthManager) {
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
    uint32_t Renderer::beginFrame(Pipeline& pipeline, GlobalUBO& globalUBO) {
        vkWaitForFences(vulkanDevice.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            vulkanDevice.device,
            swapChain.swapChain,
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame], // per-frame semaphore for acquire
            VK_NULL_HANDLE,
            &imageIndex
        );

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

        memcpy(globalUBOManager.bufferDatas[currentFrame].mapped, &globalUBO, sizeof(globalUBO));

        return imageIndex;
    }

    // Draw scene with specified pipeline - uses pre-computed mesh mapping from ModelGroup
    void Renderer::drawScene(Pipeline& pipeline, ModelGroup& modelGroup) {
        const auto& matManager = *resourceManager.materialManager;
        const auto& texManager = *resourceManager.textureManager;
        const auto& meshManager = *resourceManager.meshManager;

        VkDescriptorSet globalSet = globalUBOManager.getDescriptorSet(currentFrame);

        // Bind pipeline once
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

        for (auto& [hash, mapData] : modelGroup.modelMapping) {
            uint32_t instanceCount = static_cast<uint32_t>(mapData.datas.size());
            if (instanceCount == 0) continue;

            mapData.updateBufferData();

            std::pair<size_t, size_t> modelDecode = ModelGroup::ModelPair::decode(hash);
            size_t meshIndex = modelDecode.first;
            size_t materialIndex = modelDecode.second;

            // Material descriptor set
            VkDescriptorSet materialSet = matManager.getDescriptorSet(materialIndex, currentFrame, MAX_FRAMES_IN_FLIGHT);

            // Texture descriptor set
            size_t textureIndex = matManager.materials[materialIndex]->diffTxtr;
            VkDescriptorSet textureSet = texManager.getDescriptorSet(textureIndex, currentFrame, MAX_FRAMES_IN_FLIGHT);

            std::array<VkDescriptorSet, 3> sets = {globalSet, materialSet, textureSet};
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

            const auto& vertexBufferData = meshManager.vertexBufferDatas[meshIndex];
            const auto& indexBufferData = meshManager.indexBufferDatas[meshIndex];
            const auto& instanceBufferData = mapData.bufferData;

            // Skip if nothing to draw or bad data
            if (indexBufferData.resourceCount == 0) continue;
            if (instanceBufferData.buffer == VK_NULL_HANDLE) continue;

            // Bind vertex + instance buffers in a single call (starting at binding 0)
            VkBuffer buffers[] = { vertexBufferData.buffer, instanceBufferData.buffer };
            VkDeviceSize offsets[] = { 0, 0 };
            vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 2, buffers, offsets);

            // Bind index buffer
            vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBufferData.buffer, 0, VK_INDEX_TYPE_UINT32);

            // Draw all instances
            vkCmdDrawIndexed(commandBuffers[currentFrame], indexBufferData.resourceCount, instanceCount, 0, 0, 0);

        }
    }

    // Sky rendering using dedicated sky pipeline
    void Renderer::drawSky(Pipeline& skyPipeline) {
        // Bind sky pipeline
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline.graphicsPipeline);

        // Bind only the global descriptor set (set 0) for sky
        VkDescriptorSet globalSet = globalUBOManager.getDescriptorSet(currentFrame);
        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                skyPipeline.pipelineLayout, 0, 1, &globalSet, 0, nullptr);

        // Draw fullscreen triangle (3 vertices, no input)
        vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);
    }


    // End frame: finalize command buffer, submit, and present
    void Renderer::endFrame(uint32_t imageIndex) {
        if (imageIndex == UINT32_MAX) return;

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        // Copy depth buffer for sampling (after render pass, before ending command buffer)
        depthManager.copyDepthForSampling(commandBuffers[currentFrame], swapChain.extent.width, swapChain.extent.height);

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
