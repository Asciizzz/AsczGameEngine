#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

struct BufferData;

// This is not gonna cut it, you gotta have to split them into parts

struct DynamicDescriptor {
    DynamicDescriptor() = default;
    DynamicDescriptor(VkDevice lDevice) : lDevice(lDevice) {}
    void init(VkDevice lDevice) { this->lDevice = lDevice; }
    
    ~DynamicDescriptor();

    VkDevice lDevice;

    bool uniqueLayout = true;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    void createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void referenceLayout(VkDescriptorSetLayout layout);

    uint32_t maxSets = 0;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPoolSize> poolSizes; // Contain both <Type> and <size>
    void createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    std::vector<VkDescriptorSet> sets;
    const VkDescriptorSet getSet(uint32_t index) const { return sets[index]; }

    VkDescriptorSet set = VK_NULL_HANDLE;
    const VkDescriptorSet getSet() const { return set; }

    // Some really helpful functions
    static VkDescriptorSetLayoutBinding fastBinding(uint32_t binding,
                                                    VkDescriptorType type,
                                                    VkShaderStageFlags stageFlags,
                                                    uint32_t descriptorCount = 1);
};

struct DescLayout {
    struct BindInfo {
        uint32_t binding;
        uint32_t descCount;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
    };

    DescLayout() = default;
    ~DescLayout() { cleanup(); } void cleanup();

    // Delete copy constructor and assignment operator
    DescLayout(const DescLayout&) = delete;
    DescLayout& operator=(const DescLayout&) = delete;
    // Implement move constructor and move assignment operator
    DescLayout(DescLayout&& other) noexcept;
    DescLayout& operator=(DescLayout&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    const VkDescriptorSetLayout get() const { return layout; }

    void create(const VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void create(const VkDevice lDevice, const std::vector<BindInfo>& bindings);

    static VkDescriptorSetLayoutBinding fastBinding(const BindInfo& binder);
};

struct DescPool {
    DescPool() = default;
    ~DescPool() { cleanup(); }
    void cleanup();

    // Delete copy constructor and assignment operator
    DescPool(const DescPool&) = delete;
    DescPool& operator=(const DescPool&) = delete;
    // Implement move constructor and move assignment operator
    DescPool(DescPool&& other) noexcept;
    DescPool& operator=(DescPool&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    const VkDescriptorPool get() const { return pool; }

    void create(const VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
};


struct DescSets {
    DescSets() = default;
    ~DescSets() { cleanup(); } void cleanup();

    VkDevice lDevice = VK_NULL_HANDLE;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> sets;
    void allocate(const VkDevice lDevice, const VkDescriptorPool pool, const VkDescriptorSetLayout layout, uint32_t count);

    const VkDescriptorSet get(uint32_t index=0) const { return sets[index]; }
};

}
