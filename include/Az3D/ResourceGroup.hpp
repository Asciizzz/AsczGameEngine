#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshStatic.hpp"
#include "Az3D/MeshSkinned.hpp"


namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* vkDevice);
    ~ResourceGroup() = default;

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    size_t addTexture(std::string name, std::string imagePath, uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const Material& material);

    size_t addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH = false);
    size_t addMeshStatic(std::string name, std::string filePath, bool hasBVH = false);

    size_t addMeshSkinned(std::string name, SharedPtr<MeshSkinned> mesh);
    size_t addMeshSkinned(std::string name, std::string filePath);

    AzVulk::Device* vkDevice;


    // Mesh static
    SharedPtrVec<MeshStatic> meshStatics;
    SharedPtrVec<AzVulk::BufferData> vstaticBuffers;
    SharedPtrVec<AzVulk::BufferData> istaticBuffers;
    void createMeshStaticBuffers();

    // Material
    std::vector<Material> materials;
    AzVulk::BufferData matBuffer;
    void createMaterialBuffer(); // One big buffer for all

    AzVulk::DescLayout matDescLayout;
    AzVulk::DescPool matDescPool;
    AzVulk::DescSets matDescSet;
    void createMaterialDescSet(); // Only need one

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
    UniquePtr<MeshSkinnedGroup> meshSkinnedGroup;
};

} // namespace Az3D
