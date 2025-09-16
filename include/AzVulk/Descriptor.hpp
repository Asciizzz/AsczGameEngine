#pragma once

#include "AzVulk/Device.hpp"

namespace AzVulk {

struct DescPool {
    DescPool(VkDevice lDevice = VK_NULL_HANDLE) : lDevice(lDevice) {}
    void init(VkDevice lDevice) { this->lDevice = lDevice; }

    ~DescPool() { destroy(); }

    DescPool(const DescPool&) = delete;
    DescPool& operator=(const DescPool&) = delete;

    VkDescriptorPool get() const { return pool; }

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
    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorPool getPool() const { return pool; }

    void allocate(VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count=1); // Borrowed pool and layout version
    void allocate(uint32_t count=1); // Owned pool and layout version

    // For self contained descriptor sets with owned pool and layout
    void createOwnLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void createOwnPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    void free(VkDescriptorPool pool);
    void destroyPool();
    void destroyLayout();

    void cleanup() { free(pool); destroyPool(); destroyLayout(); }

    /* Create standalone descriptor set
    This is for the most part safe since destroying a pool also frees its sets
    It is recommended to use the DescPool wrapper for automatic lifetime management */
    static VkDescriptorSet create(VkDevice lDevice, VkDescriptorPool pool, VkDescriptorSetLayout layout);

private:
    VkDevice lDevice = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> sets;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    bool layoutOwned = false;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    bool poolOwned = false;
};

}
