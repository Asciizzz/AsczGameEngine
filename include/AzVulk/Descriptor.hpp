#pragma once

#include "AzVulk/DeviceVK.hpp"

namespace AzVulk {

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
    DescPool(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    void init(VkDevice lDevice) { this->lDevice = lDevice; }

    ~DescPool() { destroy(); }

    DescPool(const DescPool&) = delete;
    DescPool& operator=(const DescPool&) = delete;

    VkDescriptorPool get() const { return pool; }
    operator VkDescriptorPool() const { return pool; }

    void create(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    static VkDescriptorPool create(VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    void destroy();

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

struct DescLayout {
    DescLayout(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    void init(VkDevice lDevice) { this->lDevice = lDevice; }

    ~DescLayout() { destroy(); }

    DescLayout(const DescLayout&) = delete;
    DescLayout& operator=(const DescLayout&) = delete;

    VkDescriptorSetLayout get() const { return layout; }
    operator VkDescriptorSetLayout() const { return layout; }

    void create(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    static VkDescriptorSetLayout create(VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    void destroy();

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

struct DescSet {
    DescSet(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    void init(VkDevice lDevice) { this->lDevice = lDevice; }

    ~DescSet() { cleanup(); }

    DescSet(const DescSet&) = delete;
    DescSet& operator=(const DescSet&) = delete;

    VkDescriptorSet get(uint32_t index=0) const { return sets.at(index); }
    // Implicit conversion only works on single descriptor set
    operator VkDescriptorSet() const { return get(); }

    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorPool getPool() const { return pool; }

    void allocate(VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count=1); // Borrowed pool and layout version
    void allocate(uint32_t count=1); // Owned pool and layout version

    // For self contained descriptor sets with owned pool and layout
    void createOwnLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void createOwnPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    // Explicit free of descriptor sets
    void free(VkDescriptorPool pool);

    void destroyPool();
    void destroyLayout();

    void cleanup() { destroyPool(); destroyLayout(); }

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sets;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    bool layoutOwned = false;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    bool poolOwned = false;
};


struct DescWrite {
    uint32_t writeCount = 0;
    std::vector<VkWriteDescriptorSet> writes;

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

    DescWrite& updateDescSet(VkDevice lDevice);
    DescWrite& updateDescSets(VkDevice lDevice);
};

}
