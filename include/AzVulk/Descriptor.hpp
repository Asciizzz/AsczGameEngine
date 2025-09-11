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

    void init(const VkDevice lDevice) { this->lDevice = lDevice; }

    void createLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void createLayout(const std::vector<LayoutBind>& bindingInfos); // Version using LayoutBind struct
    void borrowLayout(VkDescriptorSetLayout layout);

    void createPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    void borrowPool(VkDescriptorPool pool);

    void allocate(uint32_t count=1);

    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorPool getPool() const { return pool; }
    VkDescriptorSet get(uint32_t index=0) const {
        return index < sets.size() ? sets[index] : VK_NULL_HANDLE;
    }

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sets;
    VkDescriptorPool      pool        = VK_NULL_HANDLE;
    bool                  poolOwned   = true;
    VkDescriptorSetLayout layout      = VK_NULL_HANDLE;
    bool                  layoutOwned = true;

    static VkDescriptorSetLayoutBinding fastBinding(const LayoutBind& binder);
};

}
