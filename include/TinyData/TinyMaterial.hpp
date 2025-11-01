#pragma once

#include "tinyExt/tinyPool.hpp"
#include "tinyData/tinyTexture.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

struct tinyMaterialVk {
    struct Props {
        glm::vec4 baseColor = glm::vec4(1.0f); // RGBA base color multiplier
        
        // Future properties can be added here without affecting texture bindings:
        // float metallic = 0.0f;
        // float roughness = 1.0f;
        // float emissive = 0.0f;
        // etc.
    };


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

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            { 0, DescType::UniformBuffer, 1, ShaderStage::Fragment, nullptr } // Material properties
        };

        // Other texture bindings
        for (uint32_t i = 0; i < texCount; i++) {
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
            { DescType::CombinedImageSampler, maxSets * texCount }
        }, maxSets);

        return pool;
    }

    void init(const tinyVk::Device* deviceVk, const tinyTextureVk* defTex, VkDescriptorSetLayout descSLayout, VkDescriptorPool descPool) {
        using namespace tinyVk;

        deviceVk_ = deviceVk;
        defTex_ = defTex;

        descSet_.allocate(deviceVk->device, descPool, descSLayout);

        // Create uniform buffer for material properties
        propsBuffer_
            .setDataSize(sizeof(Props))
            .setUsageFlags(BufferUsage::Uniform)
            .setMemPropFlags(MemProp::HostVisibleAndCoherent)
            .createBuffer(deviceVk);

        // Initialize with default properties
        propsBuffer_.uploadData(&props_);

        updateAllBindings();
    }


    tinyMaterialVk(const tinyMaterialVk&) = delete;
    tinyMaterialVk& operator=(const tinyMaterialVk&) = delete;

    // Require move semantics to invalidate to avoid instant decrement
    tinyMaterialVk(tinyMaterialVk&& other) noexcept
    :   name(std::move(other.name)),
        descSet_(std::move(other.descSet_)),
        albTex_(other.albTex_),
        nrmlTex_(other.nrmlTex_),
        props_(other.props_),
        propsBuffer_(std::move(other.propsBuffer_)),
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
            props_ = other.props_;
            propsBuffer_ = std::move(other.propsBuffer_);
            deviceVk_ = other.deviceVk_;
            defTex_ = other.defTex_;

            other.albTex_ = nullptr;
            other.nrmlTex_ = nullptr;
        }
        return *this;
    }

// ---------------- Texture Setters ---------------

    bool setAlbTex(tinyTextureVk* texture) noexcept {
        return setTexture(albTex_, texture, 1);
    }

    bool setNrmlTex(tinyTextureVk* texture) noexcept {
        return setTexture(nrmlTex_, texture, 2);
    }

    bool setMetalTex(tinyTextureVk* texture) noexcept {
        return setTexture(metalTex_, texture, 3);
    }

    bool setEmisTex(tinyTextureVk* texture) noexcept {
        return setTexture(emisTex_, texture, 4);
    }

// ---------------- Properties Setters ---------------

    void setBaseColor(const glm::vec4& color) noexcept {
        props_.baseColor = color;
        propsBuffer_.uploadData(&props_);
    }

    void setProps(const Props& props) noexcept {
        props_ = props;
        propsBuffer_.uploadData(&props_);
    }

// ---------------- Getters ---------------

    // implicit conversion
    VkDescriptorSet descSet() const noexcept { return descSet_; }

    const tinyTextureVk* albTex() const noexcept { return albTex_; }
    const tinyTextureVk* nrmlTex() const noexcept { return nrmlTex_; }
    const Props& properties() const noexcept { return props_; }
    const glm::vec4& baseColor() const noexcept { return props_.baseColor; }

private:
    bool setTexture(tinyTextureVk*& curTex, tinyTextureVk*& newTex, uint32_t binding) noexcept {
        if (!newTex) return false;

        if (curTex) curTex->decrementUse();

        curTex = newTex;
        curTex->incrementUse();

        updateTexBinding(binding, curTex);
        return true;
    }

    void updateTexBinding(uint32_t binding, const tinyTextureVk* texture) {
        using namespace tinyVk;

        const tinyTextureVk* tex = texture ? texture : defTex_;

        DescWrite()
            .addWrite()
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

    void updatePropsBinding() {
        using namespace tinyVk;

        DescWrite().addWrite()
            .setDstSet(descSet_)
            .setDstBinding(0) // Properties always at binding = 0
            .setType(DescType::UniformBuffer)
            .setBufferInfo({ VkDescriptorBufferInfo{
                propsBuffer_.get(),
                0,
                sizeof(Props)
            }})
            .updateDescSets(deviceVk_->device);
    }

    void updateAllBindings() {
        updatePropsBinding();
        updateTexBinding(1, albTex_);
        updateTexBinding(2, nrmlTex_);
        updateTexBinding(3, metalTex_);
        updateTexBinding(4, emisTex_);
    }

    const tinyVk::Device* deviceVk_ = nullptr;
    const tinyTextureVk* defTex_ = nullptr; // Fallback texture

    // Ay guys, I used deque so this won't be dangling, nice

    constexpr static uint32_t texCount = 4; // Change as needed
    tinyTextureVk* albTex_ = nullptr;
    tinyTextureVk* nrmlTex_ = nullptr;
    tinyTextureVk* metalTex_ = nullptr;
    tinyTextureVk* emisTex_ = nullptr;

    Props props_;
    tinyVk::DataBuffer propsBuffer_; // Modifiable

    tinyVk::DescSet descSet_;
};