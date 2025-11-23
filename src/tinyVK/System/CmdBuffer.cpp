#include "tinyVk/System/CmdBuffer.hpp"
#include <stdexcept>

using namespace tinyVk;

void CmdBuffer::cleanup() {
    // In the event that pool isn't destroyed first
    if (device != VK_NULL_HANDLE && !cmdBuffers.empty()) {
        vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    }
}

CmdBuffer::CmdBuffer(CmdBuffer&& other) noexcept {
    device = other.device;
    cmdPool = other.cmdPool;
    cmdBuffers = std::move(other.cmdBuffers);

    other.device = VK_NULL_HANDLE;
    other.cmdPool = VK_NULL_HANDLE;
}

CmdBuffer& CmdBuffer::operator=(CmdBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        device = other.device;
        cmdPool = other.cmdPool;
        cmdBuffers = std::move(other.cmdBuffers);

        other.device = VK_NULL_HANDLE;
        other.cmdPool = VK_NULL_HANDLE;
    }
    return *this;
}

void CmdBuffer::create(VkDevice device, VkCommandPool pool, uint32_t count) {
    device = device;
    cmdPool = pool;
    cmdBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) cmdBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, cmdBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}


// ---------------- TEMPORARY COMMAND BUFFER ----------------
TempCmd::TempCmd(const Device* dev, const Device::PoolWrapper& pool) 
    : dvk(dev), poolWrapper(pool) {
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = poolWrapper.pool;
    alloc.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(dvk->device, &alloc, &cmdBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffer");

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &begin);
}

TempCmd::~TempCmd() {
    if (cmdBuffer != VK_NULL_HANDLE) {
        if (!submitted) endAndSubmit();
        vkFreeCommandBuffers(dvk->device, poolWrapper.pool, 1, &cmdBuffer);
    }
}

void TempCmd::endAndSubmit(VkPipelineStageFlags waitStage) {
    if (submitted) return;
    submitted = true;

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(dvk->getQueue(poolWrapper.type), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(dvk->getQueue(poolWrapper.type));

    cmdBuffer = VK_NULL_HANDLE;
}
