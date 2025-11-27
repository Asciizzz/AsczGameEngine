#pragma once

#include "tinyType.hpp"

#include "tinyVk/Resource/DataBuffer.hpp"
#include "tinyVk/Resource/Descriptor.hpp"

#include <glm/glm.hpp>

#include <string>
#include <limits>
namespace tinyVertex {
    struct Static {
        // Pack into vec4 to guarantee alignment
        glm::vec4 pos_tu;
        glm::vec4 nrml_tv;
        glm::vec4 tang;

        // Some helpers
        Static& setPos (const glm::vec3& p) { pos_tu.x = p.x; pos_tu.y = p.y; pos_tu.z = p.z; return *this; }
        Static& setNrml(const glm::vec3& n) { nrml_tv.x = n.x; nrml_tv.y = n.y; nrml_tv.z = n.z; return *this; }
        Static& setUV  (const glm::vec2& uv){ pos_tu.w = uv.x; nrml_tv.w = uv.y; return *this; }
        Static& setTang(const glm::vec4& t) { tang = t; return *this; }
    };

    struct Rigged {
        glm::uvec4 boneIDs;
        glm::vec4 boneWs;
    };

    struct Morph {
        glm::vec3 dPos;
        glm::vec3 dNrml;
        glm::vec3 dTang;
    };

    struct Color {
        glm::vec4 color = glm::vec4(1.f);
    };

    enum class Type : uint32_t {
        Static = 0,
        Rig    = 1 << 0,
        Morph  = 1 << 1,
        Color  = 1 << 2
    };

    inline Type& operator|=(Type& a, Type b) {
        a = static_cast<Type>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
        return a;
    }
};

// HUGE UPDATE (IN PROGRESS)
/*

Completely redesign this mesh structure to be submesh based

*/

struct tinyMesh {
    tinyMesh() noexcept = default;

    tinyMesh(const tinyMesh&) = delete;
    tinyMesh& operator=(const tinyMesh&) = delete;

    tinyMesh(tinyMesh&&) noexcept = default;
    tinyMesh& operator=(tinyMesh&&) noexcept = default;

    struct Submesh {
        Submesh() noexcept = default;

        Submesh(const Submesh&) = delete;
        Submesh& operator=(const Submesh&) = delete;

        Submesh(Submesh&&) noexcept = default;
        Submesh& operator=(Submesh&&) noexcept = default;

        struct MorphTarget {
            std::vector<tinyVertex::Morph> morphs;
            std::string name;
        };

        std::vector<tinyVertex::Static> vstaticData; // Required
        std::vector<tinyVertex::Rigged> vriggedData; // Optional
        std::vector<tinyVertex::Color>  vcolorData;  // Optional
        std::vector<uint8_t>            indxRaw; // Will be casted to proper type

        // Will be introduced later, too much hassle I am tired
        std::vector<MorphTarget> mrphTargets; // Optional

        tinyHandle material;

        uint32_t vrtxCount = 0;
        uint32_t indxCount = 0;
        uint32_t mrphCount = 0;

        uint32_t    indxStride = 0;
        VkIndexType indxType = VK_INDEX_TYPE_UINT8;

        tinyVertex::Type vrtxTypes = tinyVertex::Type::Static;
        bool hasType(tinyVertex::Type t) const {
            return (static_cast<uint8_t>(vrtxTypes) & static_cast<uint8_t>(t)) != 0;
        }
        uint32_t typeFlags() const {
            return static_cast<uint32_t>(vrtxTypes);
        }

        // Basic vertex data
        tinyVk::DataBuffer vstaticBuffer;
        tinyVk::DataBuffer vriggedBuffer;
        tinyVk::DataBuffer vcolorBuffer;
        tinyVk::DataBuffer indxBuffer;

        // MORPH DATA LELELE
        tinyVk::DescSet mrphDltsDescSet;
        tinyVk::DataBuffer mrphDltsBuffer;

        glm::vec3 ABmin = glm::vec3( std::numeric_limits<float>::max());
        glm::vec3 ABmax = glm::vec3(-std::numeric_limits<float>::max());

        // Methods

        Submesh& setVrtxStatic(const std::vector<tinyVertex::Static>& stas) {
            vrtxTypes |= tinyVertex::Type::Static;
            vrtxCount = stas.size();
            vstaticData = stas;

            for (const auto& v : stas) {
                glm::vec3 pos(v.pos_tu.x, v.pos_tu.y, v.pos_tu.z);
                ABmin = glm::min(ABmin, pos);
                ABmax = glm::max(ABmax, pos);
            }

            return *this;
        }

        Submesh& setVrtxRigged(const std::vector<tinyVertex::Rigged>& rigs) {
            if (rigs.size() != vrtxCount) return *this; // Error, size mismatch

            vrtxTypes |= tinyVertex::Type::Rig;
            vriggedData = rigs;
            return *this;
        }

        Submesh& setVrtxColor(const std::vector<tinyVertex::Color>& colors) {
            if (colors.size() != vrtxCount) return *this; // Error, size mismatch

            vrtxTypes |= tinyVertex::Type::Color;
            vcolorData = colors;
            return *this;
        }

        Submesh& addMrphTarget(const MorphTarget& target) {
            if (target.morphs.size() != vrtxCount) return *this; // Error, size mismatch

            vrtxTypes |= tinyVertex::Type::Morph;
            mrphTargets.push_back(target);
            return *this;
        }

        template<typename IndexT>
        Submesh& setIndxs(const std::vector<IndexT>& indices) {
            indxCount = indices.size();
            indxStride = sizeof(IndexT);
            indxType = strideToType(indxStride);

            size_t byteSize = indxCount * indxStride;
            indxRaw.resize(byteSize);
            std::memcpy(indxRaw.data(), indices.data(), byteSize);

            return *this;   
        }
        
        void vkCreate(const tinyVk::Device* dvk_, VkDescriptorSetLayout mrphDltsLayout = VK_NULL_HANDLE, VkDescriptorPool mrphDltsPool = VK_NULL_HANDLE) {
            using namespace tinyVk;

        // Vertex buffers

            vstaticBuffer
                .setDataSize(vstaticData.size() * sizeof(tinyVertex::Static))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vstaticData.data());

            if (!hasType(tinyVertex::Type::Rig)) vriggedData.push_back(tinyVertex::Rigged()); // Dummy entry to avoid errors
            vriggedBuffer
                .setDataSize(vriggedData.size() * sizeof(tinyVertex::Rigged))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vriggedData.data());

            if (!hasType(tinyVertex::Type::Color)) vcolorData.push_back(tinyVertex::Color()); // Dummy white color
            vcolorBuffer
                .setDataSize(vcolorData.size() * sizeof(tinyVertex::Color))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vcolorData.data());

            indxBuffer
                .setDataSize(indxRaw.size())
                .setUsageFlags(BufferUsage::Index)
                .createDeviceLocalBuffer(dvk_, indxRaw.data());

            // Will implement morphs later
        }
        
        void clearCPU() {
            vstaticData.clear();
            vriggedData.clear();
            vcolorData.clear();
            indxRaw.clear();
            mrphTargets.clear();
        }

        static VkIndexType strideToType(size_t stride) noexcept {
            switch (stride) {
                case sizeof(uint8_t):  return VK_INDEX_TYPE_UINT8;
                case sizeof(uint16_t): return VK_INDEX_TYPE_UINT16;
                case sizeof(uint32_t): return VK_INDEX_TYPE_UINT32;
                default:               return VK_INDEX_TYPE_MAX_ENUM;
            }
        }
    };

    void vkCreate(const tinyVk::Device* dvk_, VkDescriptorSetLayout mrphDltsLayout = VK_NULL_HANDLE, VkDescriptorPool mrphDltsPool = VK_NULL_HANDLE) {
        for (auto& submesh : submeshes) {
            submesh.vkCreate(dvk_, mrphDltsLayout, mrphDltsPool);

            ABmin = glm::min(ABmin, submesh.ABmin);
            ABmax = glm::max(ABmax, submesh.ABmax);
        }
    }

    size_t append(Submesh&& submesh) {
        ABmin = glm::min(ABmin, submesh.ABmin);
        ABmax = glm::max(ABmax, submesh.ABmax);

        submeshes.push_back(std::move(submesh));
        return submeshes.size() - 1;
    }

    std::vector<Submesh> submeshes;

    Submesh* submesh(size_t index) {
        return index < submeshes.size() ? &submeshes[index] : nullptr;
    }
    const Submesh* submesh(size_t index) const {
        return index < submeshes.size() ? &submeshes[index] : nullptr;
    }

    // void vkCreate(const tinyVk::Device* device, VkDescriptorSetLayout mrphLayout = VK_NULL_HANDLE, VkDescriptorPool mrphPool = VK_NULL_HANDLE) {


    // // Morph deltas

    //     if (mrphTargets_.empty() ||
    //         mrphLayout == VK_NULL_HANDLE ||
    //         mrphPool == VK_NULL_HANDLE) {
    //         clearData(); // Optional
    //         return; // Can't create morph data
    //     }

    //     hasMorphs_ = true;

    //     mrphDeltas_.resize(mrphCount_ * vrtxCount_);
    //     for (size_t i = 0; i < mrphCount_; ++i) {
    //         // Should not happen, hopefully
    //         assert(mrphTargets_[i].deltas.size() == vrtxCount_ && "Morph target delta count must match vertex count");

    //         std::memcpy(
    //             mrphDeltas_.data() + i * vrtxCount_,
    //             mrphTargets_[i].deltas.data(),
    //             vrtxCount_ * sizeof(tinyMorph::Delta)
    //         );
    //     }

    //     // Construct an empty weights array for CPU use
    //     mrphWeights_ = std::vector<float>(mrphCount_, 0.0f);

    //     mrphDltsBuffer_
    //         .setDataSize(mrphDeltas_.size() * sizeof(tinyMorph::Delta))
    //         .setUsageFlags(BufferUsage::Storage)
    //         .createDeviceLocalBuffer(dvk_, mrphDeltas_.data());

    //     // Write descriptor set
    //     mrphDltsDescSet_.allocate(dvk_->device, mrphPool, mrphLayout);
    //     DescWrite()
    //         .setDstSet(mrphDltsDescSet_)
    //         .setType(DescType::StorageBuffer)
    //         .setDescCount(1)
    //         .setBufferInfo({ VkDescriptorBufferInfo{
    //             mrphDltsBuffer_, 0, VK_WHOLE_SIZE
    //         } })
    //         .updateDescSets(dvk_->device);

    //     clearData(); // Optional
    // }

    glm::vec3 ABmin = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 ABmax = glm::vec3(std::numeric_limits<float>::lowest());
};