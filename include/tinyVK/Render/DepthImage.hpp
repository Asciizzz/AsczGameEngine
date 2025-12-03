#pragma once

#include "tinyVk/Resource/TextureVk.hpp"

namespace tinyVk {

class DepthImage : public ImageVk {
public:
    using ImageVk::ImageVk; // Inherit constructors

    void create(VkDevice device, VkPhysicalDevice pDevice, uint32_t width, uint32_t height);

private:
    // Helper methods
    VkFormat findDepthFormat(VkPhysicalDevice pDevice);
};

}
