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
    
    // Later

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

    tinyMesh::createVertexExtensionDescriptors(dvk_, &vrtxExtLayout_, &vrtxExtPool_);
}

static tinyHandle defaultMatHandle = tinyHandle::make<tinyMaterial>(0, 0);

// --------------------------- Batching process --------------------------

/* Idea:

Batch by shader -> mesh -> submesh

Shader 0
    - Mesh 0
        - Submesh 0
            - Instance 0
            - Instance 1
        - Submesh 1

Shader 1
    - Mesh 0
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
    tinyHandle meshHandle = entry.mesh;
    size_t submeshIndex = entry.submesh;

    // Get or create SubmeshGroup
    tinyHandle hash = entry.hash();
    auto subIt = batchMap_.find(hash); // Entry hash -> SubmeshGroup index

    if (subIt == batchMap_.end()) {
        const tinyMesh* rMesh = fsr_->get<tinyMesh>(meshHandle);
        if (!rMesh) return;

        const tinyMesh::Submesh* submesh = rMesh->submesh(submeshIndex);
        if (!submesh) return;

        tinyHandle materialHandle = submesh->material;

        // Check for material existence as well as getting/creating ShaderGroup
        auto shaderIt = batchMap_.find(materialHandle); // Material handle -> ShaderGroup index
        if (shaderIt == batchMap_.end()) {
            tinyHandle shaderHandle;

            // Retrieve submesh's material's shader
            if (const tinyMaterial* rMat = fsr_->get<tinyMaterial>(materialHandle)) {

                // If material not in buffer, add it
                if (dataMap_.find(materialHandle) == dataMap_.end()) { //
                    dataMap_[materialHandle] = matData_.size();
                    matData_.push_back(rMat->data);
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
        tinyHandle skeleNode = skeleData.skeleNode;

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

    // If mesh has morph targets
    const Entry::MorphData& morphData = entry.morphData;
    if (morphData.weights && !morphData.weights->empty() && morphData.count > 0) {
        uint32_t thisCount = morphData.count;
        uint32_t thisOffset = morphData.offset;

        size_t mrphWsDataSize = thisCount * sizeof(float);
        size_t mrphWsDataOffset = mrphWsCount_ * sizeof(float) + mrphWsOffset(frameIndex_);
        
        // Only copy slice of the weights vector
        mrphWsBuffer_.copyData(morphData.weights->data() + thisOffset, mrphWsDataSize, mrphWsDataOffset);

        instaData.other.z = mrphWsCount_;
        instaData.other.w = thisCount;
        mrphWsCount_ += thisCount;
    }

    submeshGroup.push(instaData);
}

void tinyDrawable::finalize() {
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