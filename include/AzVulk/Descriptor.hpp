#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

struct BufferData;

struct DescLayout {
    struct BindInfo {
        uint32_t binding;
        uint32_t descCount;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
    };

    DescLayout(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    ~DescLayout() { cleanup(); } void cleanup();

    DescLayout(const DescLayout&) = delete;
    DescLayout& operator=(const DescLayout&) = delete;
    DescLayout(DescLayout&& other) noexcept;
    DescLayout& operator=(DescLayout&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    void init(const VkDevice lDevice) { this->lDevice = lDevice; }

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout get() const { return layout; }

    void create(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void create(const std::vector<BindInfo>& bindings);

    static VkDescriptorSetLayoutBinding fastBinding(const BindInfo& binder);
};

struct DescPool {
    DescPool(VkDevice lDevice=VK_NULL_HANDLE) : lDevice(lDevice) {}
    ~DescPool() { cleanup(); } void cleanup();

    DescPool(const DescPool&) = delete;
    DescPool& operator=(const DescPool&) = delete;
    DescPool(DescPool&& other) noexcept;
    DescPool& operator=(DescPool&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    void init(const VkDevice lDevice) { this->lDevice = lDevice; }

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorPool get() const { return pool; }

    void create(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
};


struct DescSets {
    DescSets(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    ~DescSets() { weakCleanup(); } void weakCleanup();

    void explicitCleanup(VkDescriptorPool pool);

    DescSets(const DescSets&) = delete;
    DescSets& operator=(const DescSets&) = delete;
    DescSets(DescSets&& other) noexcept;
    DescSets& operator=(DescSets&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    void init(const VkDevice lDevice) { this->lDevice = lDevice; }

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> sets;
    void allocate(VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count);

    VkDescriptorSet get(uint32_t index=0) const {
        return index < sets.size() ? sets[index] : VK_NULL_HANDLE;
    }
};

}
