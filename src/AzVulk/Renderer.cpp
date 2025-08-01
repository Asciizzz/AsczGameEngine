#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <unordered_map>
#include <iostream>

namespace AzVulk {
    Renderer::Renderer (const Device& device, SwapChain& swapChain, RasterPipeline& pipeline, 
                        Buffer& buffer, DescriptorManager& descriptorManager, Az3D::Camera& camera,
                        Az3D::ResourceManager& resourceManager)
        : vulkanDevice(device), swapChain(swapChain), graphicsPipeline(pipeline), buffer(buffer),
          descriptorManager(descriptorManager), billboardDescriptorManager(device, pipeline.billboardDescriptorSetLayout),
          camera(camera), resourceManager(resourceManager), 
          startTime(std::chrono::high_resolution_clock::now()) {
        
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    Renderer::~Renderer() {
        VkDevice device = vulkanDevice.device;

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

    void Renderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicsPipeline.renderPass;
        renderPassInfo.framebuffer = swapChain.framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain.extent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChain.extent.width;
        viewport.height = (float) swapChain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain.extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {buffer.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, buffer.indexBuffer, 0, buffer.indexType);

        vkCmdDrawIndexed(commandBuffer, buffer.indexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    // New render system-based draw function
    void Renderer::drawFrame(RasterPipeline& pipeline,
                            Az3D::RenderSystem& renderSystem,
                            const std::vector<Az3D::Billboard>& billboards) {
        vkWaitForFences(vulkanDevice.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice.device, swapChain.swapChain, 
                                                UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            framebufferResized = true;
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(vulkanDevice.device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        // Update instance buffers using the new rendering system
        auto meshToInstances = renderSystem.groupInstancesByMesh();
        
        for (const auto& [meshIndex, instancePtrs] : meshToInstances) {
            // Extract full ModelInstance objects to get both matrix and color
            std::vector<Az3D::ModelInstance> instances;
            instances.reserve(instancePtrs.size());
            
            for (const Az3D::ModelInstance* instancePtr : instancePtrs) {
                instances.push_back(*instancePtr);
            }
            
            // Update or create the instance buffer for this mesh
            if (meshIndex < buffer.getMeshCount()) {
                const auto& meshBuffers = buffer.getMeshBuffers();
                if (meshIndex < meshBuffers.size()) {
                    if (instances.size() != meshBuffers[meshIndex].instanceCount) {
                        vkDeviceWaitIdle(vulkanDevice.device);
                        buffer.createInstanceBufferForMesh(meshIndex, instances);
                    } else {
                        buffer.updateInstanceBufferForMesh(meshIndex, instances);
                    }
                }
            }
        }

        // Update billboard instance buffer if we have billboards
        if (!billboards.empty()) {
            // Group billboards by texture for optimal rendering
            std::unordered_map<size_t, std::vector<const Az3D::Billboard*>> billboardsByTexture;
            for (const auto& billboard : billboards) {
                billboardsByTexture[billboard.textureIndex].push_back(&billboard);
            }

            std::vector<BillboardInstance> billboardInstances;
            billboardInstances.reserve(billboards.size());

            // Add instances in texture group order
            for (const auto& [textureIndex, textureBillboards] : billboardsByTexture) {
                for (const auto* billboard : textureBillboards) {
                    BillboardInstance instance{};
                    instance.position = billboard->position;
                    instance.width = billboard->width;
                    instance.height = billboard->height;
                    instance.textureIndex = static_cast<uint32_t>(billboard->textureIndex);
                    instance.uvMin = billboard->uvMin;
                    instance.uvMax = billboard->uvMax;
                    instance.color = billboard->color;
                    billboardInstances.push_back(instance);
                }
            }

            // Update billboard instance buffer
            if (billboardInstances.size() != buffer.billboardInstanceCount) {
                buffer.createBillboardInstanceBuffer(billboardInstances);
            } else {
                buffer.updateBillboardInstanceBuffer(billboardInstances);
            }
        }

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

        // Bind main graphics pipeline for models
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

        // Render all meshes with their instances
        const auto& meshBuffers = buffer.getMeshBuffers();
        const auto& modelResources = renderSystem.getModelResources();
        
        // Group mesh indices by material for better batching
        std::unordered_map<size_t, std::vector<size_t>> materialToMeshes;
        for (const auto& [meshIndex, instancePtrs] : meshToInstances) {
            if (!instancePtrs.empty()) {
                // Get material from the first instance's resource
                const auto& resource = modelResources[instancePtrs[0]->modelResourceIndex];
                materialToMeshes[resource.materialIndex].push_back(meshIndex);
            }
        }

        for (const auto& [materialIndex, meshIndices] : materialToMeshes) {
            // Bind descriptor set for this material
            VkDescriptorSet descriptorSet = descriptorManager.getDescriptorSet(currentFrame, materialIndex);
            vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            for (size_t meshIndex : meshIndices) {
                if (meshIndex < meshBuffers.size() && meshBuffers[meshIndex].instanceCount > 0) {
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

                    // Draw instanced
                    vkCmdDrawIndexed(commandBuffers[currentFrame], meshBuffer.indexCount, meshBuffer.instanceCount, 0, 0, 0);
                }
            }
        }

        // Render billboards if we have any
        if (!billboards.empty()) {
            // Switch to billboard pipeline
            vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.billboardGraphicsPipeline);

            // Group billboards by texture for batching
            std::unordered_map<size_t, std::vector<const Az3D::Billboard*>> billboardsByTexture;
            for (const auto& billboard : billboards) {
                billboardsByTexture[billboard.textureIndex].push_back(&billboard);
            }

            uint32_t instanceOffset = 0;
            for (const auto& [textureIndex, textureBillboards] : billboardsByTexture) {
                // Bind descriptor set for this texture
                VkDescriptorSet descriptorSet = billboardDescriptorManager.getDescriptorSet(currentFrame, textureIndex);
                vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      pipeline.billboardPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

                // Bind billboard instance buffer
                VkBuffer billboardBuffers[] = {buffer.billboardInstanceBuffer};
                VkDeviceSize billboardOffsets[] = {instanceOffset * sizeof(BillboardInstance)};
                vkCmdBindVertexBuffers(commandBuffers[currentFrame], 1, 1, billboardBuffers, billboardOffsets);

                // Draw billboards for this texture (4 vertices per billboard, triangle strip)
                vkCmdDraw(commandBuffers[currentFrame], 4, static_cast<uint32_t>(textureBillboards.size()), 0, 0);

                instanceOffset += static_cast<uint32_t>(textureBillboards.size());
            }
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

        result = vkQueuePresentKHR(vulkanDevice.presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = true;
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Renderer::setupBillboardDescriptors() {
        auto& texManager = *resourceManager.textureManager;
        
        // Create billboard descriptor sets for textures
        billboardDescriptorManager.createDescriptorPool(2, texManager.textures.size());

        for (size_t i = 0; i < texManager.textures.size(); ++i) {
            billboardDescriptorManager.createDescriptorSetsForMaterial(buffer.uniformBuffers, sizeof(GlobalUBO), 
                                                                       &texManager.textures[i], i);
        }
    }

}
