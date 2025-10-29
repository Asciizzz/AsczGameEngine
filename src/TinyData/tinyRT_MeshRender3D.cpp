#include "tinyData/tinyRT_MeshRender3D.hpp"

using namespace tinyRT;

MeshRender3D* MeshRender3D::init(const tinyVk::Device* deviceVk, const tinyRegistry* fsRegistry) {
    deviceVk_ = deviceVk;
    fsRegistry_ = fsRegistry;
    return this;
}

void MeshRender3D::copy(const MeshRender3D* other) {
    if (!other) return;

    meshHandle_ = other->meshHandle_;
    skeleNodeHandle_ = other->skeleNodeHandle_;
}