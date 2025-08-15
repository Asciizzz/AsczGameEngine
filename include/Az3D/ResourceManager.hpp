#pragma once

#include "Az3D/TextureManager.hpp"
#include "Az3D/MaterialManager.hpp"
#include "Az3D/MeshManager.hpp"


namespace Az3D {

    // All these resource are static and fixed, created upon load
    class ResourceManager {
    public:
        ResourceManager(const AzVulk::Device& device, VkCommandPool commandPool);
        ~ResourceManager() = default;

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;

        size_t addTexture(std::string name, std::string imagePath,
                        Texture::Mode addressMode = Texture::Repeat,
                        bool semiTransparent = false);
        size_t addMaterial(std::string name, const Material& material);

        size_t addMesh(std::string name, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, bool hasBVH = false);
        size_t addMesh(std::string name, SharedPtr<Mesh> mesh, bool hasBVH = false);
        size_t addMesh(std::string name, std::string filePath, bool hasBVH = false);

        // String-to-index getters
        size_t getTexture(std::string name) const;
        size_t getMaterial(std::string name) const;
        size_t getMesh(std::string name) const;

        // String-to-index maps
        UnorderedMap<std::string, size_t> textureNameToIndex;
        UnorderedMap<std::string, size_t> materialNameToIndex;
        UnorderedMap<std::string, size_t> meshNameToIndex;

        UniquePtr<MeshManager> meshManager;
        UniquePtr<TextureManager> textureManager;
        UniquePtr<MaterialManager> materialManager;
    };
    
} // namespace Az3D
