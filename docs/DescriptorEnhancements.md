# Descriptor System Enhancements

## Overview

The descriptor system has been enhanced with additional capabilities while maintaining full backward compatibility. All existing code will continue to work without any changes, but new features are now available for more advanced use cases.

## Enhanced Classes

### 1. DescLayout

**Enhanced `create()` function:**
```cpp
void create(VkDevice device, 
           const std::vector<VkDescriptorSetLayoutBinding>& bindings, 
           VkDescriptorSetLayoutCreateFlags flags = 0, 
           const void* pNext = nullptr);
```

**New Features:**
- **Flags Support**: Can now specify layout creation flags (e.g., `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR`)
- **Extension Chain**: Support for `pNext` extension structures
- **Binding Count Tracking**: Internal tracking of binding count for utility purposes

**New Methods:**
- `uint32_t getBindingCount() const` - Returns the number of bindings in the layout

**Usage Examples:**
```cpp
// Backward compatible (existing code works unchanged)
descLayout.create(device, bindings);

// With flags for push descriptors
descLayout.create(device, bindings, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

// With extension chain
descLayout.create(device, bindings, 0, &someExtensionStruct);
```

### 2. DescPool

**Enhanced `create()` function:**
```cpp
void create(VkDevice device, 
           const std::vector<VkDescriptorPoolSize>& poolSizes, 
           uint32_t maxSets, 
           VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
```

**New Features:**
- **Flexible Flags**: Customizable pool creation flags
- **Pool Management**: Enhanced pool lifecycle management

**New Methods:**
- `void reset(VkDescriptorPoolResetFlags flags = 0)` - Reset the pool to recycle descriptor sets
- `uint32_t getMaxSets() const` - Get the maximum number of sets this pool can allocate

**Usage Examples:**
```cpp
// Backward compatible (existing code works unchanged)
descPool.create(device, poolSizes, maxSets);

// Without free descriptor set bit (more efficient if you don't need to free individual sets)
descPool.create(device, poolSizes, maxSets, 0);

// Reset pool to recycle all descriptor sets
descPool.reset();
```

### 3. DescSet

**Enhanced `allocate()` function:**
```cpp
void allocate(VkDevice device, 
             VkDescriptorPool pool, 
             VkDescriptorSetLayout layout, 
             const uint32_t* variableDescriptorCounts = nullptr, 
             const void* pNext = nullptr);
```

**New Features:**
- **Variable Descriptor Counts**: Support for variable descriptor count extensions
- **Extension Chain**: Support for `pNext` extension structures
- **Batch Allocation**: Static method for allocating multiple descriptor sets efficiently

**New Methods:**
- `static std::vector<DescSet> allocateBatch(...)` - Allocate multiple descriptor sets in one call
- `bool valid() const` - Check if the descriptor set is valid

**Usage Examples:**
```cpp
// Backward compatible (existing code works unchanged)
descSet.allocate(device, pool, layout);

// With variable descriptor counts (for bindless textures, etc.)
uint32_t variableCounts[] = {1000}; // Allocate space for 1000 textures
descSet.allocate(device, pool, layout, variableCounts);

// Batch allocation
std::vector<VkDescriptorSetLayout> layouts = {layout1, layout2, layout3};
auto descSets = DescSet::allocateBatch(device, pool, layouts);
```

### 4. DescWrite

**Enhanced Features:**
- **Texel Buffer Support**: Added support for texel buffer views
- **Copy Operations**: Support for descriptor set copy operations
- **Better Resource Management**: Enhanced lifetime management for descriptor info
- **Flexible Updates**: Combined write and copy operations

**New Methods:**
```cpp
// Texel buffer support
DescWrite& setTexelBufferView(std::vector<VkBufferView> texelBufferViews);

// Extension support
DescWrite& setNext(const void* pNext);

// Copy operations
DescWrite& addCopy();
DescWrite& setSrcSet(VkDescriptorSet srcSet);
DescWrite& setSrcBinding(uint32_t srcBinding);
DescWrite& setSrcArrayElement(uint32_t srcArrayElement);
DescWrite& setCopyDescCount(uint32_t count);

// Enhanced update with copy support
DescWrite& updateDescSets(VkDevice device, bool includeCopies = false);

// Utility methods
void clear();
uint32_t getWriteCount() const;
uint32_t getCopyCount() const;
DescWrite& reset(); // Alias for clear() with chaining
```

**Usage Examples:**
```cpp
// Backward compatible (existing code works unchanged)
DescWrite()
    .setDstSet(descriptorSet)
    .setType(DescType::UniformBuffer)
    .setBufferInfo({bufferInfo})
    .updateDescSets(device);

// With texel buffer views
DescWrite()
    .setDstSet(descriptorSet)
    .setType(DescType::UniformTexelBuffer)
    .setTexelBufferView({bufferView})
    .updateDescSets(device);

// Copy operations
DescWrite()
    .addCopy()
    .setSrcSet(sourceDescSet)
    .setSrcBinding(0)
    .setDstSet(destDescSet)
    .setDstBinding(1)
    .setCopyDescCount(1)
    .updateDescSets(device, true); // Include copies

// Multiple operations
DescWrite writer;
writer.addWrite()
      .setDstSet(set1)
      .setType(DescType::UniformBuffer)
      .setBufferInfo({buffer1Info});
      
writer.addWrite()
      .setDstSet(set2)
      .setType(DescType::CombinedImageSampler)
      .setImageInfo({imageInfo});
      
writer.addCopy()
      .setSrcSet(set1)
      .setDstSet(set3)
      .setCopyDescCount(1);
      
writer.updateDescSets(device, true);
```

## Advanced Use Cases

### 1. Bindless Textures
```cpp
// Create layout with variable descriptor count
std::vector<VkDescriptorSetLayoutBinding> bindings = {
    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
};

VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
bindingFlags.bindingCount = 1;
bindingFlags.pBindingFlags = &flags;

descLayout.create(device, bindings, 0, &bindingFlags);

// Allocate with variable count
uint32_t maxTextures = 1000;
descSet.allocate(device, pool, layout, &maxTextures);
```

### 2. Push Descriptors
```cpp
// Create layout for push descriptors
descLayout.create(device, bindings, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
```

### 3. Efficient Pool Management
```cpp
// Create pool without individual free capability (more efficient)
descPool.create(device, poolSizes, maxSets, 0);

// Reset entire pool when done with frame
descPool.reset();
```

## Backward Compatibility

All existing code continues to work without any changes:

```cpp
// This existing code works exactly the same
DescLayout layout;
layout.create(device, bindings);

DescPool pool;
pool.create(device, poolSizes, maxSets);

DescSet descSet;
descSet.allocate(device, pool, layout);

DescWrite()
    .setDstSet(descSet)
    .setType(DescType::UniformBuffer)
    .setBufferInfo({bufferInfo})
    .updateDescSets(device);
```

## Performance Considerations

1. **Batch Allocation**: Use `DescSet::allocateBatch()` when allocating multiple descriptor sets
2. **Pool Reset**: Use `DescPool::reset()` instead of individual `free()` calls when possible
3. **Copy Operations**: Use descriptor copies to duplicate similar descriptor sets efficiently
4. **Variable Counts**: Use variable descriptor counts for bindless rendering patterns

## Migration Guide

No migration is required - all existing code continues to work. To take advantage of new features:

1. **Add flags where needed**: Simply add flag parameters to existing `create()` calls
2. **Use new utility methods**: Leverage `reset()`, `valid()`, and count methods for better management
3. **Enable copy operations**: Use `addCopy()` and `updateDescSets(device, true)` for descriptor copying
4. **Support texel buffers**: Use `setTexelBufferView()` for texel buffer descriptors

The enhanced descriptor system provides significantly more flexibility while maintaining the simple, chainable API design of the original implementation.