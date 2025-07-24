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

        // Manager access
        TextureManager& getTextureManager() { return *textureManager; }
        const TextureManager& getTextureManager() const { return *textureManager; }
        
        MaterialManager& getMaterialManager() { return *materialManager; }
        const MaterialManager& getMaterialManager() const { return *materialManager; }
        
        MeshManager& getMeshManager() { return *meshManager; }
        const MeshManager& getMeshManager() const { return *meshManager; }
        
        // ============ CENTRALIZED STRING-TO-INDEX MAPPING ============
        
        // Texture management with string IDs
        size_t loadTexture(const std::string& textureId, const std::string& imagePath);
        bool hasTexture(const std::string& textureId) const;
        const Texture* getTexture(const std::string& textureId) const;
        size_t getTextureIndex(const std::string& textureId) const;
        
        // Material management with string IDs  
        size_t createMaterial(const std::string& materialId, const std::string& materialName = "");
        bool hasMaterial(const std::string& materialId) const;
        Material* getMaterial(const std::string& materialId) const;
        size_t getMaterialIndex(const std::string& materialId) const;
        
        // Mesh management with string IDs
        size_t loadMesh(const std::string& meshId, const std::string& filePath);
        size_t createCubeMesh(const std::string& meshId);  // Convenience method for cube
        bool hasMesh(const std::string& meshId) const;
        Mesh* getMesh(const std::string& meshId) const;
        size_t getMeshIndex(const std::string& meshId) const;

        // ============ UTILITY METHODS ============
        
        // Get all registered string IDs for iteration
        std::vector<std::string> getTextureIds() const;
        std::vector<std::string> getMaterialIds() const;
        std::vector<std::string> getMeshIds() const;

        std::unique_ptr<TextureManager> textureManager;
        std::unique_ptr<MaterialManager> materialManager;
        std::unique_ptr<MeshManager> meshManager;
        
        // Centralized ID-to-index mapping for all resource types
        std::unordered_map<std::string, size_t> textureIdToIndex;
        std::unordered_map<std::string, size_t> materialIdToIndex;
        std::unordered_map<std::string, size_t> meshIdToIndex;
    };
    
} // namespace Az3D
