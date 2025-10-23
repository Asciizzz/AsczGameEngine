#pragma once

#include "TinyData/TinyVertex.hpp"
#include "TinyExt/TinyHandle.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"

#include <string>

struct TinySubmesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    TinyHandle material;
};

// Uniform mesh structure that holds raw data only
struct TinyMesh {
    TinyMesh() = default;

    TinyMesh(const TinyMesh&) = delete;
    TinyMesh& operator=(const TinyMesh&) = delete;

    TinyMesh(TinyMesh&&) = default;
    TinyMesh& operator=(TinyMesh&&) = default;

    std::string name; // Mesh name from glTF

    TinyVertexLayout vertexLayout;
    std::vector<uint8_t> vertexData; // raw bytes
    size_t vertexCount = 0;

    std::vector<uint8_t> indexData; // raw bytes
    size_t indexCount = 0;
    size_t indexStride = 0;

    std::vector<TinySubmesh> submeshes;

    TinyMesh& setSubmeshes(const std::vector<TinySubmesh>& subs);

    TinyMesh& addSubmesh(const TinySubmesh& sub);
    TinyMesh& writeSubmesh(const TinySubmesh& sub, uint32_t index);

    template<typename VertexT>
    TinyMesh& setVertices(const std::vector<VertexT>& verts) {
        vertexCount = verts.size();
        vertexLayout = VertexT::getLayout();

        vertexData.resize(vertexCount * sizeof(VertexT));
        std::memcpy(vertexData.data(), verts.data(), vertexData.size());

        return *this;
    }

    template<typename IndexT>
    TinyMesh& setIndices(const std::vector<IndexT>& idx) {
        indexCount = idx.size();
        indexStride = sizeof(IndexT);
        indexType = sizeToIndexType(indexStride);

        indexData.resize(indexCount * indexStride);
        std::memcpy(indexData.data(), idx.data(), indexData.size());

        return *this;
    }

    // BETA! AABB
    glm::vec3 abMin = glm::vec3(0.0f);
    glm::vec3 abMax = glm::vec3(0.0f);
    void updateAABB(const glm::vec3& point) {
        abMin = glm::min(abMin, point);
        abMax = glm::max(abMax, point);
    }

    // Buffers for runtime use
    TinyVK::DataBuffer vertexBuffer;
    TinyVK::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT16;

    bool vkCreate(const TinyVK::Device* deviceVK);
    static VkIndexType sizeToIndexType(size_t size);
};