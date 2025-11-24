#include "tinyEngine/tinyDrawable.hpp"

using namespace Mesh3D;

using namespace tinyVk;

VkVertexInputBindingDescription Static::bindingDesc() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 1;
    bindingDescription.stride = sizeof(Static);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Static::attrDescs() {
    std::vector<VkVertexInputAttributeDescription> attribs(5);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, model) + sizeof(glm::vec4) * 0};
    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, model) + sizeof(glm::vec4) * 1};
    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, model) + sizeof(glm::vec4) * 2};
    attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, model) + sizeof(glm::vec4) * 3};
    attribs[4] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Static, other)};
    return attribs;
}

//---------------------------------------------------------------

void tinyDrawable::init(const CreateInfo& info) {
    maxFramesInFlight_ = info.maxFramesInFlight;
    fsr_ = info.fsr;
    dvk_ = info.dvk;

    size_t bufferSize = MAX_INSTANCES * sizeof(Mesh3D::Static);
    instaBuffer_
        .setDataSize(bufferSize)
        .setUsageFlags(tinyVk::BufferUsage::Vertex)
        .setMemPropFlags(tinyVk::MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk_)
        .mapMemory();
}


size_t tinyDrawable::finalize() {
    instaRanges_.clear();

    size_t totalInstances = 0;
    for (const auto& [meshH, dataVec] : instaMap_) {
        if (totalInstances + dataVec.size() > MAX_INSTANCES) break; // Should astronomically rarely happen

        InstaRange range;
        range.mesh = meshH;
        range.offset = static_cast<uint32_t>(totalInstances);
        range.count = static_cast<uint32_t>(dataVec.size());
        instaRanges_.push_back(range);

        // Copy data
        size_t dataOffset = totalInstances * sizeof(Static);
        size_t dataSize = dataVec.size() * sizeof(Static);
        instaBuffer_.copyData(dataVec.data(), dataSize, dataOffset);

        totalInstances += dataVec.size();
    }

    instaMap_.clear();
    return totalInstances;
}