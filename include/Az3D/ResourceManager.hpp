#pragma once

#include "Az3D/TextureManager.hpp"
#include "Az3D/MaterialManager.hpp"
#include "Az3D/MeshManager.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations
namespace AzVulk {
    class VulkanDevice;
}

namespace Az3D {
    
    // ResourceManager - central manager for all Az3D resources with string-to-index mapping
    class ResourceManager {
    public:
        ResourceManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool);
        ~ResourceManager() = default;

        // Delete copy constructor and assignment operator
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        size_t addTexture(const std::string& imagePath);
        size_t addMaterial(const Material& material);
        size_t addMesh(const Mesh& mesh);

        // Get resource by index
        Mesh* getMesh(size_t index) const;
        Material* getMaterial(size_t index) const;

        // ============ UTILITY METHODS ============

        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
        std::unique_ptr<MeshManager> meshManager;
    };
    
} // namespace Az3D
