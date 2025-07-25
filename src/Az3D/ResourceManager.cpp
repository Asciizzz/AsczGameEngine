#include "Az3D/ResourceManager.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

namespace Az3D {

    ResourceManager::ResourceManager(const AzVulk::Device& device, VkCommandPool commandPool) {
        textureManager = std::make_unique<TextureManager>(device, commandPool);
        materialManager = std::make_unique<MaterialManager>();
        meshManager = std::make_unique<MeshManager>();
    }

    size_t ResourceManager::addTexture(const char* imagePath) {
        // Load texture and get index
        return textureManager->loadTexture(imagePath);
    }
    
    size_t ResourceManager::addMaterial(const Material& material) {
        return materialManager->addMaterial(new Material(material));
    }
    
    size_t ResourceManager::addMesh(const Mesh& mesh) {
        // Instead of copying, create a new mesh with same data
        auto newMesh = std::make_shared<Mesh>();
        // Copy mesh data manually if needed, or better - pass shared_ptr directly
        return meshManager->addMesh(newMesh);
    }

} // namespace Az3D
