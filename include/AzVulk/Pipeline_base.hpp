#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AzVulk {

class PipelineBase {
public:
    explicit PipelineBase(VkDevice lDevice) : lDevice(lDevice) {}
    virtual ~PipelineBase() { cleanup(); }

    // Lifecycle
    virtual void create() = 0;
    virtual void recreate() = 0;

    virtual void cleanup() {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(lDevice, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(lDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    virtual void bindCmd(VkCommandBuffer cmd) const = 0;
    virtual void bindSets(VkCommandBuffer cmd, VkDescriptorSet* sets, uint32_t count) const = 0;
    
    // Push constants support
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const {
        vkCmdPushConstants(cmd, layout, stageFlags, offset, size, pValues);
    }

    VkDevice lDevice = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // helpers
    static std::vector<char> readFile(const std::string& path);
    VkShaderModule createModule(const std::vector<char>& code) const;
};

} // namespace AzVulk
