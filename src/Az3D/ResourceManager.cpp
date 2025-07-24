#include "Az3D/ResourceManager.hpp"
#include "AzVulk/VulkanDevice.hpp"

namespace Az3D {
    
    ResourceManager::ResourceManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool) {
        textureManager = std::make_unique<TextureManager>(device, commandPool);
        materialManager = std::make_unique<MaterialManager>();
        meshManager = std::make_unique<MeshManager>();
    }
    
} // namespace Az3D
