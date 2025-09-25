#pragma once

#include "Tiny3D/TinyLoader.hpp"
#include "Tiny3D/TinyPool.hpp"

#include "AzVulk/TextureVK.hpp"
#include "AzVulk/Descriptor.hpp"
#include "AzVulk/DataBuffer.hpp"

struct TinyMaterialVK {
    AzVulk::DataBuffer matBuffer; // Mapable
    AzVulk::DescSet matDescSet;
};

struct TinyMeshVK {
    AzVulk::DataBuffer vertexBuffer;
    AzVulk::DataBuffer indexBuffer;

    std::vector<TinySubmesh> submeshes;
    std::vector<int> submeshMaterials; // Point to global material index
};

struct TinySkeletonVK {
    AzVulk::DataBuffer invBindMatrixBuffer;
    AzVulk::DescSet skeleDescSet;
};

class TinyResource {
public:

    // Require rework of descriptor sets and bindings
    void setMaxTextureCount(uint32_t count);
    void setMaxMaterialCount(uint32_t count);

    uint32_t getMaxTextureCount() const { return maxTextureCount; }
    uint32_t getMaxMaterialCount() const { return maxMaterialCount; }

private: 
    uint32_t maxTextureCount = 4096;
    uint32_t maxMaterialCount = 4096;

    TinyPoolPtr<TinyMaterialVK> materialPool;
    TinyPoolPtr<TinyMeshVK>     meshPool;
};