#pragma once

#include "Tiny3D/TinyModel.hpp"

#include "AzVulk/DataBuffer.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/ImageWrapper.hpp"

namespace Az3D {

struct MaterialVK {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albTexIndex, nrmlTexIndex, unused, unused>
};

struct SubmeshVK {
    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
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
    ResourceGroup(AzVulk::Device* deviceVK);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return matDescSet->getLayout(); }
    VkDescriptorSet getMatDescSet() const { return matDescSet->get(); }

    VkDescriptorSetLayout getTexDescLayout() const { return texDescSet->getLayout(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return skeleDescSets->getLayout(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return skeleDescSets->get(index); }

    size_t addModel(const TinyModel& model) {
        models.push_back(model);
        return models.size() - 1;
    }

    void uploadAllToGPU();

    void createComponentVKsFromModels();

// private:
    AzVulk::Device* deviceVK;

    std::vector<TinyModel>            models;
    std::vector<ModelVK>              modelVKs; // Contain indices to vulkan resources
    std::vector<MaterialVK>           materialVKs; // Very different from TinyMaterial
    UniquePtrVec<SubmeshVK>           submeshVKs;
    std::vector<AzVulk::ImageWrapper> textures;

    size_t addSubmeshVK(const TinySubmesh& submesh);
    VkBuffer getSubmeshVertexBuffer(size_t submeshVK_index) const;
    VkBuffer getSubmeshIndexBuffer(size_t submeshVK_index) const;
    VkIndexType getSubmeshIndexType(size_t submeshVK_index) const;

    SharedPtrVec<TinySkeleton>        skeletons;
    UniquePtrVec<AzVulk::DataBuffer>  skeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescSets>       skeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

    UniquePtr<AzVulk::DataBuffer>     matBuffer;
    UniquePtr<AzVulk::DescSets>       matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    std::vector<VkSampler>            samplers;
    void createTextureSamplers();

    std::vector<uint32_t>             texSamplerIndices; // Per-texture sampler index (maps to samplers array)
    UniquePtr<AzVulk::DataBuffer>     textSampIdxBuffer; // Buffer containing sampler indices for each texture
    void createTexSampIdxBuffer(); // Create buffer for texture sampler indices

    UniquePtr<AzVulk::DescSets>       texDescSet;
    void createTextureDescSet();

    // Useful methods
    AzVulk::ImageWrapper createTexture(const TinyTexture& texture);
};

} // namespace Az3D
