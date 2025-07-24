#pragma once

#include "Az3D/TextureManager.hpp"
#include "Az3D/MaterialManager.hpp"
#include "Az3D/MeshManager.hpp"
#include <memory>
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
        
        // Convenience methods
        bool loadTexture(const std::string& textureId, const std::string& imagePath) {
            return textureManager->loadTexture(textureId, imagePath);
        }
        
        Material* createMaterial(const std::string& materialId, const std::string& materialName = "") {
            return materialManager->createMaterial(materialId, materialName);
        }
        
        Mesh* loadMesh(const std::string& meshId, const std::string& filePath) {
            return meshManager->loadMeshFromOBJ(meshId, filePath);
        }

    private:
        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
        std::unique_ptr<MeshManager> meshManager;
    };
    
} // namespace Az3D
