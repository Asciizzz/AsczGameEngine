#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/Mesh.hpp"

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations
namespace AzVulk {
    class Device;
}

namespace Az3D {
    
    // ResourceManager - central manager for all Az3D resources with string-to-index mapping
    class ResourceManager {
    public:
        ResourceManager(const AzVulk::Device& device, VkCommandPool commandPool);
        ~ResourceManager() = default;

        // Delete copy constructor and assignment operator
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        size_t addTexture(const char* imagePath);
        size_t addMaterial(const Material& material);
        size_t addMesh(const Mesh& mesh);

        // ============ UTILITY METHODS ============

        std::unique_ptr<MeshManager> meshManager;
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
    };
    
} // namespace Az3D
