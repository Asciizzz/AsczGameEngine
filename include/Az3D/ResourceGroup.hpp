#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshTypes.hpp"
#include "Az3D/RigSkeleton.hpp"

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

// Forward declaration
namespace Az3D {
    struct TinyTexture;
}

namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* vkDevice);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    size_t addTexture(std::string name, std::string imagePath, uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const Material& material);

    size_t addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH = false);
    size_t addMeshStatic(std::string name, std::string filePath, bool hasBVH = false);

    size_t addMeshSkinned(std::string name, SharedPtr<MeshSkinned> mesh);
    size_t addMeshSkinned(std::string name, std::string filePath);

    size_t addRig(std::string name, SharedPtr<RigSkeleton> rig);
    size_t addRig(std::string name, std::string filePath);

    std::pair<size_t, size_t> addRiggedModel(std::string name, std::string filePath); // Adds both mesh and skeleton from file


    VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout->get(); }
    VkDescriptorSetLayout getTexDescLayout() const { return texDescLayout->get(); }
    VkDescriptorSetLayout getRigDescLayout() const { return rigDescLayout->get(); }

    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }
    VkDescriptorSet getRigDescSet(size_t rigIndex) const {
        return rigIndex < rigs.size() ? rigDescSets->get(rigIndex) : VK_NULL_HANDLE;
    }


    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getMeshStaticIndex(std::string name) const;
    size_t getMeshSkinnedIndex(std::string name) const;
    size_t getRigIndex(std::string name) const;

    Texture* getTexture(std::string name) const;
    Material* getMaterial(std::string name) const;
    MeshStatic* getMeshStatic(std::string name) const;
    MeshSkinned* getMeshSkinned(std::string name) const;
    RigSkeleton* getRig(std::string name) const;

    void uploadAllToGPU();

// private: i dont care about safety
    AzVulk::Device* vkDevice;

    // Mesh static - Resources: SharedPtr, Buffers: UniquePtr
    SharedPtrVec<MeshStatic>          meshStatics;
    UniquePtrVec<AzVulk::BufferData>  vstaticBuffers;
    UniquePtrVec<AzVulk::BufferData>  istaticBuffers;
    void createMeshStaticBuffers();

    // Mesh skinned - Resources: SharedPtr, Buffers: UniquePtr
    SharedPtrVec<MeshSkinned>         meshSkinneds;
    UniquePtrVec<AzVulk::BufferData>  vskinnedBuffers;
    UniquePtrVec<AzVulk::BufferData>  iskinnedBuffers;
    void createMeshSkinnedBuffers();

    // Convenient meshes functions
    inline uint32_t getStaticIndexCount(size_t meshIndex) const { return static_cast<uint32_t>(meshStatics[meshIndex]->indices.size()); }
    inline uint32_t getSkinnedIndexCount(size_t meshIndex) const { return static_cast<uint32_t>(meshSkinneds[meshIndex]->indices.size()); }

    inline VkBuffer getStaticVertexBuffer(size_t meshIndex) const { return vstaticBuffers[meshIndex]->buffer; }
    inline VkBuffer getStaticIndexBuffer(size_t meshIndex) const { return istaticBuffers[meshIndex]->buffer; }

    inline VkBuffer getSkinnedVertexBuffer(size_t meshIndex) const { return vskinnedBuffers[meshIndex]->buffer; }
    inline VkBuffer getSkinnedIndexBuffer(size_t meshIndex) const { return iskinnedBuffers[meshIndex]->buffer; }

    // Skeleton data - Resources: SharedPtr, Buffers & Descriptors: UniquePtr
    SharedPtrVec<RigSkeleton>         rigs;
    UniquePtrVec<AzVulk::BufferData>  rigInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescLayout>     rigDescLayout;
    UniquePtr<AzVulk::DescPool>       rigDescPool;
    UniquePtr<AzVulk::DescSets>       rigDescSets;
    void createRigBuffers();
    void createRigDescSets();

    // Material - Resources: SharedPtr, Buffers & Descriptors: UniquePtr
    SharedPtrVec<Material>            materials;
    UniquePtr<AzVulk::BufferData>     matBuffer;
    UniquePtr<AzVulk::DescLayout>     matDescLayout;
    UniquePtr<AzVulk::DescPool>       matDescPool;
    UniquePtr<AzVulk::DescSets>       matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    // Texture Sampler - Raw handles managed manually
    std::vector<VkSampler>            samplers;
    void createSamplers();

    // Texture Image - Resources: SharedPtr, Descriptors: UniquePtr
    SharedPtrVec<Texture>             textures;
    UniquePtr<AzVulk::DescLayout>     texDescLayout;
    UniquePtr<AzVulk::DescPool>       texDescPool;
    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Useful texture methods
    SharedPtr<Texture> createTexture(const TinyTexture& tinyTexture, uint32_t mipLevels = 0);


    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> meshStaticNameToIndex;
    UnorderedMap<std::string, size_t> meshSkinnedNameToIndex;
    UnorderedMap<std::string, size_t> rigNameToIndex;

    // Maps to track duplicate counts for automatic renaming
    UnorderedMap<std::string, size_t> textureNameCounts;
    UnorderedMap<std::string, size_t> materialNameCounts;
    UnorderedMap<std::string, size_t> meshStaticNameCounts;
    UnorderedMap<std::string, size_t> meshSkinnedNameCounts;
    UnorderedMap<std::string, size_t> rigNameCounts;

private:
    // Helper function to generate unique names with suffixes
    std::string getUniqueName(const std::string& baseName, UnorderedMap<std::string, size_t>& nameCounts);
};

} // namespace Az3D
