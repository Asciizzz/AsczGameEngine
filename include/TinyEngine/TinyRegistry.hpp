#pragma once

#include "TinyEngine/TinyPool.hpp"
#include "TinyEngine/TinyHandle.hpp"

#include "TinyData/TinyModel.hpp"

#include "AzVulk/TextureVK.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"

class TinyRegistry { // For raw resource data
public:
    struct MeshData {
        AzVulk::DataBuffer vertexBuffer;
        AzVulk::DataBuffer indexBuffer;
        std::vector<TinySubmesh> submeshes;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;

        bool import(const AzVulk::DeviceVK* deviceVK, const TinyMesh& mesh);

        static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
    };

    struct MaterialData {
        glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved

        void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
        void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
    };

    struct TextureData {
        AzVulk::TextureVK textureVK;
        bool import(const AzVulk::DeviceVK* deviceVK, const TinyTexture& texture);
    };

    TinyRegistry(const AzVulk::DeviceVK* deviceVK);

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    uint32_t getMaxTextureCount() const { return maxTextureCount; }
    uint32_t getMaxMaterialCount() const { return maxMaterialCount; }
    uint32_t getMaxMeshCount() const { return maxMeshCount; }
    uint32_t getMaxSkeletonCount() const { return maxSkeletonCount; }
    uint32_t getMaxNodeCount() const { return maxNodeCount; }

    TinyHandle addMesh(const TinyMesh& mesh);
    TinyHandle addTexture(const TinyTexture& texture);
    TinyHandle addMaterial(const MaterialData& matData);
    TinyHandle addSkeleton(const TinySkeleton& skeleton);
    TinyHandle addNode(const TinyNode& node);

    // Access to resources - allow modification
    MeshData*     getMeshData(const TinyHandle& handle);
    MaterialData* getMaterialData(const TinyHandle& handle);
    TextureData*  getTextureData(const TinyHandle& handle);
    TinySkeleton* getSkeletonData(const TinyHandle& handle);
    TinyNode*     getNodeData(const TinyHandle& handle);

private:
    const AzVulk::DeviceVK* deviceVK;

    uint32_t maxMeshCount = 1024;
    uint32_t maxTextureCount = 1024;
    uint32_t maxMaterialCount = 1024;
    uint32_t maxSkeletonCount = 256;
    uint32_t maxNodeCount = 2048;

    void initVkResources();

    // Shared descriptor resources

    // All materials in a buffer
    UniquePtr<AzVulk::DescLayout> matDescLayout;
    UniquePtr<AzVulk::DescPool>   matDescPool;
    UniquePtr<AzVulk::DataBuffer> matBuffer;
    UniquePtr<AzVulk::DescSet>    matDescSet;
    void createMaterialVkResources();

    // All textures
    UniquePtr<AzVulk::DescLayout> texDescLayout;
    UniquePtr<AzVulk::DescPool>   texDescPool;
    UniquePtr<AzVulk::DescSet>    texDescSet;
    void createTextureVkResources();

    // Resource pools registry
    TinyPoolPtr<MeshData>     meshDatas;
    TinyPoolRaw<MaterialData> materialDatas;
    TinyPoolPtr<TextureData>  textureDatas;
    TinyPoolRaw<TinySkeleton> skeletonDatas;
    TinyPoolRaw<TinyNode>     nodeDatas;
};