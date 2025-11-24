#pragma once

#include "tinyVk/Resource/TextureVk.hpp"
#include "tinyVk/System/CmdBuffer.hpp"

#include <cstdint>
#include <string>

struct tinyTexture {
    tinyTexture() noexcept = default;

    tinyTexture(const tinyTexture&) = delete;
    tinyTexture& operator=(const tinyTexture&) = delete;

    tinyTexture(tinyTexture&&) noexcept = default;
    tinyTexture& operator=(tinyTexture&&) noexcept = default;

    enum class WrapMode {
        Repeat,
        ClampToEdge,
        ClampToBorder
    };

    explicit operator bool() const noexcept {
        return !data_.empty() && width_ > 0 && height_ > 0 && channels_ > 0;
    }

// -----------------------------------------

    tinyTexture& create(const std::vector<uint8_t>& data,
                        uint32_t width, uint32_t height, uint32_t channels,
                        WrapMode wrapMode = WrapMode::Repeat) noexcept {
        data_ = data;
        width_ = width;
        height_ = height;
        channels_ = channels;
        wrapMode_ = wrapMode;
        makeHash();
        return *this;
    }

    tinyTexture& setWrapMode(WrapMode mode) noexcept {
        wrapMode_ = mode;
        makeHash();
        return *this;
    }

// -----------------------------------------

    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }
    float aspectRatio() const noexcept {
        return (height_ == 0) ? 0.0f : static_cast<float>(width_) / static_cast<float>(height_);
    }
    uint32_t channels() const noexcept { return channels_; }

    const std::vector<uint8_t>& data() const noexcept { return data_; }
    const uint8_t* dataPtr() const noexcept { return data_.data(); }
    void clearData() noexcept { data_.clear(); }

    WrapMode wrapMode() const noexcept { return wrapMode_; }
    VkSamplerAddressMode vkWrapMode() const noexcept {
        switch (wrapMode_) {
            case WrapMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case WrapMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    uint64_t hash() const noexcept { return hash_; }

// ----------------------- Vulkan API -----------------------

    VkImageView view() const noexcept { return textureVk_.getView(); }
    VkSampler sampler() const noexcept { return textureVk_.getSampler(); }

    bool vkCreate(const tinyVk::Device* device) {
        using namespace tinyVk;

        dvk_ = device;

        VkFormat textureFormat;
        switch (channels()) {
            case 1: textureFormat = VK_FORMAT_R8_SRGB; break;
            case 2: textureFormat = VK_FORMAT_R8G8_SRGB; break;
            case 3: textureFormat = VK_FORMAT_R8G8B8_SRGB; break;
            case 4: textureFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
            default: return false; // Unsupported channel count
        }

        VkDeviceSize imageSize = static_cast<VkDeviceSize>(width() * height() * channels());

        // Create staging buffer for texture data upload
        DataBuffer stagingBuffer;
        stagingBuffer
            .setDataSize(imageSize * sizeof(uint8_t))
            .setUsageFlags(ImageUsage::TransferSrc)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(dvk_)
            .uploadData(dataPtr());

        uint32_t mipLevel = ImageVk::autoMipLevels(width(), height());

        ImageConfig imageConfig = ImageConfig()
            .withPhysicalDevice(dvk_->pDevice)
            .withDimensions(width(), height())
            .withMipLevels(mipLevel)
            .withFormat(textureFormat)
            .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
            .withTiling(ImageTiling::Optimal)
            .withMemProps(MemProp::DeviceLocal);

        ImageViewConfig viewConfig = ImageViewConfig()
            .withAspectMask(ImageAspect::Color)
            .withMipLevels(mipLevel);

        SamplerConfig sampConfig = SamplerConfig()
            .withAddressModes(vkWrapMode());

        textureVk_ = TextureVk(); // Reset texture
        bool success = textureVk_
            .init(dvk_->device)
            .createImage(imageConfig)
            .createView(viewConfig)
            .createSampler(sampConfig)
            .valid();

        if (!success) return false;

        TempCmd tempCmd(dvk_, dvk_->graphicsPoolWrapper);

        textureVk_
            .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
            .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
            .generateMipmapsImmediate(tempCmd.get(), dvk_->pDevice);

        tempCmd.endAndSubmit();

        // Clear the original cpu texture data to save memory
        clearData();

        return true;
    }

    static tinyTexture aPixel(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255) {
        tinyTexture texture;
        texture.create({ r, g, b, a }, 1, 1, 4);
        return texture;
    }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t channels_ = 0;
    std::vector<uint8_t> data_;
    WrapMode wrapMode_ = WrapMode::Repeat;

    uint64_t hash_ = 0;

    // Vulkan parts
    const tinyVk::Device* dvk_ = nullptr;
    tinyVk::TextureVk textureVk_;

    uint64_t makeHash() noexcept {
        const uint64_t FNV_offset = 1469598103934665603ULL;
        const uint64_t FNV_prime  = 1099511628211ULL;
        hash_ = FNV_offset;

        auto fnv1a = [&](auto value) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            for (size_t i = 0; i < sizeof(value); i++) {
                hash_ ^= bytes[i];
                hash_ *= FNV_prime;
            }
        };

        // Dimensions & channels
        fnv1a(width_);
        fnv1a(height_);
        fnv1a(channels_);

        // Sampler state
        auto addr = static_cast<uint32_t>(wrapMode_);
        fnv1a(addr);

        // Image data
        for (auto b : data_) {
            hash_ ^= b;
            hash_ *= FNV_prime;
        }

        return hash_;
    }
};