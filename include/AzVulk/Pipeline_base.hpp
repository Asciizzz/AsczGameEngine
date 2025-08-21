#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AzVulk {

class BasePipeline {
public:
    explicit BasePipeline(VkDevice device) : device(device) {}
    virtual ~BasePipeline() { cleanup(); }

    // Lifecycle
    virtual void create() = 0;
    virtual void recreate() = 0;

    virtual void cleanup() {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }

    virtual void bind(VkCommandBuffer cmd) const = 0;

    VkDevice device = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    // helpers
    static std::vector<char> readFile(const std::string& path);
    VkShaderModule createModule(const std::vector<char>& code) const;
};

} // namespace AzVulk
