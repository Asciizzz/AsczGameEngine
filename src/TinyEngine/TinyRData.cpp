#include "TinyEngine/TinyRData.hpp"

#include "TinyVK/System/CmdBuffer.hpp"

#include <stdexcept>

using namespace TinyVK;

bool TinyRMesh::create(const TinyVK::Device* deviceVK) {
    if (vertexData.empty() || indexData.empty()) return false;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

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

bool TinyRTexture::create(const TinyVK::Device* deviceVK) {
    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertToValidData(
        channels, width, height, data.data());

    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (channels == 3) ? 4 : channels; // RGB becomes RGBA
    VkDeviceSize imageSize = width * height * vulkanChannels;

    if (data.empty()) return false;

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
        .withDimensions(width, height)
        .withAutoMipLevels()
        .withFormat(textureFormat)
        .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .withTiling(ImageTiling::Optimal)
        .withMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .withAspectMask(ImageAspect::Color)
        .withAutoMipLevels(width, height);

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
        .withAddressModes(convertAddressMode(addressMode));

    textureVK = TextureVK(); // Reset texture
    bool success = textureVK
        .init(*deviceVK)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .valid();

    if (!success) return false;

    TempCmd tempCmd(deviceVK, deviceVK->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVK->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

    return true;
}
