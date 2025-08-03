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

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        size_t addTexture(const char* name, const char* imagePath);
        size_t addMaterial(const char* name, const Material& material);
        size_t addMesh(const char* name, const Mesh& mesh, bool hasBVH = false);
        size_t addMesh(const char* name, const char* filePath, bool hasBVH = false);

        // String-to-index getters
        size_t getTexture(const char* name) const;
        size_t getMaterial(const char* name) const;
        size_t getMesh(const char* name) const;

        // String-to-index maps
        std::unordered_map<const char*, size_t> textureNameToIndex;
        std::unordered_map<const char*, size_t> materialNameToIndex;
        std::unordered_map<const char*, size_t> meshNameToIndex;

        std::unique_ptr<MeshManager> meshManager;
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
    };
    
} // namespace Az3D
