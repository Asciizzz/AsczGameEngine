#pragma once

#include "TinyVK/System/Device.hpp"

namespace TinyVK {

struct DescType {
    static constexpr VkDescriptorType Sampler              = VK_DESCRIPTOR_TYPE_SAMPLER;
    static constexpr VkDescriptorType CombinedImageSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    static constexpr VkDescriptorType SampledImage         = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    static constexpr VkDescriptorType StorageImage         = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    static constexpr VkDescriptorType UniformTexelBuffer   = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    static constexpr VkDescriptorType StorageTexelBuffer   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    static constexpr VkDescriptorType UniformBuffer        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    static constexpr VkDescriptorType StorageBuffer        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    static constexpr VkDescriptorType UniformBufferDynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    static constexpr VkDescriptorType StorageBufferDynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    static constexpr VkDescriptorType InputAttachment      = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
};

struct DescPool {
    DescPool() = default;
    ~DescPool() { destroy(); }

    DescPool(const DescPool&) = delete;
    DescPool& operator=(const DescPool&) = delete;
    // Move semantics
    DescPool(DescPool&& other) noexcept;
    DescPool& operator=(DescPool&& other) noexcept;

    VkDescriptorPool get() const { return pool; }
    operator VkDescriptorPool() const { return pool; }

    void create(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    void destroy();

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

struct DescLayout {
    DescLayout() = default;
    ~DescLayout() { destroy(); }

    DescLayout(const DescLayout&) = delete;
    DescLayout& operator=(const DescLayout&) = delete;
    // Move semantics
    DescLayout(DescLayout&& other) noexcept;
    DescLayout& operator=(DescLayout&& other) noexcept;

    VkDescriptorSetLayout get() const { return layout; }
    operator VkDescriptorSetLayout() const { return layout; }
    operator VkDescriptorSetLayout&() { return layout; }

    void create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    void destroy();

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

struct DescSet {
    DescSet() = default;
    ~DescSet() = default;

    DescSet(const DescSet&) = delete;
    DescSet& operator=(const DescSet&) = delete;
    // Move semantics
    DescSet(DescSet&& other) noexcept;
    DescSet& operator=(DescSet&& other) noexcept;

    VkDescriptorSet get() const { return set; }
    operator VkDescriptorSet() const { return get(); }

    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorPool getPool() const { return pool; }

    void allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);

    // Explicit free of descriptor sets
    void free(VkDescriptorPool pool);

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};


struct DescWrite {
    uint32_t writeCount = 0;
    std::vector<VkWriteDescriptorSet> writes;

    // Storage for image and buffer info to ensure lifetime - one per write
    std::vector<std::vector<VkDescriptorImageInfo>> imageInfoStorage;
    std::vector<std::vector<VkDescriptorBufferInfo>> bufferInfoStorage;

    DescWrite() = default;
    VkWriteDescriptorSet* operator&() { return writes.data(); }

    DescWrite& addWrite();
    VkWriteDescriptorSet& lastWrite();

    DescWrite& setBufferInfo(std::vector<VkDescriptorBufferInfo> bufferInfo);
    DescWrite& setImageInfo(std::vector<VkDescriptorImageInfo> imageInfos);

    DescWrite& setDstSet(VkDescriptorSet dstSet);
    DescWrite& setDstBinding(uint32_t dstBinding);
    DescWrite& setDstArrayElement(uint32_t dstArrayElement);
    DescWrite& setDescCount(uint32_t count);
    DescWrite& setDescType(VkDescriptorType type);

    DescWrite& updateDescSets(VkDevice device);
};

}
