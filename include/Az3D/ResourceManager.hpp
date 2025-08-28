#pragma once

#include "Az3D/Texture.hpp"
#include "Az3D/Material.hpp"
#include "Az3D/MeshStatic.hpp"
#include "Az3D/MeshSkinned.hpp"


namespace Az3D {

// All these resource are static and fixed, created upon load
class ResourceManager {
public:
    ResourceManager(AzVulk::Device* vkDevice);
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    size_t addTexture(std::string name, std::string imagePath,
                    Texture::Mode addressMode = Texture::Repeat,
                    uint32_t mipLevels = 0);
    size_t addMaterial(std::string name, const Material& material);

    size_t addMeshStatic(std::string name, SharedPtr<MeshStatic> mesh, bool hasBVH = false);
    size_t addMeshStatic(std::string name, std::string filePath, bool hasBVH = false);

    // String-to-index getters
    size_t getTextureIndex(std::string name) const;
    size_t getMaterialIndex(std::string name) const;
    size_t getMeshStaticIndex(std::string name) const;

    MeshStatic* getMeshStatic(std::string name) const;
    Material* getMaterial(std::string name) const;
    Texture* getTexture(std::string name) const;

    // String-to-index maps
    UnorderedMap<std::string, size_t> textureNameToIndex;
    UnorderedMap<std::string, size_t> materialNameToIndex;
    UnorderedMap<std::string, size_t> meshStaticNameToIndex;

    UniquePtr<MeshStaticGroup> meshStaticGroup;
    UniquePtr<TextureGroup> textureGroup;
    UniquePtr<MaterialGroup> materialGroup;
};

} // namespace Az3D
