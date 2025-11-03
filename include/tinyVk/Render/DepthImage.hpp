#pragma once

#include "tinyVk/Resource/TextureVk.hpp"

namespace tinyVk {

class DepthImage : public ImageVk {
public:
    using ImageVk::ImageVk; // Inherit constructors

    void create(VkPhysicalDevice pDevice, uint32_t width, uint32_t height);
    void create(VkPhysicalDevice pDevice, VkExtent2D extent) {
        create(pDevice, extent.width, extent.height);
    }

private:
    // Helper methods
    VkFormat findDepthFormat(VkPhysicalDevice pDevice);
};

}
