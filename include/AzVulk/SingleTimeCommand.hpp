#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

    class SingleTimeCommand {
    public:
        SingleTimeCommand(Device& device, const std::string& poolName);
        ~SingleTimeCommand();

        VkCommandBuffer getCmdBuffer() const { return cmd; }
        VkCommandBuffer operator*() const { return cmd; }

        Device& device;
        std::string poolName;
        VkCommandBuffer cmd{ VK_NULL_HANDLE };
    };

}