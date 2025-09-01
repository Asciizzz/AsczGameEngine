#include "AzVulk/Renderer.hpp"
#include "Az3D/Az3D.hpp"
#include <stdexcept>
#include <cstring>

using namespace Az3D;
using namespace AzVulk;


Renderer::Renderer (Device* vkDevice,
                    SwapChain* swapChain,
                    DepthManager* depthManager,
                    GlobalUBOManager* globalUBOManager,
                    ResourceGroup* resGroup) :
vkDevice(vkDevice),
swapChain(swapChain),
depthManager(depthManager),
globalUBOManager(globalUBOManager),
resGroup(resGroup) {
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
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkDevice->graphicsPoolWrapper.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(vkDevice->lDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
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
uint32_t Renderer::beginFrame(RasterPipeline& gPipeline, GlobalUBO& globalUBO) {
    vkWaitForFences(vkDevice->lDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquire = vkAcquireNextImageKHR(
        vkDevice->lDevice, swapChain->swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) { framebufferResized = true; return UINT32_MAX; }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    // If that image is already in flight, wait for its fence
    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(vkDevice->lDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    vkResetFences(vkDevice->lDevice, 1, &inFlightFences[currentFrame]);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = gPipeline.cfg.renderPass;
    renderPassInfo.framebuffer = swapChain->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->extent;

    // No MSAA: depth + color, with MSAA: depth + color + resolve
    uint32_t clearValueCount = 2 + gPipeline.cfg.hasMSAA;

    std::vector<VkClearValue> clearValues(clearValueCount);
    clearValues[0].depthStencil = {1.0f, 0};
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    if (clearValueCount == 3) {
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    }

    renderPassInfo.clearValueCount = clearValueCount;
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

    memcpy(globalUBOManager->bufferData.mapped, &globalUBO, sizeof(globalUBO));

    return imageIndex;
}


void Renderer::drawInstanceStaticGroup(RasterPipeline& rasterPipeline, Az3D::InstanceStaticGroup& instanceGroup) {
    uint32_t instanceCount = static_cast<uint32_t>(instanceGroup.datas.size());
    size_t meshIndex = instanceGroup.meshIndex;

    // const Az3D::MeshStaticGroup* meshStaticGroup = resGroup->meshStaticGroup.get();
    const auto& mesh = resGroup->meshStatics[meshIndex];
    uint64_t indexCount = mesh->indices.size();

    if (instanceCount == 0 || meshIndex == SIZE_MAX || indexCount == 0) return;

    rasterPipeline.bind(commandBuffers[currentFrame]);

    // Bind descriptor sets once
    VkDescriptorSet globalSet = globalUBOManager->getDescSet();
    VkDescriptorSet materialSet = resGroup->getMatDescSet();
    VkDescriptorSet textureSet = resGroup->getTexDescSet();

    std::vector<VkDescriptorSet> sets = {globalSet, materialSet, textureSet};
    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rasterPipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    instanceGroup.updateBufferData();

    VkBuffer vertexBuffer = resGroup->vstaticBuffers[meshIndex]->buffer;
    VkBuffer indexBuffer = resGroup->istaticBuffers[meshIndex]->buffer;

    VkBuffer instanceBuffer = instanceGroup.bufferData.buffer;

    VkBuffer buffers[] = { vertexBuffer, instanceBuffer };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 2, buffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Draw all instances
    vkCmdDrawIndexed(commandBuffers[currentFrame], indexCount, instanceCount, 0, 0, 0);
}

void Renderer::drawDemoSkinned(RasterPipeline& rasterPipeline, const Az3D::MeshSkinned& meshSkinned) {
    uint64_t indexCount = meshSkinned.indices.size();
    if (indexCount == 0) return;

    rasterPipeline.bind(commandBuffers[currentFrame]);

    // Bind descriptor sets
    VkDescriptorSet globalSet = globalUBOManager->getDescSet();
    VkDescriptorSet materialSet = resGroup->getMatDescSet();
    VkDescriptorSet textureSet = resGroup->getTexDescSet();

    std::vector<VkDescriptorSet> sets = {globalSet, materialSet, textureSet};
    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rasterPipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    VkBuffer vertexBuffer = meshSkinned.vertexBufferData.buffer;
    VkBuffer indexBuffer = meshSkinned.indexBufferData.buffer;

    VkBuffer buffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, buffers, offsets);

    vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffers[currentFrame], indexCount, 1, 0, 0, 0);
}

void Renderer::drawInstanceSkinnedGroup(RasterPipeline& rasterPipeline, Az3D::InstanceSkinnedGroup& instanceGroup) {
    // uint32_t instanceCount = static_cast<uint32_t>(instanceGroup.datas.size());
    // size_t meshIndex = instanceGroup.meshIndex;

    // if (instanceCount == 0 || meshIndex == SIZE_MAX) return;

    // const Az3D::MaterialGroup* matGroup = resGroup->materialGroup.get();
    // const Az3D::TextureGroup* texGroup = resGroup->textureGroup.get();
    // const Az3D::MeshSkinnedGroup* meshSkinnedGroup = resGroup->meshSkinnedGroup.get();

    // VkDescriptorSet globalSet = globalUBOManager->getDescSet();

    // rasterPipeline.bind(commandBuffers[currentFrame]);

    // // Bind descriptor sets once
    // VkDescriptorSet materialSet = matGroup->getDescSet();
    // VkDescriptorSet textureSet = texGroup->getDescSet();

    // std::vector<VkDescriptorSet> sets = {globalSet, materialSet, textureSet};
    // vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                         rasterPipeline.layout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    // instanceGroup.updateBufferData();

    // const auto& mesh = meshSkinnedGroup->meshes[meshIndex];
    // uint64_t indexCount = mesh->indices.size();

    // if (indexCount == 0) return;

    // const auto& vertexBufferData = mesh->vertexBufferData;
    // const auto& indexBufferData = mesh->indexBufferData;

    // const auto& instanceBufferData = instanceGroup.bufferData;

    // VkBuffer buffers[] = { vertexBufferData.buffer, instanceBufferData.buffer };
    // VkDeviceSize offsets[] = { 0, 0 };
    // vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 2, buffers, offsets);

    // // Bind index buffer
    // vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBufferData.buffer, 0, VK_INDEX_TYPE_UINT32);

    // // Draw all instances
    // vkCmdDrawIndexed(commandBuffers[currentFrame], indexCount, instanceCount, 0, 0, 0);
}


// Sky rendering using dedicated sky pipeline
void Renderer::drawSky(RasterPipeline& rasterPipeline) {
    // Bind sky pipeline
    rasterPipeline.bind(commandBuffers[currentFrame]);

    // Bind only the global descriptor set (set 0) for sky
    VkDescriptorSet globalSet = globalUBOManager->getDescSet();
    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rasterPipeline.layout, 0, 1, &globalSet, 0, nullptr);

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