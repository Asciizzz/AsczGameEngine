#pragma once

#include "AzVulk/DeviceVK.hpp"

namespace AzVulk {

class CmdBuffer {
public:
    CmdBuffer() = default;
    ~CmdBuffer() { cleanup(); } void cleanup();

    // Delete copy constructor and assignment operator
    CmdBuffer(const CmdBuffer&) = delete;
    CmdBuffer& operator=(const CmdBuffer&) = delete;
    // Move constructor and assignment operator
    CmdBuffer(CmdBuffer&& other) noexcept;
    CmdBuffer& operator=(CmdBuffer&& other) noexcept;

    void create(VkDevice device, VkCommandPool pool, uint32_t count);

    // Operator [] for easy access
    template<typename T>
    const VkCommandBuffer& operator[](T index) const { return cmdBuffers[index]; }

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    VkCommandPool cmdPool = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> cmdBuffers;
};


// RAII temporary command buffer wrapper
class TempCmd {
public:
    TempCmd(const DeviceVK* deviceVK, const DeviceVK::PoolWrapper& poolWrapper);
    ~TempCmd();
    void endAndSubmit(VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkCommandBuffer get() const { return cmdBuffer; }

    const DeviceVK* deviceVK = nullptr;
    DeviceVK::PoolWrapper poolWrapper{};
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    bool submitted = false;
};

};