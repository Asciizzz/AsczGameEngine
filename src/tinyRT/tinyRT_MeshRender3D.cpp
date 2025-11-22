#include "tinyRT_MeshRender3D.hpp"

using namespace tinyRT;
using namespace tinyVk;

void MeshRender3D::init(const Device* deviceVk, const tinyPool<tinyMeshVk>* meshPool, VkDescriptorSetLayout mrphWsDescSetLayout, VkDescriptorPool mrphWsDescPool, uint32_t maxFramesInFlight) {
    deviceVk_ = deviceVk;
    meshPool_ = meshPool;
    maxFramesInFlight_ = maxFramesInFlight;
    vkValid = true;

    mrphWsDescSet_.allocate(deviceVk->device, mrphWsDescPool, mrphWsDescSetLayout);
}

MeshRender3D& MeshRender3D::setMesh(tinyHandle meshHandle) {
    if (!vkValid || !meshHandle) return *this;

    meshHandle_ = meshHandle;
    if (!rMesh()) return *this;

    // Copy the material slots
    matSlots_.clear();
    for (const auto& part : rMesh()->parts()) {
        matSlots_.push_back(part.material);
    }

    vkWrite(deviceVk_, &mrphWsBuffer_, &mrphWsDescSet_, maxFramesInFlight_, mrphCount(), &unalignedSize_, &alignedSize_);
    mrphWeights_.resize(mrphCount(), 0.0f);

    return *this;
}

void MeshRender3D::vkWrite(const tinyVk::Device* deviceVk, tinyVk::DataBuffer* buffer, tinyVk::DescSet* descSet, size_t maxFramesInFlight, size_t mrphCount, uint32_t* unalignedSize, uint32_t* alignedSize) {
    if (mrphCount == 0) return; // No morph targets

    VkDeviceSize perFrameSize = sizeof(float) * mrphCount;
    VkDeviceSize perFrameAligned = deviceVk->alignSizeSSBO(perFrameSize);

    bool isDynamic = maxFramesInFlight > 1;
    VkDeviceSize finalSize = isDynamic ? perFrameAligned * maxFramesInFlight : perFrameSize; // Non-dynamic can keep original size

    if (unalignedSize) *unalignedSize = static_cast<uint32_t>(perFrameSize);
    if (alignedSize)   *alignedSize   = static_cast<uint32_t>(perFrameAligned);

    buffer
        ->setDataSize(finalSize)
        .setUsageFlags(BufferUsage::Storage)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(deviceVk)
        .mapMemory();

    DescWrite()
        .setDstSet(*descSet)
        .setType(isDynamic ? DescType::StorageBufferDynamic : DescType::StorageBuffer)
        .setDescCount(1)
        .setBufferInfo({ VkDescriptorBufferInfo{
            *buffer,
            0,
            isDynamic ? perFrameAligned : perFrameSize
        } })
        .updateDescSets(deviceVk->device);
}


MeshRender3D& MeshRender3D::setSkeleNode(tinyHandle skeleNodeHandle) {
    skeleNodeHandle_ = skeleNodeHandle ? skeleNodeHandle : skeleNodeHandle_;
    return *this;
}

void MeshRender3D::copy(const MeshRender3D* other) {
    if (!other) return;

    setMesh(other->meshHandle_);
    setSkeleNode(other->skeleNodeHandle_);
}



VkDescriptorSet MeshRender3D::mrphWsDescSet() const noexcept {
    return mrphWsDescSet_;
}
VkDescriptorSet MeshRender3D::mrphDsDescSet() const noexcept {
    const tinyMeshVk* mesh = rMesh();
    return mesh ? mesh->mrphDsDescSet() : VK_NULL_HANDLE;
}

uint32_t MeshRender3D::mrphWsDynamicOffset(uint32_t curFrame) const noexcept {
    return curFrame * alignedSize_;
}

void MeshRender3D::vkUpdate(uint32_t curFrame) noexcept {
    if (!hasMrph()) return;

    VkDeviceSize offset = mrphWsDynamicOffset(curFrame);
    mrphWsBuffer_.copyData(mrphWeights_.data(), unalignedSize_, offset);
}