#pragma once

#include "TinyData/TinyModel.hpp"

#include "TinyVK/Pipeline_manager.hpp"
#include "TinyVK/DataBuffer.hpp"
#include "TinyVK/Descriptor.hpp"
#include "TinyVK/TextureVK.hpp"

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

    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32; // Default to uint32

    std::vector<TinySubmesh> submeshes;
    std::vector<int> meshMaterials; // Point to global material index

    void fromMesh(const TinyVK::DeviceVK* deviceVK, const TinyMesh& mesh, const std::vector<int>& meshMats);
    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct ModelVK {
    // Deleted copy constructor and assignment
    ModelVK() = default;
    ModelVK(const ModelVK&) = delete;
    ModelVK& operator=(const ModelVK&) = delete;
    
    MeshVK mesh;

    // All material of this mesh
    TinyVK::DataBuffer matBuffer;
    TinyVK::DescSet matDescSet;

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
    ResourceGroup(TinyVK::DeviceVK* deviceVK);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    // Descriptor's getters
    VkDescriptorSetLayout getMatDescLayout() const { return *matDescLayout; }

    VkDescriptorSetLayout getTexDescLayout() const { return texDescSet->getLayout(); }
    VkDescriptorSet getTexDescSet() const { return texDescSet->get(); }

    size_t addModel(const TinyModel& model) {
        models.push_back(model);
        return models.size() - 1;
    }

    void uploadAllToGPU();

    void createComponentVKsFromModels();

// private:
    TinyVK::DeviceVK* deviceVK;

    TinyVK::PipelineManager pipelines;


    std::vector<TinyModel>            models;
    UniquePtrVec<ModelVK>             modelVKs;
    UniquePtrVec<TinyVK::TextureVK>   textures;

    // Shared pool and layout for all models
    UniquePtr<TinyVK::DescPool>       skeleDescPool;
    UniquePtr<TinyVK::DescLayout>     skeleDescLayout;

    UniquePtr<TinyVK::DescPool>       matDescPool;
    UniquePtr<TinyVK::DescLayout>     matDescLayout;
    void createMaterialDescPoolAndLayout();

    void createMaterialDescSet(const std::vector<MaterialVK>& materials, ModelVK& modelVK);

    // Global list of all textures
    UniquePtr<TinyVK::DescPool>       texDescPool;
    UniquePtr<TinyVK::DescLayout>     texDescLayout;
    UniquePtr<TinyVK::DescSet>        texDescSet;
    void createTextureDescSet();

    // Useful methods
    UniquePtr<TinyVK::TextureVK> createTexture(const TinyTexture& texture);
};

} // namespace TinyEngine
