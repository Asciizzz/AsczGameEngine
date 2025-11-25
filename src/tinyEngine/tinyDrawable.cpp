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

// Setup instance buffer

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

// Setup material data

    matBuffer_
        .setDataSize(matSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    matDescLayout_.create(dvk_->device, {
        {0, DescType::StorageBufferDynamic, 1, ShaderStage::Fragment, nullptr}
    });

    matDescPool_.create(dvk_->device, {
        {DescType::StorageBufferDynamic, 1}
    }, 1);

    matDescSet_.allocate(dvk_->device, matDescPool_, matDescLayout_);

    DescWrite()
        .setDstSet(matDescSet_)
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            matBuffer_,
            0,
            matSize_x1_.unaligned // true size
        } })
        .updateDescSets(dvk_->device);

// Setup textures data
        // Later

// Setup skin data

    skinSize_x1_.unaligned = MAX_BONES * sizeof(glm::mat4);
    skinSize_x1_.aligned = dvk_->alignSizeSSBO(skinSize_x1_.unaligned);

    skinBuffer_
        .setDataSize(skinSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    skinDescLayout_.create(dvk_->device, {
        {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool_.create(dvk_->device, {
        {DescType::StorageBufferDynamic, 1}
    }, 1);

    skinDescSet_.allocate(dvk_->device, skinDescPool_, skinDescLayout_);

    DescWrite()
        .setDstSet(skinDescSet_)
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            skinBuffer_,
            0,
            skinSize_x1_.unaligned // true size
        } })
        .updateDescSets(dvk_->device);

// Setup morph weights data
    mrphWsSize_x1_.unaligned = MAX_MORPH_WS * sizeof(float);
    mrphWsSize_x1_.aligned = dvk_->alignSizeSSBO(mrphWsSize_x1_.unaligned);

    mrphWsBuffer_
        .setDataSize(mrphWsSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    mrphWsDescLayout_.create(dvk_->device, {
        {0, DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr}
    });

    mrphWsDescPool_.create(dvk_->device, {
        {DescType::StorageBufferDynamic, 1}
    }, 1);

    mrphWsDescSet_.allocate(dvk_->device, mrphWsDescPool_, mrphWsDescLayout_);

    DescWrite()
        .setDstSet(mrphWsDescSet_)
        .setType(DescType::StorageBufferDynamic)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            mrphWsBuffer_,
            0,
            mrphWsSize_x1_.unaligned // true size
        } })
        .updateDescSets(dvk_->device);
}


// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    meshInstaMap_.clear();
    shaderGroups_.clear();

    matData_.clear();
    matIdxMap_.clear();

    skinCount_ = 0;
    mrphWsCount_ = 0;

// Some initialization if needed

    matData_.emplace_back();
    matIdxMap_[tinyHandle()] = 0;
}

void tinyDrawable::submit(const MeshEntry& entry) noexcept {
    const tinyMesh* rMesh = fsr_->get<tinyMesh>(entry.mesh);
    if (!rMesh) return;

    // Get or create mesh group
    auto& meshIt = meshInstaMap_.find(entry.mesh);
    if (meshIt == meshInstaMap_.end()) {
        meshInstaMap_.emplace(entry.mesh, std::vector<InstaData>{});
        std::vector<InstaData>& instaVec = meshInstaMap_[entry.mesh];

        // Build shader to submesh map
        std::unordered_map<tinyHandle, std::vector<uint32_t>> shaderToSubmeshes;

        const std::vector<tinyMesh::Submesh>& submeshes = rMesh->submeshes();

        for (uint32_t i = 0; i < submeshes.size(); ++i) {
            tinyHandle matHandle = submeshes[i].material;

            const tinyMaterial* rMat = fsr_->get<tinyMaterial>(matHandle);
            if (!rMat) continue;

            // Ensure material is in the buffer
            if (matIdxMap_.find(matHandle) == matIdxMap_.end()) {
                matIdxMap_[matHandle] = static_cast<uint32_t>(matData_.size());
                matData_.push_back(rMat->data);
            }

            tinyHandle shaderHandle = rMat->shader;

            shaderToSubmeshes[shaderHandle].push_back(i);
        }

        // Create shader groups
        for (const auto& [shaderH, submeshIndices] : shaderToSubmeshes) {
            MeshRange meshRange;
            meshRange.mesh = entry.mesh;
            meshRange.submeshes = submeshIndices;

            shaderGroups_[shaderH].push_back(meshRange);
        }

        meshIt = meshInstaMap_.find(entry.mesh);
    }

    // Add instance data
    InstaData instaData;
    instaData.model = entry.model;

    // If mesh entry has bone
    if (entry.skinData && !entry.skinData->empty()) {
        uint32_t thisCount = static_cast<uint32_t>(entry.skinData->size());

        size_t skinDataSize = thisCount * sizeof(glm::mat4);
        size_t skinDataOffset = skinCount_ * sizeof(glm::mat4) + skinOffset(frameIndex_);
        skinBuffer_.copyData(entry.skinData->data(), skinDataSize, skinDataOffset);
        
        instaData.other.x = skinCount_;
        instaData.other.y = thisCount;
        skinCount_ += thisCount;
    }

    if (entry.mrphWeights && !entry.mrphWeights->empty()) {
        uint32_t thisCount = static_cast<uint32_t>(entry.mrphWeights->size());

        size_t mrphWsDataSize = thisCount * sizeof(float);
        size_t mrphWsDataOffset = mrphWsCount_ * sizeof(float) + mrphWsOffset(frameIndex_);
        mrphWsBuffer_.copyData(entry.mrphWeights->data(), mrphWsDataSize, mrphWsDataOffset);

        instaData.other.z = mrphWsCount_;
        instaData.other.w = thisCount;
        mrphWsCount_ += thisCount;
    }

    meshIt->second.push_back(instaData);


}

void tinyDrawable::finalize() {
    uint32_t curInstances = 0;

    for (auto& [shaderHandle, shaderGroupVec] : shaderGroups_) { // For each shader group:
        for (MeshRange& meshRange : shaderGroupVec) {            // For each mesh group:
            const auto& meshIt = meshInstaMap_.find(meshRange.mesh);
            if (meshIt == meshInstaMap_.end()) continue; // Should not happen

            const std::vector<InstaData>& dataVec = meshIt->second;

            // Update mesh range info
            meshRange.instaOffset = curInstances;
            meshRange.instaCount = static_cast<uint32_t>(dataVec.size());

            // Copy data
            size_t dataOffset = curInstances * sizeof(InstaData) + instaOffset(frameIndex_);
            size_t dataSize = dataVec.size() * sizeof(InstaData);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += meshRange.instaCount;
        }
    }

    // Update the material buffer
    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);
}