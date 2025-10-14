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

    void create(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets, 
                VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

    void destroy();
    
    // Additional utility methods
    void reset(VkDescriptorPoolResetFlags flags = 0);
    uint32_t getMaxSets() const { return maxSets; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    uint32_t maxSets = 0;
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

    void create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings, 
                VkDescriptorSetLayoutCreateFlags flags = 0, const void* pNext = nullptr);

    void destroy();
    
    // Utility methods
    uint32_t getBindingCount() const { return bindingCount; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    uint32_t bindingCount = 0;
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

    void allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, 
                  const uint32_t* variableDescriptorCounts = nullptr, const void* pNext = nullptr);

    // Batch allocation helper
    static std::vector<DescSet> allocateBatch(VkDevice device, VkDescriptorPool pool, 
                                              const std::vector<VkDescriptorSetLayout>& layouts,
                                              const uint32_t* variableDescriptorCounts = nullptr);

    // Explicit free of descriptor sets
    void free(VkDescriptorPool pool);
    
    // Utility methods
    bool valid() const { return set != VK_NULL_HANDLE; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};


struct DescWrite {
    uint32_t writeCount = 0;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkCopyDescriptorSet> copies;

    // Storage for image and buffer info to ensure lifetime - one per write
    std::vector<std::vector<VkDescriptorImageInfo>> imageInfoStorage;
    std::vector<std::vector<VkDescriptorBufferInfo>> bufferInfoStorage;
    std::vector<std::vector<VkBufferView>> texelBufferStorage;

    DescWrite() = default;
    VkWriteDescriptorSet* operator&() { return writes.data(); }

    // Write operations
    DescWrite& addWrite();
    VkWriteDescriptorSet& lastWrite();

    DescWrite& setBufferInfo(std::vector<VkDescriptorBufferInfo> bufferInfo);
    DescWrite& setImageInfo(std::vector<VkDescriptorImageInfo> imageInfos);
    DescWrite& setTexelBufferView(std::vector<VkBufferView> texelBufferViews);

    DescWrite& setDstSet(VkDescriptorSet dstSet);
    DescWrite& setDstBinding(uint32_t dstBinding);
    DescWrite& setDstArrayElement(uint32_t dstArrayElement);
    DescWrite& setDescCount(uint32_t count);
    DescWrite& setType(VkDescriptorType type);
    
    // Enhanced write methods with flags
    DescWrite& setNext(const void* pNext);

    // Copy operations
    DescWrite& addCopy();
    DescWrite& setSrcSet(VkDescriptorSet srcSet);
    DescWrite& setSrcBinding(uint32_t srcBinding);
    DescWrite& setSrcArrayElement(uint32_t srcArrayElement);
    DescWrite& setCopyDescCount(uint32_t count);

    // Batch operations
    DescWrite& updateDescSets(VkDevice device, bool includeCopies = false);
    
    // Utility methods
    void clear();
    uint32_t getWriteCount() const { return static_cast<uint32_t>(writes.size()); }
    uint32_t getCopyCount() const { return static_cast<uint32_t>(copies.size()); }
    
    // Builder pattern enhancements
    DescWrite& reset() { clear(); return *this; }

private:
    VkCopyDescriptorSet& lastCopy();
};

}
