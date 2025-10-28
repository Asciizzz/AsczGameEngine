#pragma once

#include "tinyData/tinyMesh.hpp"
#include "tinyExt/tinyRegistry.hpp"
#include "tinyVK/Resource/DataBuffer.hpp"
#include "tinyVK/Resource/Descriptor.hpp"

struct tinyMeshRT {
    tinyMeshRT() = default;

    tinyMeshRT(const tinyMeshRT&) = delete;
    tinyMeshRT& operator=(const tinyMeshRT&) = delete;

    tinyMeshRT(tinyMeshRT&&) = default;
    tinyMeshRT& operator=(tinyMeshRT&&) = default;

private:
    tinyHandle meshHandle_;
    const tinyRegistry* fsRegistry_ = nullptr;
    const tinyVK::Device* deviceVK_ = nullptr;
};