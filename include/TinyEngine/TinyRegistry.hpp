#pragma once

#include "TinyEngine/TinyLoader.hpp"
#include "TinyEngine/TinyPool.hpp"

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

        void import(const TinyMesh& mesh, const AzVulk::DeviceVK* deviceVK);

        static VkIndexType tinyToVkIndexType(TinyMesh::IndexType type);
    };

    struct MaterialData {
        glm::uvec4 texIndices = glm::uvec4(0); // Albedo, Normal, Reserved, Reserved
    };

    struct TextureData {
        AzVulk::TextureVK texture;
        void import(const AzVulk::DeviceVK* deviceVK, const TinyTexture& texture);
    };



    TinyRegistry(const AzVulk::DeviceVK* deviceVK): deviceVK(deviceVK) {
        setMaxTextureCount(maxTextureCount);
        setMaxMaterialCount(maxMaterialCount);
    }

    // Require rework of descriptor sets and bindings
    void setMaxTextureCount(uint32_t count);
    void setMaxMaterialCount(uint32_t count);

    uint32_t getMaxTextureCount() const { return maxTextureCount; }
    uint32_t getMaxMaterialCount() const { return maxMaterialCount; }

    uint32_t addMesh(const TinyMesh& mesh) {
        UniquePtr<MeshData> meshData = MakeUnique<MeshData>();
        meshData->import(mesh, deviceVK);

        return meshDatas.insert(std::move(meshData));
    }

    uint32_t addTexture(const TinyTexture& texture) {
        UniquePtr<TextureData> textureData = MakeUnique<TextureData>();
        textureData->import(deviceVK, texture);

        return textureDatas.insert(std::move(textureData));
    }

    // Usually you need to know the texture beforehand to remap the material texture indices
    uint32_t addMaterial(const MaterialData& matData) {
        return materialDatas.insert(matData);
    }

private:
    const AzVulk::DeviceVK* deviceVK;

    uint32_t maxTextureCount = 1024;
    uint32_t maxMaterialCount = 1024;

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
};