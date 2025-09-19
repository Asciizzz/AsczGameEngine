#include "Az3D/ResourceGroup.hpp"
#include "Tiny3D/TinyLoader.hpp"
#include "AzVulk/Device.hpp"
#include <iostream>

using namespace AzVulk;
using namespace Az3D;

// ============================================================================
// ======================= General resource group stuff =======================
// ============================================================================

ResourceGroup::ResourceGroup(Device* deviceVK): deviceVK(deviceVK) {}

void ResourceGroup::cleanup() {
    VkDevice lDevice = deviceVK->lDevice;

    // ImageVK handles its own cleanup automatically
    textures.clear();

    // Cleanup textures' samplers
    for (auto& sampler : samplers) {
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(lDevice, sampler, nullptr);
    }
    samplers.clear();
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = deviceVK->lDevice;

    createComponentVKsFromModels();

    createMaterialBuffer();
    createMaterialDescSet();

    createTextureSamplers();
    createTexSampIdxBuffer();
    createTextureDescSet();

    createRigSkeleBuffers();
    createRigSkeleDescSets();

    createLightBuffer();
    createLightDescSet();
}

// ===========================================================================
// Special function to convert TinyModel to Vulkan resources with lookup table
// ===========================================================================


void ResourceGroup::createComponentVKsFromModels() {
    // Create default texture and material
    MaterialVK defaultMat{};
    materialVKs.push_back(defaultMat);

    TinyTexture defaultTex = TinyTexture::createDefaultTexture();
    auto defaultTexture = createTexture(std::move(defaultTex));
    textures.push_back(std::move(defaultTexture));
    texSamplerIndices.push_back(0);

    for (auto& model : models) {
        ModelPtr modelVK;

        std::vector<size_t> tempGlobalTextures; // A temporary list to convert local to global indices
        for (const auto& texture : model.textures) {
            auto tex = createTexture(texture);
            tempGlobalTextures.push_back(textures.size());
            
            textures.push_back(std::move(tex));
            
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

            size_t submeshVK_index = addSubmeshVK(submesh);
            modelVK.submeshVK_indices.push_back(submeshVK_index);
            
            uint32_t indexCount = static_cast<uint32_t>(submesh.indexCount);
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

    matBuffer = MakeUnique<DataBuffer>();
    matBuffer
        ->setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::DeviceLocal)
        .createDeviceLocalBuffer(deviceVK, materialVKs.data());
}

// Descriptor set creation
void ResourceGroup::createMaterialDescSet() {
    VkDevice lDevice = deviceVK->lDevice;

    matDescSet = MakeUnique<DescSet>(lDevice);

    matDescSet->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    });

    matDescSet->createOwnPool({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

    matDescSet->allocate();

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
// ========================= TEXTURES =========================================
// ============================================================================

ImageVK ResourceGroup::createTexture(const TinyTexture& texture) {
    // Get appropriate Vulkan format and convert data if needed
    VkFormat textureFormat = ImageVK::getVulkanFormatFromChannels(texture.channels);
    std::vector<uint8_t> vulkanData = ImageVK::convertToValidData(
        texture.channels, texture.width, texture.height, texture.data.data());
    
    // Calculate image size based on Vulkan format requirements
    int vulkanChannels = (texture.channels == 3) ? 4 : texture.channels; // RGB becomes RGBA
    VkDeviceSize imageSize = texture.width * texture.height * vulkanChannels;

    if (texture.data.empty()) {
        throw std::runtime_error("Failed to load texture from TinyTexture");
    }

    // Create staging buffer for texture data upload
    DataBuffer stagingBuffer;
    stagingBuffer
        .setDataSize(imageSize * sizeof(uint8_t))
        .setUsageFlags(ImageUsage::TransferSrc)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .uploadData(vulkanData.data());

    // Create ImageVK with texture configuration
    ImageVK textureVK(deviceVK);

    ImageConfig config = ImageConfig()
        .setDimensions(texture.width, texture.height)
        .setFormat(textureFormat)
        .setUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .setAutoMipLevels(texture.width, texture.height)
        .setTiling(VK_IMAGE_TILING_OPTIMAL)
        .setMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .setAspectMask(VK_IMAGE_ASPECT_COLOR_BIT)
        .setAutoMipLevels(texture.width, texture.height);

    bool success = textureVK.createImage(config);
    success &= textureVK.createImageView(viewConfig);

    if (!success) throw std::runtime_error("Failed to create ImageVK for texture");

    textureVK
        .transitionLayoutImmediate( VK_IMAGE_LAYOUT_UNDEFINED, 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        .copyFromBufferImmediate(stagingBuffer.buffer, texture.width, texture.height)
        .generateMipmapsImmediate();

    return textureVK;
}


// --- Shared Samplers ---
void ResourceGroup::createTextureSamplers() {
    VkDevice lDevice = deviceVK->lDevice;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(deviceVK->pDevice, &properties);
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
    // --- Device-local buffer (GPU only, STORAGE + DST) ---
    if (!textSampIdxBuffer) textSampIdxBuffer = MakeUnique<DataBuffer>();
    
    VkDeviceSize bufferSize = sizeof(uint32_t) * texSamplerIndices.size();

    textSampIdxBuffer
        ->setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .createDeviceLocalBuffer(deviceVK, texSamplerIndices.data());

}

void ResourceGroup::createTextureDescSet() {
    VkDevice lDevice = deviceVK->lDevice;
    uint32_t textureCount = static_cast<uint32_t>(textures.size());
    uint32_t samplerCount = static_cast<uint32_t>(samplers.size());

    texDescSet = MakeUnique<DescSet>(lDevice);

    // layout: binding 0 = images, binding 1 = sampler indices buffer, binding 2 = samplers
    texDescSet->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  textureCount, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_SAMPLER,        samplerCount, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    });

    texDescSet->createOwnPool({
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureCount},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER,       samplerCount}
    }, 1);

    texDescSet->allocate();

    // Write sampled images
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; ++i) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView   = textures[i].getView();
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

size_t ResourceGroup::addSubmeshVK(const TinySubmesh& submesh) {
    const auto& vertexData = submesh.vertexData;
    const auto& indexData = submesh.indexData;

    DataBuffer vDataBuffer;
    vDataBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    DataBuffer iDataBuffer;
    iDataBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    // Append buffers
    UniquePtr<SubmeshVK> submeshVK = MakeUnique<SubmeshVK>();
    submeshVK->vertexBuffer = std::move(vDataBuffer);
    submeshVK->indexBuffer  = std::move(iDataBuffer);
    switch (submesh.indexType) {
        case TinySubmesh::IndexType::Uint8:  submeshVK->indexType = VK_INDEX_TYPE_UINT8;  break;
        case TinySubmesh::IndexType::Uint16: submeshVK->indexType = VK_INDEX_TYPE_UINT16; break;
        case TinySubmesh::IndexType::Uint32: submeshVK->indexType = VK_INDEX_TYPE_UINT32; break;
        default: throw std::runtime_error("Unsupported index type in submesh");
    }

    submeshVKs.push_back(std::move(submeshVK));
    return submeshVKs.size() - 1; // Return index of newly added buffers
}

VkBuffer ResourceGroup::getSubmeshVertexBuffer(size_t submeshVK_index) const {
    return submeshVKs[submeshVK_index]->vertexBuffer.get();
}
VkBuffer ResourceGroup::getSubmeshIndexBuffer(size_t submeshVK_index) const {
    return submeshVKs[submeshVK_index]->indexBuffer.get();
}
VkIndexType ResourceGroup::getSubmeshIndexType(size_t submeshVK_index) const {
    return submeshVKs[submeshVK_index]->indexType;
}

// ============================================================================
// ========================== SKELETON DATA ==================================
// ============================================================================

void ResourceGroup::createRigSkeleBuffers() {
    skeleInvMatBuffers.clear();

    for (size_t i = 0; i < skeletons.size(); ++i) {
        const auto* skeleton = skeletons[i].get();

        const auto& inverseBindMatrices = skeleton->inverseBindMatrices;

        UniquePtr<DataBuffer> rigInvMatBuffer = MakeUnique<DataBuffer>();

        rigInvMatBuffer
            ->setDataSize(inverseBindMatrices.size() * sizeof(glm::mat4))
            .setUsageFlags(BufferUsage::Storage)
            .setMemPropFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            .createDeviceLocalBuffer(deviceVK, inverseBindMatrices.data());

        // Append buffer
        skeleInvMatBuffers.push_back(std::move(rigInvMatBuffer));
    }
}

void ResourceGroup::createRigSkeleDescSets() {
    VkDevice lDevice = deviceVK->lDevice;

    // For the time being only create the descriptor set layout
    skeleDescSets = MakeUnique<DescSet>(lDevice);
    skeleDescSets->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    });
}

// ============================================================================
// ============================= LIGHTING ===================================
// ============================================================================

void ResourceGroup::createLightBuffer() {
    // Initialize with at least one default light if none exist
    if (lightVKs.empty()) {
        LightVK defaultLight{};
        defaultLight.position = glm::vec4(0.0f, 10.0f, 0.0f, 1.0f); // Point light at origin
        defaultLight.color = glm::vec4(1.0f, 1.0f, 1.0f, 2.0f); // White light with intensity 2.0
        defaultLight.direction = glm::vec4(0.0f, -1.0f, 0.0f, 10.0f); // Range of 10 units
        lightVKs.push_back(defaultLight);
    }

    VkDeviceSize bufferSize = sizeof(LightVK) * lightVKs.size();

    lightBuffer = MakeUnique<DataBuffer>();
    lightBuffer
        ->setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        .createBuffer(deviceVK)
        .uploadData(lightVKs.data());

    lightsDirty = false;
}

void ResourceGroup::createLightDescSet() {
    VkDevice lDevice = deviceVK->lDevice;

    lightDescSet = MakeUnique<DescSet>(lDevice);

    lightDescSet->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
    });
    
    lightDescSet->createOwnPool({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

    lightDescSet->allocate();

    // Bind light buffer to descriptor
    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = lightBuffer->buffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = lightDescSet->get();
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &lightBufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
}

void ResourceGroup::updateLightBuffer() {
    if (!lightsDirty || !lightBuffer) return;

    // Check if we need to resize the buffer
    VkDeviceSize requiredSize = sizeof(LightVK) * lightVKs.size();
    if (requiredSize > lightBuffer->dataSize) {
        // Need to recreate buffer with larger size
        createLightBuffer();
        createLightDescSet();
    } else {
        // Just update the existing buffer
        lightBuffer->uploadData(lightVKs.data());
    }

    lightsDirty = false;
}