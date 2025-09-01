#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshStatic.hpp"
#include "Az3D/MeshSkinned.hpp"


namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceManager {
public:
    ResourceManager(AzVulk::Device* vkDevice);
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    size_t addTexture(std::string name, std::string imagePath, uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const Material& material);

    size_t addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH = false);
    size_t addMeshStatic(std::string name, std::string filePath, bool hasBVH = false);

    size_t addMeshSkinned(std::string name, SharedPtr<MeshSkinned> mesh);
    size_t addMeshSkinned(std::string name, std::string filePath);

    AzVulk::Device* vkDevice;

    AzVulk::DescLayout matDescLayout;
    AzVulk::DescPool matDescPool;

    SharedPtrVec<MeshStatic> meshStatics;
    SharedPtrVec<AzVulk::BufferData> vstaticBuffers;
    SharedPtrVec<AzVulk::BufferData> istaticBuffers;
    void createMeshStaticBuffers();

    void uploadAllToGPU();

    const VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout.get(); }
    const VkDescriptorSetLayout getTexDescLayout() const { return textureGroup->getDescLayout(); }

    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getMeshStaticIndex(std::string name) const;
    size_t getMeshSkinnedIndex(std::string name) const;

    Texture* getTexture(std::string name) const;
    Material* getMaterial(std::string name) const;
    MeshStatic* getMeshStatic(std::string name) const;
    MeshSkinned* getMeshSkinned(std::string name) const;

    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> meshStaticNameToIndex;
    UnorderedMap<std::string, size_t> meshSkinnedNameToIndex;

    UniquePtr<TextureGroup> textureGroup;
    UniquePtr<MaterialGroup> materialGroup;
    UniquePtr<MeshSkinnedGroup> meshSkinnedGroup;
};

} // namespace Az3D
