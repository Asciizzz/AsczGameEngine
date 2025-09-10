#include "AzVulk/CmdBuffer.hpp"
#include <stdexcept>

using namespace AzVulk;

void CmdBuffer::cleanup() {
    // In the event that pool isn't destroyed first
    if (lDevice != VK_NULL_HANDLE && !cmdBuffers.empty()) {
        vkFreeCommandBuffers(lDevice, cmdPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    }
}

CmdBuffer::CmdBuffer(CmdBuffer&& other) noexcept {
    lDevice = other.lDevice;
    cmdPool = other.cmdPool;
    cmdBuffers = std::move(other.cmdBuffers);

    other.lDevice = VK_NULL_HANDLE;
    other.cmdPool = VK_NULL_HANDLE;
}

CmdBuffer& CmdBuffer::operator=(CmdBuffer&& other) noexcept {
    if (this != &other) {
        cleanup();

        lDevice = other.lDevice;
        cmdPool = other.cmdPool;
        cmdBuffers = std::move(other.cmdBuffers);

        other.lDevice = VK_NULL_HANDLE;
        other.cmdPool = VK_NULL_HANDLE;
    }
    return *this;
}

void CmdBuffer::create(VkDevice device, VkCommandPool pool, uint32_t count) {
    lDevice = device;
    cmdPool = pool;
    cmdBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) cmdBuffers.size();

    if (vkAllocateCommandBuffers(lDevice, &allocInfo, cmdBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}