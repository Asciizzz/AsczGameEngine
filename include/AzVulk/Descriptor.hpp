#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

struct DescWrapper {
    struct LayoutBind {
        uint32_t binding;
        uint32_t descCount;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
    };

    DescWrapper(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    ~DescWrapper() { cleanup(); } void cleanup();

    void cleanupLayout();
    void cleanupPool();
    void cleanupSets();

    DescWrapper(const DescWrapper&) = delete;
    DescWrapper& operator=(const DescWrapper&) = delete;
    DescWrapper(DescWrapper&& other) noexcept;
    DescWrapper& operator=(DescWrapper&& other) noexcept;

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

    // Create standalone descriptor objects
    static VkDescriptorSetLayout createLayout(VkDevice lDevice, const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    static VkDescriptorSetLayout createLayout(VkDevice lDevice, const std::vector<LayoutBind>& bindingInfos);
    static VkDescriptorPool createPool(VkDevice lDevice, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    static VkDescriptorSet createSet(VkDevice lDevice, VkDescriptorPool pool, VkDescriptorSetLayout layout);

    static void destroyLayout(VkDevice lDevice, VkDescriptorSetLayout layout);
    static void destroyPool(VkDevice lDevice, VkDescriptorPool pool);
    // If you destroy a pool, all sets allocated from it are also freed
    static void freeSet(VkDevice lDevice, VkDescriptorPool pool, VkDescriptorSet set);

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
