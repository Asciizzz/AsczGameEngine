#pragma once

#include "TinyExt/TinyPool.hpp"
#include "TinyExt/TinyHandle.hpp"

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