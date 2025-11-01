#pragma once

#include <array>

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    struct alignas(16) Props {
        glm::vec4 baseColor = glm::vec4(1.0f); // RGBA base color multiplier
    };

    enum class TexSlot : uint32_t {
        Albedo = 0,
        Normal = 1,
        MetalRough = 2,
        Emissive = 3,
        Count = 4
    };

    std::string name;
    tinyMaterialVk() noexcept {
        textures_.fill(nullptr);
    }

    ~tinyMaterialVk() noexcept {
        for (auto* tex : textures_) {
            if (tex) tex->decrementUse();
        }
    }

    static tinyVk::DescSLayout createDescSetLayout(VkDevice deviceVk) {
        using namespace tinyVk;

        DescSLayout layout;

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            { 0, DescType::UniformBuffer, 1, ShaderStage::Fragment, nullptr } // Material properties
        };

        for (uint32_t i = 0; i < static_cast<uint32_t>(TexSlot::Count); i++) {
            bindings.push_back({ i + 1, DescType::CombinedImageSampler, 1, ShaderStage::Fragment, nullptr });
        }

        layout.create(deviceVk, bindings);

        return layout;
    }

    static tinyVk::DescPool createDescPool(VkDevice deviceVk, uint32_t maxSets) {
        using namespace tinyVk;

        DescPool pool;
        pool.create(deviceVk, {
            { DescType::UniformBuffer, maxSets },
            { DescType::CombinedImageSampler, maxSets * static_cast<uint32_t>(TexSlot::Count) }
        }, maxSets);

        return pool;
    }

    void init(const tinyVk::Device* deviceVk, const tinyTextureVk* defTex, VkDescriptorSetLayout descSLayout, VkDescriptorPool descPool) {
        using namespace tinyVk;

        deviceVk_ = deviceVk;
        defTex_ = defTex;

        descSet_.allocate(deviceVk->device, descPool, descSLayout);

        propsBuffer_
            .setDataSize(sizeof(Props))
            .setUsageFlags(BufferUsage::Uniform)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(deviceVk);

        propsBuffer_.uploadData(&props_);

        updateAllBindings();
    }


    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    // Require move semantics to invalidate to avoid instant decrement
    tinyMaterialVk(tinyMaterialVk&& other) noexcept
    :   name(std::move(other.name)),
        descSet_(std::move(other.descSet_)),
        defTex_(other.defTex_),
        textures_(other.textures_),
        props_(other.props_),
        propsBuffer_(std::move(other.propsBuffer_)),
        deviceVk_(other.deviceVk_)
    {
        // Null out the old object so its destructor doesn't decrement
        other.textures_.fill(nullptr);
    }

    tinyMaterialVk& operator=(tinyMaterialVk&& other) noexcept {
        if (this != &other) {
            // Decrement current textures
            for (auto* tex : textures_) {
                if (tex) tex->decrementUse();
            }

            name = std::move(other.name);
            descSet_ = std::move(other.descSet_);
            deviceVk_ = other.deviceVk_;
            defTex_ = other.defTex_;
            textures_ = other.textures_;
            props_ = other.props_;
            propsBuffer_ = std::move(other.propsBuffer_);

            // Null out the old object
            other.textures_.fill(nullptr);

            updateAllBindings();
        }
        return *this;
    }

// ---------------- Texture Setters ---------------

    bool setTexture(TexSlot slot, tinyTextureVk* texture) noexcept {
        if (!texture) return false;

        uint32_t index = static_cast<uint32_t>(slot);
        if (index >= static_cast<uint32_t>(TexSlot::Count)) return false;

        // Decrement old texture if it exists
        if (textures_[index]) textures_[index]->decrementUse();

        // Set new texture and increment use count
        textures_[index] = texture; texture->incrementUse();

        updateTexBinding(slot);
        return true;
    }

// ---------------- Properties Setters ---------------

    void setBaseColor(const glm::vec4& color) noexcept {
        props_.baseColor = color;
        setProps(props_);
    }

    void setProps(const Props& props) noexcept {
        props_ = props;
        propsBuffer_.uploadData(&props_);
    }

// ---------------- Getters ---------------

    // implicit conversion
    VkDescriptorSet descSet() const noexcept { return descSet_; }

    const Props& properties() const noexcept { return props_; }
    const glm::vec4& baseColor() const noexcept { return props_.baseColor; }

    const tinyTextureVk* texture(TexSlot slot) const noexcept {
        uint32_t index = static_cast<uint32_t>(slot);
        if (index >= static_cast<uint32_t>(TexSlot::Count)) return nullptr;
        return textures_[index];
    }

private:
    void updateTexBinding(TexSlot slot) {
        using namespace tinyVk;

        uint32_t index = static_cast<uint32_t>(slot);
        uint32_t binding = index + 1; // Properties at 0, textures start at 1

        const tinyTextureVk* tex = textures_[index] ? textures_[index] : defTex_;

        DescWrite()
            .addWrite()
            .setDstSet(descSet_)
            .setDstBinding(binding)
            .setType(DescType::CombinedImageSampler)
            .setImageInfo({ VkDescriptorImageInfo{
                tex->sampler(),
                tex->view(),
                ImageLayout::ShaderReadOnlyOptimal
            }})
            .updateDescSets(deviceVk_->device);
    }

    void updatePropsBinding() {
        using namespace tinyVk;

        DescWrite().addWrite()
            .setDstSet(descSet_)
            .setDstBinding(0) // Properties always at binding = 0
            .setType(DescType::UniformBuffer)
            .setBufferInfo({ VkDescriptorBufferInfo{
                propsBuffer_,
                0,
                sizeof(Props)
            }})
            .updateDescSets(deviceVk_->device);
    }

    void updateAllBindings() {
        updatePropsBinding();
        
        // Update all texture bindings dynamically
        for (uint32_t i = 0; i < static_cast<uint32_t>(TexSlot::Count); ++i) {
            updateTexBinding(static_cast<TexSlot>(i));
        }
    }

    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    std::array<tinyTextureVk*, static_cast<uint32_t>(TexSlot::Count)> textures_;

    Props props_;
    tinyVk::DataBuffer propsBuffer_; // Modifiable

    tinyVk::DescSet descSet_;
};

using MTexSlot = tinyMaterialVk::TexSlot;