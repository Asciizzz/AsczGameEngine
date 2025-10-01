#pragma once

#include "TinyEngine/TinyPool.hpp"
#include "TinyEngine/TinyHandle.hpp"

#include "TinyData/TinyModel.hpp"

#include "AzVulk/TextureVK.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"

class TinyRegistry { // For raw resource data
public:
    struct RMesh {
        constexpr static TinyHandle::Type kType = TinyHandle::Type::Mesh;

        AzVulk::DataBuffer vertexBuffer;
        AzVulk::DataBuffer indexBuffer;
        std::vector<TinySubmesh> submeshes;
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;

        bool import(const AzVulk::DeviceVK* deviceVK, const TinyMesh& mesh);

        static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
    };

    struct RMaterial {
        constexpr static TinyHandle::Type kType = TinyHandle::Type::Material;

        glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved

        void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
        void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
    };

    struct RTexture {
        constexpr static TinyHandle::Type kType = TinyHandle::Type::Texture;

        AzVulk::TextureVK textureVK;
        bool import(const AzVulk::DeviceVK* deviceVK, const TinyTexture& texture);
    };

    struct RSkeleton {
        constexpr static TinyHandle::Type kType = TinyHandle::Type::Skeleton;

        std::vector<TinyBone> bones;
    };

    struct RNode : public TinyNode {
        constexpr static TinyHandle::Type kType = TinyHandle::Type::Node;
    };

    TinyRegistry(const AzVulk::DeviceVK* deviceVK);

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    uint32_t getMaxTextureCount() const { return textureDatas.capacity; }
    uint32_t getMaxMaterialCount() const { return materialDatas.capacity; }
    uint32_t getMaxMeshCount() const { return meshDatas.capacity; }
    uint32_t getMaxSkeletonCount() const { return skeletonDatas.capacity; }
    uint32_t getMaxNodeCount() const { return nodeDatas.capacity; }

    TinyHandle addMesh(const TinyMesh& mesh);
    TinyHandle addTexture(const TinyTexture& texture);
    TinyHandle addMaterial(const RMaterial& matData);
    TinyHandle addSkeleton(const RSkeleton& skeleton);
    TinyHandle addNode(const RNode& node);

    // Access to resources - allow modification
    RMesh*     getMeshData(const TinyHandle& handle);
    RMaterial* getMaterialData(const TinyHandle& handle);
    RTexture*  getTextureData(const TinyHandle& handle);
    RSkeleton* getSkeletonData(const TinyHandle& handle);
    RNode*     getNodeData(const TinyHandle& handle);


    void printDataCounts() const {
        printf("TinyRegistry Data Counts:\n");
        printf("  Meshes:    %u / %u\n", meshDatas.count, meshDatas.capacity);
        printf("  Textures:  %u / %u\n", textureDatas.count, textureDatas.capacity);
        printf("  Materials: %u / %u\n", materialDatas.count, materialDatas.capacity);
        printf("  Skeletons: %u / %u\n", skeletonDatas.count, skeletonDatas.capacity);
        printf("  Nodes:     %u / %u\n", nodeDatas.count, nodeDatas.capacity);
    }

private:
    const AzVulk::DeviceVK* deviceVK;

    size_t prevMaterialCapacity = 128;
    size_t prevTextureCapacity  = 128;
    size_t prevMeshCapacity     = 128;
    size_t prevSkeletonCapacity = 128;
    size_t prevNodeCapacity     = 128;
    void resizeCheck();

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
    TinyPoolPtr<RMesh>     meshDatas;
    TinyPoolRaw<RMaterial> materialDatas;
    TinyPoolPtr<RTexture>  textureDatas;
    TinyPoolRaw<RSkeleton> skeletonDatas;
    TinyPoolRaw<RNode>     nodeDatas;
};