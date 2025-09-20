#include "Tiny3D/TinyMesh.hpp"
#include "AzVulk/DataBuffer.hpp"

struct SubmeshVK {
    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32; // Default to uint32

    SubmeshVK() = default;

    SubmeshVK(const SubmeshVK&) = delete;
    SubmeshVK& operator=(const SubmeshVK&) = delete;

    void create(const AzVulk::Device* deviceVK, const TinySubmesh& submesh);

    static VkIndexType tinyToVkIndexType(TinySubmesh::IndexType type);
};

struct MeshVK {
    std::vector<SubmeshVK> submeshVKs;
};