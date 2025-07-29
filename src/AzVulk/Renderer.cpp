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

    void Renderer::drawFrame(RasterPipeline& pipeline,
                            const std::vector<Az3D::Model>& models, 
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
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

        // Set viewport and scissor (required for dynamic state)
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

        // Update uniform buffer with view, projection matrices (once per frame)
        GlobalUBO ubo{};
        ubo.proj = camera.projectionMatrix;
        ubo.view = camera.viewMatrix;
        memcpy(buffer.uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

        // Group models by material for efficient rendering
        std::unordered_map<size_t, std::vector<const Az3D::Model*>> modelsByMaterial;
        for (const auto& model : models) {
            modelsByMaterial[model.materialIndex].push_back(&model);
        }

        // Multi-material rendering! Render each material group
        for (const auto& [materialIndex, materialModels] : modelsByMaterial) {
            // Get material and its diffuse texture
            auto* material = resourceManager.materialManager->materials[materialIndex].get();
            auto& txtrManager = *resourceManager.textureManager;

            const Az3D::Texture* diffuseTexture = nullptr;
            
            if (material && material->diffTxtr > 0) {
                diffuseTexture = &txtrManager.textures[material->diffTxtr];
            }
            
            // If no texture, use default texture (index 0)
            if (!diffuseTexture) {
                diffuseTexture = &txtrManager.textures[0];
            }

            // Bind material-specific descriptor set (includes uniform buffer + texture)
            try {
                VkDescriptorSet materialDescriptorSet = descriptorManager.getDescriptorSet(currentFrame, materialIndex);
                vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                        graphicsPipeline.pipelineLayout, 0, 1, &materialDescriptorSet, 0, nullptr);

                // Render all models with this material
                for (const Az3D::Model* model : materialModels) {
                    // Get mesh using index 
                    const auto* mesh = resourceManager.meshManager->meshes[model->meshIndex].get();
                    if (!mesh) continue;

                    size_t meshIndex = model->meshIndex;

                    const auto& meshBuffers = buffer.getMeshBuffers();
                    if (meshIndex >= meshBuffers.size()) continue;

                    const auto& meshData = meshBuffers[meshIndex];
                    
                    // Only draw if this mesh has instances
                    if (meshData.instanceCount > 0) {
                        // Bind vertex and instance buffers for this mesh
                        VkBuffer vertexBuffers[] = {meshData.vertexBuffer, meshData.instanceBuffer};
                        VkDeviceSize offsets[] = {0, 0};
                        vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 2, vertexBuffers, offsets);
                        vkCmdBindIndexBuffer(commandBuffers[currentFrame], meshData.indexBuffer, 0, meshData.indexType);
                        
                        // Draw all instances of this mesh type
                        vkCmdDrawIndexed(commandBuffers[currentFrame], meshData.indexCount, meshData.instanceCount, 0, 0, 0);
                    }
                }
            } catch (const std::exception& e) {
                // Fallback: skip this material if descriptor set not found
                continue;
            }
        }

        // Render billboards after models (if provided) in the same render pass
        if (!billboards.empty()) {
            // Group billboards by texture first
            std::unordered_map<size_t, std::vector<const Az3D::Billboard*>> billboardsByTexture;
            for (const auto& billboard : billboards) {
                billboardsByTexture[billboard.textureIndex].push_back(&billboard);
            }

            // Create billboard instances grouped by texture for correct instance buffer ordering
            std::vector<BillboardInstance> billboardInstances;
            billboardInstances.reserve(billboards.size());

            // Add instances in texture group order
            for (const auto& [textureIndex, textureBillboards] : billboardsByTexture) {
                for (const auto* billboard : textureBillboards) {
                    BillboardInstance instance{};
                    instance.pos = billboard->pos;
                    instance.width = billboard->width;
                    instance.height = billboard->height;
                    instance.textureIndex = static_cast<uint32_t>(billboard->textureIndex);
                    instance.uvMin = billboard->uvMin;
                    instance.uvMax = billboard->uvMax;
                    instance.opacity = billboard->opacity;  // CRITICAL: Copy opacity value!
                    billboardInstances.push_back(instance);
                }
            }

            // Update billboard instance buffer
            if (billboardInstances.size() != buffer.billboardInstanceCount) {
                buffer.createBillboardInstanceBuffer(billboardInstances);
            } else {
                buffer.updateBillboardInstanceBuffer(billboardInstances);
            }

            // Bind billboard pipeline (within the same render pass)
            vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.billboardGraphicsPipeline);

            // Render each texture group
            uint32_t instanceOffset = 0;
            for (const auto& [textureIndex, textureBillboards] : billboardsByTexture) {
                auto& txtrManager = *resourceManager.textureManager;
                const Az3D::Texture* texture = nullptr;
                
                if (textureIndex < txtrManager.textures.size()) {
                    texture = &txtrManager.textures[textureIndex];
                }
                
                // Use default texture if not found
                if (!texture) {
                    texture = &txtrManager.textures[0];
                }

                // Bind descriptor set with camera uniforms and texture
                try {
                    VkDescriptorSet descriptorSet = billboardDescriptorManager.getDescriptorSet(currentFrame, textureIndex);
                    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                           pipeline.billboardPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

                    // Bind billboard instance buffer
                    VkBuffer billboardBuffers[] = {buffer.billboardInstanceBuffer};
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 1, 1, billboardBuffers, offsets);

                    // Draw billboards as triangle strip (4 vertices per billboard, instanced)
                    // Use correct instance offset so each texture group draws the right instances
                    vkCmdDraw(commandBuffers[currentFrame], 4, static_cast<uint32_t>(textureBillboards.size()), 0, instanceOffset);
                    
                    instanceOffset += static_cast<uint32_t>(textureBillboards.size());

                } catch (const std::exception& e) {
                    // Skip this texture group if descriptor set not found
                    continue;
                }
            }
        }

        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        // Submit and present (same as original drawFrame)
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
