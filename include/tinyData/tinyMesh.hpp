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
        glm::vec4 dPos;
        glm::vec4 dNrml;
        glm::vec4 dTang;
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

    inline bool operator&(Type a, Type b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
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
            vrtxTypes |= tinyVertex::Type::Static;
            vrtxCount = stas.size();
            vstaticData = stas;

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

        Submesh& setIndxs(const std::vector<uint32_t>& indxs) {
            indxCount = indxs.size();
            indxData = indxs;
            return *this;
        }

        void clearCPU() {
            vstaticData.clear();
            indxData.clear();
            vriggedData.clear();
            vcolorData.clear();

            // Only clear CPU data, keep name
            for (auto& mt : mrphTargets) {
                mt.morphs.clear();
            }
        }

        void expandABmin(const glm::vec3& point) { ABmin = glm::min(ABmin, point); }
        void expandABmax(const glm::vec3& point) { ABmax = glm::max(ABmax, point); }
        void expandAABB(const glm::vec3& point) {
            expandABmin(point);
            expandABmax(point);
        }

        friend class tinyMesh; // Not needed but good to have

        tinyVertex::Type vrtxTypes = tinyVertex::Type::Static;
        uint32_t vrtxFlags() const { return static_cast<uint32_t>(vrtxTypes); }

        std::vector<tinyVertex::Static> vstaticData; // Required
        std::vector<uint32_t>           indxData;    // Required

        std::vector<tinyVertex::Rigged> vriggedData; // Optional
        std::vector<tinyVertex::Color>  vcolorData;  // Optional

        std::vector<MorphTarget> mrphTargets; // Optional

        tinyHandle material;

        uint32_t vrtxCount = 0;
        uint32_t indxCount = 0;

        uint32_t vstaticOffset;
        uint32_t vriggedOffset;
        uint32_t vcolorOffset;
        uint32_t indxOffset;

        uint32_t vmrphsOffset;

        glm::vec3 ABmin = glm::vec3( std::numeric_limits<float>::max());
        glm::vec3 ABmax = glm::vec3(-std::numeric_limits<float>::max());

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
        ABmin_ = glm::min(ABmin_, submesh.ABmin);
        ABmax_ = glm::max(ABmax_, submesh.ABmax);

        submeshes_.push_back(std::move(submesh));
        return submeshes_.size() - 1;
    }

    std::vector<tinyMesh::Submesh>&       submeshes()       { return submeshes_; }
    const std::vector<tinyMesh::Submesh>& submeshes() const { return submeshes_; }

    Submesh* submesh(size_t index) { return index < submeshes_.size() ? &submeshes_[index] : nullptr; }
    const Submesh* submesh(size_t index) const { return const_cast<tinyMesh*>(this)->submesh(index); }

    const glm::vec3& ABmin() const { return ABmin_; }
    const glm::vec3& ABmax() const { return ABmax_; }
    void setABmin(const glm::vec3& abMin) { ABmin_ = abMin; }
    void setABmax(const glm::vec3& abMax) { ABmax_ = abMax; }

    static constexpr uint32_t MAX_VERTEX_EXTENSIONS = 32768; // No chance in hell we reach this lmao

    static void createVertexExtensionDescriptors(
        const tinyVk::Device* dvk,
        tinyVk::DescSLayout* layout,
        tinyVk::DescPool*    pool
    ) {
        using namespace tinyVk;

        if (!dvk || !layout || !pool) return;

        layout->create(dvk->device, {
            // Rig vertex buffer
            VkDescriptorSetLayoutBinding{ 0, DescType::StorageBuffer, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
            // Color vertex buffer
            VkDescriptorSetLayoutBinding{ 1, DescType::StorageBuffer, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
            // Morph vertex buffer
            VkDescriptorSetLayoutBinding{ 2, DescType::StorageBuffer, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr }
        });

        pool->create(dvk->device, {
            VkDescriptorPoolSize{ DescType::StorageBuffer, 3 }
        }, MAX_VERTEX_EXTENSIONS * 3);
    }

    void vkCreate(const tinyVk::Device* dvk_, VkDescriptorSetLayout vrtxExtLayout = VK_NULL_HANDLE, VkDescriptorPool vrtxExtPool = VK_NULL_HANDLE) {
        using namespace tinyVk;
        
        uint32_t    totalStaticCount = 0,
                    totalRiggedCount = 0,
                    totalColorCount  = 0,
                    totalDeltaCount  = 0,
                    totalIndexCount  = 0;

        // Iterate through submeshes to calculate total sizes and setup ranges
        for (size_t subIdx = 0; subIdx < submeshes_.size(); ++subIdx) {
            auto& submesh = submeshes_[subIdx];
            // Setup ranges
            submesh.vstaticOffset = totalStaticCount;
            totalStaticCount    += submesh.vrtxCount;

            if (submesh.vrtxTypes & tinyVertex::Type::Rig) {
                submesh.vriggedOffset = totalRiggedCount;
                totalRiggedCount    += submesh.vrtxCount;
            }

            if (submesh.vrtxTypes & tinyVertex::Type::Color) {
                submesh.vcolorOffset = totalColorCount;
                totalColorCount    += submesh.vrtxCount;
            }

            if (submesh.vrtxTypes & tinyVertex::Type::Morph) {
                uint32_t targetCount = submesh.mrphTargets.size();
                submesh.vmrphsOffset = totalDeltaCount;
                totalDeltaCount    += targetCount * submesh.vrtxCount;
            }

            submesh.indxOffset = totalIndexCount;
            totalIndexCount  += submesh.indxCount;
        }

        // Setup buffers

        auto createBuffer = [&](DataBuffer& buffer, size_t dataSize, VkBufferUsageFlags usage, const void* data) {
            if (dataSize == 0) return;
            buffer
                .setDataSize(dataSize)
                .setUsageFlags(usage)
                .createDeviceLocalBuffer(dvk_, data);
        };

        bool vhasRigged = totalRiggedCount > 0;
        bool vhasColor  = totalColorCount  > 0;
        bool vhasMorph  = totalDeltaCount  > 0;

        bool vhasExt = vhasRigged || vhasColor || vhasMorph;

        // Avoid zero-sized buffer creation
        totalRiggedCount = vhasRigged ? totalRiggedCount : 1;
        totalColorCount  = vhasColor  ? totalColorCount  : 1;
        totalDeltaCount  = vhasMorph  ? totalDeltaCount  : 1;

        std::vector<tinyVertex::Static> vstaticRaw(totalStaticCount);
        std::vector<uint32_t>           indxRaw   (totalIndexCount);
        std::vector<tinyVertex::Rigged> vriggedRaw(totalRiggedCount);
        std::vector<tinyVertex::Color>  vcolorRaw (totalColorCount);
        std::vector<tinyVertex::Morph>  vmrphsRaw (totalDeltaCount);

        // for (auto& submesh : submeshes_) {
        for (size_t subIdx = 0; subIdx < submeshes_.size(); ++subIdx) {
            auto& submesh = submeshes_[subIdx];

        // Data copy
            // Copy static vertices
            std::memcpy(
                vstaticRaw.data() + submesh.vstaticOffset,
                submesh.vstaticData.data(),
                submesh.vrtxCount * sizeof(tinyVertex::Static)
            );

            // Copy rigged vertices
            if (submesh.vrtxTypes & tinyVertex::Type::Rig) {
                std::memcpy(
                    vriggedRaw.data() + submesh.vriggedOffset,
                    submesh.vriggedData.data(),
                    submesh.vrtxCount * sizeof(tinyVertex::Rigged)
                );
            }

            // Copy color vertices
            if (submesh.vrtxTypes & tinyVertex::Type::Color) {
                std::memcpy(
                    vcolorRaw.data() + submesh.vcolorOffset,
                    submesh.vcolorData.data(),
                    submesh.vrtxCount * sizeof(tinyVertex::Color)
                );
            }

            // Copy morph targets
            if (submesh.vrtxTypes & tinyVertex::Type::Morph) {
                for (uint32_t i = 0; i < submesh.mrphTargets.size(); ++i) {
                    const auto& target = submesh.mrphTargets[i];
                    std::memcpy(
                        vmrphsRaw.data() + submesh.vmrphsOffset + i * submesh.vrtxCount,
                        target.morphs.data(),
                        submesh.vrtxCount * sizeof(tinyVertex::Morph)
                    );
                }
            }

            // Copy indices
            std::memcpy(
                indxRaw.data() + submesh.indxOffset,
                submesh.indxData.data(),
                submesh.indxCount * sizeof(uint32_t)
            );

        // Other

            // Clear CPU data to save memory
            submesh.clearCPU();
        }

        createBuffer(vstaticBuffer_, vstaticRaw.size() * sizeof(tinyVertex::Static), BufferUsage::Vertex,  vstaticRaw.data());
        createBuffer(indxBuffer_,    indxRaw.size()    * sizeof(uint32_t),           BufferUsage::Index,   indxRaw.data());
        createBuffer(vriggedBuffer_, totalRiggedCount  * sizeof(tinyVertex::Rigged), BufferUsage::Storage, vriggedRaw.data());
        createBuffer(vcolorBuffer_,  totalColorCount   * sizeof(tinyVertex::Color),  BufferUsage::Storage, vcolorRaw.data());
        createBuffer(vmrphsBuffer_,  totalDeltaCount   * sizeof(tinyVertex::Morph),  BufferUsage::Storage, vmrphsRaw.data());

        if (vrtxExtLayout == VK_NULL_HANDLE || vrtxExtPool == VK_NULL_HANDLE) return; // Can't create descriptor set

        // if (!vhasExt) return; // No need to create descriptor set

        vrtxExt_.allocate(dvk_->device, vrtxExtPool, vrtxExtLayout);

        // Write descriptor set
        DescWrite()
            .addWrite()
                .setDstSet(vrtxExt_).setType(DescType::StorageBuffer)
                .setDstBinding(0).setBufferInfo({ VkDescriptorBufferInfo{
                    vriggedBuffer_, 0, VK_WHOLE_SIZE
                } })
            .addWrite()
                .setDstSet(vrtxExt_).setType(DescType::StorageBuffer)
                .setDstBinding(1).setBufferInfo({ VkDescriptorBufferInfo{
                    vcolorBuffer_, 0, VK_WHOLE_SIZE
                } })
            .addWrite()
                .setDstSet(vrtxExt_).setType(DescType::StorageBuffer)
                .setDstBinding(2).setBufferInfo({ VkDescriptorBufferInfo{
                    vmrphsBuffer_, 0, VK_WHOLE_SIZE
                } })
            .updateDescSets(dvk_->device);
    }

    VkBuffer vstaticBuffer()     const { return vstaticBuffer_; }
    VkBuffer indxBuffer()        const { return indxBuffer_; }
    VkDescriptorSet vrtxExtSet() const { return vrtxExt_; }

private:
    std::vector<Submesh> submeshes_;

    glm::vec3 ABmin_ = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 ABmax_ = glm::vec3(std::numeric_limits<float>::lowest());

    tinyVk::DescSet mrphDltsDescSet_;
    tinyVk::DataBuffer mrphDltsBuffer_;

    tinyVk::DataBuffer vstaticBuffer_; // Bind vertex buffers
    tinyVk::DataBuffer indxBuffer_;    // Bind index buffer

    tinyVk::DescSet    vrtxExt_;       // Bind descriptor set for vertex extensions
    tinyVk::DataBuffer vriggedBuffer_;
    tinyVk::DataBuffer vcolorBuffer_;
    tinyVk::DataBuffer vmrphsBuffer_;
};