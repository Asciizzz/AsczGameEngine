#pragma once

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    std::string name;
    tinyMaterialVk() noexcept = default;

    ~tinyMaterialVk() noexcept {
        // Decrease texture use counts
        if (albTex_) albTex_->decrementUse();
        if (nrmlTex_) nrmlTex_->decrementUse();

        // DescSet should auto-cleanup (should) (I think we're good)
    }

    static tinyVk::DescSLayout createDescSetLayout(VkDevice deviceVk) {
        using namespace tinyVk;

        DescSLayout layout;

        layout.create(deviceVk, {
            { 0, DescType::CombinedImageSampler, 1, ShaderStage::Fragment, nullptr },
            { 1, DescType::CombinedImageSampler, 1, ShaderStage::Fragment, nullptr }
        });

        return layout;
    }

    static tinyVk::DescPool createDescPool(VkDevice deviceVk, uint32_t maxSets) {
        using namespace tinyVk;

        DescPool pool;
        pool.create(deviceVk, {
            { DescType::CombinedImageSampler, maxSets * texCount }
        }, maxSets);

        return pool;
    }

    void init(const tinyVk::Device* deviceVk, const tinyTextureVk* defTex, VkDescriptorSetLayout descSLayout, VkDescriptorPool descPool) {
        deviceVk_ = deviceVk;
        defTex_ = defTex;

        descSet_.allocate(deviceVk->device, descPool, descSLayout);

        updateBinding(0, nullptr); // Albedo
        updateBinding(1, nullptr); // Normal

        // More in the future, stay tuned
    }


    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    // Require move semantics to invalidate to avoid instant decrement
    tinyMaterialVk(tinyMaterialVk&& other) noexcept
    :   name(std::move(other.name)),
        descSet_(std::move(other.descSet_)),
        albTex_(other.albTex_),
        nrmlTex_(other.nrmlTex_),
        deviceVk_(other.deviceVk_),
        defTex_(other.defTex_)
    {
        // null out the old object so its destructor doesn't decrement
        other.albTex_ = nullptr;
        other.nrmlTex_ = nullptr;
    }
    tinyMaterialVk& operator=(tinyMaterialVk&& other) noexcept {
        if (this != &other) {
            // Decrement current textures (type shi)
            if (albTex_) albTex_->decrementUse();
            if (nrmlTex_) nrmlTex_->decrementUse();

            name = std::move(other.name);
            descSet_ = std::move(other.descSet_);
            albTex_ = other.albTex_;
            nrmlTex_ = other.nrmlTex_;
            deviceVk_ = other.deviceVk_;
            defTex_ = other.defTex_;

            other.albTex_ = nullptr;
            other.nrmlTex_ = nullptr;
        }
        return *this;
    }

// -----------------------------------------

    bool setAlbedoTexture(tinyTextureVk* texture) noexcept {
        if (!setTexture(albTex_, texture)) return false;

        updateBinding(0, albTex_);
        return true;
    }

    bool setNormalTexture(tinyTextureVk* texture) noexcept {
        if (!setTexture(nrmlTex_, texture)) return false;

        updateBinding(1, nrmlTex_);
        return true;
    }

    // implicit conversion
    VkDescriptorSet descSet() const noexcept { return descSet_; }

private:
    bool setTexture(tinyTextureVk*& curTex, tinyTextureVk*& newTex) noexcept {
        if (!newTex) return false;

        if (curTex) curTex->decrementUse();

        curTex = newTex;
        curTex->incrementUse();
        return true;
    }

    void updateBinding(uint32_t binding, const tinyTextureVk* texture) {
        using namespace tinyVk;

        const tinyTextureVk* tex = texture ? texture : defTex_;

        DescWrite().addWrite()
            .setDstSet(descSet_) // implicit conversion
            .setDstBinding(binding)
            .setType(DescType::CombinedImageSampler)
            .setImageInfo({ VkDescriptorImageInfo{
                tex->sampler(),
                tex->view(),
                ImageLayout::ShaderReadOnlyOptimal
            }})
            .updateDescSets(deviceVk_->device);
    }

    void updateAllBindings() {
        updateBinding(0, albTex_);
        updateBinding(1, nrmlTex_);
    }

    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    // Ay guys, I used deque so this won't be dangling, nice

    constexpr static uint32_t texCount = 2; // Albedo + Normal
    tinyTextureVk* albTex_ = nullptr; // Albedo texture
    tinyTextureVk* nrmlTex_ = nullptr; // Normal texture

    tinyVk::DescSet descSet_;
};