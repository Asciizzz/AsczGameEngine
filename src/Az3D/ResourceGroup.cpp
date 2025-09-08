#include "Az3D/ResourceGroup.hpp"
#include "Tiny3D/TinyLoader.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

using namespace AzVulk;
using namespace Az3D;


ModelVK::ModelVK(const AzVulk::Device* vkDevice, const TinyModel& model) {
    
}

// ============================================================================
// ======================= General resource group stuff =======================
// ============================================================================

ResourceGroup::ResourceGroup(Device* vkDevice): vkDevice(vkDevice) {}

void ResourceGroup::cleanup() {
    VkDevice lDevice = vkDevice->lDevice;

    // Cleanup texture VK resources first
    for (auto& texVK : textureVKs) {
        if (texVK->view != VK_NULL_HANDLE) vkDestroyImageView(lDevice, texVK->view, nullptr);
        if (texVK->image != VK_NULL_HANDLE) vkDestroyImage(lDevice, texVK->image, nullptr);
        if (texVK->memory != VK_NULL_HANDLE) vkFreeMemory(lDevice, texVK->memory, nullptr);
    }
    textureVKs.clear();

    // Cleanup textures' samplers
    for (auto& sampler : samplers) {
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(lDevice, sampler, nullptr);
    }
    samplers.clear();
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = vkDevice->lDevice;

    createComponentVKsFromModels();

    createMaterialBuffer();
    createMaterialDescSet();

    createTextureSamplers();
    createTexSampIdxBuffer();
    createTextureDescSet();

    createRigSkeleBuffers();
    createRigSkeleDescSets();
}

// ===========================================================================
// Special function to convert TinyModel to Vulkan resources with lookup table
// ===========================================================================


void ResourceGroup::createComponentVKsFromModels() {
    // Create default texture and material
    MaterialVK defaultMat{};
    materialVKs.push_back(defaultMat);

    TinyTexture defaultTex;
    defaultTex.width = 1;
    defaultTex.height = 1;
    defaultTex.channels = 4;
    defaultTex.data = { 255, 255, 255, 255 }; // White pixel
    defaultTex.addressMode = TinyTexture::AddressMode::Repeat; // Default address mode
    auto defaultTexVK = createTextureVK(defaultTex);
    textureVKs.push_back(std::move(defaultTexVK));
    texSamplerIndices.push_back(0); // Default texture uses Repeat sampler (index 0)

    for (auto& model : models) {
        TinyModelVK modelVK;

        std::vector<size_t> tempGlobalTextures; // A temporary list to convert local to global indices
        for (const auto& texture : model.textures) {
            auto texVK = createTextureVK(texture);
            tempGlobalTextures.push_back(textureVKs.size());
            
            textureVKs.push_back(std::move(texVK));
            
            // Map texture address mode to sampler index
            uint32_t samplerIndex = 0; // Default to Repeat sampler (index 0)
            switch (texture.addressMode) {
                case TinyTexture::AddressMode::Repeat:
                    samplerIndex = 0;
                    break;
                case TinyTexture::AddressMode::ClampToEdge:
                    samplerIndex = 1;
                    break;
                case TinyTexture::AddressMode::ClampToBorder:
                    samplerIndex = 2;
                    break;
                default:
                    samplerIndex = 0; // Fallback to Repeat
                    break;
            }
            texSamplerIndices.push_back(samplerIndex); 
        }

        std::vector<size_t> tempGlobalMaterials; // A temporary list to convert local to global indices
        for (const auto& material : model.materials) {
            MaterialVK matVK{};

            // Shading params
            matVK.shadingParams.x = material.shading ? 1.0f : 0.0f;
            matVK.shadingParams.y = static_cast<float>(material.toonLevel);
            matVK.shadingParams.z = material.normalBlend;
            matVK.shadingParams.w = material.discardThreshold;

            // Texture indices - x for albedo, y for normal map
            bool validAlbTex = material.albTexture >= 0 && material.albTexture < static_cast<int>(model.textures.size());
            matVK.texIndices.x = validAlbTex ? static_cast<uint32_t>(tempGlobalTextures[material.albTexture]) : 0;

            bool validNrmlTex = material.nrmlTexture >= 0 && material.nrmlTexture < static_cast<int>(model.textures.size());
            matVK.texIndices.y = validNrmlTex ? static_cast<uint32_t>(tempGlobalTextures[material.nrmlTexture]) : 0;

            // z and w components unused for now
            matVK.texIndices.z = 0;
            matVK.texIndices.w = 0;

            tempGlobalMaterials.push_back(materialVKs.size());
            materialVKs.push_back(matVK);
        }


        for (size_t i = 0; i < model.submeshes.size(); ++i) {
            const auto& submesh = model.submeshes[i];

            size_t submeshVK_index = addSubmeshBuffers(submesh);
            modelVK.submeshVK_indices.push_back(submeshVK_index);
            
            uint32_t indexCount = submesh.indices.size();
            modelVK.submesh_indexCounts.push_back(indexCount);

            int localMatIndex = model.submeshes[i].matIndex;

            bool validMat = localMatIndex >= 0 && localMatIndex < static_cast<int>(model.materials.size());
            size_t globalMatIndex = validMat ? tempGlobalMaterials[localMatIndex] : 0;
            modelVK.materialVK_indices.push_back(globalMatIndex);
        }

        modelVK.skeletonIndex = -1;
        if (model.skeleton.names.size() > 0) {
            // Create skeleton
            SharedPtr<TinySkeleton> skeleton = std::make_shared<TinySkeleton>(model.skeleton);
            modelVK.skeletonIndex = skeletons.size();
            skeletons.push_back(skeleton);
        }

        modelVKs.push_back(modelVK);
    }
}


// ============================================================================
// ========================= MATERIAL =========================================
// ============================================================================

void ResourceGroup::createMaterialBuffer() {
    VkDeviceSize bufferSize = sizeof(MaterialVK) * materialVKs.size();

    // --- staging buffer (CPU visible) ---
    BufferData stagingBuffer;
    stagingBuffer.initVkDevice(vkDevice);
    stagingBuffer.setProperties(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.createBuffer();

    stagingBuffer.uploadData(materialVKs.data());

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

    matDescSet = MakeUnique<DescSets>(lDevice);
    matDescPool = MakeUnique<DescPool>(lDevice);
    matDescLayout = MakeUnique<DescLayout>(lDevice);

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
// =================== TinyTexture Implementation Utilities ===================
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

// Helper function to get Vulkan format based on channel count
// Note: Uses formats that are widely supported by Vulkan implementations
VkFormat getVulkanFormatFromChannels(int channels) {
    switch (channels) {
        case 1: return VK_FORMAT_R8_UNORM;          // Grayscale (UNORM more widely supported than SRGB for single channel)
        case 2: return VK_FORMAT_R8G8_UNORM;        // Grayscale + Alpha
        case 3: return VK_FORMAT_R8G8B8A8_SRGB;     // Convert RGB to RGBA (RGB not universally supported)
        case 4: return VK_FORMAT_R8G8B8A8_SRGB;     // RGBA
        default: 
            printf("Warning: Unsupported channel count %d, defaulting to RGBA\n", channels);
            return VK_FORMAT_R8G8B8A8_SRGB;
    }
}

// Helper function to convert texture data to match Vulkan format requirements
std::vector<uint8_t> convertTextureDataForVulkan(const TinyTexture& texture) {
    int width = texture.width;
    int height = texture.height;
    int channels = texture.channels;
    const uint8_t* srcData = texture.data.data();
    
    if (channels == 3) {
        // Convert RGB to RGBA by adding alpha channel
        size_t pixelCount = width * height;
        std::vector<uint8_t> convertedData(pixelCount * 4);
        
        for (size_t i = 0; i < pixelCount; ++i) {
            convertedData[i * 4 + 0] = srcData[i * 3 + 0]; // R
            convertedData[i * 4 + 1] = srcData[i * 3 + 1]; // G
            convertedData[i * 4 + 2] = srcData[i * 3 + 2]; // B
            convertedData[i * 4 + 3] = 255;                // A (fully opaque)
        }
        
        return convertedData;
    } else {
        // For 1, 2, or 4 channels, use data as-is
        return std::vector<uint8_t>(srcData, srcData + texture.data.size());
    }
}

UniquePtr<TextureVK> ResourceGroup::createTextureVK(const TinyTexture& texture) {
    TextureVK textureVK;

    // Use the provided TinyTexture data
    int width = texture.width;
    int height = texture.height;
    int channels = texture.channels;

    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = getVulkanFormatFromChannels(channels);
    std::vector<uint8_t> vulkanData = convertTextureDataForVulkan(texture);
    
    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (channels == 3) ? 4 : channels; // RGB becomes RGBA
    VkDeviceSize imageSize = width * height * vulkanChannels;

    if (texture.data.empty()) throw std::runtime_error("Failed to load texture from TinyTexture");

    // Dynamic mipmap levels
    uint32_t mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height)))) + 1;

    BufferData stagingBuffer;
    stagingBuffer.initVkDevice(vkDevice);
    stagingBuffer.setProperties(
        imageSize * sizeof(uint8_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.createBuffer();
    stagingBuffer.uploadData(vulkanData.data());

    // Note: TinyTexture cleanup is handled by caller

    // Create texture image with proper format
    createImageImpl(vkDevice, width, height, mipLevels, textureFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureVK.image, textureVK.memory);

    // Transfer data and generate mipmaps with proper format
    transitionImageLayoutImpl(vkDevice, textureVK.image, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, 
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
    copyBufferToImageImpl(vkDevice, stagingBuffer.buffer, textureVK.image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    generateMipmapsImpl(vkDevice, textureVK.image, textureFormat, width, height, mipLevels);

    // Create image view with proper format
    createImageViewImpl(vkDevice, textureVK.image, textureFormat, mipLevels, textureVK.view);

    return MakeUnique<TextureVK>(textureVK);
}


// --- Shared Samplers ---
void ResourceGroup::createTextureSamplers() {
    VkDevice lDevice = vkDevice->lDevice;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vkDevice->pDevice, &properties);
    float maxAnisotropy = fminf(16.0f, properties.limits.maxSamplerAnisotropy);

    VkSamplerAddressMode addressModes[] = {
        VK_SAMPLER_ADDRESS_MODE_REPEAT,         // 0
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,  // 1
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER // 2
    };

    for (const auto& addressMode : addressModes) {
        VkSamplerCreateInfo sci{};
        sci.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter        = VK_FILTER_LINEAR;
        sci.minFilter        = VK_FILTER_LINEAR;
        sci.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        sci.addressModeU     = addressMode;
        sci.addressModeV     = addressMode;
        sci.addressModeW     = addressMode;
        
        sci.anisotropyEnable = VK_TRUE;
        sci.maxAnisotropy    = maxAnisotropy;
        sci.mipLodBias       = 0.5f; // Sharper mipmaps

        sci.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        sci.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler;
        if (vkCreateSampler(lDevice, &sci, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shared sampler");
        }

        samplers.push_back(sampler);
    }
}

void ResourceGroup::createTexSampIdxBuffer() {
    VkDeviceSize bufferSize = sizeof(uint32_t) * texSamplerIndices.size();

    // --- staging buffer (CPU visible) ---
    BufferData stagingBuffer;
    stagingBuffer.initVkDevice(vkDevice);
    stagingBuffer.setProperties(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    stagingBuffer.createBuffer();
    stagingBuffer.uploadData(texSamplerIndices.data());

    // --- Device-local buffer (GPU only, STORAGE + DST) ---
    if (!textSampIdxBuffer) textSampIdxBuffer = MakeUnique<BufferData>();
    
    textSampIdxBuffer->initVkDevice(vkDevice);
    textSampIdxBuffer->setProperties(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    textSampIdxBuffer->createBuffer();

    // --- copy staging -> Device local ---
    TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, textSampIdxBuffer->buffer, 1, &copyRegion);

    copyCmd.endAndSubmit();
}

void ResourceGroup::createTextureDescSet() {
    VkDevice lDevice = vkDevice->lDevice;
    uint32_t textureCount = static_cast<uint32_t>(textureVKs.size());
    uint32_t samplerCount = static_cast<uint32_t>(samplers.size());

    texDescSet = MakeUnique<DescSets>(lDevice);
    texDescPool = MakeUnique<DescPool>(lDevice);
    texDescLayout = MakeUnique<DescLayout>(lDevice);

    // layout: binding 0 = images, binding 1 = sampler indices buffer, binding 2 = samplers  
    texDescLayout->create({
        {0, textureCount, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
        {1, 1,            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT},
        {2, samplerCount, VK_DESCRIPTOR_TYPE_SAMPLER,       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT}
    });

    texDescPool->create({
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureCount},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       samplerCount}
    }, 1);

    texDescSet->allocate(texDescPool->get(), texDescLayout->get(), 1);

    // Write sampled images
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; ++i) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView   = textureVKs[i]->view;
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

    
    // Write sampler indices buffer
    VkDescriptorBufferInfo texSampIdxBufferInfo{};
    texSampIdxBufferInfo.buffer = textSampIdxBuffer->buffer;
    texSampIdxBufferInfo.offset = 0;
    texSampIdxBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet texSampIdxBufferWrite{};
    texSampIdxBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texSampIdxBufferWrite.dstSet = texDescSet->get();
    texSampIdxBufferWrite.dstBinding = 1;
    texSampIdxBufferWrite.dstArrayElement = 0;
    texSampIdxBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    texSampIdxBufferWrite.descriptorCount = 1;
    texSampIdxBufferWrite.pBufferInfo = &texSampIdxBufferInfo;

    // Write samplers
    std::vector<VkDescriptorImageInfo> samplerInfos(samplers.size());
    for (uint32_t i = 0; i < samplers.size(); ++i) {
        samplerInfos[i].sampler     = samplers[i];
        samplerInfos[i].imageView   = VK_NULL_HANDLE;
        samplerInfos[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkWriteDescriptorSet samplerWrite{};
    samplerWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    samplerWrite.dstSet          = texDescSet->get();
    samplerWrite.dstBinding      = 2;
    samplerWrite.dstArrayElement = 0;
    samplerWrite.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    samplerWrite.descriptorCount = static_cast<uint32_t>(samplerInfos.size());
    samplerWrite.pImageInfo      = samplerInfos.data();

    std::vector<VkWriteDescriptorSet> writes = {imageWrite, texSampIdxBufferWrite, samplerWrite};
    vkUpdateDescriptorSets(lDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}


// ============================================================================
// =========================== MESH STATIC ====================================
// ============================================================================

size_t ResourceGroup::addSubmeshBuffers(const TinySubmesh& submesh) {
    const auto& vertexData = submesh.vertexData;
    const auto& indices = submesh.indices;

    UniquePtr<BufferData> vBufferData = MakeUnique<BufferData>();
    UniquePtr<BufferData> iBufferData = MakeUnique<BufferData>();

    // Upload vertex data
    BufferData vertexStagingBuffer;
    vertexStagingBuffer.initVkDevice(vkDevice);
    vertexStagingBuffer.setProperties(
        vertexData.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexStagingBuffer.createBuffer();
    vertexStagingBuffer.mapAndCopy(vertexData.data());

    vBufferData->initVkDevice(vkDevice);
    vBufferData->setProperties(
        vertexData.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    vBufferData->createBuffer();

    TemporaryCommand vertexCopyCmd(vkDevice, vkDevice->transferPoolWrapper);

    VkBufferCopy vertexCopyRegion{};
    vertexCopyRegion.srcOffset = 0;
    vertexCopyRegion.dstOffset = 0;
    vertexCopyRegion.size = vertexData.size();

    vkCmdCopyBuffer(vertexCopyCmd.cmdBuffer, vertexStagingBuffer.buffer, vBufferData->buffer, 1, &vertexCopyRegion);
    vBufferData->hostVisible = false;

    vertexCopyCmd.endAndSubmit();

    // Upload index data
    BufferData indexStagingBuffer;
    indexStagingBuffer.initVkDevice(vkDevice);
    indexStagingBuffer.setProperties(
        indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexStagingBuffer.createBuffer();
    indexStagingBuffer.mapAndCopy(indices.data());

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
    subMeshVertexBuffers.push_back(std::move(vBufferData));
    subMeshIndexBuffers.push_back(std::move(iBufferData));

    return subMeshVertexBuffers.size() - 1; // Return index of newly added buffers
}

VkBuffer ResourceGroup::getSubmeshVertexBuffer(size_t submeshVK_index) const {
    return subMeshVertexBuffers[submeshVK_index]->get();
}

VkBuffer ResourceGroup::getSubmeshIndexBuffer(size_t submeshVK_index) const {
    return subMeshIndexBuffers[submeshVK_index]->get();
}

// ============================================================================
// ========================== SKELETON DATA ==================================
// ============================================================================

void ResourceGroup::createRigSkeleBuffers() {
    skeleInvMatBuffers.clear();

    for (size_t i = 0; i < skeletons.size(); ++i) {
        const auto* skeleton = skeletons[i].get();

        const auto& inverseBindMatrices = skeleton->inverseBindMatrices;

        UniquePtr<BufferData> rigInvMatBuffer = MakeUnique<BufferData>();

        // Upload inverse bind matrices
        BufferData stagingBuffer;
        stagingBuffer.initVkDevice(vkDevice);
        stagingBuffer.setProperties(
            inverseBindMatrices.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        stagingBuffer.createBuffer();
        stagingBuffer.mapAndCopy(inverseBindMatrices.data());

        rigInvMatBuffer->initVkDevice(vkDevice);
        rigInvMatBuffer->setProperties(
            inverseBindMatrices.size() * sizeof(glm::mat4),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        rigInvMatBuffer->createBuffer();

        TemporaryCommand copyCmd(vkDevice, vkDevice->transferPoolWrapper);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = inverseBindMatrices.size() * sizeof(glm::mat4);

        vkCmdCopyBuffer(copyCmd.cmdBuffer, stagingBuffer.buffer, rigInvMatBuffer->buffer, 1, &copyRegion);
        rigInvMatBuffer->hostVisible = false;

        copyCmd.endAndSubmit();

        // Append buffer
        skeleInvMatBuffers.push_back(std::move(rigInvMatBuffer));
    }
}

void ResourceGroup::createRigSkeleDescSets() {
    VkDevice lDevice = vkDevice->lDevice;

    skeleDescPool = MakeUnique<DescPool>(lDevice);
    skeleDescLayout = MakeUnique<DescLayout>(lDevice);
    skeleDescSets.clear();

    // Create shared descriptor pool and layout
    skeleDescPool->create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);
    skeleDescLayout->create({
        DescLayout::BindInfo{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    if (skeletons.empty()) return;

    uint32_t rigCount = static_cast<uint32_t>(skeletons.size());
    skeleDescSets.resize(rigCount);
    for (size_t i = 0; i < rigCount; ++i) {
        skeleDescSets[i] = MakeUnique<DescSets>(lDevice);
        skeleDescSets[i]->allocate(skeleDescPool->get(), skeleDescLayout->get(), 1);
    }

    // Bind each skeleton buffer to its respective descriptor set
    for (size_t i = 0; i < skeletons.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer               = skeleInvMatBuffers[i]->buffer;
        bufferInfo.offset               = 0;
        bufferInfo.range                = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = skeleDescSets[i]->get();  // Get the i-th descriptor set
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
    }
}