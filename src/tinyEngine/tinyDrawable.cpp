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

    size_t instaSize = MAX_INSTANCES * sizeof(Mesh3D::Insta);
    instaBuffer_
        .setDataSize(instaSize)
        .setUsageFlags(tinyVk::BufferUsage::Vertex)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    size_t matSize = MAX_MATERIALS * sizeof(tinyMaterial::Data);
    matBuffer_
        .setDataSize(matSize)
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

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;
    shaderGroups_.clear();
    shaderIdxMap_.clear();
}

void tinyDrawable::submit(const MeshEntry& entry) noexcept {
    // For the time being just use tinyHandle() as the key

    tinyHandle handle = tinyHandle();
    
    auto it = shaderIdxMap_.find(handle);

    if (it == shaderIdxMap_.end()) {
        shaderIdxMap_[handle] = static_cast<uint32_t>(shaderGroups_.size());
        shaderGroups_.emplace_back();
        it = shaderIdxMap_.find(handle);
    }

    ShaderGroup& shaderGroup = shaderGroups_[it->second];
    // Add instance data
    shaderGroup.instaMap[entry.mesh].push_back({ entry.model, entry.other });
    // Add material data (in the future)
}



void tinyDrawable::finalize(uint32_t* totalInstances, uint32_t* totalMaterials) {
    uint32_t curInstances = 0;
    uint32_t curMaterials = 0;

    for (ShaderGroup& shaderGroup : shaderGroups_) {
        shaderGroup.instaRanges.clear();

        for (const auto& [meshH, dataVec] : shaderGroup.instaMap) {
            if (curInstances + dataVec.size() > MAX_INSTANCES) break; // Should astronomically rarely happen

            Mesh3D::InstaRange range;
            range.mesh = meshH;
            range.matIndex = shaderGroup.getMatIndex(tinyHandle()); // For the time being use default material
            range.offset = curInstances;
            range.count = dataVec.size();
            shaderGroup.instaRanges.push_back(range);

            // Copy data
            size_t dataOffset = curInstances * sizeof(Mesh3D::Insta);
            size_t dataSize = dataVec.size() * sizeof(Mesh3D::Insta);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += static_cast<uint32_t>(dataVec.size());
        }
        shaderGroup.instaMap.clear();

        // Handle materials
        shaderGroup.matOffset = static_cast<uint32_t>(curMaterials);

        // size_t matDataSize = shaderGroup.mats.size() * sizeof(tinyMaterial::Data);
        // matBuffer_.copyData(shaderGroup.mats.data(), matDataSize, curMaterials * sizeof(tinyMaterial::Data));
        // curMaterials += shaderGroup.mats.size();

        curMaterials += static_cast<uint32_t>(shaderGroup.mats.size());
    }

    if (totalInstances) *totalInstances = curInstances;
    if (totalMaterials) *totalMaterials = curMaterials;
}