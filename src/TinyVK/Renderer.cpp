#include "TinyVK/Renderer.hpp"

#include <stdexcept>
#include <cstring>
#include <SDL2/SDL.h>

using namespace TinyVK;


Renderer::Renderer (Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window, uint32_t maxFramesInFlight)
: deviceVK(deviceVK), maxFramesInFlight(maxFramesInFlight) {

    swapChain = MakeUnique<SwapChain>(deviceVK, surface, window);

    depthManager = MakeUnique<DepthManager>(deviceVK);
    depthManager->createDepthResources(swapChain->extent.width, swapChain->extent.height);

    createRenderPasses();
    swapChain->createFramebuffers(mainRenderPass->get(), depthManager->getDepthImageView());

    postProcess = MakeUnique<PostProcess>(deviceVK, swapChain.get(), depthManager.get());
    postProcess->initialize(offscreenRenderPass->get());

    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    VkDevice lDevice = deviceVK->lDevice;

    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (inFlightFences[i])           vkDestroyFence(    lDevice, inFlightFences[i],           nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(lDevice, imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(lDevice, renderFinishedSemaphores[i], nullptr);
    }
}

void Renderer::createCommandBuffers() {
    cmdBuffers.create(deviceVK->lDevice, deviceVK->graphicsPoolWrapper.pool, maxFramesInFlight);
}

void Renderer::createSyncObjects() {
    swapchainImageCount = swapChain->images.size();

    imageAvailableSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);

    // IMPORTANT: one render-finished semaphore *per swapchain image*
    renderFinishedSemaphores.resize(swapchainImageCount);

    // track which image is tied to which fence
    imagesInFlight.resize(swapchainImageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // per-frame acquire + fence
    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(deviceVK->lDevice, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(    deviceVK->lDevice, &fenceInfo, nullptr, &inFlightFences[i])          != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-frame sync objects!");
        }
    }

    // per-image render-finished
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(deviceVK->lDevice, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-image renderFinished semaphore!");
        }
    }
}

void Renderer::recreateRenderPasses() {
    createRenderPasses();

    VkRenderPass mainRenderPassVK = mainRenderPass->get();
    swapChain->createFramebuffers(mainRenderPassVK, depthManager->getDepthImageView());
}

void Renderer::createRenderPasses() {
    VkDevice lDevice = deviceVK->lDevice;
    VkPhysicalDevice pDevice = deviceVK->pDevice;
    
    // Create main render pass for final presentation to swapchain
    auto mainRenderPassConfig = RenderPassConfig::createForwardRenderingConfig(
        swapChain->imageFormat
    );
    mainRenderPass = MakeUnique<RenderPass>(lDevice, pDevice, mainRenderPassConfig);
    
    // Create offscreen render pass for scene rendering (use ping-pong image format)
    auto offscreenRenderPassConfig = RenderPassConfig::createPostProcessConfig(
        VK_FORMAT_R8G8B8A8_UNORM  // Match the ping-pong image format
    );
    offscreenRenderPass = MakeUnique<RenderPass>(lDevice, pDevice, offscreenRenderPassConfig);
}

VkRenderPass Renderer::getMainRenderPass() const {
    return mainRenderPass ? mainRenderPass->get() : VK_NULL_HANDLE;
}

VkRenderPass Renderer::getOffscreenRenderPass() const {
    return offscreenRenderPass ? offscreenRenderPass->get() : VK_NULL_HANDLE;
}

VkExtent2D Renderer::getSwapChainExtent() const {
    return swapChain ? swapChain->extent : VkExtent2D{0, 0};
}

VkCommandBuffer Renderer::getCurrentCommandBuffer() const {
    return cmdBuffers[currentFrame];
}

void Renderer::handleWindowResize(SDL_Window* window) {
    // Wait for device to be idle
    vkDeviceWaitIdle(deviceVK->lDevice);
    
    // Get new window dimensions for depth resources
    int newWidth, newHeight;
    SDL_GetWindowSize(window, &newWidth, &newHeight);
    
    // Recreate depth resources before recreating other resources
    depthManager->createDepthResources(newWidth, newHeight);
    
    // Now safe to cleanup and recreate SwapChain
    swapChain->cleanup();
    swapChain->createSwapChain(window);
    swapChain->createImageViews();
    
    // Recreate render passes and framebuffers
    recreateRenderPasses();
    
    // Use PostProcess's existing recreate method - it will preserve effects internally
    if (postProcess) {
        postProcess->setOffscreenRenderPass(offscreenRenderPass->get());
        postProcess->recreate();
    }
}

// Begin frame: handle synchronization, image acquisition, and render pass setup
uint32_t Renderer::beginFrame() {
    // Wait for the current frame's fence
    vkWaitForFences(deviceVK->lDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquire = vkAcquireNextImageKHR(
        deviceVK->lDevice, swapChain->swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    // CRITICAL: If this image is still being used by another frame, wait for that frame's completion
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(deviceVK->lDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Reset the current frame's fence ONLY after we're sure the image is free
    vkResetFences(deviceVK->lDevice, 1, &inFlightFences[currentFrame]);
    
    // Now assign this image to the current frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(currentCmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Use offscreen render pass instead of swapchain render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = postProcess->getOffscreenRenderPass();
    renderPassInfo.framebuffer = postProcess->getOffscreenFramebuffer(currentFrame);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->extent;

    // Clear values to match render pass attachment order: [color, depth]
    uint32_t clearValueCount = 2;

    std::vector<VkClearValue> clearValues(clearValueCount);
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};    // Color attachment (index 0)
    clearValues[1].depthStencil = {1.0f, 0};               // Depth attachment (index 1)

    renderPassInfo.clearValueCount = clearValueCount;
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(currentCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain->extent.width);
    viewport.height = static_cast<float>(swapChain->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->extent;
    vkCmdSetScissor(currentCmd, 0, 1, &scissor);

    return imageIndex;
}

// Sky rendering using dedicated sky pipeline
void Renderer::drawSky(const TinyProject* project, const PipelineRaster* skyPipeline) const {
    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    // Bind sky pipeline
    skyPipeline->bindCmd(currentCmd);

    // Bind only the global descriptor set (set 0) for sky
    VkDescriptorSet glbSet = project->getGlbDescSet(currentFrame);
    skyPipeline->bindSets(currentCmd, &glbSet, 1);

    // Draw fullscreen triangle (3 vertices, no input)
    vkCmdDraw(currentCmd, 3, 1, 0, 0);
}


void Renderer::drawScene(const TinyProject* project, const PipelineRaster* rPipeline) const {
    const auto& rtNodes = project->getRuntimeNodes();
    const auto& rtMeshRenderIdxs = project->getRuntimeMeshRenderIndices();

    const auto& registry = project->getRegistry();

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];
    rPipeline->bindCmd(currentCmd);

    // Bind only the global descriptor set (set 0) for test pipeline
    VkDescriptorSet glbSet = project->getGlbDescSet(currentFrame);
    rPipeline->bindSets(currentCmd, &glbSet, 1);

    for (uint32_t meshIdx : rtMeshRenderIdxs) {
        const auto& rtNode = rtNodes[meshIdx];

        const auto& transform = rtNode->globalTransform;

        const auto& regNode = registry->get<TinyRNode>(rtNode->rHandle);
        const auto& regMeshData = regNode->get<TinyNode::MeshRender>();

        const auto& regMesh = registry->get<TinyRMesh>(regMeshData->mesh);
        const auto& submeshes = regMesh->submeshes;

        const auto& submeshMats = regMeshData->submeshMats;

        // Normally you'd bind the material based on the mesh node, but because we haven't setup the bind descriptor, ignore it

        // Draw each individual submeshes
        VkBuffer vertexBuffer = regMesh->vertexBuffer;
        VkBuffer indexBuffer = regMesh->indexBuffer;
        VkIndexType indexType = regMesh->indexType;

        VkBuffer buffers[] = { vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(currentCmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(currentCmd, indexBuffer, 0, indexType);

        for (size_t i = 0; i < submeshes.size(); ++i) {
            uint32_t indexCount = submeshes[i].indexCount;
            if (indexCount == 0) continue;

            uint32_t matIndex  = submeshMats[i].index;
            glm::uvec4 props1 = glm::uvec4(matIndex, 0, 0, 0);

            // // Print submesh mats index
            // printf("Submesh %zu uses material index %u\n", i, submeshMats[i].index);

            // Offset 0: global transform
            // Offset 64: other properties (1)
            rPipeline->pushConstants(currentCmd, ShaderStage::VertexAndFragment, 0,  transform);
            rPipeline->pushConstants(currentCmd, ShaderStage::VertexAndFragment, 64, props1);

            uint32_t indexOffset = submeshes[i].indexOffset;
            vkCmdDrawIndexed(currentCmd, indexCount, 1, indexOffset, 0, 0);
        }
    }
}


// End frame: finalize command buffer, submit, and present
void Renderer::endFrame(uint32_t imageIndex) {
    if (imageIndex == UINT32_MAX) return;

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    vkCmdEndRenderPass(currentCmd);

    postProcess->executeEffects(currentCmd, currentFrame);
    postProcess->executeFinalBlit(currentCmd, currentFrame, imageIndex);

    if (vkEndCommandBuffer(currentCmd) != VK_SUCCESS) {
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
    submit.pCommandBuffers      = &currentCmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(deviceVK->graphicsQueue, 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    // PRESENT waits on the same per-IMAGE semaphore:
    VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = signalSemaphores;
    present.swapchainCount     = 1;
    VkSwapchainKHR chains[]    = { swapChain->get() };
    present.pSwapchains        = chains;
    present.pImageIndices      = &imageIndex;

    VkResult res = vkQueuePresentKHR(deviceVK->presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = true;
    } else if (res != VK_SUCCESS) {
        throw std::runtime_error("failed to present");
    }

    currentFrame = (currentFrame + 1) % maxFramesInFlight;
}

void Renderer::addPostProcessEffect(const std::string& name, const std::string& computeShaderPath) {
    // Delegate to PostProcess - it now handles its own effect storage
    if (postProcess) {
        postProcess->addEffect(name, computeShaderPath);
    }
}

void Renderer::loadPostProcessEffectsFromJson(const std::string& configPath) {
    // Delegate to PostProcess to load effects from JSON
    if (postProcess) {
        postProcess->loadEffectsFromJson(configPath);
    }
}