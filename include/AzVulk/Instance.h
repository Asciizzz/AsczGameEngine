#pragma once

#include <vulkan/vulkan.h>

namespace AzVulk {

class Instance {
public:
    Instance();
    ~Instance();

    VkInstance instance;
};

} // namespace AzVulk