#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>

// Forward declarations
namespace tinyobj {
    struct material_t;
}

namespace Az3D {
    
    // Forward declaration
    class Texture;
    
    // Material properties structure
    struct MaterialProperties {
        // Basic material properties
        glm::vec3 albedoColor{1.0f, 1.0f, 1.0f};     // Base color tint
        glm::vec3 emissiveColor{0.0f, 0.0f, 0.0f};   // Emissive color
        
        // PBR properties
        float roughness = 0.5f;      // Surface roughness [0.0, 1.0]
        float metallic = 0.0f;       // Metallic factor [0.0, 1.0]
        float specular = 0.5f;       // Specular intensity
        float opacity = 1.0f;        // Transparency [0.0, 1.0]
        
        // Lighting properties
        float normalIntensity = 1.0f; // Normal map intensity
        float aoIntensity = 1.0f;     // Ambient occlusion intensity
    };
    
    // Material class - contains textures and material properties
    class Material {
    public:
        Material(const std::string& name = "DefaultMaterial");
        ~Material() = default;
        
        // Copy and move semantics
        Material(const Material& other) = default;
        Material& operator=(const Material& other) = default;
        Material(Material&& other) noexcept = default;
        Material& operator=(Material&& other) noexcept = default;
        
        // Material identification
        const std::string& getName() const { return name; }
        void setName(const std::string& materialName) { name = materialName; }
        
        // Material properties
        MaterialProperties& getProperties() { return properties; }
        const MaterialProperties& getProperties() const { return properties; }
        
        // Texture management - using texture IDs for flexibility
        void setDiffuseTexture(const std::string& textureId);
        void setNormalTexture(const std::string& textureId);
        void setSpecularTexture(const std::string& textureId);
        void setRoughnessTexture(const std::string& textureId);
        void setMetallicTexture(const std::string& textureId);
        void setAOTexture(const std::string& textureId);
        void setEmissiveTexture(const std::string& textureId);
        
        // Texture getters
        const std::string& getDiffuseTexture() const { return diffuseTextureId; }
        const std::string& getNormalTexture() const { return normalTextureId; }
        const std::string& getSpecularTexture() const { return specularTextureId; }
        const std::string& getRoughnessTexture() const { return roughnessTextureId; }
        const std::string& getMetallicTexture() const { return metallicTextureId; }
        const std::string& getAOTexture() const { return aoTextureId; }
        const std::string& getEmissiveTexture() const { return emissiveTextureId; }
        
        // Utility methods
        bool hasDiffuseTexture() const { return !diffuseTextureId.empty(); }
        bool hasNormalTexture() const { return !normalTextureId.empty(); }
        bool hasSpecularTexture() const { return !specularTextureId.empty(); }
        bool hasRoughnessTexture() const { return !roughnessTextureId.empty(); }
        bool hasMetallicTexture() const { return !metallicTextureId.empty(); }
        bool hasAOTexture() const { return !aoTextureId.empty(); }
        bool hasEmissiveTexture() const { return !emissiveTextureId.empty(); }
        
        // Create material from OBJ material data (tiny_obj_loader integration)
        static std::shared_ptr<Material> createFromOBJ(const tinyobj::material_t& objMaterial);
        
    private:
        std::string name;
        MaterialProperties properties;
        
        // Texture references by ID (managed by TextureManager)
        std::string diffuseTextureId;     // Albedo/Diffuse map
        std::string normalTextureId;      // Normal map
        std::string specularTextureId;    // Specular map
        std::string roughnessTextureId;   // Roughness map
        std::string metallicTextureId;    // Metallic map
        std::string aoTextureId;          // Ambient Occlusion map
        std::string emissiveTextureId;    // Emissive map
    };
    
} // namespace Az3D
