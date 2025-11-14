#pragma once

#include "tinyVk/Resource/TextureVk.hpp"
#include "tinyVk/System/CmdBuffer.hpp"

#include <cstdint>
#include <string>

struct tinyTexture {
    tinyTexture() noexcept = default; // Allow copy and move semantics

    enum class WrapMode {
        Repeat,
        ClampToEdge,
        ClampToBorder
    };

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

    bool valid() const noexcept {
        return !data_.empty() && width_ > 0 && height_ > 0 && channels_ > 0;
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

struct tinyTextureVk {
    tinyTextureVk() noexcept = default;
    tinyTextureVk(tinyTexture&& texture, const tinyVk::Device* deviceVk = nullptr) noexcept {
        set(std::forward<tinyTexture>(texture));
        if (deviceVk) create(deviceVk);
    }

    tinyTextureVk(const tinyTextureVk&) = delete;
    tinyTextureVk& operator=(const tinyTextureVk&) = delete;

    tinyTextureVk(tinyTextureVk&&) noexcept = default;
    tinyTextureVk& operator=(tinyTextureVk&&) noexcept = default;

// -----------------------------------------

    tinyTextureVk& set(tinyTexture&& texture) {
        texture_ = std::forward<tinyTexture>(texture);
        return *this;
    }

    bool create(const tinyVk::Device* deviceVk) {
        using namespace tinyVk;

        if (!texture_.valid()) return false;

        VkFormat textureFormat;
        switch (texture_.channels()) {
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
            .createBuffer(deviceVk)
            .uploadData(texture_.dataPtr());

        uint32_t mipLevel = ImageVk::autoMipLevels(texture_.width(), texture_.height());

        ImageConfig imageConfig = ImageConfig()
            .withPhysicalDevice(deviceVk->pDevice)
            .withDimensions(texture_.width(), texture_.height())
            .withMipLevels(mipLevel)
            .withFormat(textureFormat)
            .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
            .withTiling(ImageTiling::Optimal)
            .withMemProps(MemProp::DeviceLocal);

        ImageViewConfig viewConfig = ImageViewConfig()
            .withAspectMask(ImageAspect::Color)
            .withMipLevels(mipLevel);

        // A quick function to convert tinyTexture::WrapMode to VkSamplerAddressMode
        auto convertAddressMode = [](tinyTexture::WrapMode mode) {
            switch (mode) {
                case tinyTexture::WrapMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case tinyTexture::WrapMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case tinyTexture::WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                default:                                   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            }
        };

        SamplerConfig sampConfig = SamplerConfig()
        // The only thing we care about right now is address mode
            .withAddressModes(convertAddressMode(texture_.wrapMode()));

        textureVk_ = TextureVk(); // Reset texture
        bool success = textureVk_
            .init(deviceVk->device)
            .createImage(imageConfig)
            .createView(viewConfig)
            .createSampler(sampConfig)
            .valid();

        if (!success) return false;

        TempCmd tempCmd(deviceVk, deviceVk->graphicsPoolWrapper);

        textureVk_
            .transitionLayoutImmediate(tempCmd.get(), ImageLayout::Undefined, ImageLayout::TransferDstOptimal)
            .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
            .generateMipmapsImmediate(tempCmd.get(), deviceVk->pDevice);

        tempCmd.endAndSubmit(); // Kinda redundant with RAII but whatever

        return true;
    }

    bool createFrom(tinyTexture&& texture, const tinyVk::Device* deviceVk) {
        return set(std::forward<tinyTexture>(texture)).create(deviceVk);
    }

// -----------------------------------------

    tinyTexture& cpu() noexcept { return texture_; }
    const tinyTexture& cpu() const noexcept { return texture_; }
    const tinyVk::TextureVk& gpu() const noexcept { return textureVk_; }

    uint32_t width() const noexcept { return texture_.width(); }
    uint32_t height() const noexcept { return texture_.height(); }
    float aspectRatio() const noexcept { return texture_.aspectRatio(); }
    uint32_t channels() const noexcept { return texture_.channels(); }

    VkImageView view() const noexcept { return textureVk_.getView(); }
    VkSampler sampler() const noexcept { return textureVk_.getSampler(); }

    const std::vector<uint8_t>& data() const noexcept { return texture_.data(); }
    const uint8_t* dataPtr() const noexcept { return texture_.dataPtr(); }

    uint32_t useCount() const noexcept { return useCount_; }
    uint32_t incrementUse() noexcept { return ++useCount_; }
    uint32_t decrementUse() noexcept { return useCount_ > 0 ? --useCount_ : 0; }

    static tinyTextureVk aPixel(const tinyVk::Device* deviceVk, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255) {
        tinyTextureVk textureVk;
        textureVk.createFrom(tinyTexture::aPixel(r, g, b, a), deviceVk);
        return textureVk;
    }

private: // Composition
    tinyTexture texture_;
    tinyVk::TextureVk textureVk_;

    uint32_t useCount_ = 0;
};