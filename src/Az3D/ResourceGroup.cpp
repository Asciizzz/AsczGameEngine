#include "Az3D/ResourceGroup.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

using namespace AzVulk;

namespace Az3D {

ResourceGroup::ResourceGroup(Device* vkDevice):
vkDevice(vkDevice) {
    textureGroup = MakeUnique<TextureGroup>(vkDevice);
    meshSkinnedGroup = MakeUnique<MeshSkinnedGroup>(vkDevice);
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = vkDevice->lDevice;

    createMeshStaticBuffers();
    createMaterialBuffer();

    meshSkinnedGroup->createDeviceBuffers();

    matDescPool.create(lDevice, { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    matDescLayout.create(lDevice, {
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    createMaterialDescSet();

    textureGroup->createDescriptorInfo();
}


size_t ResourceGroup::addTexture(std::string name, std::string imagePath, uint32_t mipLevels) {
    size_t index = textureGroup->addTexture(imagePath, mipLevels);
    textureNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMaterial(std::string name, const Material& material) {
    size_t index = materials.size();
    materials.push_back(material);

    materialNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH) {
    if (hasBVH) mesh->createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(mesh);

    meshStaticNameToIndex[name] = index;
    return index;
}
size_t ResourceGroup::addMeshStatic(std::string name, std::string filePath, bool hasBVH) {
    // Check the extension
    std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
    SharedPtr<MeshStatic> newMesh;
    if (extension == "obj" )
        newMesh = MeshStatic::loadFromOBJ(filePath);
    else if (extension == "gltf" || extension == "glb")
        newMesh = MeshStatic::loadFromGLTF(filePath);

    if (hasBVH) newMesh->createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(newMesh);

    meshStaticNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMeshSkinned(std::string name, std::string filePath) {
    size_t index = meshSkinnedGroup->addFromGLTF(filePath);
    meshSkinnedNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::getTextureIndex(std::string name) const {
    auto it = textureNameToIndex.find(name);
    return it != textureNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMaterialIndex(std::string name) const {
    auto it = materialNameToIndex.find(name);
    return it != materialNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMeshStaticIndex(std::string name) const {
    auto it = meshStaticNameToIndex.find(name);
    return it != meshStaticNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMeshSkinnedIndex(std::string name) const {
    auto it = meshSkinnedNameToIndex.find(name);
    return it != meshSkinnedNameToIndex.end() ? it->second : SIZE_MAX;
}


Texture* ResourceGroup::getTexture(std::string name) const {
    size_t index = getTextureIndex(name);
    return index != SIZE_MAX ? textureGroup->textures[index].get() : nullptr;
}

Material* ResourceGroup::getMaterial(std::string name) const {
    size_t index = getMaterialIndex(name);
    return index != SIZE_MAX ? const_cast<Material*>(&materials[index]) : nullptr;
}

MeshStatic* ResourceGroup::getMeshStatic(std::string name) const {
    size_t index = getMeshStaticIndex(name);
    return index != SIZE_MAX ? meshStatics[index].get() : nullptr;
}

MeshSkinned* ResourceGroup::getMeshSkinned(std::string name) const {
    size_t index = getMeshSkinnedIndex(name);
    return index != SIZE_MAX ? meshSkinnedGroup->meshes[index].get() : nullptr;
}

} // namespace Az3D
