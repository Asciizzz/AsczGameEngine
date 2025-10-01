#pragma once

#include "TinyEngine/TinyPool.hpp"
#include "TinyEngine/TinyHandle.hpp"

#include "TinyData/TinyModel.hpp"

#include "TinyVK/TextureVK.hpp"
#include "TinyVK/Descriptor.hpp"
#include "TinyVK/DataBuffer.hpp"

struct TinyRMesh {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Mesh;

    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    std::vector<TinySubmesh> submeshes;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    bool import(const TinyVK::DeviceVK* deviceVK, const TinyMesh& mesh);

    static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
};

struct TinyRMaterial {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Material;

    glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved

    void setAlbTexIndex(uint32_t index) { texIndices.x = index; }
    void setNrmlTexIndex(uint32_t index) { texIndices.y = index; }
};

struct TinyRTexture {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Texture;

    TinyVK::TextureVK textureVK;
    bool import(const TinyVK::DeviceVK* deviceVK, const TinyTexture& texture);
};

struct TinyRSkeleton {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Skeleton;

    std::vector<TinyBone> bones;
};

struct TinyRNode : public TinyNode {
    constexpr static TinyHandle::Type kType = TinyHandle::Type::Node;
};


class TinyRegistry { // For raw resource data
public:
    TinyRegistry(const TinyVK::DeviceVK* deviceVK);

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    uint32_t getPoolCapacity(TinyHandle::Type type) const;

    TinyHandle addMesh(const TinyMesh& mesh);
    TinyHandle addTexture(const TinyTexture& texture);
    TinyHandle addMaterial(const TinyRMaterial& matData);
    TinyHandle addSkeleton(const TinyRSkeleton& skeleton);
    TinyHandle addNode(const TinyRNode& node);

    template<typename T>
    T* get(const TinyHandle& handle) {
        // Clean the type
        if (!handle.isType(T::kType)) return nullptr;

        // Only allow correct type
        if constexpr (std::is_same_v<T, TinyRMesh>)
            return meshDatas.get(handle.index);
        else if constexpr (std::is_same_v<T, TinyRMaterial>)
            return materialDatas.get(handle.index);
        else if constexpr (std::is_same_v<T, TinyRTexture>)
            return textureDatas.get(handle.index);
        else if constexpr (std::is_same_v<T, TinyRSkeleton>)
            return skeletonDatas.get(handle.index);
        else if constexpr (std::is_same_v<T, TinyRNode>)
            return nodeDatas.get(handle.index);
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
    const TinyVK::DeviceVK* deviceVK;

    void resizeCheck();

    void initVkResources();

    // Shared descriptor resources

    // All materials in a buffer
    UniquePtr<TinyVK::DescLayout> matDescLayout;
    UniquePtr<TinyVK::DescPool>   matDescPool;
    UniquePtr<TinyVK::DataBuffer> matBuffer;
    UniquePtr<TinyVK::DescSet>    matDescSet;
    void createMaterialVkResources();

    // All textures
    UniquePtr<TinyVK::DescLayout> texDescLayout;
    UniquePtr<TinyVK::DescPool>   texDescPool;
    UniquePtr<TinyVK::DescSet>    texDescSet;
    void createTextureVkResources();

    // Resource pools registry
    TinyPoolPtr<TinyRMesh>     meshDatas;
    TinyPoolRaw<TinyRMaterial> materialDatas;
    TinyPoolPtr<TinyRTexture>  textureDatas;
    TinyPoolRaw<TinyRSkeleton> skeletonDatas;
    TinyPoolRaw<TinyRNode>     nodeDatas;
};

