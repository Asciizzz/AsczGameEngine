#include "Az3D/ResourceManager.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

namespace Az3D {

    ResourceManager::ResourceManager(AzVulk::Device& device) {
        textureManager = MakeUnique<TextureManager>(device);
        materialManager = MakeUnique<MaterialManager>(device);
        meshManager = MakeUnique<MeshManager>(device);
    }

    size_t ResourceManager::addTexture(std::string name, std::string imagePath,
                                        Texture::Mode addressMode, bool semiTransparent) {
        size_t index = textureManager->addTexture(imagePath, semiTransparent, addressMode);
        textureNameToIndex[name] = index;
        return index;
    }
    
    size_t ResourceManager::addMaterial(std::string name, const Material& material) {
        size_t index = materialManager->addMaterial(material);
        materialNameToIndex[name] = index;
        return index;
    }

    size_t ResourceManager::addMesh(std::string name, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool hasBVH) {
        auto newMesh = MakeShared<Mesh>(std::move(vertices), std::move(indices));
        if (hasBVH) newMesh->createBVH();

        size_t index = meshManager->addMesh(newMesh);
        meshNameToIndex[name] = index;
        return index;
    }

    size_t ResourceManager::addMesh(std::string name, SharedPtr<Mesh> mesh, bool hasBVH) {
        if (hasBVH) mesh->createBVH();

        size_t index = meshManager->addMesh(mesh);
        meshNameToIndex[name] = index;
        return index;
    }
    size_t ResourceManager::addMesh(std::string name, std::string filePath, bool hasBVH) {
        auto newMesh = Mesh::loadFromOBJ(filePath); 
        if (hasBVH) newMesh->createBVH();

        size_t index = meshManager->addMesh(newMesh);
        meshNameToIndex[name] = index;
        return index;
    }

    size_t ResourceManager::getTexture(std::string name) const {
        auto it = textureNameToIndex.find(name);
        return it != textureNameToIndex.end() ? it->second : SIZE_MAX;
    }

    size_t ResourceManager::getMaterial(std::string name) const {
        auto it = materialNameToIndex.find(name);
        return it != materialNameToIndex.end() ? it->second : SIZE_MAX;
    }

    size_t ResourceManager::getMesh(std::string name) const {
        auto it = meshNameToIndex.find(name);
        return it != meshNameToIndex.end() ? it->second : SIZE_MAX;
    }

} // namespace Az3D
