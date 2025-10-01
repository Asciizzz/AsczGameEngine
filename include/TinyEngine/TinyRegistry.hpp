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

    bool import(const TinyVK::Device* deviceVK, const TinyMesh& mesh);

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
    bool import(const TinyVK::Device* deviceVK, const TinyTexture& texture);
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
    TinyRegistry(const TinyVK::Device* deviceVK);

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    template<typename T>
    TinyHandle add(T& data) {
        uint32_t index = pool<T>().insert(std::move(data));

        resizeCheck();
        return TinyHandle(index, T::kType);
    }

    template<typename T>
    T* get(const TinyHandle& handle) {
        // Clean the type
        if (!handle.isType(T::kType)) return nullptr;

        return pool<T>().get(handle.index);
    }

    template<typename T>
    uint32_t poolCapacity() const {
        return pool<T>().capacity;
    }


    void printDataCounts() const {
        printf("TinyRegistry Data Counts:\n");
        printf("  Meshes:    %u / %u\n", pool<TinyRMesh>().count, pool<TinyRMesh>().capacity);
        printf("  Textures:  %u / %u\n", pool<TinyRTexture>().count, pool<TinyRTexture>().capacity);
        printf("  Materials: %u / %u\n", pool<TinyRMaterial>().count, pool<TinyRMaterial>().capacity);
        printf("  Skeletons: %u / %u\n", pool<TinyRSkeleton>().count, pool<TinyRSkeleton>().capacity);
        printf("  Nodes:     %u / %u\n", pool<TinyRNode>().count, pool<TinyRNode>().capacity);
    }

    VkDescriptorSetLayout getMaterialDescSetLayout() const { return *matDescLayout; }
    VkDescriptorSet getMaterialDescSet() const { return *matDescSet; }

    VkDescriptorSetLayout getTextureDescSetLayout() const { return *texDescLayout; }
    VkDescriptorSet getTextureDescSet() const { return *texDescSet; }

private:
    const TinyVK::Device* deviceVK;

    std::tuple<
        TinyPool<TinyRMesh>,
        TinyPool<TinyRMaterial>,
        TinyPool<TinyRTexture>,
        TinyPool<TinyRSkeleton>,
        TinyPool<TinyRNode>
    > pools;

    template<typename T>
    TinyPool<T>& pool() {
        return std::get<TinyPool<T>>(pools);
    }

    template<typename T>
    const TinyPool<T>& pool() const {
        return std::get<TinyPool<T>>(pools);
    }

    void resizeCheck();

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
};

