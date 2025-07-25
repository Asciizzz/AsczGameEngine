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
        return textureManager->addTexture(imagePath);
    }
    
    size_t ResourceManager::addMaterial(const Material& material) {
        return materialManager->addMaterial(new Material(material));
    }
    
    size_t ResourceManager::addMesh(const Mesh& mesh) {
        auto newMesh = std::make_shared<Mesh>(mesh);
        return meshManager->addMesh(newMesh);
    }

} // namespace Az3D
