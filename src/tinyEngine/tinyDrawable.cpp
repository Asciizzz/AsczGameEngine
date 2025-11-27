#include "tinyEngine/tinyDrawable.hpp"

using namespace tinyVk;

std::vector<VkVertexInputBindingDescription> tinyDrawable::bindingDesc() noexcept {
    VkVertexInputBindingDescription dataBinding{};
    dataBinding.binding = 0;
    dataBinding.stride = sizeof(tinyVertex::Static);
    dataBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription rigBinding{};
    rigBinding.binding = 1;
    rigBinding.stride = sizeof(tinyVertex::Rigged);
    rigBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputBindingDescription instaBinding{};
    instaBinding.binding = 2;
    instaBinding.stride = sizeof(InstaData);
    instaBinding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    return { dataBinding, rigBinding, instaBinding };
}

std::vector<VkVertexInputAttributeDescription> tinyDrawable::attributeDescs() noexcept {
    std::vector<VkVertexInputAttributeDescription> descs(10);

    // Data buffer (binding = 0)
    descs[0] = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, pos_tu)  };
    descs[1] = { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, nrml_tv) };
    descs[2] = { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Static, tang) };

    // Rigging buffer (binding = 1)
    descs[3] = { 3, 1, VK_FORMAT_R32G32B32A32_UINT,   offsetof(tinyVertex::Rigged, boneIDs) };
    descs[4] = { 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(tinyVertex::Rigged, boneWs)  };

    // Insta buffer (binding = 2)
    descs[5] = { 5, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 0 };
    descs[6] = { 6, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 1 };
    descs[7] = { 7, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 2 };
    descs[8] = { 8, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstaData, model) + sizeof(glm::vec4) * 3 };
    descs[9] = { 9, 2, VK_FORMAT_R32G32B32A32_UINT,   offsetof(InstaData, other) };

    return descs;
}

//---------------------------------------------------------------

void tinyDrawable::init(const CreateInfo& info) {
    maxFramesInFlight_ = info.maxFramesInFlight;
    fsr_ = info.fsr;
    dvk_ = info.dvk;

    VkDevice device = dvk_->device;

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

// ------------------ Setup instance buffer ------------------

    instaSize_x1_.unaligned = MAX_INSTANCES * sizeof(InstaData);
    instaSize_x1_.aligned = instaSize_x1_.unaligned; // Vertex attributes doesnt need minAlignment

    instaBuffer_
        .setDataSize(instaSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Vertex)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    matSize_x1_.unaligned = MAX_MATERIALS * sizeof(tinyMaterial::Data);
    matSize_x1_.aligned = dvk_->alignSizeSSBO(matSize_x1_.unaligned); // SSBO DO need alignment

// ------------------ Setup material data ------------------

    matBuffer_
        .setDataSize(matSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    matDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Fragment, nullptr} });
    matDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    matDescSet_.allocate(device, matDescPool_, matDescLayout_);
    writeDescSetDynamicBuffer(matDescSet_, matBuffer_, matSize_x1_.unaligned);

// ------------------ Setup textures data ------------------
    
    // Later

// ------------------ Setup skin data ------------------

    skinSize_x1_.unaligned = MAX_BONES * sizeof(glm::mat4);
    skinSize_x1_.aligned = dvk_->alignSizeSSBO(skinSize_x1_.unaligned);

    skinBuffer_
        .setDataSize(skinSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    skinDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr} });
    skinDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    skinDescSet_.allocate(device, skinDescPool_, skinDescLayout_);
    writeDescSetDynamicBuffer(skinDescSet_, skinBuffer_, skinSize_x1_.unaligned);

// ------------------ Setup morph data ------------------
    
    // Create pool and layout for deltas
    mrphDltsDescLayout_.create(device, { {0, DescType::StorageBuffer, 1, ShaderStage::Vertex, nullptr} });
    mrphDltsDescPool_.create(device, { {DescType::StorageBuffer, MAX_MORPH_DLTS} }, MAX_MORPH_DLTS);

    // Create dynamic buffer for weights
    mrphWsSize_x1_.unaligned = MAX_MORPH_WS * sizeof(float);
    mrphWsSize_x1_.aligned = dvk_->alignSizeSSBO(mrphWsSize_x1_.unaligned);

    mrphWsBuffer_
        .setDataSize(mrphWsSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    mrphWsDescLayout_.create(device, { {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr} });
    mrphWsDescPool_.create(device, { {DescType::StorageBufferDynamic, 1} }, 1);
    mrphWsDescSet_.allocate(device, mrphWsDescPool_, mrphWsDescLayout_);
    writeDescSetDynamicBuffer(mrphWsDescSet_, mrphWsBuffer_, mrphWsSize_x1_.unaligned);


// --------------------------- Dummy -----------------------------

    // Dummy morph delta buffer

    dummy_.morphDltsBuffer
        .setDataSize(sizeof(VkBuffer))
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    dummy_.morphDltsDescSet.allocate(device, mrphDltsDescPool_, mrphDltsDescLayout_);
    DescWrite()
        .setDstSet(dummy_.morphDltsDescSet)
        .setType(DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            dummy_.morphDltsBuffer, 0, VK_WHOLE_SIZE
        } })
        .updateDescSets(device);
}

static tinyHandle defaultMatHandle = tinyHandle::make<tinyMaterial>(0, 0);

// --------------------------- Batching process --------------------------

/* Batch:

Shader 0
    - Submesh 0
        - Instance 0
        - Instance 1
    - Submesh 1

Shader 1
    - Submesh 0
        - Instance 0
    - Submesh 1
        - Instance 0
        - Instance 1
        - Instance 2

You get the idea

*/

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    instaGroups_.clear();
    shaderGroups_.clear();
    matData_.clear();
    skinRanges_.clear();

    skinCount_ = 0;
    mrphWsCount_ = 0;

    handleMap_.clear();

// Initialize
    matData_.push_back(tinyMaterial::Data());
}

void tinyDrawable::submit(const Entry& entry) noexcept {
    const tinyMesh* rMesh = fsr_->get<tinyMesh>(entry.mesh);
    if (!rMesh) return;

    // Get or create sub group

    tinyHandle hash = entry.hash();
    auto subIt = handleMap_.find(hash);
    if (subIt == handleMap_.end()) {
        size_t instaGroupIdx = instaGroups_.size();
        handleMap_[hash] = instaGroupIdx;

        instaGroups_.emplace_back();

        // This new design uses submesh directly, no need to group

        const tinyMesh::Submesh* submesh = rMesh->submesh(entry.submesh);
        if (!submesh) return;

        tinyHandle shaderHandle;

        if (const tinyMaterial* rMat = fsr_->get<tinyMaterial>(submesh->material)) {
            // If material not in buffer, add it
            if (handleMap_.find(submesh->material) == handleMap_.end()) {
                handleMap_[submesh->material] = matData_.size();
                matData_.push_back(rMat->data);
            }

            shaderHandle = rMat->shader;
        }

        // If shader group not exists, create it
        auto shaderIt = handleMap_.find(shaderHandle);
        if (shaderIt == handleMap_.end()) {
            handleMap_[shaderHandle] = shaderGroups_.size();

            shaderGroups_.emplace_back();
            shaderGroups_.back().shader = shaderHandle;

            shaderIt = handleMap_.find(shaderHandle);
        }

        DrawGroup drawGroup;
        drawGroup.mesh = entry.mesh;
        drawGroup.submesh = entry.submesh;
        drawGroup.instaIndex = instaGroupIdx;

        size_t shaderIdx = shaderIt->second;
        shaderGroups_[shaderIdx].add(drawGroup);

        subIt = handleMap_.find(hash);
    }

    // Add instance data
    InstaData instaData;
    instaData.model = entry.model;

    // If mesh entry uses a skeleton from a skeleton node
    const Entry::SkeleData& skeleData = entry.skeleData;
    if (skeleData.skinData && !skeleData.skinData->empty()) {
        tinyHandle skeleNode = skeleData.skeleNode;

        // If this skeleton node already registered, use existing range
        auto skinRangeIt = handleMap_.find(skeleNode);
        if (skinRangeIt == handleMap_.end()) {
            uint32_t thisCount = static_cast<uint32_t>(skeleData.skinData->size());

            SkinRange newRange;
            newRange.skinOffset = skinCount_;
            newRange.skinCount = thisCount;
            
            // Append range
            handleMap_[skeleNode] = skinRanges_.size();
            skinRanges_.push_back(newRange);

            // Copy skin data
            size_t skinDataSize = thisCount * sizeof(glm::mat4);
            size_t skinDataOffset = skinCount_ * sizeof(glm::mat4) + skinOffset(frameIndex_);
            skinBuffer_.copyData(skeleData.skinData->data(), skinDataSize, skinDataOffset);

            skinRangeIt = handleMap_.find(skeleNode);

            skinCount_ += thisCount;
        }

        SkinRange& skinRange = skinRanges_[skinRangeIt->second];
        instaData.other.x = skinRange.skinOffset; // Only offset needed
    }

    // If mesh has morph targets
    if (entry.mrphWeights && !entry.mrphWeights->empty()) {
        uint32_t thisCount = static_cast<uint32_t>(entry.mrphWeights->size());

        size_t mrphWsDataSize = thisCount * sizeof(float);
        size_t mrphWsDataOffset = mrphWsCount_ * sizeof(float) + mrphWsOffset(frameIndex_);
        mrphWsBuffer_.copyData(entry.mrphWeights->data(), mrphWsDataSize, mrphWsDataOffset);

        instaData.other.y = mrphWsCount_; // Only offset needed
        mrphWsCount_ += thisCount;
    }

    instaGroups_[subIt->second].push_back(instaData);
}

void tinyDrawable::finalize() {
    uint32_t curInstances = 0;

    struct InstaRange {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    std::unordered_map<size_t, InstaRange> instaCache;

    for (auto& shaderGroup : shaderGroups_) {
        for (DrawGroup& drawGroup : shaderGroup.drawGroups) {
            if (instaCache.find(drawGroup.instaIndex) != instaCache.end()) {
                // Already processed
                const InstaRange& range = instaCache[drawGroup.instaIndex];
                drawGroup.instaOffset = range.offset;
                drawGroup.instaCount = range.count;

                continue;
            }

            const std::vector<InstaData>& instaVec = instaGroups_[drawGroup.instaIndex];

            // Update draw group range
            drawGroup.instaOffset = curInstances;
            drawGroup.instaCount = static_cast<uint32_t>(instaVec.size());

            // Copy instance data
            size_t dataOffset = curInstances * sizeof(InstaData) + instaOffset(frameIndex_);
            size_t dataSize = instaVec.size() * sizeof(InstaData);
            instaBuffer_.copyData(instaVec.data(), dataSize, dataOffset);

            curInstances += drawGroup.instaCount;

            // Update cache
            instaCache[drawGroup.instaIndex] = InstaRange{
                drawGroup.instaOffset,
                drawGroup.instaCount
            };
        }
    }

    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);
}