#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <iostream>

namespace Az3D {

    struct Material {
        // Generic material properties using vec4 for alignment and flexibility
        // Put this FIRST to ensure proper alignment
        glm::vec4 prop1 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <empty>
        
        // Custom material properties exclusive to AsczGameEngine
        size_t diffTxtr = 0; // Albedo/Diffuse map

        static Material fastTemplate(float a, float b, float c, float d, size_t diffTexture) {
            Material mat;
            mat.prop1 = glm::vec4(a, b, c, d);
            mat.diffTxtr = diffTexture;
            return mat;
        }
    };


    // MaterialManager - manages materials using index-based access
    class MaterialManager {
    public:
        MaterialManager() {
            auto defaultMaterial = std::make_shared<Material>();
            materials.push_back(defaultMaterial);
            count = 1; // Start with one default material
        }
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        size_t addMaterial(const Material& material);

        // Material storage - index-based
        size_t count = 0; // Track the number of materials
        std::vector<std::shared_ptr<Material>> materials;
    };
    
} // namespace Az3D
