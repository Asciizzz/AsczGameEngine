#pragma once

#include "TinyData/TinyMesh.hpp"
#include "TinyExt/TinyRegistry.hpp"
#include "TinyVK/Resource/DataBuffer.hpp"
#include "TinyVK/Resource/Descriptor.hpp"

struct TinyMeshRT {
    TinyMeshRT() = default;

    TinyMeshRT(const TinyMeshRT&) = delete;
    TinyMeshRT& operator=(const TinyMeshRT&) = delete;

    TinyMeshRT(TinyMeshRT&&) = default;
    TinyMeshRT& operator=(TinyMeshRT&&) = default;

private:
    TinyHandle meshHandle_;
    const TinyRegistry* fsRegistry_ = nullptr;
    const TinyVK::Device* deviceVK_ = nullptr;
};