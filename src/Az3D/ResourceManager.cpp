#include "Az3D/ResourceManager.hpp"
#include "AzVulk/VulkanDevice.hpp"
#include <iostream>

namespace Az3D {
    
    ResourceManager::ResourceManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool) {
        textureManager = std::make_unique<TextureManager>(device, commandPool);
        materialManager = std::make_unique<MaterialManager>();
        meshManager = std::make_unique<MeshManager>();
    }
    
    // ============ TEXTURE MANAGEMENT ============
    
    size_t ResourceManager::loadTexture(const std::string& textureId, const std::string& imagePath) {
        // Check if texture already exists
        auto it = textureIdToIndex.find(textureId);
        if (it != textureIdToIndex.end()) {
            return it->second;
        }
        
        // Load texture and get index
        size_t index = textureManager->loadTexture(imagePath);
        
        // Map ID to index
        textureIdToIndex[textureId] = index;
        
        std::cout << "  → Mapped texture '" << textureId << "' to index " << index << std::endl;
        
        return index;
    }

    bool ResourceManager::hasTexture(const std::string& textureId) const {
        return textureIdToIndex.find(textureId) != textureIdToIndex.end();
    }

    const Texture* ResourceManager::getTexture(const std::string& textureId) const {
        auto it = textureIdToIndex.find(textureId);
        if (it != textureIdToIndex.end()) {
            return textureManager->getTexture(it->second);
        }
        
        std::cout << "Warning: Texture '" << textureId << "' not found, using default" << std::endl;
        return textureManager->getDefaultTexture();
    }

    size_t ResourceManager::getTextureIndex(const std::string& textureId) const {
        auto it = textureIdToIndex.find(textureId);
        if (it != textureIdToIndex.end()) {
            return it->second;
        }
        
        return 0; // Default texture index
    }

    // ============ MATERIAL MANAGEMENT ============
    
    size_t ResourceManager::createMaterial(const std::string& materialId, const std::string& materialName) {
        // Check if material already exists
        auto it = materialIdToIndex.find(materialId);
        if (it != materialIdToIndex.end()) {
            return it->second;
        }
        
        // Create material and get index
        size_t index = materialManager->createMaterial(materialName.empty() ? materialId : materialName);
        
        // Map ID to index
        materialIdToIndex[materialId] = index;
        
        std::cout << "  → Mapped material '" << materialId << "' to index " << index << std::endl;
        
        return index;
    }

    bool ResourceManager::hasMaterial(const std::string& materialId) const {
        return materialIdToIndex.find(materialId) != materialIdToIndex.end();
    }

    Material* ResourceManager::getMaterial(const std::string& materialId) const {
        auto it = materialIdToIndex.find(materialId);
        if (it != materialIdToIndex.end()) {
            return materialManager->getMaterial(it->second);
        }
        
        std::cout << "Warning: Material '" << materialId << "' not found, using default" << std::endl;
        return materialManager->getDefaultMaterial();
    }

    size_t ResourceManager::getMaterialIndex(const std::string& materialId) const {
        auto it = materialIdToIndex.find(materialId);
        if (it != materialIdToIndex.end()) {
            return it->second;
        }
        
        return 0; // Default material index
    }

    // ============ MESH MANAGEMENT ============
    
    size_t ResourceManager::loadMesh(const std::string& meshId, const std::string& filePath) {
        // Check if mesh already exists
        auto it = meshIdToIndex.find(meshId);
        if (it != meshIdToIndex.end()) {
            return it->second;
        }
        
        // Load mesh and get index
        size_t index = meshManager->loadMeshFromOBJ(filePath);
        
        if (index != SIZE_MAX) {
            // Map ID to index
            meshIdToIndex[meshId] = index;
            std::cout << "  → Mapped mesh '" << meshId << "' to index " << index << std::endl;
        }
        
        return index;
    }

    size_t ResourceManager::createCubeMesh(const std::string& meshId) {
        // Check if mesh already exists
        auto it = meshIdToIndex.find(meshId);
        if (it != meshIdToIndex.end()) {
            return it->second;
        }
        
        // Create cube mesh and get index
        size_t index = meshManager->createCubeMesh();
        
        // Map ID to index
        meshIdToIndex[meshId] = index;
        std::cout << "  → Mapped cube mesh '" << meshId << "' to index " << index << std::endl;
        
        return index;
    }

    bool ResourceManager::hasMesh(const std::string& meshId) const {
        return meshIdToIndex.find(meshId) != meshIdToIndex.end();
    }

    Mesh* ResourceManager::getMesh(const std::string& meshId) const {
        auto it = meshIdToIndex.find(meshId);
        if (it != meshIdToIndex.end()) {
            return meshManager->getMesh(it->second);
        }
        
        std::cout << "Warning: Mesh '" << meshId << "' not found" << std::endl;
        return nullptr;
    }

    size_t ResourceManager::getMeshIndex(const std::string& meshId) const {
        auto it = meshIdToIndex.find(meshId);
        if (it != meshIdToIndex.end()) {
            return it->second;
        }
        
        return SIZE_MAX; // Invalid index
    }

    // ============ UTILITY METHODS ============
    
    std::vector<std::string> ResourceManager::getTextureIds() const {
        std::vector<std::string> ids;
        ids.reserve(textureIdToIndex.size());
        for (const auto& pair : textureIdToIndex) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    std::vector<std::string> ResourceManager::getMaterialIds() const {
        std::vector<std::string> ids;
        ids.reserve(materialIdToIndex.size());
        for (const auto& pair : materialIdToIndex) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    std::vector<std::string> ResourceManager::getMeshIds() const {
        std::vector<std::string> ids;
        ids.reserve(meshIdToIndex.size());
        for (const auto& pair : meshIdToIndex) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
} // namespace Az3D
