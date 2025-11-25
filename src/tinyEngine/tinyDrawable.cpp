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
        // Later

// Setup skin data

    skinSize_x1_.unaligned = MAX_BONES * sizeof(glm::mat4);
    skinSize_x1_.aligned = dvk_->alignSizeSSBO(skinSize_x1_.unaligned);

    skinBuffer_
        .setDataSize(skinSize_x1_.aligned * maxFramesInFlight_)
        .setUsageFlags(tinyVk::BufferUsage::Storage)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();

    skinDescLayout_.create(dvk_->device, {
        {0, tinyVk::DescType::StorageBufferDynamic, 1, ShaderStage::Vertex, nullptr}
    });

    skinDescPool_.create(dvk_->device, {
        {tinyVk::DescType::StorageBufferDynamic, 1}
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
}


// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    meshInstaMap_.clear();
    shaderGroups_.clear();

    matData_.clear();
    matIdxMap_.clear();

    boneCount_ = 0;

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
        meshInstaMap_.emplace(entry.mesh, std::vector<Mesh3D::Insta>{});
        std::vector<Mesh3D::Insta>& instaVec = meshInstaMap_[entry.mesh];

        // Build shader to submesh map
        std::unordered_map<tinyHandle, std::vector<uint32_t>> shaderToSubmeshes;

        const std::vector<tinyMesh::Part>& parts = rMesh->parts();

        for (uint32_t i = 0; i < parts.size(); ++i) {
            tinyHandle matHandle = parts[i].material;

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
    Mesh3D::Insta instaData;
    instaData.model = entry.model;

    // If mesh entry has bone
    if (entry.skins) {
        uint32_t skinSize = static_cast<uint32_t>(entry.skins->size());

        size_t skinDataSize = skinSize * sizeof(glm::mat4);
        size_t skinDataOffset = boneCount_ * sizeof(glm::mat4) + skinOffset(frameIndex_);
        skinBuffer_.copyData(entry.skins->data(), skinDataSize, skinDataOffset);

        instaData.other = glm::uvec4(boneCount_, skinSize, 0, 0);

        boneCount_ += skinSize;

    }

    meshIt->second.push_back(instaData);


}

void tinyDrawable::finalize() {
    uint32_t curInstances = 0;

    for (auto& [shaderHandle, shaderGroupVec] : shaderGroups_) { // For each shader group:
        for (MeshRange& meshRange : shaderGroupVec) {            // For each mesh group:
            const auto& meshIt = meshInstaMap_.find(meshRange.mesh);
            if (meshIt == meshInstaMap_.end()) continue; // Should not happen

            const std::vector<Mesh3D::Insta>& dataVec = meshIt->second;

            // Update mesh range info
            meshRange.instaOffset = curInstances;
            meshRange.instaCount = static_cast<uint32_t>(dataVec.size());

            // Copy data
            size_t dataOffset = curInstances * sizeof(Mesh3D::Insta) + instaOffset(frameIndex_);
            size_t dataSize = dataVec.size() * sizeof(Mesh3D::Insta);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += meshRange.instaCount;
        }
    }

    // Update the material buffer
    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);
}