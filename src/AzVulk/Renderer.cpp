#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <cstring>

using namespace Az3D;

namespace AzVulk {
        Renderer::Renderer (Device* vkDevice,
                            SwapChain* swapChain,
                            DepthManager* depthManager,
                            GlobalUBOManager* globalUBOManager,
                            ResourceManager* resourceManager) :
        vkDevice(vkDevice),
        swapChain(swapChain),
        depthManager(depthManager),
        globalUBOManager(globalUBOManager),
        resourceManager(resourceManager) {
            createCommandPool();
            createCommandBuffers();
            createSyncObjects();
        }

    Renderer::~Renderer() {
        VkDevice device = vkDevice->device;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (inFlightFences[i])           vkDestroyFence(    device, inFlightFences[i],           nullptr);
            if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
        for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
            if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        }
    }

    void Renderer::createCommandPool() {
        commandPool = vkDevice->createCommandPool("RendererPool", Device::GraphicsType, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }

    void Renderer::createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(vkDevice->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void Renderer::createSyncObjects() {
        swapchainImageCount = swapChain->images.size();

        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        // IMPORTANT: one render-finished semaphore *per swapchain image*
        renderFinishedSemaphores.resize(swapchainImageCount);

        // track which image is tied to which fence
        imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // per-frame acquire + fence
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vkDevice->device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(    vkDevice->device, &fenceInfo, nullptr, &inFlightFences[i])          != VK_SUCCESS) {
                throw std::runtime_error("failed to create per-frame sync objects!");
            }
        }

        // per-image render-finished
        for (size_t i = 0; i < swapchainImageCount; ++i) {
            if (vkCreateSemaphore(vkDevice->device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create per-image renderFinished semaphore!");
            }
        }
    }

    // Begin frame: handle synchronization, image acquisition, and render pass setup
    uint32_t Renderer::beginFrame(Pipeline& pipeline, GlobalUBO& globalUBO) {
        vkWaitForFences(vkDevice->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = UINT32_MAX;
        VkResult acquire = vkAcquireNextImageKHR(
            vkDevice->device, swapChain->swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("failed to acquire swapchain image");

        // If that image is already in flight, wait for its fence
        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(vkDevice->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        vkResetFences(vkDevice->device, 1, &inFlightFences[currentFrame]);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pipeline.config.renderPass;
        renderPassInfo.framebuffer = swapChain->framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain->extent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChain->extent.width);
        viewport.height = static_cast<float>(swapChain->extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain->extent;
        vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        memcpy(globalUBOManager->bufferDatas[currentFrame].mapped, &globalUBO, sizeof(globalUBO));

        return imageIndex;
    }

    // Draw scene with specified pipeline - uses pre-computed mesh mapping from ModelGroup
    void Renderer::drawScene(Pipeline& pipeline, ModelGroup& modelGroup) {
        const Az3D::MaterialManager* matManager = resourceManager->materialManager.get();
        const Az3D::TextureManager* texManager = resourceManager->textureManager.get();
        const Az3D::MeshManager* meshManager = resourceManager->meshManager.get();

        VkDescriptorSet globalSet = globalUBOManager->getDescriptorSet(currentFrame);

        // Bind pipeline once
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

        for (auto& [hash, mapData] : modelGroup.modelMapping) {
            uint32_t instanceCount = static_cast<uint32_t>(mapData.datas.size());
            if (instanceCount == 0) continue;

            mapData.updateBufferData();

            std::pair<size_t, size_t> modelDecode = ModelGroup::Hash::decode(hash);
            size_t meshIndex = modelDecode.first;
            size_t materialIndex = modelDecode.second;

            // Material descriptor set
            VkDescriptorSet materialSet = matManager->getDescriptorSet(materialIndex, currentFrame, MAX_FRAMES_IN_FLIGHT);

            // Texture descriptor set
            size_t textureIndex = matManager->materials[materialIndex]->diffTxtr;
            VkDescriptorSet textureSet = texManager->getDescriptorSet(textureIndex, currentFrame, MAX_FRAMES_IN_FLIGHT);

            std::array<VkDescriptorSet, 3> sets = {globalSet, materialSet, textureSet};
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

            const auto& vertexBufferData = meshManager->vertexGPUBufferDatas[meshIndex];
            const auto& indexBufferData = meshManager->indexGPUBufferDatas[meshIndex];
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
        // return;
        /* 
        Dont worry about the validation warning too much, since the sky is sharing the same shader
        as the main rasterization renderer, it expects the vertex - index - instance buffers to be bound
        in the same way, but its not, because we don't need them.
        */

        // Bind sky pipeline
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipeline.graphicsPipeline);

        // Bind only the global descriptor set (set 0) for sky
        VkDescriptorSet globalSet = globalUBOManager->getDescriptorSet(currentFrame);
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
        depthManager->copyDepthForSampling(commandBuffers[currentFrame], swapChain->extent.width, swapChain->extent.height);

        if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        VkSemaphore waitSemaphores[]   = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        // SIGNAL the per-IMAGE semaphore:
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };

        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = waitSemaphores;
        submit.pWaitDstStageMask    = &waitStage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &commandBuffers[currentFrame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = signalSemaphores;

        if (vkQueueSubmit(vkDevice->graphicsQueue, 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        // PRESENT waits on the same per-IMAGE semaphore:
        VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = signalSemaphores;
        present.swapchainCount     = 1;
        VkSwapchainKHR chains[]    = { swapChain->swapChain };
        present.pSwapchains        = chains;
        present.pImageIndices      = &imageIndex;

        VkResult res = vkQueuePresentKHR(vkDevice->presentQueue, &present);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = true;
        } else if (res != VK_SUCCESS) {
            throw std::runtime_error("failed to present");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

}
