#include "AzVulk/CmdBuffer.hpp"
#include <stdexcept>

using namespace AzVulk;

CmdBuffer::~CmdBuffer() {
    // In the event that pool isn't destroyed first
    if (lDevice != VK_NULL_HANDLE && !cmdBuffers.empty()) {
        vkFreeCommandBuffers(lDevice, cmdPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    }
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