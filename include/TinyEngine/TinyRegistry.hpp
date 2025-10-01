#pragma once

#include "TinyEngine/TinyPool.hpp"
#include "TinyEngine/TinyHandle.hpp"

#include "TinyData/TinyModel.hpp"

#include "AzVulk/TextureVK.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"

struct TinyRMesh {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Mesh;
    using pType = TinyPoolPtr<TinyRMesh>;

    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
    std::vector<TinySubmesh> submeshes;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    bool import(const AzVulk::DeviceVK* deviceVK, const TinyMesh& mesh);

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct TinyRMaterial {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Material;
    using pType = TinyPoolRaw<TinyRMaterial>;

    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};

struct TinyRTexture {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Texture;
    using pType = TinyPoolPtr<TinyRTexture>;

    AzVulk::TextureVK textureVK;
    bool import(const AzVulk::DeviceVK* deviceVK, const TinyTexture& texture);
};

struct TinyRSkeleton {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Skeleton;
    using pType = TinyPoolRaw<TinyRSkeleton>;

    std::vector<TinyBone> bones;
};

struct TinyRNode : public TinyNode {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Node;
    using pType = TinyPoolRaw<TinyRNode>;
};


class TinyRegistry { // For raw resource data
public:
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
    TinyHandle addMaterial(const TinyRMaterial& matData);
    TinyHandle addSkeleton(const TinyRSkeleton& skeleton);
    TinyHandle addNode(const TinyRNode& node);

    // Access to resources - allow modification
    TinyRMesh*     getMeshData(const TinyHandle& handle);
    TinyRMaterial* getMaterialData(const TinyHandle& handle);
    TinyRTexture*  getTextureData(const TinyHandle& handle);
    TinyRSkeleton* getSkeletonData(const TinyHandle& handle);
    TinyRNode*     getNodeData(const TinyHandle& handle);

    template<typename T>
    T* get(const TinyHandle& handle) {
        // Clean the type
        if (!handle.isType(T::kType)) return nullptr;
        
        auto getPtrFromPool = [&](auto& pool) -> T* {
            using PoolType = std::decay_t<decltype(pool)>;
            if constexpr (std::is_same_v<PoolType, TinyPoolPtr<T>>) {
                return pool.getPtr(handle.index); // already a pointer
            } else if constexpr (std::is_same_v<PoolType, TinyPoolRaw<T>>) {
                return &pool.get(handle.index);   // wrap reference as pointer
            } else {
                static_assert(sizeof(PoolType) == 0, "Unsupported pool type");
            }
        };

        // Only allow correct type
        if constexpr (std::is_same_v<T, TinyRMesh>)
            return getPtrFromPool(meshDatas);
        else if constexpr (std::is_same_v<T, TinyRMaterial>)
            return getPtrFromPool(materialDatas);
        else if constexpr (std::is_same_v<T, TinyRTexture>)
            return getPtrFromPool(textureDatas);
        else if constexpr (std::is_same_v<T, TinyRSkeleton>)
            return getPtrFromPool(skeletonDatas);
        else if constexpr (std::is_same_v<T, TinyRNode>)
            return getPtrFromPool(nodeDatas);
        else
            static_assert(sizeof(T) == 0, "Unsupported type for get<T>");
    }


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
    TinyPoolPtr<TinyRMesh>     meshDatas;
    TinyPoolRaw<TinyRMaterial> materialDatas;
    TinyPoolPtr<TinyRTexture>  textureDatas;
    TinyPoolRaw<TinyRSkeleton> skeletonDatas;
    TinyPoolRaw<TinyRNode>     nodeDatas;
};

