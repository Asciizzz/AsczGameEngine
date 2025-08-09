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

        size_t addTexture(std::string name, std::string imagePath,
                        TextureMode addressMode = TextureMode::Repeat,
                        bool semiTransparent = false);
        size_t addMaterial(std::string name, const Material& material);

        size_t addMesh(std::string name, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool hasBVH = false);
        size_t addMesh(std::string name, const Mesh& mesh, bool hasBVH = false);
        size_t addMesh(std::string name, std::string filePath, bool hasBVH = false);

        // String-to-index getters
        size_t getTexture(std::string name) const;
        size_t getMaterial(std::string name) const;
        size_t getMesh(std::string name) const;

        // String-to-index maps
        std::unordered_map<std::string, size_t> textureNameToIndex;
        std::unordered_map<std::string, size_t> materialNameToIndex;
        std::unordered_map<std::string, size_t> meshNameToIndex;

        std::unique_ptr<MeshManager> meshManager;
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
    };
    
} // namespace Az3D
