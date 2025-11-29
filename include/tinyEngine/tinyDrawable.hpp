#pragma once

#include <unordered_set>

#include "tinyRegistry.hpp"

#include "tinyData/tinyMesh.hpp"
#include "tinyData/tinyMaterial.hpp"
#include "tinyData/tinyTexture.hpp"

/* RENDER RULES:

Instance Data: {
    mat4 model matrix,
    uvec4 props {
        x: skin offset
        y: morph offset
        z: material override index
        w: unused
    }
}

*/

class tinyDrawable {
public:
    static constexpr size_t MAX_INSTANCES = 100000; // 8mb - more than enough
    static constexpr size_t MAX_MATERIALS = 10000;  // 0.96mb - more than enough
    static constexpr size_t MAX_TEXTURES  = 65536;  // Hopefully not needing this many
    static constexpr size_t MAX_BONES     = 102400; // 6.5mb ~ 400 model x 256 bones x 64 bytes (mat4) - plenty
    static constexpr size_t MAX_MORPH_WS  = 65536;  // Morph WEIGHTS, not Delta, 65536 x 4 bytes = 256kb, literally invisible

    static std::vector<VkVertexInputBindingDescription> bindingDesc() noexcept;
    static std::vector<VkVertexInputAttributeDescription> attributeDescs() noexcept;

    struct CreateInfo {
        uint32_t maxFramesInFlight = 2;
        tinyRegistry* fsr = nullptr;
        const tinyVk::Device* dvk = nullptr;
    };

    struct Size_x1 { // Per-frame
        VkDeviceSize aligned = 0;   // Data aligned to min offset alignment
        VkDeviceSize unaligned = 0; // Actual data size to copy
    };

    struct InstaData {
        glm::mat4 model = glm::mat4(1.0f); // Model matrix
        glm::uvec4 other = glm::uvec4(0); // Additional data
    };

    struct Entry {
        tinyHandle mesh;
        size_t submesh;

        inline tinyHandle hash() const {
            tinyHandle h{};
            uint64_t x = mesh.value;

            // A simple 64-bit mixing function (very cheap, very good)
            x ^= uint64_t(submesh) + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2);

            h.value = x;
            return h;
        }

        glm::mat4 model = glm::mat4(1.0f);

        struct SkeleData { // Can be shared among multiple entries
            tinyHandle skeleNode; // Cache this handle
            const std::vector<glm::mat4>* skinData = nullptr;
        } skeleData;

        struct MorphData { // Unique per entry
            const std::vector<float>* weights = nullptr;
            uint32_t offset = 0;
            uint32_t count  = 0;
        } morphData;
    };

    struct SubmeshGroup {
        uint32_t submesh = 0;
        std::vector<InstaData> instaData;

        inline size_t push(const InstaData& data) {
            instaData.push_back(data);
            return instaData.size() - 1;
        }

        inline void clear() { instaData.clear(); }
        inline size_t size() const { return instaData.size(); }
        inline size_t sizeBytes() const { return instaData.size() * sizeof(InstaData); }

        // Calculated during finalize
        uint32_t instaOffset = 0;
        uint32_t instaCount  = 0;
    };

    struct MeshGroup {
        tinyHandle mesh;
        std::vector<size_t> submeshGroupIndices;
        std::unordered_map<size_t, size_t> submeshGroupMap; // Submesh index -> SubmeshGroup index
    };

    struct ShaderGroup {
        tinyHandle shader;
        std::vector<size_t> meshGroupIndices;
        std::unordered_map<tinyHandle, size_t> meshGroupMap; // Mesh handle -> MeshGroup index
    };

    struct SkinRange {
        uint32_t skinOffset = 0;
        uint32_t skinCount = 0;
    };

// ---------------------------------------------------------------

    tinyDrawable() noexcept = default;
    void init(const CreateInfo& info);

    tinyDrawable(const tinyDrawable&) = delete;
    tinyDrawable& operator=(const tinyDrawable&) = delete;

    tinyDrawable(tinyDrawable&&) noexcept = default;
    tinyDrawable& operator=(tinyDrawable&&) noexcept = default;

// --------------------------- Basic Getters ----------------------------

    uint32_t maxFramesInFlight() const noexcept { return maxFramesInFlight_; }
    uint32_t frameIndex() const noexcept { return frameIndex_; }

    tinyRegistry& fsr() noexcept { return *fsr_; }
    const tinyRegistry& fsr() const noexcept { return *fsr_; }

    // Vulkan resources

    VkBuffer instaBuffer() const noexcept { return instaBuffer_; }

    VkDescriptorSet matDescSet() const noexcept { return matDescSet_; }
    VkDescriptorSetLayout matDescLayout() const noexcept { return matDescLayout_; }

    VkDescriptorSet skinDescSet() const noexcept { return skinDescSet_; }
    VkDescriptorSetLayout skinDescLayout() const noexcept { return skinDescLayout_; }

    VkDescriptorSet mrphWsDescSet() const noexcept { return mrphWsDescSet_; }
    VkDescriptorSetLayout mrphWsDescLayout() const noexcept { return mrphWsDescLayout_; }

    uint32_t matIndex(tinyHandle matHandle) const noexcept {
        auto it = dataMap_.find(matHandle);
        return (it != dataMap_.end()) ? it->second : 0;
    }

    Size_x1 instaSize_x1() const noexcept { return instaSize_x1_; }
    Size_x1 matSize_x1() const noexcept { return matSize_x1_; }
    Size_x1 skinSize_x1() const noexcept { return skinSize_x1_; }
    Size_x1 mrphWsSize_x1() const noexcept { return mrphWsSize_x1_; }

    inline VkDeviceSize instaOffset(uint32_t frameIndex) const noexcept { return frameIndex * instaSize_x1_.aligned; }
    inline VkDeviceSize matOffset(uint32_t frameIndex) const noexcept { return frameIndex * matSize_x1_.aligned; }
    inline VkDeviceSize skinOffset(uint32_t frameIndex) const noexcept { return frameIndex * skinSize_x1_.aligned; }
    inline VkDeviceSize mrphWsOffset(uint32_t frameIndex) const noexcept { return frameIndex * mrphWsSize_x1_.aligned; }


    VkDescriptorSetLayout vrtxExtLayout() const noexcept { return vrtxExtLayout_; }
    VkDescriptorPool      vrtxExtPool()   const noexcept { return vrtxExtPool_; }

// --------------------------- Bacthking --------------------------

    void startFrame(uint32_t frameIndex) noexcept;
    void submit(const Entry& entry) noexcept;
    void finalize() noexcept;

    const std::vector<ShaderGroup>& shaderGroups() const noexcept { return shaderGroups_; }
    const std::vector<MeshGroup>& meshGroups() const noexcept { return meshGroups_; }
    const std::vector<SubmeshGroup>& submeshGroups() const noexcept { return submeshGroups_; }

// --------------------------- Other --------------------------

    uint32_t addTexture(tinyHandle texHandle) noexcept;

    struct Dummy {
        tinyMesh mesh;
    };

    const Dummy& dummy() const noexcept { return dummy_; }

private:
// Basic info
    uint32_t maxFramesInFlight_ = 2;
    uint32_t frameIndex_ = 0;

    tinyRegistry* fsr_ = nullptr;
    const tinyVk::Device* dvk_ = nullptr;

    // Batching data (reset each frame)
    std::vector<ShaderGroup>  shaderGroups_;
    std::vector<MeshGroup>    meshGroups_;
    std::vector<SubmeshGroup> submeshGroups_;

    // Runtime data
    uint32_t skinCount_ = 0;
    uint32_t mrphWsCount_ = 0;

    std::vector<tinyMaterial::Data> matData_;
    std::vector<SkinRange> skinRanges_;
    
    /* batchMap_:

    Material handle -> ShaderGroup index
    Entry hash -> SubmeshGroup index
    */
    std::unordered_map<tinyHandle, size_t> batchMap_;
    std::unordered_map<tinyHandle, size_t> dataMap_;

    // Instances (runtime)
    tinyVk::DataBuffer instaBuffer_;
    Size_x1            instaSize_x1_;

    // Materials (runtime)
    tinyVk::DescSLayout matDescLayout_;
    tinyVk::DescPool    matDescPool_;
    tinyVk::DescSet     matDescSet_;
    tinyVk::DataBuffer  matBuffer_;
    Size_x1             matSize_x1_;

    // Textures (static)
    tinyVk::DescSLayout texDescLayout_;
    tinyVk::DescPool    texDescPool_;
    tinyVk::DescSet     texDescSet_; // Dynamic descriptor set for textures
    std::unordered_map<tinyHandle, uint32_t> texIdxMap_;
    std::vector<tinyVk::SamplerVk> texSamplers_;

    // Skinning (runtime)
    tinyVk::DescSLayout skinDescLayout_;
    tinyVk::DescPool    skinDescPool_;
    tinyVk::DescSet     skinDescSet_;
    tinyVk::DataBuffer  skinBuffer_;
    Size_x1             skinSize_x1_;

    // Morph Weights (runtime)
    tinyVk::DescSLayout mrphWsDescLayout_;
    tinyVk::DescPool    mrphWsDescPool_;
    tinyVk::DescSet     mrphWsDescSet_;
    tinyVk::DataBuffer  mrphWsBuffer_;
    Size_x1             mrphWsSize_x1_;

// ==== Static default stuff ====

    // Vertex extension
    tinyVk::DescSLayout vrtxExtLayout_;
    tinyVk::DescPool    vrtxExtPool_;

    Dummy dummy_;
};