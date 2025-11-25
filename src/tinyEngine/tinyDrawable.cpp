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

/*
Mesh:
    Index, Vertex, Morph, basic stuff

    Part { materialHandle; range (offset, count) } // Submesh
    Vector<Part> submeshes

MeshGroup:
    Vector<InstanceData> instances; // All instance data for this mesh

ShaderGroup: // All submeshes that use the same shader
    meshHandle
    Vector<submesh_index> submeshes


Global:
    Map<mesh handle -> MeshGroup> meshGroups

    Map<shader handle -> Vector<ShaderGroup>> shaderGroups

    Vector<InstanceData> instanceBuffer (100000 instances x 80 bytes) = 8 MB

    Vector<Material> matBuffer (65536 materials x 96 bytes) = ~6 MB
    Map<handle, int> matIndexMap

Start Frame:
    Clear meshGroups
    Clear shaderGroups
    Clear matBuffer
    Clear matIndexMap

For each Entry:
    meshGroup = meshGroups[entry.meshHandle]

    Lazy init meshGroup if not exists:
        Map<shader handle, Vector<submesh_index>> shaderToSubmeshes

        For each (submesh, submesh_index) in mesh.submeshes:
            matHandle = submesh.materialHandle

            if material[matHandle] exists and matHandle not in matIndexMap :
                matIndexMap[matHandle] = matBuffer.count
                matBuffer.push(material[matHandle].data)

            shaderHandle = getShaderFromMaterial(matHandle) or defaultShaderHandle

            shaderToSubmeshes[shaderHandle].push(submesh_index)

        For each [shaderHandle, Vector<submesh_index>] in shaderToSubmeshes:
            shaderGroups[shaderHandle].push( ShaderGroup {
                meshHandle = entry.meshHandle
                submeshes  = Vector<submesh_index>
            } )

    meshGroup.instances.push(entry.instanceData)

*/


// --------------------------- Batching process --------------------------

void tinyDrawable::startFrame(uint32_t frameIndex) noexcept {
    frameIndex_ = frameIndex % maxFramesInFlight_;

    meshInstaMap_.clear();
    shaderGroups_.clear();

    matData_.clear();
    matIdxMap_.clear();

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
            MeshGroup meshGroup;
            meshGroup.mesh = entry.mesh;
            meshGroup.submeshes = submeshIndices;

            shaderGroups_[shaderH].push_back(meshGroup);
        }

        meshIt = meshInstaMap_.find(entry.mesh);
    }

    // Add instance data
    Mesh3D::Insta instaData;
    instaData.model = entry.model;

    meshIt->second.push_back(instaData);
}


static int count = 0;
static int maxCount = 5;

void tinyDrawable::finalize() {
    uint32_t curInstances = 0;

    for (auto& [shaderHandle, shaderGroupVec] : shaderGroups_) { // For each shader group:
        for (MeshGroup& meshGroup : shaderGroupVec) {            // For each mesh group:
            const auto& meshIt = meshInstaMap_.find(meshGroup.mesh);
            if (meshIt == meshInstaMap_.end()) continue; // Should not happen

            const std::vector<Mesh3D::Insta>& dataVec = meshIt->second;

            // Update mesh group info
            meshGroup.instaOffset = curInstances;
            meshGroup.instaCount = static_cast<uint32_t>(dataVec.size());

            // Copy data
            size_t dataOffset = curInstances * sizeof(Mesh3D::Insta) + instaOffset(frameIndex_);
            size_t dataSize = dataVec.size() * sizeof(Mesh3D::Insta);
            instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

            curInstances += meshGroup.instaCount;
        }
    }

    // Update the material buffer
    size_t matDataOffset = matOffset(frameIndex_); // Aligned
    size_t matDataSize = matData_.size() * sizeof(tinyMaterial::Data); // True size
    matBuffer_.copyData(matData_.data(), matDataSize, matDataOffset);

    // Print the entire shader groups for debugging 
    if (count >= maxCount) return;
    count++;
}