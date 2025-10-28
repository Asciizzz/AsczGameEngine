#include "tinyVK/Resource/Descriptor.hpp"
#include "tinyVK/Resource/DataBuffer.hpp"
#include <stdexcept>

using namespace tinyVK;

DescLayout::DescLayout(DescLayout&& other) noexcept {
    device = other.device;
    layout = other.layout;

    other.device = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
}
DescLayout& DescLayout::operator=(DescLayout&& other) noexcept {
    if (this != &other) {
        device = other.device;
        layout = other.layout;

        other.device = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
    }
    return *this;
}

void DescLayout::create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings, 
                        VkDescriptorSetLayoutCreateFlags flags, const void* pNext) {
    this->device = device;
    this->bindingCount = static_cast<uint32_t>(bindings.size());
    destroy();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext        = pNext;
    layoutInfo.flags        = flags;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void DescLayout::destroy() {
    if (layout == VK_NULL_HANDLE) return;

    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    layout = VK_NULL_HANDLE;
}




DescPool::DescPool(DescPool&& other) noexcept {
    device = other.device;
    pool = other.pool;

    other.device = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
}
DescPool& DescPool::operator=(DescPool&& other) noexcept {
    if (this != &other) {
        device = other.device;
        pool = other.pool;

        other.device = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
    }
    return *this;
}

void DescPool::create(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, 
                      uint32_t maxSets, VkDescriptorPoolCreateFlags flags) {
    this->device = device;
    this->maxSets = maxSets;
    destroy();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = flags;
    poolInfo.maxSets       = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void DescPool::destroy() {
    if (pool == VK_NULL_HANDLE) return;

    vkDestroyDescriptorPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
    maxSets = 0;
}

void DescPool::reset(VkDescriptorPoolResetFlags flags) {
    if (pool == VK_NULL_HANDLE) return;
    
    if (vkResetDescriptorPool(device, pool, flags) != VK_SUCCESS) {
        throw std::runtime_error("failed to reset descriptor pool");
    }
}


DescSet::DescSet(DescSet&& other) noexcept {
    device = other.device;
    set = other.set;
    layout = other.layout;
    pool = other.pool;

    other.device = VK_NULL_HANDLE;
    other.set = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
    other.pool = VK_NULL_HANDLE;
}
DescSet& DescSet::operator=(DescSet&& other) noexcept {
    if (this != &other) {
        // Clean up current resources before taking new ones
        if (set != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device, pool, 1, &set);
        }
        
        device = other.device;
        set = other.set;
        layout = other.layout;
        pool = other.pool;

        other.device = VK_NULL_HANDLE;
        other.set = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
        other.pool = VK_NULL_HANDLE;
    }
    return *this;
}

DescSet::~DescSet() {
    // CRITICAL FIX: Automatic descriptor set cleanup to prevent pool exhaustion
    if (set != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, pool, 1, &set);
    }
}

void DescSet::allocate(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, 
                    const uint32_t* variableDescriptorCounts, const void* pNext) {
    this->pool = pool;
    this->layout = layout;
    this->device = device;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext              = pNext;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    // If we have variable descriptor counts, we need to set up the variable descriptor count allocate info
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo = {};
    if (variableDescriptorCounts != nullptr) {
        variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableCountInfo.pNext = allocInfo.pNext;
        variableCountInfo.descriptorSetCount = 1;
        variableCountInfo.pDescriptorCounts = variableDescriptorCounts;
        allocInfo.pNext = &variableCountInfo;
    }

    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets");
    }
}

std::vector<DescSet> DescSet::allocateBatch(VkDevice device, VkDescriptorPool pool, 
                                             const std::vector<VkDescriptorSetLayout>& layouts,
                                             const uint32_t* variableDescriptorCounts) {
    if (layouts.empty()) {
        return {};
    }

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts        = layouts.data();

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo = {};
    if (variableDescriptorCounts != nullptr) {
        variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variableCountInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        variableCountInfo.pDescriptorCounts = variableDescriptorCounts;
        allocInfo.pNext = &variableCountInfo;
    }

    std::vector<VkDescriptorSet> vkSets(layouts.size());
    if (vkAllocateDescriptorSets(device, &allocInfo, vkSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate batch descriptor sets");
    }

    std::vector<DescSet> descSets;
    descSets.reserve(layouts.size());
    
    for (size_t i = 0; i < layouts.size(); ++i) {
        DescSet descSet;
        descSet.device = device;
        descSet.pool = pool;
        descSet.layout = layouts[i];
        descSet.set = vkSets[i];
        descSets.push_back(std::move(descSet));
    }

    return descSets;
}

void DescSet::free(VkDescriptorPool pool) {
    if (pool == VK_NULL_HANDLE) return;

    if (set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, pool, 1, &set);
        set = VK_NULL_HANDLE;
    }

    set = VK_NULL_HANDLE;
}

DescWrite& DescWrite::addWrite() {
    // Setting some default values
    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;
    newWrite.dstBinding = 0;
    newWrite.dstArrayElement = 0;
    newWrite.descriptorCount = 1;

    writes.push_back(newWrite);
    writeCount = static_cast<uint32_t>(writes.size());
    
    // Add storage slots for this write
    imageInfoStorage.emplace_back();
    bufferInfoStorage.emplace_back();
    texelBufferStorage.emplace_back();
    
    return *this;
}

VkWriteDescriptorSet& DescWrite::lastWrite() {
    if (writes.empty()) addWrite();
    return writes.back();
}

DescWrite& DescWrite::setBufferInfo(std::vector<VkDescriptorBufferInfo> bufferInfo) {
    // Store the buffer info for the current write (last one)  
    if (!writes.empty()) {
        size_t writeIndex = writes.size() - 1;
        if (writeIndex < bufferInfoStorage.size()) {
            bufferInfoStorage[writeIndex] = std::move(bufferInfo);
            lastWrite().pBufferInfo = bufferInfoStorage[writeIndex].data();
        }
    }
    lastWrite().pImageInfo = nullptr;
    lastWrite().pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setImageInfo(std::vector<VkDescriptorImageInfo> imageInfos) {
    // Store the image info for the current write (last one)
    if (!writes.empty()) {
        size_t writeIndex = writes.size() - 1;
        if (writeIndex < imageInfoStorage.size()) {
            imageInfoStorage[writeIndex] = std::move(imageInfos);
            lastWrite().pImageInfo = imageInfoStorage[writeIndex].data();
        }
    }
    lastWrite().pBufferInfo = nullptr;
    lastWrite().pTexelBufferView = nullptr;
    return *this;
}

DescWrite& DescWrite::setTexelBufferView(std::vector<VkBufferView> texelBufferViews) {
    // Store the texel buffer view for the current write (last one)
    if (!writes.empty()) {
        size_t writeIndex = writes.size() - 1;
        if (writeIndex < texelBufferStorage.size()) {
            texelBufferStorage[writeIndex] = std::move(texelBufferViews);
            lastWrite().pTexelBufferView = texelBufferStorage[writeIndex].data();
        }
    }
    lastWrite().pBufferInfo = nullptr;
    lastWrite().pImageInfo = nullptr;
    return *this;
}

DescWrite& DescWrite::setDstSet(VkDescriptorSet dstSet) {
    lastWrite().dstSet = dstSet;
    return *this;
}
DescWrite& DescWrite::setDstBinding(uint32_t dstBinding) {
    lastWrite().dstBinding = dstBinding;
    return *this;
}
DescWrite& DescWrite::setDstArrayElement(uint32_t dstArrayElement) {
    lastWrite().dstArrayElement = dstArrayElement;
    return *this;
}
DescWrite& DescWrite::setDescCount(uint32_t count) {
    lastWrite().descriptorCount = count;
    return *this;
}
DescWrite& DescWrite::setType(VkDescriptorType type) {
    lastWrite().descriptorType = type;
    return *this;
}

DescWrite& DescWrite::setNext(const void* pNext) {
    if (!writes.empty()) {
        lastWrite().pNext = pNext;
    }
    return *this;
}

DescWrite& DescWrite::addCopy() {
    VkCopyDescriptorSet newCopy{};
    newCopy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    newCopy.pNext = nullptr;
    newCopy.srcBinding = 0;
    newCopy.srcArrayElement = 0;
    newCopy.dstBinding = 0;
    newCopy.dstArrayElement = 0;
    newCopy.descriptorCount = 1;

    copies.push_back(newCopy);
    return *this;
}

VkCopyDescriptorSet& DescWrite::lastCopy() {
    if (copies.empty()) addCopy();
    return copies.back();
}

DescWrite& DescWrite::setSrcSet(VkDescriptorSet srcSet) {
    lastCopy().srcSet = srcSet;
    return *this;
}

DescWrite& DescWrite::setSrcBinding(uint32_t srcBinding) {
    lastCopy().srcBinding = srcBinding;
    return *this;
}

DescWrite& DescWrite::setSrcArrayElement(uint32_t srcArrayElement) {
    lastCopy().srcArrayElement = srcArrayElement;
    return *this;
}

DescWrite& DescWrite::setCopyDescCount(uint32_t count) {
    lastCopy().descriptorCount = count;
    return *this;
}

DescWrite& DescWrite::updateDescSets(VkDevice device, bool includeCopies) {
    if (includeCopies) {
        vkUpdateDescriptorSets(device, 
                              static_cast<uint32_t>(writes.size()), writes.data(),
                              static_cast<uint32_t>(copies.size()), copies.data());
    } else {
        vkUpdateDescriptorSets(device, writeCount, writes.data(), 0, nullptr);
    }
    return *this;
}

void DescWrite::clear() {
    writes.clear();
    copies.clear();
    imageInfoStorage.clear();
    bufferInfoStorage.clear();
    texelBufferStorage.clear();
    writeCount = 0;
}