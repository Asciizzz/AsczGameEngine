#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <glm/glm.hpp>

#include "AzVulk/Buffer.hpp"

namespace Az3D {

    struct MaterialUBO {
        alignas(16) glm::vec4 prop1;

        MaterialUBO() : prop1(1.0f, 0.0f, 0.0f, 0.0f) {}
        MaterialUBO(const glm::vec4& p1) : prop1(p1) {}
    };

    struct Material {
        // Generic material properties using vec4 for alignment and flexibility
        // Put this FIRST to ensure proper alignment
        glm::vec4 prop1 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
        
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
            auto defaultMaterial = MakeShared<Material>();
            materials.push_back(defaultMaterial);
            count = 1; // Start with one default material
        }
        MaterialManager(const MaterialManager&) = delete;
        MaterialManager& operator=(const MaterialManager&) = delete;

        size_t addMaterial(const Material& material);

        // Material storage - index-based
        size_t count = 0; // Track the number of materials
        SharedPtrVec<Material> materials;
    };
    
} // namespace Az3D
