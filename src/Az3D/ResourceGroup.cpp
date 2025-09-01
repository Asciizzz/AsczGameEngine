#include "Az3D/ResourceGroup.hpp"
#include "Az3D/TinyLoader.hpp"
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
    textures.insert(textures.begin(), defaultTexture); // Insert at index 0
}

void ResourceGroup::cleanup() {
    // Actually for the most part you don't have to clean up anything since they have
    // pretty robust destructors that take care of Vulkan resource deallocation.

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
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = vkDevice->lDevice;

    createMeshStaticBuffers();
    createMeshSkinnedBuffers();

    createMaterialBuffer();
    createMaterialDescSet();

    createSamplers();
    createTextureDescSet();
}


size_t ResourceGroup::addTexture(std::string name, std::string imagePath, uint32_t mipLevels) {
    // First load the image using TinyLoader
    TinyTexture tinyTexture = TinyLoader::loadImage(imagePath);
    
    // Create texture from TinyTexture
    SharedPtr<Texture> texture = createTexture(tinyTexture, mipLevels);

    // If null return the default 0 index
    if (!texture) return 0;

    // Set the texture path
    texture->path = imagePath;

    size_t index = textures.size();
    textures.push_back(texture);

    textureNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMaterial(std::string name, const Material& material) {
    size_t index = materials.size();
    materials.push_back(material);

    materialNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH) {
    if (hasBVH) mesh->createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(mesh);

    meshStaticNameToIndex[name] = index;
    return index;
}
size_t ResourceGroup::addMeshStatic(std::string name, std::string filePath, bool hasBVH) {
    // Use the general purpose loader that auto-detects file type
    SharedPtr<MeshStatic> newMesh = TinyLoader::loadMeshStatic(filePath);

    if (hasBVH) newMesh->createBVH();

    size_t index = meshStatics.size();
    meshStatics.push_back(newMesh);

    meshStaticNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMeshSkinned(std::string name, SharedPtr<MeshSkinned> mesh) {
    size_t index = meshSkinneds.size();
    meshSkinneds.push_back(mesh);
    
    meshSkinnedNameToIndex[name] = index;
    return index;
}

size_t ResourceGroup::addMeshSkinned(std::string name, std::string filePath) {
    TinyRig rig = TinyLoader::loadMeshSkinned(filePath, false); // Don't load skeleton for now
    
    size_t index = meshSkinneds.size();
    meshSkinneds.push_back(rig.mesh);
    
    meshSkinnedNameToIndex[name] = index;
    return index;
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


Texture* ResourceGroup::getTexture(std::string name) const {
    size_t index = getTextureIndex(name);
    return index != SIZE_MAX ? textures[index].get() : nullptr;
}

Material* ResourceGroup::getMaterial(std::string name) const {
    size_t index = getMaterialIndex(name);
    return index != SIZE_MAX ? const_cast<Material*>(&materials[index]) : nullptr;
}

MeshStatic* ResourceGroup::getMeshStatic(std::string name) const {
    size_t index = getMeshStaticIndex(name);
    return index != SIZE_MAX ? meshStatics[index].get() : nullptr;
}

MeshSkinned* ResourceGroup::getMeshSkinned(std::string name) const {
    size_t index = getMeshSkinnedIndex(name);
    return index != SIZE_MAX ? meshSkinneds[index].get() : nullptr;
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

    stagingBuffer.uploadData(materials.data());

    // --- lDevice-local buffer (GPU only, STORAGE + DST) ---
    matBuffer.cleanup();

    matBuffer.initVkDevice(vkDevice);
    matBuffer.setProperties(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    matBuffer.createBuffer();

    // --- copy staging -> lDevice local ---
    TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, matBuffer.buffer, 1, &copyRegion);

    copyCmd.endAndSubmit();
}

// Descriptor set creation
void ResourceGroup::createMaterialDescSet() {
    VkDevice lDevice = vkDevice->lDevice;

    // Clear existing resources
    matDescPool.cleanup();
    matDescLayout.cleanup();
    matDescSet.cleanup();

    // Create descriptor pool and layout
    matDescPool.create(lDevice, { {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    matDescLayout.create(lDevice, {
        DescLayout::BindInfo{
            0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT
        }
    });

    // Allocate descriptor set
    matDescSet.allocate(lDevice, matDescPool.get(), matDescLayout.get(), 1);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = matBuffer.buffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = matDescSet.get();
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

    // layout: binding 0 = images, binding 1 = samplers
    texDescLayout.create(lDevice, {
        {0, textureCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
        {1, samplerCount, VK_DESCRIPTOR_TYPE_SAMPLER,       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT}
    });

    texDescPool.create(lDevice, {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureCount},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       samplerCount}
    }, 1);

    texDescSet.allocate(lDevice, texDescPool.get(), texDescLayout.get(), 1);

    // Write sampled images
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; i++) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView   = textures[i]->view;
        imageInfos[i].sampler     = VK_NULL_HANDLE;
    }

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet          = texDescSet.get();
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
    samplerWrite.dstSet          = texDescSet.get();
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

        BufferData vBufferData, iBufferData;

        // Upload vertex
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(VertexStatic), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices.data());

        vBufferData.initVkDevice(vkDevice);
        vBufferData.setProperties(
            vertices.size() * sizeof(VertexStatic),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vBufferData.createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(VertexStatic);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData.buffer, 1, &vertexCopyRegion);
        vBufferData.hostVisible = false;

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

        iBufferData.initVkDevice(vkDevice);
        iBufferData.setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        iBufferData.createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, iBufferData.buffer, 1, &indexCopyRegion);
        iBufferData.hostVisible = false;

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

        BufferData vBufferData, iBufferData;

        // Upload vertex
        BufferData vertexStagingBuffer;
        vertexStagingBuffer.initVkDevice(vkDevice);
        vertexStagingBuffer.setProperties(
            vertices.size() * sizeof(VertexSkinned), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        vertexStagingBuffer.createBuffer();
        vertexStagingBuffer.mappedData(vertices.data());

        vBufferData.initVkDevice(vkDevice);
        vBufferData.setProperties(
            vertices.size() * sizeof(VertexSkinned),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vBufferData.createBuffer();

        TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy vertexCopyRegion{};
        vertexCopyRegion.srcOffset = 0;
        vertexCopyRegion.dstOffset = 0;
        vertexCopyRegion.size = vertices.size() * sizeof(VertexSkinned);

        vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData.buffer, 1, &vertexCopyRegion);
        vBufferData.hostVisible = false;

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

        iBufferData.initVkDevice(vkDevice);
        iBufferData.setProperties(
            indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        iBufferData.createBuffer();

        TemporaryCommand indexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = 0;
        indexCopyRegion.size = indices.size() * sizeof(uint32_t);

        vkCmdCopyBuffer(indexCopyCmd.cmdBuffer, indexStagingBuffer.buffer, iBufferData.buffer, 1, &indexCopyRegion);
        iBufferData.hostVisible = false;

        indexCopyCmd.endAndSubmit();


        // Append buffers
        vskinnedBuffers.push_back(std::move(vBufferData));
        iskinnedBuffers.push_back(std::move(iBufferData));
    }
}