#pragma once

#include "TinyData/TinyModel.hpp"

#include "AzVulk/Pipeline_manager.hpp"
#include "AzVulk/DataBuffer.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/TextureVK.hpp"

#include "TinyEngine/TinyProject.hpp"

// ResourceGroup soon to be deperecated

namespace TinyEngine {

struct MaterialVK {
    glm::vec4 shadingParams = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
    glm::uvec4 texIndices = glm::uvec4(0, 0, 0, 0); // <albTexIndex, nrmlTexIndex, unused, unused>
};

struct MeshVK {
    MeshVK() = default;
    MeshVK(const MeshVK&) = delete;
    MeshVK& operator=(const MeshVK&) = delete;

    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32; // Default to uint32

    std::vector<TinySubmesh> submeshes;
    std::vector<int> meshMaterials; // Point to global material index

    void fromMesh(const AzVulk::DeviceVK* deviceVK, const TinyMesh& mesh, const std::vector<int>& meshMats);
    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct ModelVK {
    // Deleted copy constructor and assignment
    ModelVK() = default;
    ModelVK(const ModelVK&) = delete;
    ModelVK& operator=(const ModelVK&) = delete;
    
    MeshVK mesh;

    // All material of this mesh
    AzVulk::DataBuffer matBuffer;
    AzVulk::DescSet matDescSet;

    // No skeleton data yet since we are doing CPU skinning for now
};

struct LightVK {
    glm::vec4 position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // xyz = position, w = light type (0=directional, 1=point, 2=spot)
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // rgb = color, a = intensity
    glm::vec4 direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f); // xyz = direction (for directional/spot), w = range
    glm::vec4 params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // x = inner cone angle, y = outer cone angle, z = attenuation, w = unused
};


// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::DeviceVK* deviceVK);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return *matDescLayout; }

    VkDescriptorSetLayout getTexDescLayout() const { return texDescSet->getLayout(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    VkDescriptorSetLayout getRigDescLayout() const { return skeleDescSets->getLayout(); }
    VkDescriptorSet getRigSkeleDescSet(size_t index) const { return skeleDescSets->get(index); }

    VkDescriptorSetLayout getLightDescLayout() const { return lightDescSet->getLayout(); }
    VkDescriptorSet getLightDescSet() const { return lightDescSet->get(); }

    size_t addModel(const TinyModel& model) {
        models.push_back(model);
        return models.size() - 1;
    }

    // Light management
    size_t addLight(const LightVK& light) {
        lightVKs.push_back(light);
        lightsDirty = true;
        return lightVKs.size() - 1;
    }

    void updateLight(size_t index, const LightVK& light) {
        if (index < lightVKs.size()) {
            lightVKs[index] = light;
            lightsDirty = true;
        }
    }

    void removeLight(size_t index) {
        if (index < lightVKs.size()) {
            lightVKs.erase(lightVKs.begin() + index);
            lightsDirty = true;
        }
    }

    uint32_t getLightCount() const { return static_cast<uint32_t>(lightVKs.size()); }

    void updateLightBuffer(); // Update buffer when lights change

    void uploadAllToGPU();

    void createComponentVKsFromModels();

// private:
    AzVulk::DeviceVK* deviceVK;

    AzVulk::PipelineManager pipelines;


    std::vector<TinyModel>            models;
    UniquePtrVec<ModelVK>             modelVKs;
    UniquePtrVec<AzVulk::TextureVK>   textures;

    SharedPtrVec<TinySkeleton>        skeletons;
    UniquePtrVec<AzVulk::DataBuffer>  skeleInvMatBuffers; // Additional buffers in the future
    UniquePtr<AzVulk::DescSet>        skeleDescSets; // Wrong, but we'll live with it for now
    void createRigSkeleBuffers();
    void createRigSkeleDescSets();

    // Shared pool and layout for all models

    UniquePtr<AzVulk::DescPool>       skeleDescPool;
    UniquePtr<AzVulk::DescLayout>     skeleDescLayout;

    UniquePtr<AzVulk::DescPool>       matDescPool;
    UniquePtr<AzVulk::DescLayout>     matDescLayout;
    void createMaterialDescPoolAndLayout();

    void createMaterialDescSet(const std::vector<MaterialVK>& materials, ModelVK& modelVK);

    // Global list of all textures
    UniquePtr<AzVulk::DescSet>        texDescSet;
    void createTextureDescSet();

    // Light system
    std::vector<LightVK>              lightVKs; // Dynamic light data
    UniquePtr<AzVulk::DataBuffer>     lightBuffer; // Host-writable buffer for dynamic updates
    UniquePtr<AzVulk::DescSet>        lightDescSet;
    bool lightsDirty = false;
    void createLightBuffer();
    void createLightDescSet();

    // Useful methods
    UniquePtr<AzVulk::TextureVK> createTexture(const TinyTexture& texture);
};

} // namespace TinyEngine
