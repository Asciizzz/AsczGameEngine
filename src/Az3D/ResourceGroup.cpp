#include "Az3D/ResourceGroup.hpp"
#include "Az3D/TinyLoader.hpp"
#include "Az3D/RigSkeleton.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

using namespace AzVulk;
using namespace Az3D;

// ============================================================================
// ======================= General resource group stuff =======================
// ============================================================================

ResourceGroup::ResourceGroup(Device* vkDevice):
vkDevice(vkDevice) {

    // Create default white pixel texture manually
    TinyTexture defaultWhitePixel;
    defaultWhitePixel.width = 1;
    defaultWhitePixel.height = 1;
    defaultWhitePixel.channels = 4;
    defaultWhitePixel.data = new uint8_t[4]{255, 255, 255, 255}; // White pixel
    
    auto defaultTexture = createTexture(defaultWhitePixel, 1);
    defaultTexture->path = "__default__";
    textures.insert(textures.begin(), std::move(defaultTexture)); // Insert at index 0 with move
}

void ResourceGroup::cleanup() {
    VkDevice lDevice = vkDevice->lDevice;

    // Cleanup textures
    for (auto& tex : textures) {
        if (tex->view != VK_NULL_HANDLE) vkDestroyImageView(lDevice, tex->view, nullptr);
        if (tex->image != VK_NULL_HANDLE) vkDestroyImage(lDevice, tex->image, nullptr);
        if (tex->memory != VK_NULL_HANDLE) vkFreeMemory(lDevice, tex->memory, nullptr);
    }
    textures.clear();

    // Cleanup textures' samplers
    for (auto& sampler : samplers) {
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(lDevice, sampler, nullptr);
    }
    samplers.clear();

    // Skeleton descriptor cleanup is handled automatically by SharedPtr destructors
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = vkDevice->lDevice;

    createMeshStaticBuffers();
    createMeshSkinnedBuffers();
    createRigBuffers();


    createMaterialBuffer();
    createMaterialDescSet();

    createSamplers();

    createTextureDescSet();

    printf("Checkpoint\n");
    createRigDescSets();
}


size_t ResourceGroup::addTexture(std::string name, std::string imagePath, uint32_t mipLevels) {
    std::string uniqueName = getUniqueName(name, textureNameCounts);

    TinyTexture tinyTexture = TinyLoader::loadImage(imagePath);
    SharedPtr<Texture> texture = createTexture(tinyTexture, mipLevels);

    // If null return the default 0 index
    if (!texture) return 0;

    // Set the texture path
    texture->path = imagePath;

    size_t index = textures.size();
    textures.push_back(texture);

    textureNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addMaterial(std::string name, const Material& material) {
    std::string uniqueName = getUniqueName(name, materialNameCounts);
    
    size_t index = materials.size();
    materials.push_back(MakeShared<Material>(material));

    materialNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH) {
    std::string uniqueName = getUniqueName(name, meshStaticNameCounts);
    
    if (hasBVH) mesh->createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(mesh);

    meshStaticNameToIndex[uniqueName] = index;
    return index;
}
size_t ResourceGroup::addMeshStatic(std::string name, std::string filePath, bool hasBVH) {
    std::string uniqueName = getUniqueName(name, meshStaticNameCounts);

    MeshStatic newMesh = TinyLoader::loadMeshStatic(filePath);
    if (hasBVH) newMesh.createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(MakeShared<MeshStatic>(std::move(newMesh)));

    meshStaticNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addMeshSkinned(std::string name, SharedPtr<MeshSkinned> mesh) {
    std::string uniqueName = getUniqueName(name, meshSkinnedNameCounts);

    size_t index = meshSkinneds.size();
    meshSkinneds.push_back(mesh);

    meshSkinnedNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addMeshSkinned(std::string name, std::string filePath) {
    std::string uniqueName = getUniqueName(name, meshSkinnedNameCounts);
    
    TinyModel rig = TinyLoader::loadMeshSkinned(filePath, false);
    
    size_t index = meshSkinneds.size();
    meshSkinneds.push_back(MakeShared<MeshSkinned>(std::move(rig.mesh)));

    meshSkinnedNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addRig(std::string name, SharedPtr<RigSkeleton> rig) {
    std::string uniqueName = getUniqueName(name, rigNameCounts);

    size_t index = rigs.size();
    rigs.push_back(rig);

    rigNameToIndex[uniqueName] = index;
    return index;
}

size_t ResourceGroup::addRig(std::string name, std::string filePath) {
    std::string uniqueName = getUniqueName(name, rigNameCounts);

    TinyModel model = TinyLoader::loadMeshSkinned(filePath, true);

    size_t index = rigs.size();
    rigs.push_back(MakeShared<RigSkeleton>(std::move(model.rig)));
    rigNameToIndex[uniqueName] = index;
    return index;
}

std::pair<size_t, size_t> ResourceGroup::addRiggedModel(std::string name, std::string filePath) {
    std::string uniqueName = getUniqueName(name, meshSkinnedNameCounts);
    std::string skeletonUniqueName = getUniqueName(name + "_skeleton", rigNameCounts);

    TinyModel rig = TinyLoader::loadMeshSkinned(filePath, true); // Load both mesh and skeleton
    
    // Add mesh
    size_t meshIndex = meshSkinneds.size();
    meshSkinneds.push_back(MakeShared<MeshSkinned>(std::move(rig.mesh)));
    meshSkinnedNameToIndex[uniqueName] = meshIndex;
    
    // Add rig
    size_t rigIndex = rigs.size();
    rigs.push_back(MakeShared<RigSkeleton>(std::move(rig.rig)));
    rigNameToIndex[skeletonUniqueName] = rigIndex;

    return { meshIndex, rigIndex };
}

size_t ResourceGroup::getTextureIndex(std::string name) const {
    auto it = textureNameToIndex.find(name);
    return it != textureNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMaterialIndex(std::string name) const {
    auto it = materialNameToIndex.find(name);
    return it != materialNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMeshStaticIndex(std::string name) const {
    auto it = meshStaticNameToIndex.find(name);
    return it != meshStaticNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getMeshSkinnedIndex(std::string name) const {
    auto it = meshSkinnedNameToIndex.find(name);
    return it != meshSkinnedNameToIndex.end() ? it->second : SIZE_MAX;
}

size_t ResourceGroup::getRigIndex(std::string name) const {
    auto it = rigNameToIndex.find(name);
    return it != rigNameToIndex.end() ? it->second : SIZE_MAX;
}


Texture* ResourceGroup::getTexture(std::string name) const {
    size_t index = getTextureIndex(name);
    return index != SIZE_MAX ? textures[index].get() : nullptr;
}

Material* ResourceGroup::getMaterial(std::string name) const {
    size_t index = getMaterialIndex(name);
    return index != SIZE_MAX ? materials[index].get() : nullptr;
}

MeshStatic* ResourceGroup::getMeshStatic(std::string name) const {
    size_t index = getMeshStaticIndex(name);
    return index != SIZE_MAX ? meshStatics[index].get() : nullptr;
}

MeshSkinned* ResourceGroup::getMeshSkinned(std::string name) const {
    size_t index = getMeshSkinnedIndex(name);
    return index != SIZE_MAX ? meshSkinneds[index].get() : nullptr;
}

RigSkeleton* ResourceGroup::getRig(std::string name) const {
    size_t index = getRigIndex(name);
    return index != SIZE_MAX ? rigs[index].get() : nullptr;
}


// ============================================================================
// ========================= MATERIAL =========================================
// ============================================================================

void ResourceGroup::createMaterialBuffer() {
    VkDeviceSize bufferSize = sizeof(Material) * materials.size();

    // --- staging buffer (CPU visible) ---
    BufferData stagingBuffer;
    stagingBuffer.initVkDevice(vkDevice);
    stagingBuffer.setProperties(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.createBuffer();

    std::vector<Material> materialCopies;
    materialCopies.reserve(materials.size());
    for (const auto& material : materials) {
        materialCopies.push_back(*material);
    }
    // Upload the data, not the pointers
    stagingBuffer.uploadData(materialCopies.data());

    // --- Device-local buffer (GPU only, STORAGE + DST) ---
    if (!matBuffer) matBuffer = MakeUnique<BufferData>();
    
    matBuffer->initVkDevice(vkDevice);
    matBuffer->setProperties(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    matBuffer->createBuffer();

    // --- copy staging -> Device local ---
    TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, matBuffer->buffer, 1, &copyRegion);

    copyCmd.endAndSubmit();
}

// Descriptor set creation
void ResourceGroup::createMaterialDescSet() {
    VkDevice lDevice = vkDevice->lDevice;

    // Create fresh UniquePtr objects (automatically cleans up any existing ones)
    matDescSet = MakeUnique<DescSets>();
    matDescPool = MakeUnique<DescPool>();
    matDescLayout = MakeUnique<DescLayout>();
    
    matDescSet->init(lDevice);
    matDescPool->init(lDevice);
    matDescLayout->init(lDevice);

    // Create descriptor pool and layout
    matDescPool->create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    matDescLayout->create({
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    // Allocate descriptor set
    matDescSet->allocate(matDescPool->get(), matDescLayout->get(), 1);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = matBuffer->buffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = matDescSet->get();
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &materialBufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
}




// ============================================================================
// =================== Texture Implementation Utilities ===================
// ============================================================================

void createImageImpl(AzVulk::Device* vkDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, 
                                VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                                VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(vkDevice->lDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkDevice->lDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = Device::findMemoryType(memRequirements.memoryTypeBits, properties, vkDevice->pDevice);

    if (vkAllocateMemory(vkDevice->lDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vkDevice->lDevice, image, imageMemory, 0);
}

void createImageViewImpl(AzVulk::Device* vkDevice, VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.image                           = image;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(vkDevice->lDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }
}


void transitionImageLayoutImpl(AzVulk::Device* vkDevice, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    TemporaryCommand tempCmd(vkDevice, vkDevice->graphicsPoolWrapper);

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(tempCmd.cmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void copyBufferToImageImpl(AzVulk::Device* vkDevice, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    TemporaryCommand tempCmd(vkDevice, vkDevice->graphicsPoolWrapper);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    vkCmdCopyBufferToImage(tempCmd.cmdBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void generateMipmapsImpl(AzVulk::Device* vkDevice, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(vkDevice->pDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    TemporaryCommand tempCmd(vkDevice, vkDevice->graphicsPoolWrapper);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(tempCmd.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0]                 = { 0, 0, 0 };
        blit.srcOffsets[1]                 = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0]                 = { 0, 0, 0 };
        blit.dstOffsets[1]                 = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(tempCmd.cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(tempCmd.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1)  mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(tempCmd.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}



// ============================================================================
// ========================= TEXTURES =========================================
// ============================================================================

SharedPtr<Texture> ResourceGroup::createTexture(const TinyTexture& tinyTexture, uint32_t mipLevels) {
    try {
        Texture texture;

        // Use the provided TinyTexture data
        uint8_t* pixels = tinyTexture.data;
        int width = tinyTexture.width;
        int height = tinyTexture.height;

        VkDeviceSize imageSize = width * height * 4; // Force 4 channels (RGBA)

        if (!pixels) throw std::runtime_error("Failed to load texture from TinyTexture");

        // Dynamic mipmap levels (if not provided)
        mipLevels += (static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1) * !mipLevels;


        BufferData stagingBuffer;
        stagingBuffer.initVkDevice(vkDevice);
        stagingBuffer.setProperties(
            imageSize * sizeof(uint8_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        stagingBuffer.createBuffer();
        stagingBuffer.uploadData(pixels);

        // Note: TinyTexture cleanup is handled by caller

        // Create texture image
        createImageImpl(vkDevice, width, height, mipLevels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image, texture.memory);

        // Transfer data and generate mipmaps
        transitionImageLayoutImpl(vkDevice, texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, 
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
        copyBufferToImageImpl(vkDevice, stagingBuffer.buffer, texture.image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        generateMipmapsImpl(vkDevice, texture.image, VK_FORMAT_R8G8B8A8_SRGB, width, height, mipLevels);

        // Create image view
        createImageViewImpl(vkDevice, texture.image, VK_FORMAT_R8G8B8A8_SRGB, mipLevels, texture.view);

        return MakeShared<Texture>(texture);

    } catch (const std::exception& e) {
        std::cout << "Application error: " << e.what() << std::endl;
        return nullptr; // Return default texture index on failure
    }
}


// --- Shared Samplers ---
void ResourceGroup::createSamplers() {
    VkDevice lDevice = vkDevice->lDevice;

    for (uint32_t i = 0; i < 5; i++) {
        VkSamplerAddressMode addressMode = static_cast<VkSamplerAddressMode>(i);

        // Skip all complex extension needing classes
        if (addressMode == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT ||
            addressMode == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE) {
            continue;
        }

        VkSamplerCreateInfo sci{};
        sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter    = VK_FILTER_LINEAR;
        sci.minFilter    = VK_FILTER_LINEAR;
        sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = addressMode;
        sci.addressModeV = addressMode;
        sci.addressModeW = addressMode;
        sci.anisotropyEnable = VK_TRUE;
        sci.maxAnisotropy    = 16.0f;
        sci.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sci.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler;
        if (vkCreateSampler(lDevice, &sci, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shared sampler");
        }

        samplers.push_back(sampler);
    }
}

void ResourceGroup::createTextureDescSet() {
    VkDevice lDevice = vkDevice->lDevice;
    uint32_t textureCount = static_cast<uint32_t>(textures.size());
    uint32_t samplerCount = static_cast<uint32_t>(samplers.size());

    texDescSet = MakeUnique<DescSets>();
    texDescPool = MakeUnique<DescPool>();
    texDescLayout = MakeUnique<DescLayout>();

    texDescSet->init(lDevice);
    texDescPool->init(lDevice);
    texDescLayout->init(lDevice);

    // layout: binding 0 = images, binding 1 = samplers
    texDescLayout->create({
        {0, textureCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
        {1, samplerCount, VK_DESCRIPTOR_TYPE_SAMPLER,       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT}
    });

    texDescPool->create({
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureCount},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       samplerCount}
    }, 1);

    texDescSet->allocate(texDescPool->get(), texDescLayout->get(), 1);

    // Write sampled images
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; i++) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView   = textures[i]->view;
        imageInfos[i].sampler     = VK_NULL_HANDLE;
    }

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet          = texDescSet->get();
    imageWrite.dstBinding      = 0;
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageWrite.descriptorCount = textureCount;
    imageWrite.pImageInfo      = imageInfos.data();

    // Write samplers
    std::vector<VkDescriptorImageInfo> samplerInfos(samplers.size());
    for (uint32_t i = 0; i < samplers.size(); i++) {
        samplerInfos[i].sampler     = samplers[i];
        samplerInfos[i].imageView   = VK_NULL_HANDLE;
        samplerInfos[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkWriteDescriptorSet samplerWrite{};
    samplerWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    samplerWrite.dstSet          = texDescSet->get();
    samplerWrite.dstBinding      = 1;
    samplerWrite.dstArrayElement = 0;
    samplerWrite.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerWrite.descriptorCount = static_cast<uint32_t>(samplerInfos.size());
    samplerWrite.pImageInfo      = samplerInfos.data();

    std::vector<VkWriteDescriptorSet> writes = {imageWrite, samplerWrite};
    vkUpdateDescriptorSets(lDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}


// ============================================================================
// =========================== MESH STATIC ====================================
// ============================================================================

void ResourceGroup::createMeshStaticBuffers() {
    for (int i = 0; i < meshStatics.size(); ++i) {
        const auto* mesh = meshStatics[i].get();
        const auto& vertices = mesh->vertices;
        const auto& indices = mesh->indices;

        UniquePtr<BufferData> vBufferData = MakeUnique<BufferData>();
        UniquePtr<BufferData> iBufferData = MakeUnique<BufferData>();

        // Upload vertex
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(VertexStatic), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices.data());

        vBufferData->initVkDevice(vkDevice);
        vBufferData->setProperties(
            vertices.size() * sizeof(VertexStatic),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vBufferData->createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(VertexStatic);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData->buffer, 1, &vertexCopyRegion);
        vBufferData->hostVisible = false;

        vertexCopyCmd.endAndSubmit();

        // Upload index

        BufferData indexStagingBuffer;
        indexStagingBuffer.initVkDevice(vkDevice);
        indexStagingBuffer.setProperties(
            indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        indexStagingBuffer.createBuffer();
        indexStagingBuffer.mappedData(indices.data());

        iBufferData->initVkDevice(vkDevice);
        iBufferData->setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        iBufferData->createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, iBufferData->buffer, 1, &indexCopyRegion);
        iBufferData->hostVisible = false;

        indexCopyCmd.endAndSubmit();


        // Append buffers
        vstaticBuffers.push_back(std::move(vBufferData));
        istaticBuffers.push_back(std::move(iBufferData));
    }
}


// ============================================================================
// =========================== MESH SKINNED ===================================
// ============================================================================

void ResourceGroup::createMeshSkinnedBuffers() {
    for (int i = 0; i < meshSkinneds.size(); ++i) {
        const auto* mesh = meshSkinneds[i].get();
        const auto& vertices = mesh->vertices;
        const auto& indices = mesh->indices;

        UniquePtr<BufferData> vBufferData = MakeUnique<BufferData>();
        UniquePtr<BufferData> iBufferData = MakeUnique<BufferData>();

        // Upload vertex
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(VertexSkinned), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices.data());

        vBufferData->initVkDevice(vkDevice);
        vBufferData->setProperties(
            vertices.size() * sizeof(VertexSkinned),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vBufferData->createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(VertexSkinned);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData->buffer, 1, &vertexCopyRegion);
        vBufferData->hostVisible = false;

        vertexCopyCmd.endAndSubmit();

        // Upload index

        BufferData indexStagingBuffer;
        indexStagingBuffer.initVkDevice(vkDevice);
        indexStagingBuffer.setProperties(
            indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        indexStagingBuffer.createBuffer();
        indexStagingBuffer.mappedData(indices.data());

        iBufferData->initVkDevice(vkDevice);
        iBufferData->setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        iBufferData->createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, iBufferData->buffer, 1, &indexCopyRegion);
        iBufferData->hostVisible = false;

        indexCopyCmd.endAndSubmit();


        // Append buffers
        vskinnedBuffers.push_back(std::move(vBufferData));
        iskinnedBuffers.push_back(std::move(iBufferData));
    }
}

// ============================================================================
// ========================== SKELETON DATA ==================================
// ============================================================================

void ResourceGroup::createRigBuffers() {
    rigBuffers.clear();

    for (size_t i = 0; i < rigs.size(); ++i) {
        const auto* rig = rigs[i].get();

        const auto& inverseBindMatrices = rig->inverseBindMatrices;

        UniquePtr<BufferData> rigBufferData = MakeUnique<BufferData>();

        // Upload inverse bind matrices
        BufferData stagingBuffer;
        stagingBuffer.initVkDevice(vkDevice);
        stagingBuffer.setProperties(
            inverseBindMatrices.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        stagingBuffer.createBuffer();
        stagingBuffer.mappedData(inverseBindMatrices.data());

        rigBufferData->initVkDevice(vkDevice);
        rigBufferData->setProperties(
            inverseBindMatrices.size() * sizeof(glm::mat4),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        rigBufferData->createBuffer();

        TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = inverseBindMatrices.size() * sizeof(glm::mat4);

        vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, rigBufferData->buffer, 1, &copyRegion);
        rigBufferData->hostVisible = false;

        copyCmd.endAndSubmit();

        // Append buffer
        rigBuffers.push_back(std::move(rigBufferData));
    }
}

void ResourceGroup::createRigDescSets() {
    VkDevice lDevice = vkDevice->lDevice;

    rigDescPool = MakeUnique<DescPool>(lDevice);
    rigDescLayout = MakeUnique<DescLayout>(lDevice);
    rigDescSets = MakeUnique<DescSets>(lDevice);

    // Create descriptor pool and layout
    uint32_t rigCount = static_cast<uint32_t>(rigs.size());

    rigDescPool->create({
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rigCount}
    }, rigCount);  // maxSets should be rigCount, not 1
    rigDescLayout->create({
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    if (rigs.empty()) return;

    // Allocate multiple descriptor sets (one for each rig)
    rigDescSets->allocate(rigDescPool->get(), rigDescLayout->get(), rigCount);

    // Bind each rig buffer to its respective descriptor set
    for (size_t i = 0; i < rigs.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer               = rigBuffers[i]->buffer;
        bufferInfo.offset               = 0;
        bufferInfo.range                = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = rigDescSets->get(i);  // Get the i-th descriptor set
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
    }
}

// ============================================================================
// ========================= HELPER FUNCTIONS =================================
// ============================================================================

std::string ResourceGroup::getUniqueName(const std::string& baseName, UnorderedMap<std::string, size_t>& nameCounts) {
    // Check if this is the first occurrence of this base name
    if (nameCounts.find(baseName) == nameCounts.end()) {
        nameCounts[baseName] = 1;
        return baseName;
    }
    
    // This base name already exists, increment count and create suffixed name
    size_t count = nameCounts[baseName]++;
    return baseName + "_" + std::to_string(count);
}