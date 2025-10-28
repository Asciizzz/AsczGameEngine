#include "tinyVK/Render/Renderer.hpp"
#include "tinySystem/tinyImGui.hpp"

#include <stdexcept>
#include <cstring>
#include <SDL2/SDL.h>

using namespace tinyVK;


Renderer::Renderer (Device* deviceVK, VkSurfaceKHR surface, SDL_Window* window, uint32_t maxFramesInFlight)
: deviceVK(deviceVK), maxFramesInFlight(maxFramesInFlight) {

    swapchain = MakeUnique<Swapchain>(deviceVK, surface, window);

    depthImage = MakeUnique<DepthImage>(deviceVK->device);
    depthImage->create(deviceVK->pDevice, swapchain->getExtent());

    postProcess = MakeUnique<PostProcess>(deviceVK, swapchain.get(), depthImage.get());
    postProcess->initialize();  // PostProcess now manages its own render pass

    createRenderTargets();

    createCommandBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    VkDevice device = deviceVK->device;

    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (inFlightFences[i])           vkDestroyFence(    device, inFlightFences[i],           nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
    }
    for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
    }
    
    // RenderTargets are automatically cleaned up (non-owning)
    swapchainRenderTargets.clear();
}

void Renderer::createCommandBuffers() {
    cmdBuffers.create(deviceVK->device, deviceVK->graphicsPoolWrapper.pool, maxFramesInFlight);
}

void Renderer::createSyncObjects() {
    swapchainImageCount = swapchain->images.size();

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
        if (vkCreateSemaphore(deviceVK->device, &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(    deviceVK->device, &fenceInfo, nullptr, &inFlightFences[i])          != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-frame sync objects!");
        }
    }

    // per-image render-finished
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(deviceVK->device, &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create per-image renderFinished semaphore!");
        }
    }
}

void Renderer::recreateRenderPasses() {
    createRenderTargets();
}

void Renderer::createRenderTargets() {
    VkDevice device = deviceVK->device;
    VkExtent2D extent = swapchain->getExtent();
    
    // Clear existing targets and resources
    swapchainRenderTargets.clear();
    framebuffers.clear();
    
    // Create render passes with proper ownership
    auto mainRenderPassConfig = RenderPassConfig::forwardRendering(
        swapchain->getImageFormat(), 
        depthImage->getFormat()
    );
    mainRenderPass = MakeUnique<RenderPass>(device, mainRenderPassConfig);
    
    // Create swapchain framebuffers and render targets
    framebuffers.reserve(swapchain->getImageCount());
    for (uint32_t i = 0; i < swapchain->getImageCount(); ++i) {
        // Create framebuffer for this swapchain image
        FrameBufferConfig fbConfig = FrameBufferConfig()
            .withRenderPass(mainRenderPass->get())
            .addAttachment(swapchain->getImageView(i))
            .addAttachment(depthImage->getView())
            .withExtent(extent);

        auto framebuffer = MakeUnique<FrameBuffer>(device);
        bool success = framebuffer->create(fbConfig);
        if (!success) throw std::runtime_error("Failed to create swapchain framebuffer");

        // Create render target
        RenderTarget swapchainTarget = RenderTarget()
            .withRenderPass(*mainRenderPass)
            .withFrameBuffer(framebuffer->get())
            .withExtent(extent);

        // Add color attachment info
        VkClearValue colorClear;
        colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkClearValue depthClear;
        depthClear.depthStencil = {1.0f, 0};
        
        swapchainTarget.addAttachment(swapchain->getImage(i), swapchain->getImageView(i), colorClear);
        swapchainTarget.addAttachment(depthImage->getImage(), depthImage->getView(), depthClear);
        
        swapchainRenderTargets.push_back(swapchainTarget);
        
        // Store framebuffer with proper ownership
        framebuffers.push_back(std::move(framebuffer));
    }
}

void Renderer::setupImGuiRenderTargets(tinyImGui* imguiWrapper) {
    if (!imguiWrapper) return;
    
    // Convert framebuffers to vector of VkFramebuffer handles
    std::vector<VkFramebuffer> framebufferHandles;
    for (const auto& fb : framebuffers) {
        framebufferHandles.push_back(fb->get());
    }
    
    imguiWrapper->updateRenderTargets(swapchain.get(), depthImage.get(), framebufferHandles);
}



VkRenderPass Renderer::getMainRenderPass() const {
    return mainRenderPass ? mainRenderPass->get() : VK_NULL_HANDLE;
}

VkRenderPass Renderer::getOffscreenRenderPass() const {
    return postProcess ? postProcess->getOffscreenRenderPass() : VK_NULL_HANDLE;
}



VkFramebuffer Renderer::getFrameBuffer(uint32_t imageIndex) const {
    return (imageIndex < framebuffers.size() && framebuffers[imageIndex]) ? 
           framebuffers[imageIndex]->get() : VK_NULL_HANDLE;
}

VkExtent2D Renderer::getSwapChainExtent() const {
    return swapchain ? swapchain->getExtent() : VkExtent2D{0, 0};
}

VkCommandBuffer Renderer::getCurrentCommandBuffer() const {
    return cmdBuffers[currentFrame];
}

void Renderer::handleWindowResize(SDL_Window* window) {
    // Wait for device to be idle
    vkDeviceWaitIdle(deviceVK->device);
    
    // Get new window dimensions for depth resources
    int newWidth, newHeight;
    SDL_GetWindowSize(window, &newWidth, &newHeight);
    
    // Recreate depth resources before recreating other resources
    depthImage->create(deviceVK->pDevice, newWidth, newHeight);
    
    // Now safe to cleanup and recreate Swapchain
    swapchain->cleanup();
    swapchain->createSwapChain(window);
    swapchain->createImageViews();
    
    // Recreate render targets
    recreateRenderPasses();  // This calls createRenderTargets() now
    
    // Use PostProcess's existing recreate method - it will preserve effects internally
    postProcess->recreate();
}

// Begin frame: handle synchronization, image acquisition, and render pass setup
uint32_t Renderer::beginFrame() {
    VkDevice device = deviceVK->device;

    // Wait for the current frame's fence
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquire = vkAcquireNextImageKHR(
        device, *swapchain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    // CRITICAL: If this image is still being used by another frame, wait for that frame's completion
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(deviceVK->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    
    // Reset the current frame's fence ONLY after we're sure the image is free
    vkResetFences(deviceVK->device, 1, &inFlightFences[currentFrame]);
    
    // Now assign this image to the current frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(currentCmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Start rendering to PostProcess's offscreen target
    currentRenderTarget = postProcess->getOffscreenRenderTarget(currentFrame);
    if (!currentRenderTarget) {
        throw std::runtime_error("PostProcess offscreen render target not found!");
    }

    // Begin render pass using RenderTarget wrapper
    currentRenderTarget->beginRenderPass(currentCmd);
    currentRenderTarget->setViewportAndScissor(currentCmd);

    return imageIndex;  
}

// Sky rendering using dedicated sky pipeline
void Renderer::drawSky(const tinyProject* project, const PipelineRaster* skyPipeline) const {
    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    // Bind sky pipeline
    skyPipeline->bindCmd(currentCmd);

    // Bind only the global descriptor set (set 0) for sky
    const tinyGlobal* global = project->getGlobal();
    VkDescriptorSet glbSet = global->getDescSet();
    uint32_t offset = currentFrame * global->alignedSize;
    skyPipeline->bindSets(currentCmd, 0, &glbSet, 1, &offset, 1);

    // Draw fullscreen triangle (3 vertices, no input)
    vkCmdDraw(currentCmd, 3, 1, 0, 0);
}


void Renderer::drawScene(tinyProject* project, tinySceneRT* activeScene, const PipelineRaster* plRigged, const PipelineRaster* plStatic, tinyHandle selectedNodeHandle) const {
    if (!activeScene) return;

    const tinyFS& fs = project->fs();

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    const tinyGlobal* global = project->getGlobal();
    VkDescriptorSet glbSet = global->getDescSet();
    uint32_t offset = currentFrame * global->alignedSize;

    tinyHandle curSkeleNodeHandle;

    const auto& mapMR3D = activeScene->mapRT3D<tinyNodeRT::MR3D>();
    for (const auto& [nodeHandle, mr3dHandle] : mapMR3D) {
        const tinyNodeRT* rtNode = activeScene->node(nodeHandle);

        // Get mesh render component directly from runtime node
        const auto* mr3DComp = rtNode->get<tinyNodeRT::MR3D>();
        if (!mr3DComp) continue; // No mesh render component

        tinyHandle meshHandle = mr3DComp->pMeshHandle;
        const auto& regMesh = fs.rGet<tinyMesh>(meshHandle);
        if (!regMesh) continue; // No mesh found

        const auto* transform = rtNode->get<tinyNodeRT::T3D>();
        glm::mat4 transformMat = transform ? transform->global : glm::mat4(1.0f);

        // Draw each individual submeshes
        VkBuffer vrtxBuffer = regMesh->vrtxBuffer();
        VkBuffer idxBuffer = regMesh->idxBuffer();
        VkIndexType idxType = regMesh->idxType();
        const auto& submeshes = regMesh->submeshes(); // Normally you'd bind the material, but because we haven't setup the bind descriptor, ignore it

        VkBuffer buffers[] = { vrtxBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(currentCmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(currentCmd, idxBuffer, 0, idxType);

        tinyVertex::Layout vrtxLayout = regMesh->vrtxLayout();
        bool isRigged = vrtxLayout.type == tinyVertex::Layout::Type::Rigged;
        const PipelineRaster* rPipeline = isRigged ? plRigged : plStatic;
        rPipeline->bindCmd(currentCmd);

        // Retrieve skeleton descriptor set if rigged (with automatic fallback to dummy)
        tinyHandle skeleNodeHandle = mr3DComp->skeleNodeHandle;
        tinyRT_SK3D* rtSkele = activeScene->rtComp<tinyNodeRT::SK3D>(skeleNodeHandle);
        VkDescriptorSet skinSet = rtSkele ? rtSkele->descSet() : VK_NULL_HANDLE;
        uint32_t boneCount = rtSkele ? rtSkele->boneCount() : 0;

        rPipeline->bindSets(currentCmd, 0, &glbSet, 1, &offset, 1);

        if (isRigged) {
            isRigged = skinSet != VK_NULL_HANDLE;

            // skinSet = isRigged ? skinSet : project->getDummySkinDescSet();
            // rPipeline->bindSets(currentCmd, 1, &skinSet, 1);

            if (!isRigged) {
                skinSet = project->getDummySkinDescSet();
                rPipeline->bindSets(currentCmd, 1, &skinSet, 1);
            } else {
                uint32_t skinOffset = rtSkele->dynamicOffset(currentFrame);
                rPipeline->bindSets(currentCmd, 1, &skinSet, 1, &skinOffset, 1);
            }
        }

        // Check if this node is the selected node for highlighting
        bool isSelectedNode = selectedNodeHandle.valid() && (nodeHandle == selectedNodeHandle);
        
        for (size_t i = 0; i < submeshes.size(); ++i) {
            uint32_t idxCount = submeshes[i].idxCount;
            if (idxCount == 0) continue;

            tinyHandle matHandle = submeshes[i].material;
            const tinyRMaterial* material = fs.rGet<tinyRMaterial>(matHandle);
            uint32_t matIndex = material ? matHandle.index : 0;

            // Set special value to 1 for selected nodes, 0 for others
            uint32_t specialValue = isSelectedNode ? 1 : 0;
            glm::uvec4 props1 = glm::uvec4(matIndex, isRigged, specialValue, boneCount);

            // Offset 0: global transform
            // Offset 64: other properties (1)
            rPipeline->pushConstants(currentCmd, ShaderStage::VertexAndFragment, 0,  transformMat);
            rPipeline->pushConstants(currentCmd, ShaderStage::VertexAndFragment, 64, props1);

            uint32_t indexOffset = submeshes[i].indexOffset;
            vkCmdDrawIndexed(currentCmd, idxCount, 1, indexOffset, 0, 0);
        }
    }
}


// End frame: finalize command buffer, submit, and present
void Renderer::endFrame(uint32_t imageIndex, tinyImGui* imguiWrapper) {
    if (imageIndex == UINT32_MAX) return;

    VkCommandBuffer currentCmd = cmdBuffers[currentFrame];

    // End current render pass
    if (currentRenderTarget) {
        currentRenderTarget->endRenderPass(currentCmd);
        currentRenderTarget = nullptr;
    } else {
        vkCmdEndRenderPass(currentCmd);
    }

    postProcess->executeEffects(currentCmd, currentFrame);
    postProcess->executeFinalBlit(currentCmd, currentFrame, imageIndex);

    // Render ImGui if provided (before ending command buffer)
    if (imguiWrapper) {
        // Transition swapchain image from present to color attachment
        VkImageMemoryBarrier toColorAttachment{};
        toColorAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toColorAttachment.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toColorAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.image = swapchain->getImage(imageIndex);
        toColorAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toColorAttachment.subresourceRange.baseMipLevel = 0;
        toColorAttachment.subresourceRange.levelCount = 1;
        toColorAttachment.subresourceRange.baseArrayLayer = 0;
        toColorAttachment.subresourceRange.layerCount = 1;
        toColorAttachment.srcAccessMask = 0;
        toColorAttachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(currentCmd,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &toColorAttachment);

        // Use tinyImGui's own render target
        VkFramebuffer framebuffer = getFrameBuffer(imageIndex);
        imguiWrapper->renderToTarget(imageIndex, currentCmd, framebuffer);
    }

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
    VkSwapchainKHR chains[]    = { swapchain->get() };
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

void Renderer::processPendingRemovals(tinyProject* project, tinySceneRT* activeScene) {
    tinyFS& fs = project->fs();
    // No pending removals anywhere
    if (!fs.rHasPendingRms() &&
        (activeScene && !activeScene->rtTHasPendingRms<tinyRT_SK3D>())
    ) return;

    // Wait for ALL in-flight fences to ensure no resources are in use by GPU
    // This is the safest approach - wait for all frames to complete
    std::vector<VkFence> allFences;
    for (size_t i = 0; i < maxFramesInFlight; ++i) {
        if (inFlightFences[i] != VK_NULL_HANDLE) {
            allFences.push_back(inFlightFences[i]);
        }
    }
    
    if (!allFences.empty()) {
        // Wait for all frames to complete with a reasonable timeout (1 second)
        VkResult result = vkWaitForFences(deviceVK->device, 
                                        static_cast<uint32_t>(allFences.size()), 
                                        allFences.data(), 
                                        VK_TRUE, 
                                        1000000000); // 1 second timeout in nanoseconds

        if (result == VK_TIMEOUT) {
            // Log warning but continue - GPU might be hung, but we can't wait forever
            printf("Warning: Timeout waiting for GPU to finish before deleting resources\n");
        } else if (result != VK_SUCCESS) {
            return; // Don't delete if we can't confirm GPU is done
        }
    }
    
    // Safely flush pending removals now
    if (fs.rHasPendingRms()) {
        fs.rFlushAllRms();
    }

    activeScene->rtTFlushAllRms<tinyRT_SK3D>();
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