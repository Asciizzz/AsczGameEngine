#include <AzVulk/Instance.h>

#include <stdexcept>

namespace AzVulk {

Instance::Instance() {
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

Instance::~Instance() {
    vkDestroyInstance(instance, nullptr);
}

} // namespace AzVulk
