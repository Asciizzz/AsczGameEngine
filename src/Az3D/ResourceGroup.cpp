#include "Az3D/ResourceGroup.hpp"
#include "Tiny3D/TinyLoader.hpp"

#include "AzVulk/CmdBuffer.hpp"

#include <iostream>

using namespace AzVulk;
using namespace Az3D;

// ============================================================================
// ======================= General resource group stuff =======================
// ============================================================================

ResourceGroup::ResourceGroup(DeviceVK* deviceVK): deviceVK(deviceVK) {}

void ResourceGroup::cleanup() {
    VkDevice lDevice = deviceVK->lDevice;

    // TextureVK handles its own cleanup automatically
    textures.clear();

    // Ensure correct cleanup order for material
    modelVKs.clear();
}

void ResourceGroup::uploadAllToGPU() {
    VkDevice lDevice = deviceVK->lDevice;

    createMaterialDescPoolAndLayout();

    createComponentVKsFromModels();

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
    // Create default texture first
    TinyTexture defaultTex = TinyTexture::createDefaultTexture();
    auto defaultTexture = createTexture(std::move(defaultTex));
    textures.push_back(std::move(defaultTexture));

    for (auto& model : models) {
        UniquePtr<ModelVK> modelVK = MakeUnique<ModelVK>();

        std::vector<size_t> tempGlobalTextures; // A temporary list to convert local to global indices
        for (const auto& texture : model.textures) {
            auto tex = createTexture(texture);
            tempGlobalTextures.push_back(textures.size());

            textures.push_back(std::move(tex));
        }

        std::vector<MaterialVK> modelMaterialVKs;

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

            modelMaterialVKs.push_back(matVK);
        }

        createMaterialDescSet(modelMaterialVKs, *modelVK);

        const TinyMesh& mesh = model.mesh;
        modelVK->mesh.fromMesh(deviceVK, mesh);

        // for (size_t i = 0; i < mesh.submeshes.size(); ++i) {
        //     const auto& submesh = mesh.submeshes[i];

        //     size_t submeshVK_index = addSubmeshVK(submesh);
        //     modelVK.submeshVK_indices.push_back(submeshVK_index);
            
        //     uint32_t indexCount = static_cast<uint32_t>(submesh.indexCount);
        //     modelVK.submesh_indexCounts.push_back(indexCount);

        //     int localMatIndex = model.submeshes[i].matIndex;

        //     bool validMat = localMatIndex >= 0 && localMatIndex < static_cast<int>(model.materials.size());
        //     size_t globalMatIndex = validMat ? tempGlobalMaterials[localMatIndex] : 0;
        //     modelVK.materialVK_indices.push_back(globalMatIndex);
        // }

        modelVKs.push_back(std::move(modelVK));
    }
}


// ============================================================================
// ========================= MATERIAL =========================================
// ============================================================================

void ResourceGroup::createMaterialDescPoolAndLayout() {
    VkDevice lDevice = deviceVK->lDevice;

    matDescPool = MakeUnique<DescPool>(lDevice);
    matDescPool->create({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } }, 1024);

    matDescLayout = MakeUnique<DescLayout>(lDevice);
    matDescLayout->create({ {0, DescType::StorageBuffer, 1, ShaderStage::VertexAndFragment, nullptr} } );
}

void ResourceGroup::createMaterialDescSet(const std::vector<MaterialVK>& materials, ModelVK& modelVK) {
    VkDevice lDevice = deviceVK->lDevice;

    // Create material buffer
    VkDeviceSize bufferSize = sizeof(MaterialVK) * materials.size();

    // Modifiable buffer
    modelVK.matBuffer
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .mapAndCopy(materials.data());

    modelVK.matDescSet.init(lDevice);
    modelVK.matDescSet.allocate(*matDescPool, *matDescLayout, 1);

    // --- bind buffer to descriptor ---
    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = modelVK.matBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = modelVK.matDescSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &materialBufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
}


// void ResourceGroup::createMaterialBuffer() {
//     VkDeviceSize bufferSize = sizeof(MaterialVK) * materialVKs.size();

//     matBuffer = MakeUnique<DataBuffer>();
//     matBuffer
//         ->setDataSize(bufferSize)
//         .setUsageFlags(BufferUsage::Storage)
//         .setMemPropFlags(MemProp::DeviceLocal)
//         .createDeviceLocalBuffer(deviceVK, materialVKs.data());
// }

// // Descriptor set creation
// void ResourceGroup::createMaterialDescSet() {
//     VkDevice lDevice = deviceVK->lDevice;

//     matDescSet = MakeUnique<DescSet>(lDevice);

//     matDescSet->createOwnLayout({
//         {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr}
//     });

//     matDescSet->createOwnPool({ {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1} }, 1);

//     matDescSet->allocate();

//     // --- bind buffer to descriptor ---
//     VkDescriptorBufferInfo materialBufferInfo{};
//     materialBufferInfo.buffer = *matBuffer;
//     materialBufferInfo.offset = 0;
//     materialBufferInfo.range = VK_WHOLE_SIZE;

//     VkWriteDescriptorSet descriptorWrite{};
//     descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptorWrite.dstSet = *matDescSet;
//     descriptorWrite.dstBinding = 0;
//     descriptorWrite.dstArrayElement = 0;
//     descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//     descriptorWrite.descriptorCount = 1;
//     descriptorWrite.pBufferInfo = &materialBufferInfo;

//     vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
// }

// ============================================================================
// ========================= TEXTURES =========================================
// ============================================================================

UniquePtr<AzVulk::TextureVK> ResourceGroup::createTexture(const TinyTexture& texture) {
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

    // Create texture with texture configuration
    TextureVK textureVK;

    ImageConfig imageConfig = ImageConfig()
        .withPhysicalDevice(deviceVK->pDevice)
        .withDimensions(texture.width, texture.height)
        .withAutoMipLevels()
        .withFormat(textureFormat)
        .withUsage(ImageUsage::Sampled | ImageUsage::TransferDst | ImageUsage::TransferSrc)
        .withTiling(VK_IMAGE_TILING_OPTIMAL)
        .withMemProps(MemProp::DeviceLocal);

    ImageViewConfig viewConfig = ImageViewConfig()
        .withAspectMask(VK_IMAGE_ASPECT_COLOR_BIT)
        .withAutoMipLevels(texture.width, texture.height);

    // A quick function to convert TinyTexture::AddressMode to VkSamplerAddressMode
    auto convertAddressMode = [](TinyTexture::AddressMode mode) {
        switch (mode) {
            case TinyTexture::AddressMode::Repeat:        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TinyTexture::AddressMode::ClampToEdge:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case TinyTexture::AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                                      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    };

    SamplerConfig sampConfig = SamplerConfig()
    // The only thing we care about right now is address mode
        .withAddressModes(convertAddressMode(texture.addressMode));

    bool success = textureVK
        .init(deviceVK)
        .createImage(imageConfig)
        .createView(viewConfig)
        .createSampler(sampConfig)
        .isValid();

    if (!success) throw std::runtime_error("Failed to create TextureVK from TinyTexture");

    TempCmd tempCmd(deviceVK, deviceVK->graphicsPoolWrapper);

    textureVK
        .transitionLayoutImmediate(tempCmd.get(), VK_IMAGE_LAYOUT_UNDEFINED, 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        .copyFromBufferImmediate(tempCmd.get(), stagingBuffer.get())
        .generateMipmapsImmediate(tempCmd.get(), deviceVK->pDevice);

    tempCmd.endAndSubmit(); // Kinda redundant but whatever

    return MakeUnique<TextureVK>(std::move(textureVK));
}

void ResourceGroup::createTextureDescSet() {
    VkDevice lDevice = deviceVK->lDevice;
    uint32_t textureCount = static_cast<uint32_t>(textures.size());

    texDescSet = MakeUnique<DescSet>(lDevice);

    // Combined image sampler descriptor for each texture
    texDescSet->createOwnLayout({
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount, ShaderStage::Fragment, nullptr}
    });

    texDescSet->createOwnPool({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount}
    }, 1);

    texDescSet->allocate();

    // Write combined image samplers - each texture now includes its own sampler
    std::vector<VkDescriptorImageInfo> imageInfos(textureCount);
    for (uint32_t i = 0; i < textureCount; ++i) {
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[i].imageView   = textures[i]->getView();
        imageInfos[i].sampler     = textures[i]->getSampler(); // Now using the texture's own sampler
    }

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet          = *texDescSet;
    imageWrite.dstBinding      = 0;
    imageWrite.dstArrayElement = 0;
    imageWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.descriptorCount = textureCount;
    imageWrite.pImageInfo      = imageInfos.data();

    vkUpdateDescriptorSets(lDevice, 1, &imageWrite, 0, nullptr);
}


// ============================================================================
// =========================== MESH STATIC ====================================
// ============================================================================

void MeshVK::fromMesh(const DeviceVK* deviceVK, const TinyMesh& mesh) {
    const auto& vertexData = mesh.vertexData;
    const auto& indexData = mesh.indexData;

    vertexBuffer
        .setDataSize(vertexData.size())
        .setUsageFlags(BufferUsage::Vertex)
        .createDeviceLocalBuffer(deviceVK, vertexData.data());

    indexBuffer
        .setDataSize(indexData.size())
        .setUsageFlags(BufferUsage::Index)
        .createDeviceLocalBuffer(deviceVK, indexData.data());

    indexType = tinyToVkIndexType(mesh.indexType);

    submeshes = mesh.submeshes; // Direct copy
}

VkIndexType MeshVK::tinyToVkIndexType(TinyMesh::IndexType type) {
    switch (type) {
        case TinyMesh::IndexType::Uint8:  return VK_INDEX_TYPE_UINT8;
        case TinyMesh::IndexType::Uint16: return VK_INDEX_TYPE_UINT16;
        case TinyMesh::IndexType::Uint32: return VK_INDEX_TYPE_UINT32;
        default: throw std::runtime_error("Unsupported index type in TinyMesh");
    }
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
            .setMemPropFlags(MemProp::DeviceLocal)
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
        {0, DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr}
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
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVK)
        .uploadData(lightVKs.data());

    lightsDirty = false;
}

void ResourceGroup::createLightDescSet() {
    VkDevice lDevice = deviceVK->lDevice;

    lightDescSet = MakeUnique<DescSet>(lDevice);

    lightDescSet->createOwnLayout({
        {0, DescType::StorageBuffer, 1, ShaderStage::VertexAndFragment, nullptr}
    });
    
    lightDescSet->createOwnPool({ {DescType::StorageBuffer, 1} }, 1);

    lightDescSet->allocate();

    // Bind light buffer to descriptor
    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = *lightBuffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = *lightDescSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = DescType::StorageBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &lightBufferInfo;

    vkUpdateDescriptorSets(lDevice, 1, &descriptorWrite, 0, nullptr);
}

void ResourceGroup::updateLightBuffer() {
    if (!lightsDirty || !lightBuffer) return;

    // Check if we need to resize the buffer
    VkDeviceSize requiredSize = sizeof(LightVK) * lightVKs.size();
    if (requiredSize > lightBuffer->getDataSize()) {
        // Need to recreate buffer with larger size
        createLightBuffer();
        createLightDescSet();
    } else {
        // Just update the existing buffer
        lightBuffer->uploadData(lightVKs.data());
    }

    lightsDirty = false;
}