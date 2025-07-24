#include "Az3D/ResourceManager.hpp"
#include "AzVulk/VulkanDevice.hpp"
#include <iostream>

namespace Az3D {
    
    ResourceManager::ResourceManager(const AzVulk::VulkanDevice& device, VkCommandPool commandPool) {
        textureManager = std::make_unique<TextureManager>(device, commandPool);
        materialManager = std::make_unique<MaterialManager>();
        meshManager = std::make_unique<MeshManager>();
    }
    
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
        
        return textureManager->getDefaultTexture();
    }

    size_t ResourceManager::getTextureIndex(const std::string& textureId) const {
        auto it = textureIdToIndex.find(textureId);
        if (it != textureIdToIndex.end()) {
            return it->second;
        }
        
        return 0; // Default texture index
    }} // namespace Az3D
