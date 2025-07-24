#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Az3D {

    struct Material {
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

        // Texture references by ID
        int diffTxtr = 0;     // Albedo/Diffuse map
        int nrmlTxtr = 0;     // Normal map
        int specTxtr = 0;     // Specular map
        int roughTxtr = 0;    // Roughness map
        int metalTxtr = 0;    // Metallic map
        int amocTxtr = 0;     // Ambient Occlusion map
        int emssTxtr = 0;     // Emissive map
    };


    // MaterialManager - manages materials using index-based access
    class MaterialManager {
    public:
        MaterialManager() = default;
        
        // Delete copy constructor and assignment operator
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        size_t addMaterial(Material* material);

        // Material storage - index-based
        std::vector<std::shared_ptr<Material>> materials = { 
            std::make_shared<Material>()
        };
    };
    
} // namespace Az3D
