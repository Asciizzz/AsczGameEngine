#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshStatic.hpp"
#include "Az3D/MeshSkinned.hpp"

#include "AzVulk/Buffer.hpp"
#include "AzVulk/Descriptor.hpp"


namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceGroup {
public:
    ResourceGroup(AzVulk::Device* vkDevice);
    ~ResourceGroup() { cleanup(); } void cleanup();

    ResourceGroup(const ResourceGroup&) = delete;
    ResourceGroup& operator=(const ResourceGroup&) = delete;

    size_t addTexture(std::string name, std::string imagePath, uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const Material& material);

    size_t addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH = false);
    size_t addMeshStatic(std::string name, std::string filePath, bool hasBVH = false);

    size_t addMeshSkinned(std::string name, std::string filePath);


    const VkDescriptorSetLayout getMatDescLayout() const { return matDescLayout.get(); }
    const VkDescriptorSetLayout getTexDescLayout() const { return texDescLayout.get(); }

    const VkDescriptorSet getMatDescSet() const { return matDescSet.get(); }
    const VkDescriptorSet getTexDescSet() const { return texDescSet.get(); }


    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getMeshStaticIndex(std::string name) const;
    size_t getMeshSkinnedIndex(std::string name) const;

    Texture* getTexture(std::string name) const;
    Material* getMaterial(std::string name) const;
    MeshStatic* getMeshStatic(std::string name) const;
    MeshSkinned* getMeshSkinned(std::string name) const;

    void uploadAllToGPU();

// private: i dont care about safety
    AzVulk::Device* vkDevice;

    // Mesh static
    SharedPtrVec<MeshStatic>        meshStatics;
    std::vector<AzVulk::BufferData> vstaticBuffers;
    std::vector<AzVulk::BufferData> istaticBuffers;
    void createMeshStaticBuffers();

    // Material (some complication stopping it from being a shared_ptr)
    std::vector<Material> materials;
    AzVulk::BufferData    matBuffer;
    AzVulk::DescLayout    matDescLayout;
    AzVulk::DescPool      matDescPool;
    AzVulk::DescSets      matDescSet;
    void createMaterialBuffer(); // One big buffer for all
    void createMaterialDescSet(); // Only need one

    // Texture Sampler
    std::vector<VkSampler> samplers;
    void createSamplers();

    // Texture Image
    SharedPtrVec<Texture> textures;
    AzVulk::DescLayout    texDescLayout;
    AzVulk::DescPool      texDescPool;
    AzVulk::DescSets      texDescSet;
    void createTextureDescSet();

    // Texture methods
    SharedPtr<Texture> createTexture(std::string imagePath, uint32_t mipLevels = 0);
    void createSinglePixel(uint8_t r, uint8_t g, uint8_t b);
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format,
                    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkImage& image, VkDeviceMemory& imageMemory);
    void createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);
    void transitionImageLayout( VkImage image, VkFormat format, VkImageLayout oldLayout,
                                VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);


    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> meshStaticNameToIndex;
    UnorderedMap<std::string, size_t> meshSkinnedNameToIndex;

    UniquePtr<MeshSkinnedGroup> meshSkinnedGroup;
};

} // namespace Az3D
