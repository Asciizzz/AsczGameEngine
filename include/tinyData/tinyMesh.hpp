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

        glm::vec3 pos () const { return glm::vec3(pos_tu.x, pos_tu.y, pos_tu.z); }
        glm::vec3 nrml() const { return glm::vec3(nrml_tv.x, nrml_tv.y, nrml_tv.z); }
        glm::vec2 uv  () const { return glm::vec2(pos_tu.w, nrml_tv.w); }
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
        a = static_cast<Type>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
        return a;
    }
};

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

        // Methods

        Submesh& setVrtxStatic(const std::vector<tinyVertex::Static>& stas) {
            vrtxTypes_ |= tinyVertex::Type::Static;
            vrtxCount_ = stas.size();
            vstaticData_ = stas;

            return *this;
        }

        Submesh& setVrtxRigged(const std::vector<tinyVertex::Rigged>& rigs) {
            if (rigs.size() != vrtxCount_) return *this; // Error, size mismatch

            vrtxTypes_ |= tinyVertex::Type::Rig;
            vriggedData_ = rigs;
            return *this;
        }

        Submesh& setVrtxColor(const std::vector<tinyVertex::Color>& colors) {
            if (colors.size() != vrtxCount_) return *this; // Error, size mismatch

            vrtxTypes_ |= tinyVertex::Type::Color;
            vcolorData_ = colors;
            return *this;
        }

        Submesh& addMrphTarget(const MorphTarget& target) {
            if (target.morphs.size() != vrtxCount_) return *this; // Error, size mismatch

            vrtxTypes_ |= tinyVertex::Type::Morph;
            mrphTargets_.push_back(target);
            return *this;
        }

        template<typename IndexT>
        Submesh& setIndxs(const std::vector<IndexT>& indices) {
            indxCount_ = indices.size();
            indxStride_ = sizeof(IndexT);
            indxType_ = strideToType(indxStride_);

            size_t byteSize = indxCount_ * indxStride_;
            indxRaw_.resize(byteSize);
            std::memcpy(indxRaw_.data(), indices.data(), byteSize);

            return *this;   
        }

        void vkCreate(const tinyVk::Device* dvk_) {
            using namespace tinyVk;

        // Vertex and Index Buffers

            vstaticBuffer_
                .setDataSize(vstaticData_.size() * sizeof(tinyVertex::Static))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vstaticData_.data());

            if (!vrtxHas(tinyVertex::Type::Rig)) vriggedData_.push_back(tinyVertex::Rigged()); // Dummy entry to avoid errors
            vriggedBuffer_
                .setDataSize(vriggedData_.size() * sizeof(tinyVertex::Rigged))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vriggedData_.data());

            if (!vrtxHas(tinyVertex::Type::Color)) vcolorData_.push_back(tinyVertex::Color()); // Dummy white color

            vcolorBuffer_
                .setDataSize(vcolorData_.size() * sizeof(tinyVertex::Color))
                .setUsageFlags(BufferUsage::Vertex)
                .createDeviceLocalBuffer(dvk_, vcolorData_.data());

            indxBuffer_
                .setDataSize(indxRaw_.size())
                .setUsageFlags(BufferUsage::Index)
                .createDeviceLocalBuffer(dvk_, indxRaw_.data());

            // Will implement morphs later
        }
        
        void clearCPU() {
            vstaticData_.clear();
            vriggedData_.clear();
            vcolorData_.clear();
            indxRaw_.clear();
            mrphTargets_.clear();
        }

    // Getters
        inline uint32_t vrtxFlags() const { return static_cast<uint32_t>(vrtxTypes_); }
        inline uint32_t vrtxHas(tinyVertex::Type t) const { return (static_cast<uint32_t>(vrtxTypes_) & static_cast<uint32_t>(t)) != 0; }

        const std::vector<tinyVertex::Static>& vstaticData() const { return vstaticData_; }
        const std::vector<tinyVertex::Rigged>& vriggedData() const { return vriggedData_; }
        const std::vector<tinyVertex::Color>&  vcolorData() const  { return vcolorData_;  }
        const std::vector<uint8_t>&            indxRaw() const     { return indxRaw_;    }

        inline tinyHandle material() const { return material_; }
        void setMaterial(tinyHandle mat) { material_ = mat; }

        inline uint32_t vrtxCount() const { return vrtxCount_; }
        inline uint32_t indxCount() const { return indxCount_; }
        inline VkIndexType indxType() const { return indxType_; }
        
        inline uint32_t mrphCount() const { return mrphCount_; }
        inline uint32_t mrphOffset() const { return mrphOffset_; }
        void setMrphOffset(uint32_t offset) { mrphOffset_ = offset; }

        VkBuffer vstaticBuffer() const { return vstaticBuffer_; }
        VkBuffer vriggedBuffer() const { return vriggedBuffer_; }
        VkBuffer vcolorBuffer() const  { return vcolorBuffer_;  }
        VkBuffer indxBuffer() const    { return indxBuffer_;    }

        const glm::vec3& ABmin() const { return ABmin_; }
        const glm::vec3& ABmax() const { return ABmax_; }
        
        void setABmin(const glm::vec3& abMin) { ABmin_ = abMin; }
        void setABmax(const glm::vec3& abMax) { ABmax_ = abMax; }
        void setAABB(const glm::vec3& abMin, const glm::vec3& abMax) {
            ABmin_ = abMin;
            ABmax_ = abMax;
        }

        void expandABmin(const glm::vec3& point) { ABmin_ = glm::min(ABmin_, point); }
        void expandABmax(const glm::vec3& point) { ABmax_ = glm::max(ABmax_, point); }
        void expandAABB(const glm::vec3& point) {
            expandABmin(point);
            expandABmax(point);
        }
    private:
        friend class tinyMesh; // Not needed but good to have

        std::vector<tinyVertex::Static> vstaticData_; // Required
        std::vector<tinyVertex::Rigged> vriggedData_; // Optional
        std::vector<tinyVertex::Color>  vcolorData_;  // Optional
        std::vector<uint8_t>            indxRaw_; // Will be casted to proper type

        // Will be introduced later, too much hassle I am tired
        std::vector<MorphTarget> mrphTargets_; // Optional

        tinyHandle material_;

        uint32_t vrtxCount_ = 0;
        uint32_t indxCount_ = 0;

        uint32_t    indxStride_ = 0;
        VkIndexType indxType_ = VK_INDEX_TYPE_UINT8;

        uint32_t mrphCount_ = 0;
        uint32_t mrphOffset_ = 0; // Offset in the big morph deltas buffer

        tinyVertex::Type vrtxTypes_ = tinyVertex::Type::Static;

        // Basic vertex data
        tinyVk::DataBuffer vstaticBuffer_;
        tinyVk::DataBuffer vriggedBuffer_;
        tinyVk::DataBuffer vcolorBuffer_;
        tinyVk::DataBuffer indxBuffer_;

        glm::vec3 ABmin_ = glm::vec3( std::numeric_limits<float>::max());
        glm::vec3 ABmax_ = glm::vec3(-std::numeric_limits<float>::max());

        static VkIndexType strideToType(size_t stride) noexcept {
            switch (stride) {
                case sizeof(uint8_t):  return VK_INDEX_TYPE_UINT8;
                case sizeof(uint16_t): return VK_INDEX_TYPE_UINT16;
                case sizeof(uint32_t): return VK_INDEX_TYPE_UINT32;
                default:               return VK_INDEX_TYPE_MAX_ENUM;
            }
        }
    };

    size_t append(Submesh&& submesh) {
        ABmin_ = glm::min(ABmin_, submesh.ABmin());
        ABmax_ = glm::max(ABmax_, submesh.ABmax());

        submeshes_.push_back(std::move(submesh));
        return submeshes_.size() - 1;
    }

    void vkCreate(const tinyVk::Device* dvk_, VkDescriptorSetLayout mrphDltsLayout = VK_NULL_HANDLE, VkDescriptorPool mrphDltsPool = VK_NULL_HANDLE) {
        for (auto& submesh : submeshes_) {
            submesh.vkCreate(dvk_);
        }
    }

    std::vector<tinyMesh::Submesh>&       submeshes()       { return submeshes_; }
    const std::vector<tinyMesh::Submesh>& submeshes() const { return submeshes_; }

    Submesh* submesh(size_t index) { return index < submeshes_.size() ? &submeshes_[index] : nullptr; }
    const Submesh* submesh(size_t index) const { return const_cast<tinyMesh*>(this)->submesh(index); }

    const glm::vec3& ABmin() const { return ABmin_; }
    const glm::vec3& ABmax() const { return ABmax_; }
    void setABmin(const glm::vec3& abMin) { ABmin_ = abMin; }
    void setABmax(const glm::vec3& abMax) { ABmax_ = abMax; }

private:

    std::vector<Submesh> submeshes_;

    glm::vec3 ABmin_ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 ABmax_ = glm::vec3(std::numeric_limits<float>::lowest());

    tinyVk::DescSet mrphDltsDescSet_;
    tinyVk::DataBuffer mrphDltsBuffer_;

    // void vkCreate(const tinyVk::Device* device, VkDescriptorSetLayout mrphLayout = VK_NULL_HANDLE, VkDescriptorPool mrphPool = VK_NULL_HANDLE) {


    // // Morph deltas

    //     if (mrphTargets__.empty() ||
    //         mrphLayout == VK_NULL_HANDLE ||
    //         mrphPool == VK_NULL_HANDLE) {
    //         clearData(); // Optional
    //         return; // Can't create morph data
    //     }

    //     hasMorphs_ = true;

    //     mrphDeltas_.resize(mrphCount_ * vrtxCount__);
    //     for (size_t i = 0; i < mrphCount_; ++i) {
    //         // Should not happen, hopefully
    //         assert(mrphTargets__[i].deltas.size() == vrtxCount__ && "Morph target delta count must match vertex count");

    //         std::memcpy(
    //             mrphDeltas_.data() + i * vrtxCount__,
    //             mrphTargets__[i].deltas.data(),
    //             vrtxCount__ * sizeof(tinyMorph::Delta)
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
};