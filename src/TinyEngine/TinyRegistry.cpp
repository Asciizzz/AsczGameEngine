#include "TinyEngine/TinyRegistry.hpp"

#include "TinyVK/CmdBuffer.hpp"

#include <stdexcept>

using namespace TinyVK;
using HType = TinyHandle::Type;

bool TinyRMesh::import(const TinyVK::DeviceVK* deviceVK, const TinyMesh& mesh) {
    const auto& vertexData = mesh.vertexData;
    const auto& indexData = mesh.indexData;

    if (vertexData.empty() || indexData.empty()) return false;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    submeshes = mesh.submeshes;
    indexType = tinyToVkIndexType(mesh.indexType);

    return true;
}

VkIndexType TinyRMesh::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
}

bool TinyRTexture::import(const TinyVK::DeviceVK* deviceVK, const TinyTexture& texture) {
    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(texture.channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertToValidData(
        texture.channels, texture.width, texture.height, texture.data.data());

    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (texture.channels == 3) ? 4 : texture.channels; // RGB becomes RGBA
    VkDeviceSize imageSize = texture.width * texture.height * vulkanChannels;

    if (texture.data.empty()) return false;

    // Create staging buffer for texture data upload
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(imageSize * sizeof(uint8_t))
        .setUsageFlags(ImageUsage::TransferSrc)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .uploadData(vulkanData.data());

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

    textureVK = TextureVK(); // Reset texture
    bool success = textureVK
        .init(deviceVK)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .isValid();

    if (!success) return false;

    TempCmd tempCmd(deviceVK, deviceVK->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVK->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

    return true;
}






// Registry


TinyRegistry::TinyRegistry(const TinyVK::DeviceVK* deviceVK)
: deviceVK(deviceVK) {
    // Start humble
    materialDatas.resize(128);
    textureDatas.resize(128);
    meshDatas.resize(128);
    skeletonDatas.resize(128);
    nodeDatas.resize(128);

    initVkResources();
}


uint32_t TinyRegistry::getPoolCapacity(TinyHandle::Type type) const {
    switch (type) {
        case TinyHandle::Type::Mesh:     return meshDatas.capacity;
        case TinyHandle::Type::Material: return materialDatas.capacity;
        case TinyHandle::Type::Texture:  return textureDatas.capacity;
        case TinyHandle::Type::Skeleton: return skeletonDatas.capacity;
        case TinyHandle::Type::Node:     return nodeDatas.capacity;
        default:                         return 0;
    }
}

TinyHandle TinyRegistry::addMesh(const TinyMesh& mesh) {
    UniquePtr<TinyRMesh> meshData = MakeUnique<TinyRMesh>();
    meshData->import(deviceVK, mesh);

    uint32_t index = meshDatas.insert(std::move(meshData));
    resizeCheck();

    return TinyHandle(index, HType::Mesh);
}

TinyHandle TinyRegistry::addTexture(const TinyTexture& texture) {
    UniquePtr<TinyRTexture> textureData = MakeUnique<TinyRTexture>();
    textureData->import(deviceVK, texture);

    uint32_t index = textureDatas.insert(std::move(textureData));
    resizeCheck();

    // Further descriptor logic in the future

    return TinyHandle(index, HType::Texture);
}

// Usually you need to know the texture beforehand to remap the material texture indices
TinyHandle TinyRegistry::addMaterial(const TinyRMaterial& matData) {
    uint32_t index = materialDatas.insert(matData);
    resizeCheck();

    // Update the GPU buffer immediately
    matBuffer->mapAndCopy(materialDatas.data());

    return TinyHandle(index, HType::Material);
}

TinyHandle TinyRegistry::addSkeleton(const TinyRSkeleton& skeleton) {
    uint32_t index = skeletonDatas.insert(skeleton);
    resizeCheck();

    return TinyHandle(index, HType::Skeleton);
}

TinyHandle TinyRegistry::addNode(const TinyRNode& node) {
    uint32_t index = nodeDatas.insert(node);
    resizeCheck();

    return TinyHandle(index, HType::Node);
}

// Vulkan resources creation
void TinyRegistry::resizeCheck() {

    if (textureDatas.hasResized()) {
        textureDatas.resetResizeFlag();
        createTextureVkResources();
    }

    if (materialDatas.hasResized()) {
        materialDatas.resetResizeFlag();
        createMaterialVkResources();
    }

    if (meshDatas.hasResized()) {
        meshDatas.resetResizeFlag();
        // No further logic
    }

    if (skeletonDatas.hasResized()) {
        skeletonDatas.resetResizeFlag();
        // No further logic
    }

    if (nodeDatas.hasResized()) {
        nodeDatas.resetResizeFlag();
        // No further logic
    }
}


void TinyRegistry::initVkResources() {
    createMaterialVkResources();
    createTextureVkResources();
}

void TinyRegistry::createMaterialVkResources() {
    // Create descriptor layout for materials
    matDescLayout = MakeUnique<DescLayout>();
    matDescLayout->create(deviceVK->lDevice, {
        {0, DescType::StorageBuffer, materialDatas.capacity, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    matDescPool = MakeUnique<DescPool>();
    matDescPool->create(deviceVK->lDevice, {
        {DescType::StorageBuffer, materialDatas.capacity}
    }, 1);

    // Create material buffer
    matBuffer = MakeUnique<DataBuffer>();
    matBuffer->setDataSize(sizeof(TinyRMaterial) * materialDatas.capacity)
             .setUsageFlags(BufferUsage::Storage | BufferUsage::TransferDst)
             .setMemPropFlags(MemProp::HostVisibleAndCoherent)
             .createBuffer(deviceVK)
             .mapAndCopy(materialDatas.data());

    // Allocate descriptor set
    matDescSet = MakeUnique<DescSet>();
    matDescSet->allocate(deviceVK->lDevice, *matDescPool, *matDescLayout);

    // Write storage buffer info
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = *matBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = VK_WHOLE_SIZE;

    DescWrite()
        .setDstSet(*matDescSet)
        .setDstBinding(0)
        .setDescType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ bufferInfo })
        .updateDescSet(deviceVK->lDevice);
}

void TinyRegistry::createTextureVkResources() {
    // Create descriptor layout for textures
    texDescLayout = MakeUnique<DescLayout>();
    texDescLayout->create(deviceVK->lDevice, {
        {0, DescType::CombinedImageSampler, textureDatas.capacity, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    texDescPool = MakeUnique<DescPool>();
    texDescPool->create(deviceVK->lDevice, {
        {DescType::CombinedImageSampler, textureDatas.capacity}
    }, 1);

    // Allocate descriptor set
    texDescSet = MakeUnique<DescSet>();
    texDescSet->allocate(deviceVK->lDevice, *texDescPool, *texDescLayout);

    // Note: Actual image infos will be written when textures are added to the registry
}