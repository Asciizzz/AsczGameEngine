#include "tinyEngine/tinyDrawable.hpp"

using namespace tinyVk;

std::vector<VkVertexInputBindingDescription> tinyDrawable::bindingDesc() noexcept {
    VkVertexInputBindingDescription vstaticBinding{};
    vstaticBinding.binding = 0;
    vstaticBinding.stride = sizeof(tinyVertex::Static);
    vstaticBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription instaBinding{};
    instaBinding.binding = 1;
    instaBinding.stride = sizeof(InstaData);
    instaBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    return { vstaticBinding, instaBinding };
}

std::vector<VkVertexInputAttributeDescription> tinyDrawable::attributeDescs() noexcept {
    std::vector<VkVertexInputAttributeDescription> descs(8);

    // Data buffer (binding = 0)
    descs[0] = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, pos_tu)  };
    descs[1] = { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, nrml_tv) };
    descs[2] = { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, tang) };

    // Insta buffer (binding = 1)
    descs[3] = { 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 0 };
    descs[4] = { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 1 };
    descs[5] = { 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 2 };
    descs[6] = { 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 3 };
    descs[7] = { 7, 1, VK_FORMAT_R32G32B32A32_UINT,   offsetof(InstaData, other) };
    return descs;
}

// ------------------------ Helpers -------------------------

void writeImg(
    VkDevice device, VkDescriptorSet dstSet,
    uint32_t dstBinding, uint32_t dstArrayElement,
    VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout
) {
    DescWrite()
        .setDstSet(dstSet)
        .setDstBinding(dstBinding)
        .setDstArrayElement(dstArrayElement)
        .setType(DescType::CombinedImageSampler)
        .setDescCount(1)
        .setImageInfo({ VkDescriptorImageInfo{
            sampler,
            imageView,
            imageLayout
        } })
        .updateDescSets(device);
}

// ---------------------------------------------------------------

void tinyDrawable::init(const CreateInfo& info) {
    maxFramesInFlight_ = info.maxFramesInFlight;
    fsr_ = info.fsr;
    dvk_ = info.dvk;

    VkDevice device = dvk_->device;
    VkPhysicalDevice pDevice = dvk_->pDevice;

    auto writeDescSetDynamicBuffer = [&](VkDescriptorSet dstSet, VkBuffer buffer, VkDeviceSize size) {
        DescWrite()
            .setDstSet(dstSet)
            .setType(DescType::StorageBufferDynamic)
            .setDescCount(1)
            .setBufferInfo({ VkDescriptorBufferInfo{
                buffer, 0, size
            } })
            .updateDescSets(device);
    };

    auto createBuffer = [&](DataBuffer& buffer, VkDeviceSize dataSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps) {
        buffer
            .setDataSize(dataSize)
            .setUsageFlags(usage)
            .setMemPropFlags(memProps)
            .createBuffer(dvk_)
            .mapMemory();
    };

// ------------------ Setup instance buffer ------------------

    instaSize_x1_.unaligned = MAX_INSTANCES * sizeof(InstaData);
    instaSize_x1_.aligned = instaSize_x1_.unaligned; // Vertex attributes doesnt need minAlignment
    createBuffer(instaBuffer_, instaSize_x1_.aligned * maxFramesInFlight_, BufferUsage::Vertex, MemProp::HostVisibleAndCoherent);

// ------------------ Setup material data ------------------

    matSize_x1_.unaligned = MAX_MATERIALS * sizeof(tinyMaterial::Data);
    matSize_x1_.aligned = dvk_->alignSizeSSBO(matSize_x1_.unaligned); // SSBO DO need alignment
    createBuffer(matBuffer_, matSize_x1_.aligned * maxFramesInFlight_, BufferUsage::Storage, MemProp::HostVisibleAndCoherent);

    matDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Fragment, nullptr} });
    matDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    matDescSet_.allocate(device, matDescPool_, matDescLayout_);
    writeDescSetDynamicBuffer(matDescSet_, matBuffer_, matSize_x1_.unaligned);

// ------------------ Setup textures data ------------------

    // Create 3 Sampler for the 3 Wrap Modes: Repeat, ClampEdge, ClampBorder

    auto createSampler = [&](SamplerVk& sampler, VkSamplerAddressMode addressMode) {
            SamplerConfig sampConfig;
            sampConfig.device = device;
            sampConfig.pDevice = pDevice;
            sampConfig.addressModes(addressMode);
            sampler.create(sampConfig);
    };

    texSamplers_.resize(3);
    createSampler(texSamplers_[0], VK_SAMPLER_ADDRESS_MODE_REPEAT);
    createSampler(texSamplers_[1], VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    createSampler(texSamplers_[2], VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    // Create dynamic descriptor set layout and pool for textures
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = MAX_TEXTURES;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.bindingCount = 1;
    VkDescriptorBindingFlags arrayFlags[] = {
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };
    bindingFlags.pBindingFlags = arrayFlags;

    texDescLayout_.create(device, { binding }, 0, &bindingFlags);

    texDescPool_.create(device, { {DescType::CombinedImageSampler, MAX_TEXTURES} }, 1, true);

    uint32_t varDescriptorCount = MAX_TEXTURES;
    texDescSet_.allocate(device, texDescPool_, texDescLayout_, &varDescriptorCount);

    // Create empty texture
    dummy_.texture.create({255, 255, 255, 255}, 1, 1, 4, tinyTexture::WrapMode::Repeat);
    dummy_.texture.vkCreate(dvk_);

    // Add default empty texture at index 0
    writeImg(dvk_->device, texDescSet_, 0, 0, texSamplers_[0].sampler(), dummy_.texture.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texIdxMap_[Asc::Handle()] = 0; // Map empty handle to index 0

// ------------------ Setup skin data ------------------

    skinSize_x1_.unaligned = MAX_BONES * sizeof(glm::mat4);
    skinSize_x1_.aligned = dvk_->alignSizeSSBO(skinSize_x1_.unaligned);
    createBuffer(skinBuffer_, skinSize_x1_.aligned * maxFramesInFlight_, BufferUsage::Storage, MemProp::HostVisibleAndCoherent);

    skinDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr} });
    skinDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    skinDescSet_.allocate(device, skinDescPool_, skinDescLayout_);
    writeDescSetDynamicBuffer(skinDescSet_, skinBuffer_, skinSize_x1_.unaligned);

// ------------------ Setup morph data ------------------

    mrphWsSize_x1_.unaligned = MAX_MORPH_WS * sizeof(float);
    mrphWsSize_x1_.aligned = dvk_->alignSizeSSBO(mrphWsSize_x1_.unaligned);
    createBuffer(mrphWsBuffer_, mrphWsSize_x1_.aligned * maxFramesInFlight_, BufferUsage::Storage, MemProp::HostVisibleAndCoherent);

    mrphWsDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr} });
    mrphWsDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    mrphWsDescSet_.allocate(device, mrphWsDescPool_, mrphWsDescLayout_);
    writeDescSetDynamicBuffer(mrphWsDescSet_, mrphWsBuffer_, mrphWsSize_x1_.unaligned);

// -------------------------- Vertex Extension -------------------------

    tinyMesh::createVrtxExtDescriptors(dvk_, &vrtxExtLayout_, &vrtxExtPool_);

    // Create a dummy mesh
    tinyMesh::Submesh dummySubmesh;
    // Fill with minimal data
    dummySubmesh.setVrtxStatic({ tinyVertex::Static{} });
    dummySubmesh.setVrtxRigged({ tinyVertex::Rigged{} });
    dummySubmesh.setVrtxColor({ tinyVertex::Color{} });
    dummySubmesh.setMrphDeltas({ tinyVertex::Morph{} }, 1);
    dummy_.mesh.setMrphTargetNames({ "DummyMorph" });
    dummy_.mesh.append(std::move(dummySubmesh));
    dummy_.mesh.vkCreate(dvk_, vrtxExtLayout_, vrtxExtPool_);
}

uint32_t tinyDrawable::addTexture(Asc::Handle texHandle) noexcept {
    auto it = texIdxMap_.find(texHandle);
    if (it != texIdxMap_.end()) return it->second;

    const tinyTexture* texture = fsr_->get<tinyTexture>(texHandle);
    if (!texture) return 0; // Return default empty texture

    bool useFreeIndex = !texFreeIndices_.empty();
    uint32_t texIndex = useFreeIndex ? texFreeIndices_.back() : static_cast<uint32_t>(texIdxMap_.size());
    if (useFreeIndex) { texFreeIndices_.pop_back(); }

    texIdxMap_[texHandle] = texIndex;

    VkSampler sampler;
    using WrapMode = tinyTexture::WrapMode;
    switch (texture->wrapMode()) {
        case WrapMode::Repeat:        sampler = texSamplers_[0].sampler(); break;
        case WrapMode::ClampToEdge:   sampler = texSamplers_[1].sampler(); break;
        case WrapMode::ClampToBorder: sampler = texSamplers_[2].sampler(); break;
        default:                      sampler = texSamplers_[0].sampler(); break;
    }
    
    // Write to specific array index
    writeImg(dvk_->device, texDescSet_, 0, texIndex, sampler, texture->view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return texIndex;
}

bool tinyDrawable::removeTexture(Asc::Handle texHandle) noexcept {
    if (texHandle == Asc::Handle()) return false; // Cannot remove default empty texture

    auto it = texIdxMap_.find(texHandle);
    if (it == texIdxMap_.end()) return false;

    uint32_t texIndex = it->second;
    texIdxMap_.erase(it);
    texFreeIndices_.push_back(texIndex);

    // Write empty descriptor to the freed index
    DescWrite()
        .setDstSet(texDescSet_)
        .setDstBinding(0)
        .setDstArrayElement(texIndex)
        .setType(DescType::CombinedImageSampler)
        .setDescCount(1)
        .setImageInfo({ VkDescriptorImageInfo{
            texSamplers_[0].sampler(),
            dummy_.texture.view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        } })
        .updateDescSets(dvk_->device);

    return true;
}

// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    shaderGroups_.clear();
    meshGroups_.clear();
    submeshGroups_.clear();

    matData_.clear();
    skinRanges_.clear();

    skinCount_ = 0;
    mrphWsCount_ = 0;

    batchMap_.clear();
    dataMap_.clear();

// Initialize
    matData_.push_back(tinyMaterial::Data());
}

void tinyDrawable::submit(const Entry& entry) noexcept {
    Asc::Handle meshHandle = entry.mesh;
    size_t submeshIndex = entry.submesh;

    // Get or create SubmeshGroup
    Asc::Handle hash = entry.hash();
    auto subIt = batchMap_.find(hash); // Entry hash -> SubmeshGroup index

    if (subIt == batchMap_.end()) {
        const tinyMesh* rMesh = fsr_->get<tinyMesh>(meshHandle);
        if (!rMesh) return;

        const tinyMesh::Submesh* submesh = rMesh->submesh(submeshIndex);
        if (!submesh) return;

        Asc::Handle materialHandle = submesh->material;

        // Check for material existence as well as getting/creating ShaderGroup
        auto shaderIt = batchMap_.find(materialHandle); // Material handle -> ShaderGroup index
        if (shaderIt == batchMap_.end()) {
            Asc::Handle shaderHandle;

            // Retrieve submesh's material's shader
            if (const tinyMaterial* rMat = fsr_->get<tinyMaterial>(materialHandle)) {

                // If material not in buffer, add it
                if (dataMap_.find(materialHandle) == dataMap_.end()) { //
                    dataMap_[materialHandle] = matData_.size();
                    
                    tinyMaterial::Data matData;

                    matData.float1 = rMat->baseColor;

                    matData.uint1.x = getTextureIndex(rMat->albTexture);
                    matData.uint1.y = getTextureIndex(rMat->nrmlTexture);
                    matData.uint1.z = getTextureIndex(rMat->emissTexture);

                    matData_.push_back(matData);
                }

                shaderHandle = rMat->shader;
            }

            // Get or create ShaderGroup
            shaderIt = batchMap_.find(shaderHandle);
            if (shaderIt == batchMap_.end()) {
                size_t shaderGroupIdx = shaderGroups_.size();
                batchMap_[shaderHandle] = shaderGroupIdx;

                shaderGroups_.emplace_back();
                shaderGroups_.back().shader = shaderHandle;

                shaderIt = batchMap_.find(shaderHandle);
            }

            // Update Material handle -> ShaderGroup index map
            batchMap_[materialHandle] = shaderIt->second;
        }

        ShaderGroup& shaderGroup = shaderGroups_[shaderIt->second];

        // Get or create MeshGroup
        auto meshIt = shaderGroup.meshGroupMap.find(meshHandle);
        if (meshIt == shaderGroup.meshGroupMap.end()) {
            size_t meshGroupIdx = meshGroups_.size();
            shaderGroup.meshGroupMap[meshHandle] = meshGroupIdx;
            shaderGroup.meshGroupIndices.push_back(meshGroupIdx);

            meshGroups_.emplace_back();
            meshGroups_.back().mesh = meshHandle;

            meshIt = shaderGroup.meshGroupMap.find(meshHandle);
        }

        MeshGroup& meshGroup = meshGroups_[meshIt->second];

        // Get or create SubmeshGroup (likely creating)
        auto submeshIt = meshGroup.submeshGroupMap.find(submeshIndex);
        if (submeshIt == meshGroup.submeshGroupMap.end()) {
            size_t submeshGroupIdx = submeshGroups_.size();
            meshGroup.submeshGroupMap[submeshIndex] = submeshGroupIdx;
            meshGroup.submeshGroupIndices.push_back(submeshGroupIdx);

            submeshGroups_.emplace_back();

            submeshIt = meshGroup.submeshGroupMap.find(submeshIndex);
        }

        SubmeshGroup& submeshGroup = submeshGroups_[submeshIt->second];
        submeshGroup.submesh = submeshIndex;

        // Add hash entry to batch map
        batchMap_[hash] = submeshIt->second;
        subIt = batchMap_.find(hash);
    }

    SubmeshGroup& submeshGroup = submeshGroups_[subIt->second];

    // Add instance data
    InstaData instaData;
    instaData.model = entry.model;

    // If mesh entry uses a skeleton from a skeleton node
    const Entry::SkeleData& skeleData = entry.skeleData;
    if (skeleData.skinData && !skeleData.skinData->empty()) {
        Asc::Handle skeleNode = skeleData.skeleNode;

        // If this skeleton node already registered, use existing range
        auto skinRangeIt = dataMap_.find(skeleNode);
        if (skinRangeIt == dataMap_.end()) {
            uint32_t thisCount = skeleData.skinData->size();

            SkinRange newRange;
            newRange.skinOffset = skinCount_;
            newRange.skinCount = thisCount;

            // Append range
            dataMap_[skeleNode] = skinRanges_.size();
            skinRanges_.push_back(newRange);

            // Copy skin data
            size_t skinDataSize   = thisCount  * sizeof(glm::mat4);
            size_t skinDataOffset = skinCount_ * sizeof(glm::mat4) + skinOffset(frameIndex_);
            skinBuffer_.copyData(skeleData.skinData->data(), skinDataSize, skinDataOffset);

            skinRangeIt = dataMap_.find(skeleNode);

            skinCount_ += thisCount;
        }

        SkinRange& skinRange = skinRanges_[skinRangeIt->second];
        instaData.other.x = skinRange.skinOffset;
        instaData.other.y = skinRange.skinCount;
    }

    // If mesh has morph targets (node-level, each node has its own morph weights)
    const Entry::MorphData& morphData = entry.morphData;
    if (morphData.weights && !morphData.weights->empty()) {
        Asc::Handle nodeHandle = entry.morphData.node;

        // Check if we've already copied this node's morph weights
        auto mrphIt = dataMap_.find(nodeHandle);
        if (mrphIt == dataMap_.end()) {
            uint32_t thisCount = static_cast<uint32_t>(morphData.weights->size());

            // Copy this node's morph weight array
            size_t mrphWsDataSize = thisCount * sizeof(float);
            size_t mrphWsDataOffset = mrphWsCount_ * sizeof(float) + mrphWsOffset(frameIndex_);
            mrphWsBuffer_.copyData(morphData.weights->data(), mrphWsDataSize, mrphWsDataOffset);
    
            // Store the offset for this node
            dataMap_[nodeHandle] = mrphWsCount_;
            mrphIt = dataMap_.find(nodeHandle);

            mrphWsCount_ += thisCount;
        }
        
        // All submeshes of this node reference the same morph weights
        uint32_t mrphOffset = mrphIt->second;
        uint32_t mrphCount = static_cast<uint32_t>(morphData.weights->size());
        
        instaData.other.z = mrphOffset;
        instaData.other.w = mrphCount;
    }

    submeshGroup.push(instaData);
}

void tinyDrawable::finalize() noexcept {
    uint32_t curInstances = 0;

    for (auto& shaderGroup : shaderGroups_) {
        for (auto& meshGroupIdx : shaderGroup.meshGroupIndices) {
            MeshGroup& mGroup = meshGroups_[meshGroupIdx];

            for (auto& submeshGroupIdx : mGroup.submeshGroupIndices) {
                SubmeshGroup& smGroup = submeshGroups_[submeshGroupIdx];

                smGroup.instaOffset = curInstances;
                smGroup.instaCount = smGroup.size();

                // Copy instance data to buffer
                size_t dataOffset = curInstances * sizeof(InstaData) + instaOffset(frameIndex_);
                instaBuffer_.copyData(smGroup.instaData.data(), smGroup.sizeBytes(), dataOffset);

                curInstances += smGroup.size();
            }
        }
    }

    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);
}