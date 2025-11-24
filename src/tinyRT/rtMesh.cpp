#include "tinyRT/rtMesh.hpp"

using namespace tinyRT;
using namespace tinyVk;

VkVertexInputBindingDescription MeshStatic3D::Data::bindingDesc() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 1;
    bindingDescription.stride = sizeof(Data);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> MeshStatic3D::Data::attrDescs() {
    std::vector<VkVertexInputAttributeDescription> attribs(5);

    attribs[0] = {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 0};
    attribs[1] = {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 1};
    attribs[2] = {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 2};
    attribs[3] = {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, model) + sizeof(glm::vec4) * 3};
    attribs[4] = {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Data, other)};
    return attribs;
}



void MeshStatic3D::init(const tinyVk::Device* dvk, const tinyPool<tinyMesh>* meshPool) {
    meshPool_ = meshPool;

    // Create instance buffer with max size
    size_t bufferSize = MAX_INSTANCES * sizeof(MeshStatic3D::Data);
    instaBuffer_
        .setDataSize(bufferSize)
        .setUsageFlags(BufferUsage::Vertex)
        .setMemPropFlags(MemProp::HostVisibleAndCoherent)
        .createBuffer(dvk)
        .mapMemory();
}