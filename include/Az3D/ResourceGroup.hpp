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

    size_t addStaticMesh(std::string name, SharedPtr<StaticMesh> mesh, bool hasBVH = false);
    size_t addStaticMesh(std::string name, std::string filePath, bool hasBVH = false);

    size_t addRigMesh(std::string name, SharedPtr<RigMesh> mesh);
    size_t addRigMesh(std::string name, std::string filePath);

    size_t addRig(std::string name, SharedPtr<RigSkeleton> rig);
    size_t addRig(std::string name, std::string filePath);

    std::pair<size_t, size_t> addRiggedModel(std::string name, std::string filePath); // Adds both mesh and skeleton from file

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout->get(); }
    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }
    
    VkDescriptorSetLayout getTexDescLayout() const { return texDescLayout->get(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return rigSkeleDescLayout->get(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return rigSkeleDescSets[index]->get(); }

    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getStaticMeshIndex(std::string name) const;
    size_t getRigMeshIndex(std::string name) const;
    size_t getRigIndex(std::string name) const;

    Texture* getTexture(std::string name) const;
    Material* getMaterial(std::string name) const;
    StaticMesh* getStaticMesh(std::string name) const;
    RigMesh* getRigMesh(std::string name) const;
    RigSkeleton* getRig(std::string name) const;

    void uploadAllToGPU();

// private: i dont care about safety
    AzVulk::Device* vkDevice;

    // Mesh static - Resources: SharedPtr, Buffers: UniquePtr
    SharedPtrVec<StaticMesh>          staticMeshes;
    UniquePtrVec<AzVulk::BufferData>  vstaticBuffers;
    UniquePtrVec<AzVulk::BufferData>  istaticBuffers;
    void createStaticMeshBuffers();

    // Mesh rig - Resources: SharedPtr, Buffers: UniquePtr
    SharedPtrVec<RigMesh>             rigMeshes;
    UniquePtrVec<AzVulk::BufferData>  vrigBuffers;
    UniquePtrVec<AzVulk::BufferData>  irigBuffers;
    void createRigMeshBuffers();

    // Skeleton data - Resources: SharedPtr, Buffers & Descriptors: UniquePtr
    SharedPtrVec<RigSkeleton>         rigSkeletons;
    UniquePtrVec<AzVulk::BufferData>  rigSkeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescLayout>     rigSkeleDescLayout;
    UniquePtr<AzVulk::DescPool>       rigSkeleDescPool;
    UniquePtrVec<AzVulk::DescSets>    rigSkeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

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
    void createTextureSamplers();

    // Texture Image - Resources: SharedPtr, Descriptors: UniquePtr
    SharedPtrVec<Texture>             textures;
    UniquePtr<AzVulk::DescLayout>     texDescLayout;
    UniquePtr<AzVulk::DescPool>       texDescPool;
    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Useful methods
    SharedPtr<Texture> createTexture(const TinyTexture& tinyTexture, uint32_t mipLevels = 0);

    uint32_t getStaticIndexCount(size_t index) const { return static_cast<uint32_t>(staticMeshes[index]->indices.size()); }
    uint32_t getRiggedIndexCount(size_t index) const { return static_cast<uint32_t>(rigMeshes[index]->indices.size()); }

    VkBuffer getStaticVertexBuffer(size_t index) const { return vstaticBuffers[index]->buffer; }
    VkBuffer getStaticIndexBuffer(size_t index) const { return istaticBuffers[index]->buffer; }

    VkBuffer getRiggedVertexBuffer(size_t index) const { return vrigBuffers[index]->buffer; }
    VkBuffer getRiggedIndexBuffer(size_t index) const { return irigBuffers[index]->buffer; }


    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> staticMeshNameToIndex;
    UnorderedMap<std::string, size_t> rigMeshNameToIndex;
    UnorderedMap<std::string, size_t> rigNameToIndex;

    // Maps to track duplicate counts for automatic renaming
    UnorderedMap<std::string, size_t> textureNameCounts;
    UnorderedMap<std::string, size_t> materialNameCounts;
    UnorderedMap<std::string, size_t> staticMeshNameCounts;
    UnorderedMap<std::string, size_t> rigMeshNameCounts;
    UnorderedMap<std::string, size_t> rigNameCounts;

private:
    // Helper function to generate unique names with suffixes
    std::string getUniqueName(const std::string& baseName, UnorderedMap<std::string, size_t>& nameCounts);
};

} // namespace Az3D
