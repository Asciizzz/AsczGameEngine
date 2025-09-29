#include "TinyEngine/ResourceGroup.hpp"
#include "TinyEngine/TinyLoader.hpp"

#include "AzVulk/CmdBuffer.hpp"

#include <iostream>

using namespace AzVulk;
using namespace TinyEngine;

// ============================================================================
// ======================= General resource group stuff =======================
// ============================================================================

ResourceGroup::ResourceGroup(DeviceVK* deviceVK): deviceVK(deviceVK) {}

void ResourceGroup::cleanup() {
    VkDevice lDevice = deviceVK->lDevice;

    // TextureVK handles its own cleanup automatically
    textures.clear();

    // Ensure correct cleanup order for material
    modelVKs.clear();
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = deviceVK->lDevice;

    createMaterialDescPoolAndLayout();

    createComponentVKsFromModels();

    createTextureDescSet();
}

// ===========================================================================
// Special function to convert TinyModel to Vulkan resources with lookup table
// ===========================================================================


void ResourceGroup::createComponentVKsFromModels() {
    // Create default texture first
    TinyTexture defaultTex = TinyTexture::createDefaultTexture();
    auto defaultTexture = createTexture(std::move(defaultTex));
    textures.push_back(std::move(defaultTexture));

    for (auto& model : models) {
        UniquePtr<ModelVK> modelVK = MakeUnique<ModelVK>();

        std::vector<size_t> tempGlobalTextures; // A temporary list to convert local to global indices
        for (const auto& texture : model.textures) {
            auto tex = createTexture(texture);
            tempGlobalTextures.push_back(textures.size());

            textures.push_back(std::move(tex));
        }

        std::vector<MaterialVK> modelMaterialVKs;

        for (const auto& material : model.materials) {
            MaterialVK matVK{};

            // Shading params
            matVK.shadingParams.x = material.shading ? 1.0f : 0.0f;
            matVK.shadingParams.y = static_cast<float>(material.toonLevel);
            matVK.shadingParams.z = material.normalBlend;
            matVK.shadingParams.w = material.discardThreshold;

            // Texture indices - x for albedo, y for normal map
            bool validAlbTex = material.localAlbTexture >= 0 && material.localAlbTexture < static_cast<int>(model.textures.size());
            matVK.texIndices.x = validAlbTex ? static_cast<uint32_t>(tempGlobalTextures[material.localAlbTexture]) : 0;

            bool validNrmlTex = material.localNrmlTexture >= 0 && material.localNrmlTexture < static_cast<int>(model.textures.size());
            matVK.texIndices.y = validNrmlTex ? static_cast<uint32_t>(tempGlobalTextures[material.localNrmlTexture]) : 0;

            // z and w components unused for now
            matVK.texIndices.z = 0;
            matVK.texIndices.w = 0;

            modelMaterialVKs.push_back(matVK);
        }

        createMaterialDescSet(modelMaterialVKs, *modelVK);

        const TinyMesh& mesh = model.mesh;
        const auto& meshMaterials = model.meshMaterials;
        modelVK->mesh.fromMesh(deviceVK, mesh, meshMaterials);

        modelVKs.push_back(std::move(modelVK));
    }
}


// ============================================================================
// ========================= MATERIAL =========================================
// ============================================================================

void ResourceGroup::createMaterialDescPoolAndLayout() {
    VkDevice lDevice = deviceVK->lDevice;

    matDescPool = MakeUnique<DescPool>();
    matDescPool->create(lDevice, { {DescType::StorageBuffer, 1 } }, 1024);

    matDescLayout = MakeUnique<DescLayout>();
    matDescLayout->create(lDevice, { {0, DescType::StorageBuffer, 1, ShaderStage::VertexAndFragment, nullptr} } );
}

void ResourceGroup::createMaterialDescSet(const std::vector<MaterialVK>& materials, ModelVK& modelVK) {
    VkDevice lDevice = deviceVK->lDevice;

    // Create material buffer
    VkDeviceSize bufferSize = sizeof(MaterialVK) * materials.size();

    // Modifiable buffer
    modelVK.matBuffer
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(materials.data());

    printf("Created material buffer of size %llu bytes for %zu materials\n", bufferSize, materials.size());
    modelVK.matDescSet.allocate(lDevice, *matDescPool, *matDescLayout);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = modelVK.matBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    DescWrite()
        .setDstSet(modelVK.matDescSet)
        .setDescType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({materialBufferInfo})
        .updateDescSet(lDevice);
}

// ============================================================================
// ========================= TEXTURES =========================================
// ============================================================================

UniquePtr<AzVulk::TextureVK> ResourceGroup::createTexture(const TinyTexture& texture) {
    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(texture.channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertToValidData(
        texture.channels, texture.width, texture.height, texture.data.data());
    
    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (texture.channels == 3) ? 4 : texture.channels; // RGB becomes RGBA
    VkDeviceSize imageSize = texture.width * texture.height * vulkanChannels;

    if (texture.data.empty()) {
        throw std::runtime_error("Failed to load texture from TinyTexture");
    }

    // Create staging buffer for texture data upload
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(imageSize * sizeof(uint8_t))
        .setUsageFlags(ImageUsage::TransferSrc)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .uploadData(vulkanData.data());

    // Create texture with texture configuration
    TextureVK textureVK;

    ImageConfig imageConfig = ImageConfig()
        .withPhysicalDevice(deviceVK->pDevice)
        .withDimensions(texture.width, texture.height)
        .withAutoMipLevels()
        .withFormat(textureFormat)
        .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .withTiling(ImageTiling::Optimal)
        .withMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .withAspectMask(ImageAspect::Color)
        .withAutoMipLevels(texture.width, texture.height);

    // A quick function to convert TinyTexture::AddressMode to VkSamplerAddressMode
    auto convertAddressMode = [](TinyTexture::AddressMode mode) {
        switch (mode) {
            case TinyTexture::AddressMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TinyTexture::AddressMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TinyTexture::AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    SamplerConfig sampConfig = SamplerConfig()
    // The only thing we care about right now is address mode
        .withAddressModes(convertAddressMode(texture.addressMode));

    bool success = textureVK
        .init(deviceVK)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .isValid();

    if (!success) throw std::runtime_error("Failed to create TextureVK from TinyTexture");

    TempCmd tempCmd(deviceVK, deviceVK->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVK->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

    return MakeUnique<TextureVK>(std::move(textureVK));
}

void ResourceGroup::createTextureDescSet() {
    VkDevice lDevice = deviceVK->lDevice;
    uint32_t textureCount = static_cast<uint32_t>(textures.size());

    // Combined image sampler descriptor for each texture
    texDescLayout = MakeUnique<DescLayout>();
    texDescLayout->create(lDevice, {
        {0, DescType::CombinedImageSampler, textureCount, ShaderStage::Fragment, nullptr}
    });

    texDescPool = MakeUnique<DescPool>();
    texDescPool->create(lDevice, {
        {DescType::CombinedImageSampler, textureCount}
    }, 1);

    texDescSet = MakeUnique<DescSet>();
    texDescSet->allocate(lDevice, *texDescPool, *texDescLayout);

    // Write combined image samplers - each texture now includes its own sampler
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; ++i) {
        imageInfos[i].imageLayout = ImageLayout::ShaderReadOnlyOptimal;
        imageInfos[i].imageView   = textures[i]->getView();
        imageInfos[i].sampler     = textures[i]->getSampler(); // Now using the texture's own sampler
    }

    DescWrite()
        .setDstSet(*texDescSet)
        .setDescType(DescType::CombinedImageSampler)
        .setDescCount(textureCount)
        .setImageInfo(imageInfos)
        .updateDescSet(lDevice);
}


// ============================================================================
// =========================== MESH STATIC ====================================
// ============================================================================

void MeshVK::fromMesh(const DeviceVK* deviceVK, const TinyMesh& mesh, const std::vector<int>& meshMats) {
    const auto& vertexData = mesh.vertexData;
    const auto& indexData = mesh.indexData;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    indexType = tinyToVkIndexType(mesh.indexType);

    submeshes = mesh.submeshes;
    meshMaterials = meshMats;
}

VkIndexType MeshVK::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}