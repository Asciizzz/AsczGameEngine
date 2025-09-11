#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

struct DescSets {
    struct LayoutBind {
        uint32_t binding;
        uint32_t descCount;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
    };

    DescSets(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    ~DescSets() { cleanup(); } void cleanup();

    void cleanupLayout();
    void cleanupPool();
    void cleanupSets();

    DescSets(const DescSets&) = delete;
    DescSets& operator=(const DescSets&) = delete;
    DescSets(DescSets&& other) noexcept;
    DescSets& operator=(DescSets&& other) noexcept;

    VkDevice lDevice = VK_NULL_HANDLE;
    void init(const VkDevice lDevice) { this->lDevice = lDevice; }

    VkDescriptorSetLayout layout = VK_NULL_HANDLE; bool layoutOwned = true;
    void createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void createLayout(const std::vector<LayoutBind>& bindingInfos); // Version using LayoutBind struct
    void borrowLayout(VkDescriptorSetLayout layout);

    VkDescriptorPool pool = VK_NULL_HANDLE; bool poolOwned = true;
    void createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    void borrowPool(VkDescriptorPool pool);

    std::vector<VkDescriptorSet> sets;
    void allocate(uint32_t count=1);

    VkDescriptorSet get(uint32_t index=0) const {
        return index < sets.size() ? sets[index] : VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorPool getPool() const { return pool; }

    static VkDescriptorSetLayoutBinding fastBinding(const LayoutBind& binder);
};

}
