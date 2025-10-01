#include "TinyEngine/TinyRegistry.hpp"

#include "TinyVK/CmdBuffer.hpp"

#include <stdexcept>

using namespace TinyVK;
using HType = TinyHandle::Type;

bool TinyRMesh::import(const TinyVK::Device* deviceVK, const TinyMesh& mesh) {
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

bool TinyRTexture::import(const TinyVK::Device* deviceVK, const TinyTexture& texture) {
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


TinyRegistry::TinyRegistry(const TinyVK::Device* deviceVK)
: deviceVK(deviceVK) {
    pool<TinyRMesh>().resize(64).setCapacityStep(64);
    pool<TinyRMaterial>().resize(64).setCapacityStep(16);
    pool<TinyRTexture>().resize(64).setCapacityStep(16);
    pool<TinyRSkeleton>().resize(16).setCapacityStep(8);
    pool<TinyRNode>().resize(128).setCapacityStep(32);

    createMaterialVkResources();
    createTextureVkResources();
}


// Vulkan resources creation

void TinyRegistry::resizeCheck() {

    if (pool<TinyRTexture>().hasResized()) {
        pool<TinyRTexture>().resetResizeFlag();
        createTextureVkResources();
    }

    if (pool<TinyRMaterial>().hasResized()) {
        pool<TinyRMaterial>().resetResizeFlag();
        createMaterialVkResources();
    }

    if (pool<TinyRMesh>().hasResized()) {
        pool<TinyRMesh>().resetResizeFlag();
        // No further logic
    }

    if (pool<TinyRSkeleton>().hasResized()) {
        pool<TinyRSkeleton>().resetResizeFlag();
        // No further logic
    }

    if (pool<TinyRNode>().hasResized()) {
        pool<TinyRNode>().resetResizeFlag();
        // No further logic
    }
}

void TinyRegistry::createMaterialVkResources() {
    // Create descriptor layout for materials
    matDescLayout = MakeUnique<DescLayout>();
    matDescLayout->create(deviceVK->lDevice, {
        {0, DescType::StorageBuffer, 1, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    matDescPool = MakeUnique<DescPool>();
    matDescPool->create(deviceVK->lDevice, {
        {DescType::StorageBuffer, 1}
    }, 1);

    // Create material buffer
    matBuffer = MakeUnique<DataBuffer>();
    matBuffer
        ->setDataSize(sizeof(TinyRMaterial) * pool<TinyRMaterial>().capacity)
        .setUsageFlags(BufferUsage::Storage | BufferUsage::TransferDst)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(pool<TinyRMaterial>().data());

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
        .updateDescSets(deviceVK->lDevice);
}

void TinyRegistry::createTextureVkResources() {
    // Create descriptor layout for textures
    texDescLayout = MakeUnique<DescLayout>();
    texDescLayout->create(deviceVK->lDevice, {
        {0, DescType::CombinedImageSampler, pool<TinyRTexture>().capacity, ShaderStage::Fragment, nullptr}
    });

    // Create descriptor pool
    texDescPool = MakeUnique<DescPool>();
    texDescPool->create(deviceVK->lDevice, {
        {DescType::CombinedImageSampler, pool<TinyRTexture>().capacity}
    }, 1);

    // Allocate descriptor set
    texDescSet = MakeUnique<DescSet>();
    texDescSet->allocate(deviceVK->lDevice, *texDescPool, *texDescLayout);

    // Note: Actual image infos will be written when textures are added to the registry
}