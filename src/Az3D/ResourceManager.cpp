#include "Az3D/ResourceManager.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

using namespace AzVulk;

namespace Az3D {

ResourceManager::ResourceManager(Device* vkDevice):
vkDevice(vkDevice) {
    textureGroup = MakeUnique<TextureGroup>(vkDevice);
    materialGroup = MakeUnique<MaterialGroup>(vkDevice);
    meshStaticGroup = MakeUnique<MeshStaticGroup>(vkDevice);
    meshSkinnedGroup = MakeUnique<MeshSkinnedGroup>(vkDevice);
}

void ResourceManager::uploadAllToGPU() {
    VkDevice lDevice = vkDevice->lDevice;
    
    meshStaticGroup->createDeviceBuffers();
    meshSkinnedGroup->createDeviceBuffers();

    matDescPool.create(lDevice, { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    matDescLayout.create(lDevice, {
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    textureGroup->createDescriptorInfo();

    materialGroup->createDeviceBuffer();
    materialGroup->createDescSet(matDescPool.get(), matDescLayout.get());
}


size_t ResourceManager::addTexture(std::string name, std::string imagePath, uint32_t mipLevels) {
    size_t index = textureGroup->addTexture(imagePath, mipLevels);
    textureNameToIndex[name] = index;
    return index;
}

size_t ResourceManager::addMaterial(std::string name, const Material& material) {
    size_t index = materialGroup->addMaterial(material);
    materialNameToIndex[name] = index;
    return index;
}

size_t ResourceManager::addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH) {
    if (hasBVH) mesh->createBVH();

    size_t index = meshStaticGroup->addMeshStatic(mesh);
    meshStaticNameToIndex[name] = index;
    return index;
}
size_t ResourceManager::addMeshStatic(std::string name, std::string filePath, bool hasBVH) {
    // Check the extension
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    SharedPtr<MeshStatic> newMesh;
    if (extension == "obj" )
        newMesh = MeshStatic::loadFromOBJ(filePath);
    else if (extension == "gltf" || extension == "glb")
        newMesh = MeshStatic::loadFromGLTF(filePath);

    if (hasBVH) newMesh->createBVH();

    size_t index = meshStaticGroup->addMeshStatic(newMesh);
    meshStaticNameToIndex[name] = index;
    return index;
}

size_t ResourceManager::addMeshSkinned(std::string name, std::string filePath) {
    size_t index = meshSkinnedGroup->addFromGLTF(filePath);
    meshSkinnedNameToIndex[name] = index;
    return index;
}

size_t ResourceManager::getTextureIndex(std::string name) const {
    auto it = textureNameToIndex.find(name);
    return it != textureNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceManager::getMaterialIndex(std::string name) const {
    auto it = materialNameToIndex.find(name);
    return it != materialNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceManager::getMeshStaticIndex(std::string name) const {
    auto it = meshStaticNameToIndex.find(name);
    return it != meshStaticNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceManager::getMeshSkinnedIndex(std::string name) const {
    auto it = meshSkinnedNameToIndex.find(name);
    return it != meshSkinnedNameToIndex.end() ? it->second : SIZE_MAX;
}


Texture* ResourceManager::getTexture(std::string name) const {
    size_t index = getTextureIndex(name);
    return index != SIZE_MAX ? textureGroup->textures[index].get() : nullptr;
}

Material* ResourceManager::getMaterial(std::string name) const {
    size_t index = getMaterialIndex(name);
    return index != SIZE_MAX ? &materialGroup->materials[index] : nullptr;
}

MeshStatic* ResourceManager::getMeshStatic(std::string name) const {
    size_t index = getMeshStaticIndex(name);
    return index != SIZE_MAX ? meshStaticGroup->meshes[index].get() : nullptr;
}

MeshSkinned* ResourceManager::getMeshSkinned(std::string name) const {
    size_t index = getMeshSkinnedIndex(name);
    return index != SIZE_MAX ? meshSkinnedGroup->meshes[index].get() : nullptr;
}

} // namespace Az3D
