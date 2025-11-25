#include "tinyEngine/tinyDrawable.hpp"

using namespace Mesh3D;

using namespace tinyVk;

VkVertexInputBindingDescription Insta::bindingDesc(uint32_t binding) {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = binding;
    bindingDescription.stride = sizeof(Insta);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Insta::attrDescs(uint32_t binding, uint32_t locationOffset) {
    std::vector<VkVertexInputAttributeDescription> attribs(5);

    attribs[0] = {locationOffset + 0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 0};
    attribs[1] = {locationOffset + 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 1};
    attribs[2] = {locationOffset + 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 2};
    attribs[3] = {locationOffset + 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 3};
    attribs[4] = {locationOffset + 4, 1, VK_FORMAT_R32G32B32A32_UINT,  offsetof(Insta, other)};
    return attribs;
}

//---------------------------------------------------------------

void tinyDrawable::init(const CreateInfo& info) {
    maxFramesInFlight_ = info.maxFramesInFlight;
    fsr_ = info.fsr;
    dvk_ = info.dvk;

// Setup instance buffer

    instaSize_x1_.unaligned = MAX_INSTANCES * sizeof(Mesh3D::Insta);
    instaSize_x1_.aligned = instaSize_x1_.unaligned; // Vertex attributes doesnt need minAlignment

    instaBuffer_
        .setDataSize(instaSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(tinyVk::BufferUsage::Vertex)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    matSize_x1_.unaligned = MAX_MATERIALS * sizeof(tinyMaterial::Data);
    matSize_x1_.aligned = dvk_->alignSizeSSBO(matSize_x1_.unaligned); // SSBO DO need alignment

// Setup material data

    matBuffer_
        .setDataSize(matSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(tinyVk::BufferUsage::Storage)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    matDescLayout_.create(dvk_->device, {
        {0, tinyVk::DescType::StorageBufferDynamic, 1, ShaderStage::Fragment, nullptr}
    });

    matDescPool_.create(dvk_->device, {
        {tinyVk::DescType::StorageBufferDynamic, 1}
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

}


// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    shaderGroups_.clear();
    shaderIdxMap_.clear();

    matData_.clear();
    matIdxMap_.clear();

// Some initialization if needed
    matData_.emplace_back(); // First material is always default
    matIdxMap_[tinyHandle()] = 0;
}

void tinyDrawable::submit(const MeshEntry& entry) noexcept {
    const tinyMesh* rMesh = fsr_->get<tinyMesh>(entry.mesh);
    const tinyMaterial* rMat = fsr_->get<tinyMaterial>(entry.mat);

    uint32_t matIndex = 0;
    auto matIt = matIdxMap_.find(entry.mat);

    if (matIt != matIdxMap_.end()) {
        matIndex = matIt->second;
    } else if (rMat) {
        matIndex = static_cast<uint32_t>(matData_.size());

        tinyMaterial::Data matData;
        matData.base = rMat->baseColor;

        matIdxMap_[entry.mat] = matIndex;
        matData_.push_back(matData);
    }

    tinyHandle shaderHandle = entry.shader;

    auto shaderIt = shaderIdxMap_.find(shaderHandle);
    if (shaderIt == shaderIdxMap_.end()) {
        shaderIdxMap_[shaderHandle] = static_cast<uint32_t>(shaderGroups_.size());

        shaderGroups_.emplace_back();
        shaderIt = shaderIdxMap_.find(shaderHandle);
    }

    ShaderGroup& shaderGroup = shaderGroups_[shaderIt->second];
    shaderGroup.shader = shaderHandle;

    std::vector<Mesh3D::Insta>& instaVec = shaderGroup.instaMap[entry.mesh];
    instaVec.push_back({
        entry.model,
        glm::uvec4(matIndex, 0, 0, 0)
    });
}


void tinyDrawable::finalize(uint32_t* totalInstances, uint32_t* totalMaterials) {
    uint32_t curInstances = 0;

    for (ShaderGroup& shaderGroup : shaderGroups_) {
        shaderGroup.instaRanges.clear();

        for (const auto& [meshH, dataVec] : shaderGroup.instaMap) {
            if (curInstances + dataVec.size() > MAX_INSTANCES) break; // Should astronomically rarely happen

            Mesh3D::InstaRange range;
            range.mesh = meshH;
            range.offset = curInstances;
            range.count = dataVec.size();
            shaderGroup.instaRanges.push_back(range);

            // Copy data
            size_t dataOffset = curInstances * sizeof(Mesh3D::Insta) + instaOffset(frameIndex_);
            size_t dataSize = dataVec.size() * sizeof(Mesh3D::Insta);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += static_cast<uint32_t>(dataVec.size());
        }
        shaderGroup.instaMap.clear();
    }

    // Update the material buffer
    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);

    if (totalInstances) *totalInstances = curInstances;
    if (totalMaterials) *totalMaterials = static_cast<uint32_t>(matData_.size());
}