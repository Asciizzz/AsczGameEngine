#pragma once

#include "Az3D/TextureManager.hpp"
#include "Az3D/MaterialManager.hpp"
#include "Az3D/MeshManager.hpp"
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>

// Forward declarations
namespace AzVulk {
    class VulkanDevice;
}

namespace Az3D {
    
    // ResourceManager - central manager for all Az3D resources
    class ResourceManager {
    public:
        ResourceManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool);
        ~ResourceManager() = default;

        // Delete copy constructor and assignment operator
        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        // Manager access
        TextureManager& getTextureManager() { return *textureManager; }
        const TextureManager& getTextureManager() const { return *textureManager; }
        
        MaterialManager& getMaterialManager() { return *materialManager; }
        const MaterialManager& getMaterialManager() const { return *materialManager; }
        
        MeshManager& getMeshManager() { return *meshManager; }
        const MeshManager& getMeshManager() const { return *meshManager; }
        
        // Texture ID-to-index mapping
        size_t loadTexture(const std::string& textureId, const std::string& imagePath);
        bool hasTexture(const std::string& textureId) const;
        const Texture* getTexture(const std::string& textureId) const;
        size_t getTextureIndex(const std::string& textureId) const;
        
        // Material convenience methods  
        Material* createMaterial(const std::string& materialId, const std::string& materialName = "") {
            return materialManager->createMaterial(materialId, materialName);
        }
        
        // Mesh convenience methods
        Mesh* loadMesh(const std::string& meshId, const std::string& filePath) {
            return meshManager->loadMeshFromOBJ(meshId, filePath);
        }

    private:
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
        std::unique_ptr<MeshManager> meshManager;
        
        // ID-to-index mapping for textures
        std::unordered_map<std::string, size_t> textureIdToIndex;
    };
    
} // namespace Az3D
