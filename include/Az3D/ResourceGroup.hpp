#pragma once

#include "Tiny3D/TinyModel.hpp"

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"

namespace Az3D {

// Vulkan texture resource (image + view + memory only)
struct TextureVK {
    VkImage image         = VK_NULL_HANDLE;
    VkImageView view      = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct MaterialVK {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albTexIndex, nrmlTexIndex, unused, unused>
};

struct SubmeshVK {
    AzVulk::BufferData vertexBuffer;
    AzVulk::BufferData indexBuffer;
    VkIndexType indexType;
};

// Index pointing to certain Vulkan elements
struct ModelVK {
    std::vector<size_t> submeshVK_indices;

    std::vector<uint32_t> materialVK_indices;
    size_t skeletonIndex = -1;

    // Some helpers
    std::vector<uint32_t> submesh_indexCounts; // Cached index counts for each submesh
    size_t submeshCount() const { return submeshVK_indices.size(); }
};

// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* vkDevice);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout->get(); }
    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }
    
    VkDescriptorSetLayout getTexDescLayout() const { return texDescLayout->get(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return skeleDescLayout->get(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return skeleDescSets[index]->get(); }

    size_t addModel(const TinyModel& model) {
        models.push_back(model);
        return models.size() - 1;
    }

    void uploadAllToGPU();

    void createComponentVKsFromModels();

// private:
    AzVulk::Device* vkDevice;

    std::vector<TinyModel>            models;
    std::vector<ModelVK>              modelVKs; // Contain indices to vulkan resources
    std::vector<MaterialVK>           materialVKs; // Very different from TinyMaterial
    UniquePtrVec<SubmeshVK>           submeshVKs;
    UniquePtrVec<TextureVK>           textureVKs;

    size_t addSubmeshVK(const TinySubmesh& submesh);
    VkBuffer getSubmeshVertexBuffer(size_t submeshVK_index) const;
    VkBuffer getSubmeshIndexBuffer(size_t submeshVK_index) const;
    VkIndexType getSubmeshIndexType(size_t submeshVK_index) const;

    SharedPtrVec<TinySkeleton>        skeletons;
    UniquePtrVec<AzVulk::BufferData>  skeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescLayout>     skeleDescLayout;
    UniquePtr<AzVulk::DescPool>       skeleDescPool;
    UniquePtrVec<AzVulk::DescSets>    skeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

    UniquePtr<AzVulk::BufferData>     matBuffer;
    UniquePtr<AzVulk::DescLayout>     matDescLayout;
    UniquePtr<AzVulk::DescPool>       matDescPool;
    UniquePtr<AzVulk::DescSets>       matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    std::vector<VkSampler>            samplers;
    void createTextureSamplers();

    std::vector<uint32_t>             texSamplerIndices; // Per-texture sampler index (maps to samplers array)
    UniquePtr<AzVulk::BufferData>     textSampIdxBuffer; // Buffer containing sampler indices for each texture
    void createTexSampIdxBuffer(); // Create buffer for texture sampler indices

    UniquePtr<AzVulk::DescLayout>     texDescLayout;
    UniquePtr<AzVulk::DescPool>       texDescPool;
    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Useful methods
    UniquePtr<TextureVK> createTextureVK(const TinyTexture& texture);
};

} // namespace Az3D
