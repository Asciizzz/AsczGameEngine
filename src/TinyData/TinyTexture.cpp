#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/System/CmdBuffer.hpp"

using namespace tinyVk;

tinyTexture& tinyTexture::setName(const std::string& n) {
    name = n;
    return *this;
}
tinyTexture& tinyTexture::setDimensions(int w, int h) {
    width = w;
    height = h;
    return *this;
}
tinyTexture& tinyTexture::setChannels(int c) {
    channels = c;
    return *this;
}
tinyTexture& tinyTexture::setData(const std::vector<uint8_t>& d) {
    data = d;
    return *this;
}

uint64_t tinyTexture::makeHash() {
    const uint64_t FNV_offset = 1469598103934665603ULL;
    const uint64_t FNV_prime  = 1099511628211ULL;
    hash = FNV_offset;

    auto fnv1a = [&](auto value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(value); i++) {
            hash ^= bytes[i];
            hash *= FNV_prime;
        }
    };

    // Dimensions & channels
    fnv1a(width);
    fnv1a(height);
    fnv1a(channels);

    // Sampler state
    auto addr = static_cast<uint32_t>(addressMode);
    fnv1a(addr);

    // Image data
    for (auto b : data) {
        hash ^= b;
        hash *= FNV_prime;
    }

    return hash;
}

tinyTexture tinyTexture::createDefaultTexture() {
    tinyTexture texture;
    texture.width = 1;
    texture.height = 1;
    texture.channels = 4;
    texture.data = { 255, 255, 255, 255 }; // White pixel
    texture.makeHash();
    return texture;
}




bool tinyTexture::vkCreate(const tinyVk::Device* deviceVk) {
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
        .createBuffer(deviceVk)
        .uploadData(vulkanData.data());

    ImageConfig imageConfig = ImageConfig()
        .withPhysicalDevice(deviceVk->pDevice)
        .withDimensions(width, height)
        .withAutoMipLevels()
        .withFormat(textureFormat)
        .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .withTiling(ImageTiling::Optimal)
        .withMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .withAspectMask(ImageAspect::Color)
        .withAutoMipLevels(width, height);

    // A quick function to convert tinyTexture::AddressMode to VkSamplerAddressMode
    auto convertAddressMode = [](tinyTexture::AddressMode mode) {
        switch (mode) {
            case tinyTexture::AddressMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case tinyTexture::AddressMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case tinyTexture::AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    SamplerConfig sampConfig = SamplerConfig()
    // The only thing we care about right now is address mode
        .withAddressModes(convertAddressMode(addressMode));

    textureVK = TextureVK(); // Reset texture
    bool success = textureVK
        .init(deviceVk->device)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .valid();

    if (!success) return false;

    TempCmd tempCmd(deviceVk, deviceVk->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVk->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

    return true;
}