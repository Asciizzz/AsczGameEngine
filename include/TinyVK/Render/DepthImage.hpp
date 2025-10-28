#pragma once

#include "tinyVK/Resource/TextureVK.hpp"

namespace tinyVK {

class DepthImage : public ImageVK {
public:
    using ImageVK::ImageVK; // Inherit constructors

    void create(VkPhysicalDevice pDevice, uint32_t width, uint32_t height);
    void create(VkPhysicalDevice pDevice, VkExtent2D extent) {
        create(pDevice, extent.width, extent.height);
    }

private:
    // Helper methods
    VkFormat findDepthFormat(VkPhysicalDevice pDevice);
};

}
