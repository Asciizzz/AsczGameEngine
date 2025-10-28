#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace tinyVK {

// Core pipeline functionality as a component - replaces PipelineBase
class PipelineCore {
public:
    explicit PipelineCore(VkDevice device) : device(device) {}
    ~PipelineCore() { cleanup(); }

    // Lifecycle management
    void cleanup() {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
    
    // Push constants support
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const {
        vkCmdPushConstants(cmd, layout, stageFlags, offset, size, pValues);
    }

    // Template push constants helpers - for type-safe push constants
    template<typename T>
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, const T* data) const {
        vkCmdPushConstants(cmd, layout, stageFlags, offset, sizeof(T), data);
    }

    template<typename T>
    void pushConstants(VkCommandBuffer cmd, VkShaderStageFlags stageFlags, uint32_t offset, const T& data) const {
        vkCmdPushConstants(cmd, layout, stageFlags, offset, sizeof(T), &data);
    }

    // Getters for composed classes
    VkDevice getDevice() const { return device; }
    VkPipeline getPipeline() const { return pipeline; }
    VkPipelineLayout getLayout() const { return layout; }
    
    // Allow composed classes to set pipeline objects
    void setPipeline(VkPipeline p) { pipeline = p; }
    void setLayout(VkPipelineLayout l) { layout = l; }

    // Utility functions
    VkShaderModule createModule(const std::vector<char>& code) const {
        return createModule(device, code);
    }

    static std::vector<char> readFile(const std::string& path);
    static VkShaderModule createModule(VkDevice device, const std::vector<char>& code);
    static VkShaderModule createModuleFromPath(VkDevice device, const std::string& path);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

} // namespace tinyVK
