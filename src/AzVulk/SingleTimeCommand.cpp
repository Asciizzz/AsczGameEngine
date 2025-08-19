#include "AzVulk/SingleTimeCommand.hpp"
#include <stdexcept>

namespace AzVulk {

    SingleTimeCommand::SingleTimeCommand(Device& device, const std::string& poolName)
    : device(device), poolName(poolName) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.getPoolWrapper(poolName).pool;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device.device, &allocInfo, &cmd) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffer!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
    }

    SingleTimeCommand::~SingleTimeCommand() {
        if (cmd != VK_NULL_HANDLE) {
            vkEndCommandBuffer(cmd);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;

            const Device::PoolWrapper& poolWrapper = device.getPoolWrapper(poolName);

            vkQueueSubmit(device.getQueue(poolWrapper.type), 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(device.getQueue(poolWrapper.type));

            vkFreeCommandBuffers(device.device, poolWrapper.pool, 1, &cmd);
        }
    }


    }