#pragma once

#include "TinyEngine/TinyLoader.hpp"
#include "TinyEngine/TinyPool.hpp"

#include "AzVulk/TextureVK.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"

struct TinyMeshVK {
    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    std::vector<TinySubmesh> submeshes;
    void import(const TinyMesh& mesh, const AzVulk::DeviceVK* deviceVK);

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct TinyMaterialVK {

    // Modifiable
    struct Data {
        glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved
    } data;

    AzVulk::DataBuffer matBuffer; // Mapable
    AzVulk::DescSet matDescSet;

    void import(const TinyMaterialVK::Data& matData, const AzVulk::DeviceVK* deviceVK,
                VkDescriptorSetLayout layout, VkDescriptorPool pool);

    void setAlbedoTextureIndex(uint32_t index) {
        data.texIndices.x = index;
        matBuffer.copyData(&data);
    }

    void setNormalTextureIndex(uint32_t index) { 
        data.texIndices.y = index;
        matBuffer.copyData(&data);
    }
};

struct TinyTextureVK {
    AzVulk::TextureVK texture;
    AzVulk::DescSet descSet;

    void import(const TinyTexture& tex, const AzVulk::DeviceVK* deviceVK,
                VkDescriptorSetLayout layout, VkDescriptorPool pool);
};

struct TinySkeletonVK {
    AzVulk::DataBuffer invBindMatrixBuffer;
    AzVulk::DescSet skeleDescSet;
};

class TinyResource {
public:
    TinyResource(const AzVulk::DeviceVK* deviceVK): deviceVK(deviceVK) {
        setMaxTextureCount(4096);
        setMaxMaterialCount(4096);
    }

    // Require rework of descriptor sets and bindings
    void setMaxTextureCount(uint32_t count);
    void setMaxMaterialCount(uint32_t count);

    uint32_t getMaxTextureCount() const { return maxTextureCount; }
    uint32_t getMaxMaterialCount() const { return maxMaterialCount; }

private:
    const AzVulk::DeviceVK* deviceVK;

    uint32_t maxTextureCount = 4096;
    uint32_t maxMaterialCount = 4096;

    // Shared descriptor resources
    UniquePtr<AzVulk::DescLayout> matDescLayout;
    UniquePtr<AzVulk::DescPool>   matDescPool;
    void createMaterialDescResources(uint32_t count = 1);

    UniquePtr<AzVulk::DescLayout> texDescLayout;
    UniquePtr<AzVulk::DescPool>   texDescPool;
    void createTextureDescResources(uint32_t count = 1);

    // Resource pools registry
    TinyPoolPtr<TinyMeshVK>     meshes;
    TinyPoolPtr<TinyMaterialVK> materials;
    TinyPoolPtr<TinyTextureVK>  textures;
    TinyPoolPtr<TinySkeletonVK> skeletons;
    
};