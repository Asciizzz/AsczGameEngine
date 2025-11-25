#include "tinyEngine/tinyDrawable.hpp"

using namespace Mesh3D;

using namespace tinyVk;

VkVertexInputBindingDescription Insta::bindingDesc() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 1;
    bindingDescription.stride = sizeof(Insta);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Insta::attrDescs() {
    std::vector<VkVertexInputAttributeDescription> attribs(5);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 0};
    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 1};
    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 2};
    attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, model) + sizeof(glm::vec4) * 3};
    attribs[4] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Insta, other)};
    return attribs;
}

//---------------------------------------------------------------

void tinyDrawable::init(const CreateInfo& info) {
    maxFramesInFlight_ = info.maxFramesInFlight;
    fsr_ = info.fsr;
    dvk_ = info.dvk;

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

    matBuffer_
        .setDataSize(matSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(tinyVk::BufferUsage::Storage)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    // Create pool and layout
    matDescLayout_.create(dvk_->device, {
        {0, tinyVk::DescType::StorageBuffer, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    });

    matDescPool_.create(dvk_->device, {
        {tinyVk::DescType::StorageBuffer, MAX_MATERIALS}
    }, MAX_MATERIALS);

    matDescSet_.allocate(dvk_->device, matDescPool_, matDescLayout_);
}


// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    shaderGroups_.clear();
    shaderIdxMap_.clear();

    matData_.clear();
    matIdxMap_.clear();
}

void tinyDrawable::submit(const MeshEntry& entry) noexcept {
    tinyHandle shaderHandle = tinyHandle();

    auto shaderIt = shaderIdxMap_.find(shaderHandle);

    if (shaderIt == shaderIdxMap_.end()) {
        shaderIdxMap_[shaderHandle] = static_cast<uint32_t>(shaderGroups_.size());
        shaderGroups_.emplace_back();
        shaderIt = shaderIdxMap_.find(shaderHandle);
    }

    ShaderGroup& shaderGroup = shaderGroups_[shaderIt->second];
    shaderGroup.instaMap[entry.mesh].push_back({ entry.model, entry.other });

    // Push material
    tinyHandle matHandle = tinyHandle();

    auto matIt = matIdxMap_.find(matHandle);
    if (matIt == matIdxMap_.end()) {
        uint32_t newIndex = static_cast<uint32_t>(matData_.size());

        matIdxMap_[matHandle] = newIndex;
        matData_.push_back(tinyMaterial::Data());

        shaderGroup.meshToMatIndex[entry.mesh] = newIndex;
    }
}


void tinyDrawable::finalize(uint32_t* totalInstances, uint32_t* totalMaterials) {
    uint32_t curInstances = 0;

    for (ShaderGroup& shaderGroup : shaderGroups_) {
        shaderGroup.instaRanges.clear();

        for (const auto& [meshH, dataVec] : shaderGroup.instaMap) {
            if (curInstances + dataVec.size() > MAX_INSTANCES) break; // Should astronomically rarely happen

            Mesh3D::InstaRange range;
            range.mesh = meshH;
            range.matIndex = shaderGroup.meshToMatIndex[meshH];
            range.offset = curInstances;
            range.count = dataVec.size();
            shaderGroup.instaRanges.push_back(range);

            // Copy data
            size_t dataOffset = curInstances * sizeof(Mesh3D::Insta) + frameIndex_ * instaSize_x1_.aligned;
            size_t dataSize = dataVec.size() * sizeof(Mesh3D::Insta);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += static_cast<uint32_t>(dataVec.size());
        }
        shaderGroup.instaMap.clear();
    }

    if (totalInstances) *totalInstances = curInstances;
    if (totalMaterials) *totalMaterials = static_cast<uint32_t>(matData_.size());
}