#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"

#include <stdexcept>
#include <cstring>

using namespace Az3D;
using namespace AzVulk;


Renderer::Renderer (Device* vkDevice, SwapChain* swapChain)
: vkDevice(vkDevice), swapChain(swapChain) {
    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    VkDevice lDevice = vkDevice->lDevice;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (inFlightFences[i])           vkDestroyFence(    lDevice, inFlightFences[i],           nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(lDevice, imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(lDevice, renderFinishedSemaphores[i], nullptr);
    }
}

void Renderer::createCommandBuffers() {
    cmdBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkDevice->graphicsPoolWrapper.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) cmdBuffers.size();

    if (vkAllocateCommandBuffers(vkDevice->lDevice, &allocInfo, cmdBuffers.data()) != VK_SUCCESS) {
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
        if (vkCreateSemaphore(vkDevice->lDevice, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(    vkDevice->lDevice, &fenceInfo, nullptr, &inFlightFences[i])          != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-frame sync objects!");
        }
    }

    // per-image render-finished
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(vkDevice->lDevice, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-image renderFinished semaphore!");
        }
    }
}

// Begin frame: handle synchronization, image acquisition, and render pass setup
uint32_t Renderer::beginFrame(VkRenderPass renderPass, bool hasMSAA) {
    // Wait for the current frame's fence
    vkWaitForFences(vkDevice->lDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquire = vkAcquireNextImageKHR(
        vkDevice->lDevice, swapChain->swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    // CRITICAL: If this image is still being used by another frame, wait for that frame's completion
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(vkDevice->lDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Reset the current frame's fence ONLY after we're sure the image is free
    vkResetFences(vkDevice->lDevice, 1, &inFlightFences[currentFrame]);
    
    // Now assign this image to the current frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmdBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChain->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->extent;

    // No MSAA: depth + color, with MSAA: depth + color + resolve
    uint32_t clearValueCount = 2 + hasMSAA;

    std::vector<VkClearValue> clearValues(clearValueCount);
    clearValues[0].depthStencil = {1.0f, 0};
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    if (clearValueCount == 3) {
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    }

    renderPassInfo.clearValueCount = clearValueCount;
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain->extent.width);
    viewport.height = static_cast<float>(swapChain->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffers[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->extent;
    vkCmdSetScissor(cmdBuffers[currentFrame], 0, 1, &scissor);

    return imageIndex;
}


void Renderer::drawStaticInstanceGroup(const ResourceGroup* resGroup, const GlbUBOManager* glbUBO, const PipelineRaster* rPipeline, const StaticInstanceGroup* instanceGroup) const {
    uint32_t instanceCount = instanceGroup->prevInstanceCount;
    if (instanceCount == 0) return;

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];
    rPipeline->bindCmd(currentCmd);

    VkDescriptorSet glbSet = glbUBO->getDescSet(currentFrame);
    VkDescriptorSet matSet = resGroup->getMatDescSet();
    VkDescriptorSet texSet = resGroup->getTexDescSet();
    VkDescriptorSet sets[] = {glbSet, matSet, texSet};
    rPipeline->bindSets(currentCmd, sets, 3);


    size_t modelIndex = instanceGroup->modelIndex;
    const auto& modelVK = resGroup->modelVKs[modelIndex];

    // Draw submeshes
    for (size_t i = 0; i < modelVK.submeshCount(); ++i) {
        uint32_t indexCount = modelVK.submesh_indexCounts[i];
        if (indexCount == 0) continue;

        size_t submeshIndex = modelVK.submeshVK_indices[i];
        uint32_t matIndex  = modelVK.materialVK_indices[i];

        // Push constants: material index (for array indexing in shader)
        rPipeline->pushConstants(currentCmd, VK_SHADER_STAGE_FRAGMENT_BIT, 0, glm::uvec4(matIndex,0,0,0));

        VkBuffer vertexBuffer = resGroup->getSubmeshVertexBuffer(submeshIndex);
        VkBuffer indexBuffer = resGroup->getSubmeshIndexBuffer(submeshIndex);

        VkBuffer instanceBuffer = instanceGroup->bufferData.get();

        VkBuffer buffers[] = { vertexBuffer, instanceBuffer };
        VkDeviceSize offsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(currentCmd, 0, 2, buffers, offsets);
        vkCmdBindIndexBuffer(currentCmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw all submesh instances
        vkCmdDrawIndexed(currentCmd, indexCount, instanceCount, 0, 0, 0);
    }
}


void Renderer::drawSingleInstance(const Az3D::ResourceGroup* resGroup, const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* pipeline, size_t modelIndex) const {
    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];
    pipeline->bindCmd(currentCmd);

    VkDescriptorSet glbSet = glbUBO->getDescSet(currentFrame);
    VkDescriptorSet matSet = resGroup->getMatDescSet();
    VkDescriptorSet texSet = resGroup->getTexDescSet();
    VkDescriptorSet sets[] = {glbSet, matSet, texSet};
    pipeline->bindSets(currentCmd, sets, 3);

    const auto& modelVK = resGroup->modelVKs[modelIndex];

    // Draw submeshes
    for (size_t i = 0; i < modelVK.submeshVK_indices.size(); ++i) {
        uint32_t indexCount = modelVK.submesh_indexCounts[i];
        if (indexCount == 0) continue;

        size_t submeshIndex = modelVK.submeshVK_indices[i];
        uint32_t matIndex  = modelVK.materialVK_indices[i];

        // Push constants: material index (for array indexing in shader)
        pipeline->pushConstants(currentCmd, VK_SHADER_STAGE_FRAGMENT_BIT, 0, glm::uvec4(matIndex,0,0,0));

        VkBuffer vertexBuffer = resGroup->subMeshVertexBuffers[submeshIndex]->get();
        VkBuffer indexBuffer = resGroup->subMeshIndexBuffers[submeshIndex]->get();

        VkBuffer buffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(currentCmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(currentCmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw all submesh instances
        vkCmdDrawIndexed(currentCmd, indexCount, 1, 0, 0, 0);
    }
}


// No instance buffer data yet
void Renderer::drawDemoRig(const ResourceGroup* resGroup, const GlbUBOManager* glbUBO, const PipelineRaster* rPipeline, const Az3D::RigDemo& demo) const {
    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];
    rPipeline->bindCmd(currentCmd);

    VkDescriptorSet glbSet = glbUBO->getDescSet(currentFrame);
    VkDescriptorSet matSet = resGroup->getMatDescSet();
    VkDescriptorSet texSet = resGroup->getTexDescSet();
    VkDescriptorSet rigSet = demo.descSet.get();
    VkDescriptorSet sets[] = {glbSet, matSet, texSet, rigSet};
    rPipeline->bindSets(currentCmd, sets, 4);

    size_t modelIndex = demo.modelIndex;
    const auto& modelVK = resGroup->modelVKs[modelIndex];

    // Draw submeshes
    for (size_t i = 0; i < modelVK.submeshVK_indices.size(); ++i) {
        uint32_t indexCount = modelVK.submesh_indexCounts[i];
        if (indexCount == 0) continue;

        size_t submeshIndex = modelVK.submeshVK_indices[i];
        uint32_t matIndex  = modelVK.materialVK_indices[i];

        // Push constants: material index (for array indexing in shader)
        rPipeline->pushConstants(currentCmd, VK_SHADER_STAGE_FRAGMENT_BIT, 0, glm::uvec4(matIndex,0,0,0));

        VkBuffer vertexBuffer = resGroup->subMeshVertexBuffers[submeshIndex]->get();
        VkBuffer indexBuffer = resGroup->subMeshIndexBuffers[submeshIndex]->get();

        VkBuffer buffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(currentCmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(currentCmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw all submesh instances
        vkCmdDrawIndexed(currentCmd, indexCount, 1, 0, 0, 0);
    }
}


// Sky rendering using dedicated sky pipeline
void Renderer::drawSky(const Az3D::GlbUBOManager* glbUBO, const PipelineRaster* skyPipeline) const {
    // Bind sky pipeline
    skyPipeline->bindCmd(cmdBuffers[currentFrame]);

    // Bind only the global descriptor set (set 0) for sky
    VkDescriptorSet globalSet = glbUBO->getDescSet(currentFrame);
    skyPipeline->bindSets(cmdBuffers[currentFrame], &globalSet, 1);

    // Draw fullscreen triangle (3 vertices, no input)
    vkCmdDraw(cmdBuffers[currentFrame], 3, 1, 0, 0);
}


// End frame: finalize command buffer, submit, and present
void Renderer::endFrame(uint32_t imageIndex) {
    if (imageIndex == UINT32_MAX) return;

    vkCmdEndRenderPass(cmdBuffers[currentFrame]);

    if (vkEndCommandBuffer(cmdBuffers[currentFrame]) != VK_SUCCESS) {
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
    submit.pCommandBuffers      = &cmdBuffers[currentFrame];
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