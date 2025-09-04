#pragma once

#include "Az3D/TinyModel.hpp"

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* vkDevice);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    size_t addTexture(std::string name, std::string imagePath, uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const TinyMaterial& material);

    size_t addMesh(std::string name, SharedPtr<TinyMesh> mesh);
    size_t addMesh(std::string name, std::string filePath);

    size_t addRig(std::string name, SharedPtr<TinySkeleton> skeleton);
    size_t addRig(std::string name, std::string filePath);

    std::pair<size_t, size_t> addRiggedModel(std::string name, std::string filePath); // Adds both mesh and skeleton from file

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout->get(); }
    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }
    
    VkDescriptorSetLayout getTexDescLayout() const { return texDescLayout->get(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return skeleDescLayout->get(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return skeleDescSets[index]->get(); }

    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getMeshIndex(std::string name) const;
    size_t getRigIndex(std::string name) const;

    TinyMesh* getMesh(std::string name) const;
    TinyMaterial* getMaterial(std::string name) const;
    TinyTexture* getTexture(std::string name) const;
    TinySkeleton* getSkeleton(std::string name) const;

    TextureVK* getTextureVK(std::string name) const;

    void uploadAllToGPU();

// private: i dont care about safety
    AzVulk::Device* vkDevice;

    // Mesh - Resources: SharedPtr, Buffers: UniquePtr
    SharedPtrVec<TinyMesh>            meshes;
    UniquePtrVec<AzVulk::BufferData>  vertexBuffers;
    UniquePtrVec<AzVulk::BufferData>  indexBuffers;
    void createMeshBuffers();

    // Skeleton data - Resources: SharedPtr, Buffers & Descriptors: UniquePtr
    SharedPtrVec<TinySkeleton>        skeletons;
    UniquePtrVec<AzVulk::BufferData>  skeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescLayout>     skeleDescLayout;
    UniquePtr<AzVulk::DescPool>       skeleDescPool;
    UniquePtrVec<AzVulk::DescSets>    skeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

    // Material - Resources: SharedPtr, Buffers & Descriptors: UniquePtr
    SharedPtrVec<TinyMaterial>        materials;
    UniquePtr<AzVulk::BufferData>     matBuffer;
    UniquePtr<AzVulk::DescLayout>     matDescLayout;
    UniquePtr<AzVulk::DescPool>       matDescPool;
    UniquePtr<AzVulk::DescSets>       matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    // Texture Sampler - Raw handles managed manually
    std::vector<VkSampler>            samplers;
    void createTextureSamplers();

    // Texture Image - Resources: SharedPtr, Descriptors: UniquePtr
    SharedPtrVec<TinyTexture>         textures;
    SharedPtrVec<TextureVK>           textureVKs;
    UniquePtr<AzVulk::DescLayout>     texDescLayout;
    UniquePtr<AzVulk::DescPool>       texDescPool;
    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Useful methods
    SharedPtr<TextureVK> createTextureVK(const TinyTexture& texture, uint32_t mipLevels = 0);

    uint32_t getIndexCount(size_t index) const { return static_cast<uint32_t>(meshes[index]->indices.size()); }

    VkBuffer getVertexBuffer(size_t index) const { return vertexBuffers[index]->buffer; }
    VkBuffer getIndexBuffer(size_t index) const { return indexBuffers[index]->buffer; }


    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> meshNameToIndex;
    UnorderedMap<std::string, size_t> rigNameToIndex;

    // Maps to track duplicate counts for automatic renaming
    UnorderedMap<std::string, size_t> textureNameCounts;
    UnorderedMap<std::string, size_t> materialNameCounts;
    UnorderedMap<std::string, size_t> meshNameCounts;
    UnorderedMap<std::string, size_t> rigNameCounts;

private:
    // Helper function to generate unique names with suffixes
    std::string getUniqueName(const std::string& baseName, UnorderedMap<std::string, size_t>& nameCounts);
};

} // namespace Az3D
